/*
 *  Lib(X)SVF  -  A library for implementing SVF and XSVF JTAG players
 *
 *  Copyright (C) 2009  RIEGL Research ForschungsGmbH
 *  Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *    3. The names of the authors may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include "libxsvf.h"

/* command codes as defined in xilinx xapp503 */
enum xsvf_cmd {
	XCOMPLETE       = 0x00,
	XTDOMASK        = 0x01,
	XSIR            = 0x02,
	XSDR            = 0x03,
	XRUNTEST        = 0x04,
	XREPEAT         = 0x07,
	XSDRSIZE        = 0x08,
	XSDRTDO         = 0x09,
	XSETSDRMASKS    = 0x0A,
	XSDRINC         = 0x0B,
	XSDRB           = 0x0C,
	XSDRC           = 0x0D,
	XSDRE           = 0x0E,
	XSDRTDOB        = 0x0F,
	XSDRTDOC        = 0x10,
	XSDRTDOE        = 0x11,
	XSTATE          = 0x12,
	XENDIR          = 0x13,
	XENDDR          = 0x14,
	XSIR2           = 0x15,
	XCOMMENT        = 0x16,
	XWAIT           = 0x17
};

// This is to not confuse the VIM syntax highlighting
#define VAL_OPEN (
#define VAL_CLOSE )

#define READ_BITS(_buf, _len) do {                                          \
	unsigned char *_p = _buf;                                           \
	for (int _i=0; _i<(_len); _i+=8) {                                  \
		int tmp = h->read_next_byte(h);                             \
		if (tmp < 0) {                                              \
			h->report_error(h, __FILE__, __LINE__, "Unexpected EOF."); \
			goto error;                                         \
		}                                                           \
		*(_p++) = tmp;                                              \
	}                                                                   \
} while (0)

#define READ_LONG() VAL_OPEN{                                               \
	long _buf = 0;                                                      \
	for (int _i=0; _i<4; _i++) {                                        \
		int tmp = h->read_next_byte(h);                             \
		if (tmp < 0) {                                              \
			h->report_error(h, __FILE__, __LINE__, "Unexpected EOF."); \
			goto error;                                         \
		}                                                           \
		_buf = _buf << 8 | tmp;                                     \
	}                                                                   \
	_buf;                                                               \
}VAL_CLOSE

#define READ_BYTE() VAL_OPEN{                                               \
	int _tmp = h->read_next_byte(h);                                    \
	if (_tmp < 0) {                                                     \
		h->report_error(h, __FILE__, __LINE__, "Unexpected EOF.");  \
		goto error;                                                 \
	}                                                                   \
	_tmp;                                                               \
}VAL_CLOSE

#define SHIFT_DATA(_inp, _outp, _maskp, _len, _state, _estate, _edelay, _ret) do { \
	if (shift_data(h, _inp, _outp, _maskp, _len, _state, _estate, _edelay, _ret) < 0) { \
		goto error;                                                 \
	}                                                                   \
} while (0)

#define TAP(_state) do {                                                    \
	if (libxsvf_tap_walk(h, _state) < 0)                                \
		goto error;                                                 \
} while (0)

static int bits2bytes(int bits)
{
	return (bits+7) / 8;
}

static int getbit(unsigned char *data, int n)
{
	return (data[n/8] & (1 << (7 - n%8))) ? 1 : 0;
}

static void setbit(unsigned char *data, int n, int v)
{
	unsigned char mask = 1 << (7 - n%8);
	if (v)
		data[n/8] |= mask;
	else
		data[n/8] &= ~mask;
}

static int xilinx_tap(int state)
{
	/* state codes as defined in xilinx xapp503 */
	switch (state)
	{
	case 0x00:
		return LIBXSVF_TAP_RESET;
		break;
	case 0x01:
		return LIBXSVF_TAP_IDLE;
		break;
	case 0x02:
		return LIBXSVF_TAP_DRSELECT;
		break;
	case 0x03:
		return LIBXSVF_TAP_DRCAPTURE;
		break;
	case 0x04:
		return LIBXSVF_TAP_DRSHIFT;
		break;
	case 0x05:
		return LIBXSVF_TAP_DREXIT1;
		break;
	case 0x06:
		return LIBXSVF_TAP_DRPAUSE;
		break;
	case 0x07:
		return LIBXSVF_TAP_DREXIT2;
		break;
	case 0x08:
		return LIBXSVF_TAP_DRUPDATE;
		break;
	case 0x09:
		return LIBXSVF_TAP_IRSELECT;
		break;
	case 0x0A:
		return LIBXSVF_TAP_IRCAPTURE;
		break;
	case 0x0B:
		return LIBXSVF_TAP_IRSHIFT;
		break;
	case 0x0C:
		return LIBXSVF_TAP_IREXIT1;
		break;
	case 0x0D:
		return LIBXSVF_TAP_IRPAUSE;
		break;
	case 0x0E:
		return LIBXSVF_TAP_IREXIT2;
		break;
	case 0x0F:
		return LIBXSVF_TAP_IRUPDATE;
		break;
	}
	return -1;
}

static int shift_data(struct libxsvf_host *h, unsigned char *inp, unsigned char *outp, unsigned char *maskp, int len, enum libxsvf_tap_state state, enum libxsvf_tap_state estate, int edelay, int retries)
{
	int left_padding = (8 - len % 8) % 8;

	while (1)
	{
		int tdo_error = 0;
		int call_report_state = 0;

		TAP(state);
		h->set_tms(h, 0);

		for (int i=len+left_padding-1; i>=left_padding; i--) {
			if (i == left_padding && h->tap_state != estate) {
				h->set_tms(h, 1);
				h->tap_state++;
				call_report_state = 1;
			}
			h->set_tdi(h, getbit(inp, i));
			h->pulse_tck(h);
			int tdo_mask_bit = maskp ? getbit(maskp, i) : 0;
			if (tdo_mask_bit) {
				int tdo_data_bit = outp ? getbit(outp, i) : 0;
				int tdo = h->get_tdo(h);
				if (tdo >= 0 && tdo_data_bit != tdo)
					tdo_error = 1;
			}
		}

		if (call_report_state && h->report_tapstate)
			h->report_tapstate(h);
	
		if (edelay) {
			TAP(LIBXSVF_TAP_IDLE);
			h->udelay(h, edelay);
		} else {
			TAP(estate);
		}

		if (!tdo_error)
			return 0;

		if (retries <= 0) {
			h->report_error(h, __FILE__, __LINE__, "TDO mismatch.");
			return -1;
		}

		retries--;
	}

error:
	return -1;
}

int libxsvf_xsvf(struct libxsvf_host *h)
{
	int rc = 0;

	unsigned char *buf_tdi_data = (void*)0;
	unsigned char *buf_tdo_data = (void*)0;
	unsigned char *buf_tdo_mask = (void*)0;
	unsigned char *buf_addr_mask = (void*)0;
	unsigned char *buf_data_mask = (void*)0;

	long state_dr_size = 0;
	long state_data_size = 0;
	long state_runtest = 0;
	unsigned char state_xendir = 0;
	unsigned char state_xenddr = 0;
	unsigned char state_retries = 0;

	while (1)
	{
		unsigned char cmd = h->read_next_byte(h);

#define STATUS(_c) do { if (h->report_status) h->report_status(h, "XSVF Command " #_c); } while (0)

		switch (cmd)
		{
		case XCOMPLETE: {
			STATUS(XCOMPLETE);
			goto got_complete_command;
		  }
		case XTDOMASK: {
			STATUS(XTDOMASK);
			READ_BITS(buf_tdo_mask, state_dr_size);
			break;
		  }
		case XSIR: {
			STATUS(XSIR);
			int length = READ_BYTE();
			unsigned char buf[bits2bytes(length)];
			READ_BITS(buf, length);
			SHIFT_DATA(buf, (void*)0, (void*)0, length, LIBXSVF_TAP_IRSHIFT,
					state_xendir ? LIBXSVF_TAP_IRPAUSE : LIBXSVF_TAP_IDLE,
					state_runtest, state_retries);
			break;
		  }
		case XSDR: {
			STATUS(XSDR);
			READ_BITS(buf_tdi_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, buf_tdo_data, buf_tdo_mask, state_dr_size, LIBXSVF_TAP_DRSHIFT,
					state_xendir ? LIBXSVF_TAP_DRPAUSE : LIBXSVF_TAP_IDLE,
					state_runtest, state_retries);
			break;
		  }
		case XRUNTEST: {
			STATUS(XRUNTEST);
			state_runtest = READ_LONG();
			break;
		  }
		case XREPEAT: {
			STATUS(XREPEAT);
			state_retries = READ_BYTE();
			break;
		  }
		case XSDRSIZE: {
			STATUS(XSDRSIZE);
			state_dr_size = READ_LONG();
			buf_tdi_data = h->realloc(h, buf_tdi_data, bits2bytes(state_dr_size));
			buf_tdo_data = h->realloc(h, buf_tdo_data, bits2bytes(state_dr_size));
			buf_tdo_mask = h->realloc(h, buf_tdo_mask, bits2bytes(state_dr_size));
			buf_addr_mask = h->realloc(h, buf_addr_mask, bits2bytes(state_dr_size));
			buf_data_mask = h->realloc(h, buf_data_mask, bits2bytes(state_dr_size));
			if (!buf_tdi_data || !buf_tdo_data || !buf_tdo_mask || !buf_addr_mask || !buf_data_mask) {
				h->report_error(h, __FILE__, __LINE__, "Allocating memory failed.");
				goto error;
			}
			break;
		  }
		case XSDRTDO: {
			STATUS(XSDRTDO);
			READ_BITS(buf_tdi_data, state_dr_size);
			READ_BITS(buf_tdo_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, buf_tdo_data, buf_tdo_mask, state_dr_size, LIBXSVF_TAP_DRSHIFT,
					state_xendir ? LIBXSVF_TAP_DRPAUSE : LIBXSVF_TAP_IDLE,
					state_runtest, state_retries);
			break;
		  }
		case XSETSDRMASKS: {
			STATUS(XSETSDRMASKS);
			READ_BITS(buf_addr_mask, state_dr_size);
			READ_BITS(buf_data_mask, state_dr_size);
			state_data_size = 0;
			for (int i=0; i<state_dr_size; i++)
				state_data_size += getbit(buf_data_mask, i);
			break;
		  }
		case XSDRINC: {
			STATUS(XSDRINC);
			READ_BITS(buf_tdi_data, state_dr_size);
			int num = READ_BYTE();
			while (1) {
				SHIFT_DATA(buf_tdi_data, buf_tdo_data, buf_tdo_mask, state_dr_size, LIBXSVF_TAP_DRSHIFT,
						state_xendir ? LIBXSVF_TAP_DRPAUSE : LIBXSVF_TAP_IDLE,
						state_runtest, state_retries);
				if (num-- <= 0)
					break;
				for (int i=state_dr_size-1, carry=1; i>=0; i--) {
					if (getbit(buf_addr_mask, i) == 0)
						continue;
					if (getbit(buf_tdi_data, i)) {
						setbit(buf_tdi_data, i, !carry);
					} else {
						setbit(buf_tdi_data, i, carry);
						carry = 0;
					}
				}
				unsigned char this_byte = 0;
				for (int i=0, j=0; i<state_data_size; i++) {
					if (i%8 == 0)
						this_byte = READ_BYTE();
					while (getbit(buf_data_mask, j) == 0)
						j++;
					setbit(buf_tdi_data, j++, getbit(&this_byte, i%8));
				}
			}
			break;
		  }
		case XSDRB: {
			STATUS(XSDRB);
			READ_BITS(buf_tdi_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, (void*)0, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT, LIBXSVF_TAP_DRSHIFT, 0, 0);
			break;
		  }
		case XSDRC: {
			STATUS(XSDRC);
			READ_BITS(buf_tdi_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, (void*)0, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT, LIBXSVF_TAP_DRSHIFT, 0, 0);
			break;
		  }
		case XSDRE: {
			STATUS(XSDRE);
			READ_BITS(buf_tdi_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, (void*)0, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT,
					state_xendir ? LIBXSVF_TAP_DRPAUSE : LIBXSVF_TAP_IDLE, 0, 0);
			break;
		  }
		case XSDRTDOB: {
			STATUS(XSDRTDOB);
			READ_BITS(buf_tdi_data, state_dr_size);
			READ_BITS(buf_tdo_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, buf_tdo_data, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT, LIBXSVF_TAP_DRSHIFT, 0, 0);
			break;
		  }
		case XSDRTDOC: {
			STATUS(XSDRTDOC);
			READ_BITS(buf_tdi_data, state_dr_size);
			READ_BITS(buf_tdo_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, buf_tdo_data, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT, LIBXSVF_TAP_DRSHIFT, 0, 0);
			break;
		  }
		case XSDRTDOE: {
			STATUS(XSDRTDOE);
			READ_BITS(buf_tdi_data, state_dr_size);
			READ_BITS(buf_tdo_data, state_dr_size);
			SHIFT_DATA(buf_tdi_data, buf_tdo_data, (void*)0, state_dr_size, LIBXSVF_TAP_DRSHIFT,
					state_xendir ? LIBXSVF_TAP_DRPAUSE : LIBXSVF_TAP_IDLE, 0, 0);
			break;
		  }
		case XSTATE: {
			STATUS(XSTATE);
			unsigned char state = READ_BYTE();
			TAP(xilinx_tap(state));
			break;
		  }
		case XENDIR: {
			STATUS(XENDIR);
			state_xendir = READ_BYTE();
			break;
		  }
		case XENDDR: {
			STATUS(XENDDR);
			state_xenddr = READ_BYTE();
			break;
		  }
		case XSIR2: {
			STATUS(XSIR2);
			int length = READ_BYTE();
			length = length << 8 | READ_BYTE();
			unsigned char buf[bits2bytes(length)];
			READ_BITS(buf, length);
			SHIFT_DATA(buf, (void*)0, (void*)0, length, LIBXSVF_TAP_IRSHIFT,
					state_xendir ? LIBXSVF_TAP_IRPAUSE : LIBXSVF_TAP_IDLE,
					state_runtest, state_retries);
			break;
		  }
		case XCOMMENT: {
			STATUS(XCOMMENT);
			unsigned char this_byte;
			do {
				this_byte = READ_BYTE();
			} while (this_byte);
			break;
		  }
		case XWAIT: {
			STATUS(XWAIT);
			unsigned char state1 = READ_BYTE();
			unsigned char state2 = READ_BYTE();
			long usecs = READ_LONG();
			TAP(xilinx_tap(state1));
			h->udelay(h, usecs);
			TAP(xilinx_tap(state2));
			break;
		  }
		default:
			h->report_error(h, __FILE__, __LINE__, "Unknown XSVF command.");
			goto error;
		}
	}

error:
	rc = -1;

got_complete_command:
	h->realloc(h, buf_tdi_data, 0);
	h->realloc(h, buf_tdo_data, 0);
	h->realloc(h, buf_tdo_mask, 0);
	h->realloc(h, buf_addr_mask, 0);
	h->realloc(h, buf_data_mask, 0);

	return rc;
}


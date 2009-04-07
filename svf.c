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

static int read_command(struct libxsvf_host *h, char **buffer_p, int *len_p)
{
	char *buffer = *buffer_p;
	int braket_mode = 0;
	int len = *len_p;
	int p = 0;

	while (1)
	{
		if (len < p+10) {
			len = len < 64 ? 96 : len*2;
			buffer = h->realloc(h, buffer, len);
			*buffer_p = buffer;
			*len_p = len;
			if (!buffer) {
				h->report_error(h, __FILE__, __LINE__, "Allocating memory failed.");
				return -1;
			}
		}
		buffer[p] = 0;

		int ch = h->read_next_byte(h);
		if (ch < 0) {
handle_eof:
			if (p == 0)
				return 0;
			h->report_error(h, __FILE__, __LINE__, "Unexpected EOF.");
			return -1;
		}
		if (ch <= ' ') {
insert_eol:
			if (!braket_mode && p > 0 && buffer[p-1] != ' ')
				buffer[p++] = ' ';
			continue;
		}
		if (ch == '!') {
skip_to_eol:
			while (1) {
				ch = h->read_next_byte(h);
				if (ch < 0)
					goto handle_eof;
				if (ch < ' ' && ch != '\t')
					goto insert_eol;
			}
		}
		if (ch == '/' && p > 0 && buffer[p-1] == '/') {
			p--;
			goto skip_to_eol;
		}
		if (ch == ';')
			break;
		if (ch == '(') {
			if (!braket_mode && p > 0 && buffer[p-1] != ' ')
				buffer[p++] = ' ';
			braket_mode++;
		}
		if (ch >= 'a' && ch <= 'z')
			buffer[p++] = ch - ('a' - 'A');
		else
			buffer[p++] = ch;
		if (ch == ')') {
			braket_mode--;
			if (!braket_mode)
				buffer[p++] = ' ';
		}
	}
	return 1;
}

static int strtokencmp(const char *str1, const char *str2)
{
	int i = 0;
	while (1) {
		if ((str1[i] == ' ' || str1[i] == 0) && (str2[i] == ' ' || str2[i] == 0))
			return 0;
		if (str1[i] < str2[i])
			return -1;
		if (str1[i] > str2[i])
			return +1;
		i++;
	}
}

static int strtokenskip(const char *str1)
{
	int i = 0;
	while (str1[i] != 0 && str1[i] != ' ') i++;
	while (str1[i] == ' ') i++;
	return i;
}

static int token2tapstate(const char *str1)
{
#define X(_t) if (!strtokencmp(str1, #_t)) return LIBXSVF_TAP_ ## _t;
	X(RESET)
	X(IDLE)
	X(DRSELECT)
	X(DRCAPTURE)
	X(DRSHIFT)
	X(DREXIT1)
	X(DRPAUSE)
	X(DREXIT2)
	X(DRUPDATE)
	X(IRSELECT)
	X(IRCAPTURE)
	X(IRSHIFT)
	X(IREXIT1)
	X(IRPAUSE)
	X(IREXIT2)
	X(IRUPDATE)
#undef X
	return -1;
}

struct bitdata_s {
	int len, alloced_len;
	int alloced_bytes;
	unsigned char *tdi_data;
	unsigned char *tdi_mask;
	unsigned char *tdo_data;
	unsigned char *tdo_mask;
	unsigned char *ret_mask;
};

static void bitdata_free(struct libxsvf_host *h, struct bitdata_s *bd)
{
	h->realloc(h, bd->tdi_data, 0);
	h->realloc(h, bd->tdi_mask, 0);
	h->realloc(h, bd->tdo_data, 0);
	h->realloc(h, bd->tdo_mask, 0);
	h->realloc(h, bd->ret_mask, 0);

	bd->tdi_data = (void*)0;
	bd->tdi_mask = (void*)0;
	bd->tdo_data = (void*)0;
	bd->tdo_mask = (void*)0;
	bd->ret_mask = (void*)0;
}

static int hex(char ch)
{
	if (ch >= 'A' && ch <= 'Z')
		return (ch - 'A') + 10;
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	return 0;
}

static const char *bitdata_parse(struct libxsvf_host *h, const char *p, struct bitdata_s *bd)
{
	bd->len = 0;
	while (*p >= '0' && *p <= '9') {
		bd->len = bd->len * 10 + (*p - '0');
		p++;
	}
	while (*p == ' ') {
		p++;
	}
	if (bd->len != bd->alloced_len) {
		bitdata_free(h, bd);
		bd->alloced_len = bd->len;
		bd->alloced_bytes = (bd->len+7) / 8;
	}
	while (*p)
	{
		unsigned char **dp = (void*)0;
		if (!strtokencmp(p, "TDI")) {
			p += strtokenskip(p);
			dp = &bd->tdi_data;
		}
		if (!strtokencmp(p, "TDO")) {
			p += strtokenskip(p);
			dp = &bd->tdo_data;
		}
		if (!strtokencmp(p, "SMASK")) {
			p += strtokenskip(p);
			dp = &bd->tdi_mask;
		}
		if (!strtokencmp(p, "MASK")) {
			p += strtokenskip(p);
			dp = &bd->tdo_mask;
		}
		if (!strtokencmp(p, "RMASK")) {
			p += strtokenskip(p);
			dp = &bd->ret_mask;
		}
		if (!dp)
			return (void*)0;
		if (*dp == (void*)0) {
			*dp = h->realloc(h, *dp, bd->alloced_bytes);
		}
		if (*dp == (void*)0) {
			h->report_error(h, __FILE__, __LINE__, "Allocating memory failed.");
			return (void*)0;
		}

		unsigned char *d = *dp;
		for (int i=0; i<bd->alloced_bytes; i++)
			d[i] = 0;

		if (*p != '(')
			return (void*)0;
		p++;

		int hexdigits = 0;
		for (int i=0; (p[i] >= 'A' && p[i] <= 'F') || (p[i] >= '0' && p[i] <= '9'); i++)
			hexdigits++;

		int i = bd->alloced_bytes*2 - hexdigits;
		for (int j=0; j<hexdigits; j++, i++, p++) {
			if (i%2 == 0) {
				d[i/2] |= hex(*p) << 4;
			} else {
				d[i/2] |= hex(*p);
			}
		}

		if (*p != ')')
			return (void*)0;
		p++;
		while (*p == ' ') {
			p++;
		}
	}
#if 0
	/* Debugging Output, needs <stdio.h> */
	printf("--- Parsed bitdata [%d] ---\n", bd->len);
	if (bd->tdi_data) {
		printf("TDI DATA:");
		for (int i=0; i<bd->alloced_bytes; i++)
			printf(" %02x", bd->tdi_data[i]);
		printf("\n");
	}
	if (bd->tdo_data) {
		printf("TDO DATA:");
		for (int i=0; i<bd->alloced_bytes; i++)
			printf(" %02x", bd->tdo_data[i]);
		printf("\n");
	}
	if (bd->tdi_mask) {
		printf("TDI MASK:");
		for (int i=0; i<bd->alloced_bytes; i++)
			printf(" %02x", bd->tdi_mask[i]);
		printf("\n");
	}
	if (bd->tdo_mask) {
		printf("TDO MASK:");
		for (int i=0; i<bd->alloced_bytes; i++)
			printf(" %02x", bd->tdo_mask[i]);
		printf("\n");
	}
#endif
	return p;
}

static int getbit(unsigned char *data, int n)
{
	return (data[n/8] & (1 << (7 - n%8))) ? 1 : 0;
}

static int bitdata_play(struct libxsvf_host *h, int first, struct bitdata_s *bd, enum libxsvf_tap_state estate)
{
	int left_padding = (8 - bd->len % 8) % 8;
	int tdo_error = 0;
	int call_report_state = 0;

	if (first)
		h->set_tms(h, 0);

	for (int i=bd->len+left_padding-1; i >= left_padding; i--) {
		if (i == left_padding && h->tap_state != estate) {
			h->set_tms(h, 1);
			h->tap_state++;
			call_report_state = 1;
		}
		if (bd->tdi_data) {
			if (!bd->tdi_mask || getbit(bd->tdi_mask, i))
				h->set_tdi(h, getbit(bd->tdi_data, i));
		}
		h->pulse_tck(h);
		if (bd->tdo_data || bd->ret_mask) {
			int tdo_mask_bit = !bd->tdo_mask || getbit(bd->tdo_mask, i);
			int ret_mask_bit = bd->ret_mask && getbit(bd->ret_mask, i);
			if (tdo_mask_bit || ret_mask_bit) {
				int tdo = h->get_tdo(h);
				if (tdo_mask_bit && tdo >= 0 && tdo != getbit(bd->tdo_data, i))
					tdo_error = 1;
				if (ret_mask_bit && h->ret_tdo)
					h->ret_tdo(h, tdo);
			}
		}
	}

	if (call_report_state && h->report_tapstate)
		h->report_tapstate(h);

	if (!tdo_error)
		return 0;

	h->report_error(h, __FILE__, __LINE__, "TDO mismatch.");
	return -1;
}

int libxsvf_svf(struct libxsvf_host *h)
{
	char *command_buffer = (void*)0;
	int command_buffer_len = 0;
	int rc;

	struct bitdata_s bd_hdr = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };
	struct bitdata_s bd_hir = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };
	struct bitdata_s bd_tdr = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };
	struct bitdata_s bd_tir = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };
	struct bitdata_s bd_sdr = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };
	struct bitdata_s bd_sir = { 0, 0, 0, (void*)0, (void*)0, (void*)0, (void*)0, (void*)0 };

	int state_endir = LIBXSVF_TAP_IDLE;
	int state_enddr = LIBXSVF_TAP_IDLE;
	int state_run = LIBXSVF_TAP_IDLE;
	int state_endrun = LIBXSVF_TAP_IDLE;

	while (1)
	{
		rc = read_command(h, &command_buffer, &command_buffer_len);

		if (rc <= 0)
			break;

		const char *p = command_buffer;

		if (h->report_status)
			h->report_status(h, command_buffer);

		if (!strtokencmp(p, "ENDIR")) {
			p += strtokenskip(p);
			state_endir = token2tapstate(p);
			if (state_endir < 0)
				goto syntax_error;
			p += strtokenskip(p);
			goto eol_check;
		}

		if (!strtokencmp(p, "ENDDR")) {
			p += strtokenskip(p);
			state_enddr = token2tapstate(p);
			if (state_endir < 0)
				goto syntax_error;
			p += strtokenskip(p);
			goto eol_check;
		}

		if (!strtokencmp(p, "FREQUENCY")) {
			goto unsupported_error;
		}

		if (!strtokencmp(p, "HDR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_hdr);
			if (!p)
				goto syntax_error;
			goto eol_check;
		}

		if (!strtokencmp(p, "HIR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_hir);
			if (!p)
				goto syntax_error;
			goto eol_check;
		}

		if (!strtokencmp(p, "PIO") || !strtokencmp(p, "PIOMAP")) {
			goto unsupported_error;
		}

		if (!strtokencmp(p, "RUNTEST")) {
			p += strtokenskip(p);
			int tck_count = -1;
			int sck_count = -1;
			int min_time = -1;
			int max_time = -1;
			while (*p) {
				int got_maximum = 0;
				if (!strtokencmp(p, "MAXIMUM")) {
					p += strtokenskip(p);
					got_maximum = 1;
				}
				int got_endstate = 0;
				if (!strtokencmp(p, "ENDSTATE")) {
					p += strtokenskip(p);
					got_endstate = 1;
				}
				int st = token2tapstate(p);
				if (st >= 0) {
					p += strtokenskip(p);
					if (got_endstate)
						state_endrun = st;
					else
						state_run = st;
					continue;
				}
				if (*p < '0' || *p > '9')
					goto syntax_error;
				int number = 0;
				while (*p >= '0' && *p <= '9') {
					number = number*10 + (*p - '0');
					p++;
				}
				while (*p == ' ') {
					p++;
				}
				if (!strtokencmp(p, "SEC")) {
					p += strtokenskip(p);
					if (got_maximum)
						max_time = number;
					else
						min_time = number;
					continue;
				}
				if (!strtokencmp(p, "TCK")) {
					p += strtokenskip(p);
					tck_count = number;
					continue;
				}
				if (!strtokencmp(p, "SCK")) {
					p += strtokenskip(p);
					sck_count = number;
					continue;
				}
				goto syntax_error;
			}
			if (libxsvf_tap_walk(h, state_run) < 0)
				goto error;
			if (max_time >= 0) {
				h->report_error(h, __FILE__, __LINE__, "WARNING: Maximum time in SVF RUNTEST command is ignored.");
			}
			if (tck_count >= 0) {
				h->set_tms(h, 0);
				for (int i=0; i < tck_count; i++) {
					h->pulse_tck(h);
				}
			}
			if (sck_count >= 0) {
				for (int i=0; i < sck_count; i++) {
					if (h->pulse_sck)
						h->pulse_sck(h);
				}
			}
			if (min_time >= 0) {
				h->udelay(h, min_time * 1000000);
			}
			goto eol_check;
		}

		if (!strtokencmp(p, "SDR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_sdr);
			if (!p)
				goto syntax_error;
			if (libxsvf_tap_walk(h, LIBXSVF_TAP_DRSHIFT) < 0)
				goto error;
			if (bitdata_play(h, 1, &bd_hdr, LIBXSVF_TAP_DRSHIFT) < 0)
				goto error;
			if (bitdata_play(h, 0, &bd_sdr, LIBXSVF_TAP_DRSHIFT) < 0)
				goto error;
			if (bitdata_play(h, 0, &bd_tdr, state_enddr) < 0)
				goto error;
			if (libxsvf_tap_walk(h, state_enddr) < 0)
				goto error;
			goto eol_check;
		}

		if (!strtokencmp(p, "SIR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_sir);
			if (!p)
				goto syntax_error;
			if (libxsvf_tap_walk(h, LIBXSVF_TAP_IRSHIFT) < 0)
				goto error;
			if (bitdata_play(h, 1, &bd_hir, LIBXSVF_TAP_IRSHIFT) < 0)
				goto error;
			if (bitdata_play(h, 0, &bd_sir, LIBXSVF_TAP_IRSHIFT) < 0)
				goto error;
			if (bitdata_play(h, 0, &bd_tir, state_endir) < 0)
				goto error;
			if (libxsvf_tap_walk(h, state_endir) < 0)
				goto error;
			goto eol_check;
		}

		if (!strtokencmp(p, "STATE")) {
			p += strtokenskip(p);
			while (*p) {
				int st = token2tapstate(p);
				if (st < 0)
					goto syntax_error;
				if (libxsvf_tap_walk(h, st) < 0)
					goto error;
				p += strtokenskip(p);
			}
			goto eol_check;
		}

		if (!strtokencmp(p, "TDR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_tdr);
			if (!p)
				goto syntax_error;
			goto eol_check;
		}

		if (!strtokencmp(p, "TIR")) {
			p += strtokenskip(p);
			p = bitdata_parse(h, p, &bd_tir);
			if (!p)
				goto syntax_error;
			goto eol_check;
		}

		if (!strtokencmp(p, "TRST")) {
			p += strtokenskip(p);
			if (!strtokencmp(p, "ON")) {
				p += strtokenskip(p);
				if (h->set_trst)
					h->set_trst(h, 1);
				goto eol_check;
			}
			if (!strtokencmp(p, "OFF")) {
				p += strtokenskip(p);
				if (h->set_trst)
					h->set_trst(h, 0);
				goto eol_check;
			}
			if (!strtokencmp(p, "Z")) {
				p += strtokenskip(p);
				if (h->set_trst)
					h->set_trst(h, -1);
				goto eol_check;
			}
			if (!strtokencmp(p, "ABSENT")) {
				p += strtokenskip(p);
				if (h->set_trst)
					h->set_trst(h, -2);
				goto eol_check;
			}
			goto syntax_error;
		}

eol_check:
		while (*p == ' ')
			p++;
		if (*p == 0)
			continue;

syntax_error:
		h->report_error(h, __FILE__, __LINE__, "SVF Syntax Error:");
		if (0) {
unsupported_error:
			h->report_error(h, __FILE__, __LINE__, "Error in SVF input: unsupported command:");
		}
		h->report_error(h, __FILE__, __LINE__, command_buffer);
error:
		rc = -1;
		break;
	}

	bitdata_free(h, &bd_hdr);
	bitdata_free(h, &bd_hir);
	bitdata_free(h, &bd_tdr);
	bitdata_free(h, &bd_tir);
	bitdata_free(h, &bd_sdr);
	bitdata_free(h, &bd_sir);

	h->realloc(h, command_buffer, 0);

	return rc;
}


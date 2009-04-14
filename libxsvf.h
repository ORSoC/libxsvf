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

#ifndef LIBXSVF_H
#define LIBXSVF_H

enum libxsvf_mode {
	LIBXSVF_MODE_SVF,
	LIBXSVF_MODE_XSVF,
	LIBXSVF_MODE_SCAN
};

enum libxsvf_tap_state {
	/* Special States */
	LIBXSVF_TAP_INIT,
	LIBXSVF_TAP_RESET,
	LIBXSVF_TAP_IDLE,
	/* DR States */
	LIBXSVF_TAP_DRSELECT,
	LIBXSVF_TAP_DRCAPTURE,
	LIBXSVF_TAP_DRSHIFT,
	LIBXSVF_TAP_DREXIT1,
	LIBXSVF_TAP_DRPAUSE,
	LIBXSVF_TAP_DREXIT2,
	LIBXSVF_TAP_DRUPDATE,
	/* IR States */
	LIBXSVF_TAP_IRSELECT,
	LIBXSVF_TAP_IRCAPTURE,
	LIBXSVF_TAP_IRSHIFT,
	LIBXSVF_TAP_IREXIT1,
	LIBXSVF_TAP_IRPAUSE,
	LIBXSVF_TAP_IREXIT2,
	LIBXSVF_TAP_IRUPDATE
};

enum libxsvf_mem {
	LIBXSVF_MEM_XSVF_TDI_DATA = 0,
	LIBXSVF_MEM_XSVF_TDO_DATA = 1,
	LIBXSVF_MEM_XSVF_TDO_MASK = 2,
	LIBXSVF_MEM_XSVF_ADDR_MASK = 3,
	LIBXSVF_MEM_XSVF_DATA_MASK = 4,
	LIBXSVF_MEM_SVF_COMMANDBUF = 5,
	LIBXSVF_MEM_SVF_SDR_TDI_DATA = 6,
	LIBXSVF_MEM_SVF_SDR_TDI_MASK = 7,
	LIBXSVF_MEM_SVF_SDR_TDO_DATA = 8,
	LIBXSVF_MEM_SVF_SDR_TDO_MASK = 9,
	LIBXSVF_MEM_SVF_SDR_RET_MASK = 10,
	LIBXSVF_MEM_SVF_SIR_TDI_DATA = 11,
	LIBXSVF_MEM_SVF_SIR_TDI_MASK = 12,
	LIBXSVF_MEM_SVF_SIR_TDO_DATA = 13,
	LIBXSVF_MEM_SVF_SIR_TDO_MASK = 14,
	LIBXSVF_MEM_SVF_SIR_RET_MASK = 15,
	LIBXSVF_MEM_SVF_HDR_TDI_DATA = 16,
	LIBXSVF_MEM_SVF_HDR_TDI_MASK = 17,
	LIBXSVF_MEM_SVF_HDR_TDO_DATA = 18,
	LIBXSVF_MEM_SVF_HDR_TDO_MASK = 19,
	LIBXSVF_MEM_SVF_HDR_RET_MASK = 20,
	LIBXSVF_MEM_SVF_HIR_TDI_DATA = 21,
	LIBXSVF_MEM_SVF_HIR_TDI_MASK = 22,
	LIBXSVF_MEM_SVF_HIR_TDO_DATA = 23,
	LIBXSVF_MEM_SVF_HIR_TDO_MASK = 24,
	LIBXSVF_MEM_SVF_HIR_RET_MASK = 25,
	LIBXSVF_MEM_SVF_TDR_TDI_DATA = 26,
	LIBXSVF_MEM_SVF_TDR_TDI_MASK = 27,
	LIBXSVF_MEM_SVF_TDR_TDO_DATA = 28,
	LIBXSVF_MEM_SVF_TDR_TDO_MASK = 29,
	LIBXSVF_MEM_SVF_TDR_RET_MASK = 30,
	LIBXSVF_MEM_SVF_TIR_TDI_DATA = 31,
	LIBXSVF_MEM_SVF_TIR_TDI_MASK = 32,
	LIBXSVF_MEM_SVF_TIR_TDO_DATA = 33,
	LIBXSVF_MEM_SVF_TIR_TDO_MASK = 34,
	LIBXSVF_MEM_SVF_TIR_RET_MASK = 35,
	LIBXSVF_MEM_NUM = 36
};

struct libxsvf_host {
	void (*setup)(struct libxsvf_host *h);
	void (*shutdown)(struct libxsvf_host *h);
	void (*udelay)(struct libxsvf_host *h, long usecs);
	int (*getbyte)(struct libxsvf_host *h);
	int (*pulse_tck)(struct libxsvf_host *h, int tms, int tdi, int tdo, int rmask);
	void (*pulse_sck)(struct libxsvf_host *h);
	void (*set_trst)(struct libxsvf_host *h, int v);
	void (*report_tapstate)(struct libxsvf_host *h);
	void (*report_device)(struct libxsvf_host *h, unsigned long idcode);
	void (*report_status)(struct libxsvf_host *h, const char *message);
	void (*report_error)(struct libxsvf_host *h, const char *file, int line, const char *message);
	void *(*realloc)(struct libxsvf_host *h, void *ptr, int size, enum libxsvf_mem which);
	enum libxsvf_tap_state tap_state;
	void *user_data;
};

int libxsvf_play(struct libxsvf_host *, enum libxsvf_mode mode);
const char *libxsvf_state2str(enum libxsvf_tap_state tap_state);
const char *libxsvf_mem2str(enum libxsvf_mem which);

/* Internal API */ 
int libxsvf_svf(struct libxsvf_host *h);
int libxsvf_xsvf(struct libxsvf_host *h);
int libxsvf_scan(struct libxsvf_host *h);
int libxsvf_tap_walk(struct libxsvf_host *, enum libxsvf_tap_state);

/* Host accessor macros (see README) */
#define LIBXSVF_HOST_SETUP() h->setup(h)
#define LIBXSVF_HOST_SHUTDOWN() h->shutdown(h)
#define LIBXSVF_HOST_UDELAY(_usecs) h->udelay(h, _usecs)
#define LIBXSVF_HOST_GETBYTE() h->getbyte(h)
#define LIBXSVF_HOST_PULSE_TCK(_tms, _tdi, _tdo, _rmask) h->pulse_tck(h, _tms, _tdi, _tdo, _rmask)
#define LIBXSVF_HOST_PULSE_SCK() do { if (h->pulse_sck) h->pulse_sck(h); } while (0)
#define LIBXSVF_HOST_SET_TRST(_v) do { if (h->set_trst) h->set_trst(h, _v); } while (0)
#define LIBXSVF_HOST_REPORT_TAPSTATE() do { if (h->report_tapstate) h->report_tapstate(h); } while (0)
#define LIBXSVF_HOST_REPORT_DEVICE(_v) do { if (h->report_device) h->report_device(h, _v); } while (0)
#define LIBXSVF_HOST_REPORT_STATUS(_msg) do { if (h->report_status) h->report_status(h, _msg); } while (0)
#define LIBXSVF_HOST_REPORT_ERROR(_msg) h->report_error(h, __FILE__, __LINE__, _msg)
#define LIBXSVF_HOST_REALLOC(_ptr, _size, _which) h->realloc(h, _ptr, _size, _which)

#endif


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
	LIBXSVF_MODE_XSVF
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

struct libxsvf_host {
	void (*udelay)(struct libxsvf_host *h, long usecs);
	int (*read_next_byte)(struct libxsvf_host *h);
	void (*set_tms)(struct libxsvf_host *h, int v);
	void (*set_tdi)(struct libxsvf_host *h, int v);
	void (*pulse_tck)(struct libxsvf_host *h);
	void (*pulse_sck)(struct libxsvf_host *h);
	void (*set_trst)(struct libxsvf_host *h, int v);
	int (*get_tdo)(struct libxsvf_host *h);
	void (*ret_tdo)(struct libxsvf_host *h, int v);
	void (*report_tapstate)(struct libxsvf_host *h);
	void (*report_status)(struct libxsvf_host *h, const char *message);
	void (*report_error)(struct libxsvf_host *h, const char *file, int line, const char *message);
	void *(*realloc)(struct libxsvf_host *h, void *ptr, int size);
	enum libxsvf_tap_state tap_state;
	void *user_data;
};

int libxsvf_play(struct libxsvf_host *, enum libxsvf_mode mode);
const char *libxsvf_state2str(enum libxsvf_tap_state tap_state);

/* Internal API */ 
int libxsvf_svf(struct libxsvf_host *h);
int libxsvf_xsvf(struct libxsvf_host *h);
int libxsvf_tap_walk(struct libxsvf_host *, enum libxsvf_tap_state);

#endif


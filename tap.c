/*
 *  Generic JTAG (X)SVF player library
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

const char *libxsvf_state2str(enum libxsvf_tap_state tap_state)
{
#define X(_s) if (tap_state == _s) return #_s;
	X(LIBXSVF_TAP_INIT)
	X(LIBXSVF_TAP_RESET)
	X(LIBXSVF_TAP_IDLE)
	X(LIBXSVF_TAP_DRSELECT)
	X(LIBXSVF_TAP_DRCAPTURE)
	X(LIBXSVF_TAP_DRSHIFT)
	X(LIBXSVF_TAP_DREXIT1)
	X(LIBXSVF_TAP_DRPAUSE)
	X(LIBXSVF_TAP_DREXIT2)
	X(LIBXSVF_TAP_DRUPDATE)
	X(LIBXSVF_TAP_IRSELECT)
	X(LIBXSVF_TAP_IRCAPTURE)
	X(LIBXSVF_TAP_IRSHIFT)
	X(LIBXSVF_TAP_IREXIT1)
	X(LIBXSVF_TAP_IRPAUSE)
	X(LIBXSVF_TAP_IREXIT2)
	X(LIBXSVF_TAP_IRUPDATE)
#undef X
	return "UNKOWN_STATE";
}

static void tap_transition(struct libxsvf_host *h, int v)
{
	h->set_tms(h, v);
	h->pulse_tck(h);
}

int libxsvf_tap_walk(struct libxsvf_host *h, enum libxsvf_tap_state s)
{

	for (int i=0; s != h->tap_state; i++)
	{
		switch (h->tap_state)
		{
		/* Special States */
		case LIBXSVF_TAP_INIT:
			for (int j = 0; j < 6; j++)
				tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_RESET;
			break;
		case LIBXSVF_TAP_RESET:
			tap_transition(h, 0);
			h->tap_state = LIBXSVF_TAP_IDLE;
			break;
		case LIBXSVF_TAP_IDLE:
			tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_DRSELECT;
			break;

		/* DR States */
		case LIBXSVF_TAP_DRSELECT:
			if (s >= LIBXSVF_TAP_IRSELECT || s == LIBXSVF_TAP_RESET) {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_IRSELECT;
			} else {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_DRCAPTURE;
			}
			break;
		case LIBXSVF_TAP_DRCAPTURE:
			if (s == LIBXSVF_TAP_DRSHIFT) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_DRSHIFT;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_DREXIT1;
			}
			break;
		case LIBXSVF_TAP_DRSHIFT:
			tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_DREXIT1;
			break;
		case LIBXSVF_TAP_DREXIT1:
			if (s == LIBXSVF_TAP_DRPAUSE) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_DRPAUSE;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_DRUPDATE;
			}
			break;
		case LIBXSVF_TAP_DRPAUSE:
			tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_DREXIT2;
			break;
		case LIBXSVF_TAP_DREXIT2:
			if (s == LIBXSVF_TAP_DRSHIFT) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_DRSHIFT;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_DRUPDATE;
			}
			break;
		case LIBXSVF_TAP_DRUPDATE:
			if (s == LIBXSVF_TAP_IDLE) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IDLE;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_DRSELECT;
			}
			break;

		/* IR States */
		case LIBXSVF_TAP_IRSELECT:
			if (s == LIBXSVF_TAP_RESET) {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_RESET;
			} else {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IRCAPTURE;
			}
			break;
		case LIBXSVF_TAP_IRCAPTURE:
			if (s == LIBXSVF_TAP_IRSHIFT) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IRSHIFT;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_IREXIT1;
			}
			break;
		case LIBXSVF_TAP_IRSHIFT:
			tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_IREXIT1;
			break;
		case LIBXSVF_TAP_IREXIT1:
			if (s == LIBXSVF_TAP_IRPAUSE) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IRPAUSE;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_IRUPDATE;
			}
			break;
		case LIBXSVF_TAP_IRPAUSE:
			tap_transition(h, 1);
			h->tap_state = LIBXSVF_TAP_IREXIT2;
			break;
		case LIBXSVF_TAP_IREXIT2:
			if (s == LIBXSVF_TAP_IRSHIFT) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IRSHIFT;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_IRUPDATE;
			}
			break;
		case LIBXSVF_TAP_IRUPDATE:
			if (s == LIBXSVF_TAP_IDLE) {
				tap_transition(h, 0);
				h->tap_state = LIBXSVF_TAP_IDLE;
			} else {
				tap_transition(h, 1);
				h->tap_state = LIBXSVF_TAP_DRSELECT;
			}
			break;

		default:
			h->report_error(h, __FILE__, __LINE__, "Illegal tap state.");
			return -1;
		}
		if (h->report_tapstate)
			h->report_tapstate(h);
		if (i>10) {
			h->report_error(h, __FILE__, __LINE__, "Loop in tap walker.");
			return -1;
		}
	}

	return 0;
}

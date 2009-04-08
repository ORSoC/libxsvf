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


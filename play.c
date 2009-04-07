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

int libxsvf_play(struct libxsvf_host *h, enum libxsvf_mode mode)
{
	int rc = -1;

	h->tap_state = LIBXSVF_TAP_INIT;

	if (mode == LIBXSVF_MODE_SVF) {
#ifdef LIBXSVF_WITHOUT_SVF
		h->report_error(h, __FILE__, __LINE__, "SVF support in libxsvf is disabled.");
#else
		rc = libxsvf_svf(h);
#endif
	}

	if (mode == LIBXSVF_MODE_XSVF) {
#ifdef LIBXSVF_WITHOUT_XSVF
		h->report_error(h, __FILE__, __LINE__, "XSVF support in libxsvf is disabled.");
#else
		rc = libxsvf_xsvf(h);
#endif
	}

	libxsvf_tap_walk(h, LIBXSVF_TAP_RESET);
	return rc;
}


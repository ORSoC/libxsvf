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

int libxsvf_play(struct libxsvf_host *h, enum libxsvf_mode mode)
{
	int rc = -1;

	h->tap_state = LIBXSVF_TAP_INIT;
	if (LIBXSVF_HOST_SETUP() < 0) {
		LIBXSVF_HOST_REPORT_ERROR("Setup of JTAG interface failed.");
		return -1;
	}

	if (mode == LIBXSVF_MODE_SVF) {
#ifdef LIBXSVF_WITHOUT_SVF
		LIBXSVF_HOST_REPORT_ERROR("SVF support in libxsvf is disabled.");
#else
		rc = libxsvf_svf(h);
#endif
	}

	if (mode == LIBXSVF_MODE_XSVF) {
#ifdef LIBXSVF_WITHOUT_XSVF
		LIBXSVF_HOST_REPORT_ERROR("XSVF support in libxsvf is disabled.");
#else
		rc = libxsvf_xsvf(h);
#endif
	}

	if (mode == LIBXSVF_MODE_SCAN) {
#ifdef LIBXSVF_WITHOUT_SCAN
		LIBXSVF_HOST_REPORT_ERROR("SCAN support in libxsvf is disabled.");
#else
		rc = libxsvf_scan(h);
#endif
	}

	libxsvf_tap_walk(h, LIBXSVF_TAP_RESET);
	int shutdown_rc = LIBXSVF_HOST_SHUTDOWN();

	if (shutdown_rc < 0) {
		LIBXSVF_HOST_REPORT_ERROR("Shutdown of JTAG interface failed. (async TDO error?)");
		rc = rc < 0 ? rc : shutdown_rc;
	}

	return rc;
}


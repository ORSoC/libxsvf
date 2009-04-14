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

int libxsvf_scan(struct libxsvf_host *h)
{
	if (libxsvf_tap_walk(h, LIBXSVF_TAP_RESET) < 0)
		return -1;

	if (libxsvf_tap_walk(h, LIBXSVF_TAP_DRSHIFT) < 0)
		return -1;

	for (int i=0; i<256; i++)
	{
		int bit = LIBXSVF_HOST_PULSE_TCK(0, 1, -1, 0);

		if (bit < 0)
			return -1;

		if (bit == 0) {
			LIBXSVF_HOST_REPORT_DEVICE(0);
		} else {
			unsigned long idcode = 1;
			for (int j=1; j<32; j++) {
				int bit = LIBXSVF_HOST_PULSE_TCK(0, 1, -1, 0);
				if (bit < 0)
					return -1;
				idcode |= bit << j;
			}
			if (idcode == 0xffffffff)
				break;
			LIBXSVF_HOST_REPORT_DEVICE(idcode);
		}
	}

	return 0;
}


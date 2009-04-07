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

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>


/** BEGIN: Low-Level I/O Implementation **/

#ifdef XSVFTOOL_RLMS_VLINE

// Simple example with MPC8349E GPIO pins
// (RIEGL LMS V-Line motherboard)

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define IO_PORT_ADDR 0xE0000C00

struct io_layout {
	unsigned long tdi:1;
	unsigned long tdo:1;
	unsigned long tms:1;
	unsigned long tck:1;
	unsigned long reserved:28;
};

static volatile struct io_layout *io_direction;
static volatile struct io_layout *io_opendrain;
static volatile struct io_layout *io_data;

static void io_setup(void)
{
	/* open /dev/mem device file */
	int fd = open("/dev/mem", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Can't open /dev/mem: %s\n", strerror(errno));
		exit(1);
	}

	/* calculate offsets to page and within page */
	unsigned long psize = getpagesize();
	unsigned long off_inpage = IO_PORT_ADDR % psize;
	unsigned long off_topage = IO_PORT_ADDR - off_inpage;
	unsigned long mapsize = off_inpage + sizeof(struct io_layout) * 3;

	/* map it into logical memory */
	void *io_addr_map = mmap(0, mapsize, PROT_WRITE, MAP_SHARED, fd, off_topage);
	if (io_addr_map == MAP_FAILED) {
		fprintf(stderr, "Can't map physical memory: %s\n", strerror(errno));
		exit(1);
	}

	/* calculate register addresses */
	io_direction = io_addr_map + off_inpage;
	io_opendrain = io_addr_map + off_inpage + 4;
	io_data = io_addr_map + off_inpage + 8;

	/* set direction reg */
	io_direction->tms = 1;
	io_direction->tck = 1;
	io_direction->tdo = 0;
	io_direction->tdi = 1;

	/* set open drain reg */
	io_opendrain->tms = 0;
	io_opendrain->tck = 0;
	io_opendrain->tdo = 0;
	io_opendrain->tdi = 0;

	/* init */
	io_data->tms = 1;
	io_data->tck = 0;
	io_data->tdi = 0;
}

static void io_shutdown(void)
{
	io_data->tms = 1;
	io_data->tck = 0;
	io_data->tdi = 0;
}

static void io_tms(int val)
{
	io_data->tms = val;
}

static void io_tdi(int val)
{
	io_data->tdi = val;
}

static void io_tck(int val)
{
	io_data->tck = val;
}

static void io_sck(int val)
{
	/* not available */
}

static void io_trst(int val)
{
	/* not available */
}

static int io_tdo()
{
	return io_data->tdo ? 1 : 0;
}

#else

static void io_setup(void)
{
}

static void io_shutdown(void)
{
}

static void io_tms(int val)
{
}

static void io_tdi(int val)
{
}

static void io_tck(int val)
{
}

static void io_sck(int val)
{
}

static void io_trst(int val)
{
}

static int io_tdo()
{
	return -1;
}

#endif

/** END: Low-Level I/O Implementation **/


struct udata_s {
	FILE *f;
	int verbose;
	int retval_i;
	int retval[256];
};

static void h_udelay(struct libxsvf_host *h, long usecs)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 2) {
		fprintf(stderr, "[DELAY:%ld]\n", usecs);
		fflush(stderr);
	}
	usleep(usecs);
}

static int h_read_next_byte(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	return fgetc(u->f);
}

static void h_set_tms(struct libxsvf_host *h, int v)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 3) {
		fprintf(stderr, "[TMS:%d]\n", v);
	}
	io_tms(v);
}

static void h_set_tdi(struct libxsvf_host *h, int v)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 3) {
		fprintf(stderr, "[TDI:%d]\n", v);
	}
	io_tdi(v);
}

static void h_pulse_tck(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 4) {
		fprintf(stderr, "[TCK]\n");
	}
	io_tck(0);
	io_tck(1);
}

static void h_pulse_sck(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 4) {
		fprintf(stderr, "[SCK]\n");
	}
	io_sck(0);
	io_sck(1);
}

static void h_set_trst(struct libxsvf_host *h, int v)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 3) {
		fprintf(stderr, "[TRST:%d]\n", v);
	}
	io_trst(v);
}

static int h_get_tdo(struct libxsvf_host *h)
{
	int v = io_tdo();
	struct udata_s *u = h->user_data;
	if (u->verbose >= 3) {
		fprintf(stderr, "[TDO:%d]\n", v);
	}
	return v;
}

static void h_ret_tdo(struct libxsvf_host *h, int v)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 3) {
		fprintf(stderr, "[RET:%d]\n", v);
	}
	if (u->retval_i < 256)
		u->retval[u->retval_i++] = v;
}

static void h_report_tapstate(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 2) {
		fprintf(stderr, "[%s]\n", libxsvf_state2str(h->tap_state));
	}
}

static void h_report_status(struct libxsvf_host *h, const char *message)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 1) {
		fprintf(stderr, "[STATUS] %s\n", message);
	}
}

static void h_report_error(struct libxsvf_host *h, const char *file, int line, const char *message)
{
	fprintf(stderr, "[%s:%d] %s\n", file, line, message);
}

static void *h_realloc(struct libxsvf_host *h, void *ptr, int size)
{
	return realloc(ptr, size);
}

static struct udata_s u;

static struct libxsvf_host h = {
	.udelay = h_udelay,
	.read_next_byte = h_read_next_byte,
	.set_tms = h_set_tms,
	.set_tdi = h_set_tdi,
	.pulse_tck = h_pulse_tck,
	.pulse_sck = h_pulse_sck,
	.set_trst = h_set_trst,
	.get_tdo = h_get_tdo,
	.ret_tdo = h_ret_tdo,
	.report_tapstate = h_report_tapstate,
	.report_status = h_report_status,
	.report_error = h_report_error,
	.realloc = h_realloc,
	.user_data = &u
};

const char *progname;

static void help()
{
	fprintf(stderr, "Usage: %s [ -v ... ] { -s svf-file | -x xsvf-file } ...\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int rc = 0;
	int gotfiles = 0;
	int opt;

	progname = argc >= 1 ? argv[0] : "xvsftool";
	while ((opt = getopt(argc, argv, "vx:s:")) != -1)
	{
		switch (opt)
		{
		case 'v':
			u.verbose++;
			break;
		case 'x':
		case 's':
			if (!gotfiles) {
				io_setup();
				gotfiles = 1;
			}
			if (!strcmp(optarg, "-"))
				u.f = stdin;
			else
				u.f = fopen(optarg, "rb");
			if (u.f == NULL) {
				fprintf(stderr, "Can't open %s file `%s': %s\n", opt == 's' ? "SVF" : "XSVF", optarg, strerror(errno));
				rc = 1;
				break;
			}
			if (libxsvf_play(&h, opt == 's' ? LIBXSVF_MODE_SVF : LIBXSVF_MODE_XSVF) < 0) {
				fprintf(stderr, "Error while playing %s file `%s'.\n", opt == 's' ? "SVF" : "XSVF", optarg);
				rc = 1;
			}
			if (strcmp(optarg, "-"))
				fclose(u.f);
			break;
		default:
			help();
			break;
		}
	}

	if (!gotfiles)
		help();

	if (u.retval_i) {
		printf("%d rmask bits:", u.retval_i);
		for (int i=0; i < u.retval_i; i++)
			printf(" %d", u.retval[i]);
		printf("\n");
	}

	io_shutdown();
	return rc;
}


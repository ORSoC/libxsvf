/*
 *  Lib(X)SVF  -  A library for implementing SVF and XSVF JTAG players
 *
 *  Copyright (C) 2009  RIEGL Research ForschungsGmbH
 *  Copyright (C) 2009, 2011  Clifford Wolf <clifford@clifford.at>
 *  
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 *  This Lib(X)SVF frontend program interfaces to the "Bus Pirate" multi tool.
 *
 *    http://dangerousprototypes.com/bus-pirate-manual/
 *
 *  WARNING: This is work in progress and highly experimental!
 */

#define _GNU_SOURCE

// #define DEBUG_SEROUT
// #define DEBUG_SERIN
// #define DEBUG_WAITFOR

#include "libxsvf.h"

#include <termios.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

struct udata_s {
	int serial_fd;
	char buffer[256];
	int index1, index2;
	int current_mode;
	int tapstate;
};

void update_tapstate(struct libxsvf_host *h, int tms)
{
	struct udata_s *u = h->user_data;
	switch (u->tapstate)
	{
	case LIBXSVF_TAP_RESET:
		u->tapstate = tms ? LIBXSVF_TAP_RESET : LIBXSVF_TAP_IDLE;
		break;
	case LIBXSVF_TAP_IDLE:
		u->tapstate = tms ? LIBXSVF_TAP_DRSELECT : LIBXSVF_TAP_IDLE;
		break;
	case LIBXSVF_TAP_DRSELECT:
		u->tapstate = tms ? LIBXSVF_TAP_IRSELECT : LIBXSVF_TAP_DRCAPTURE;
		break;
	case LIBXSVF_TAP_DRCAPTURE:
		u->tapstate = tms ? LIBXSVF_TAP_DREXIT1 : LIBXSVF_TAP_DRSHIFT;
		break;
	case LIBXSVF_TAP_DRSHIFT:
		u->tapstate = tms ? LIBXSVF_TAP_DREXIT1 : LIBXSVF_TAP_DRSHIFT;
		break;
	case LIBXSVF_TAP_DREXIT1:
		u->tapstate = tms ? LIBXSVF_TAP_DRUPDATE : LIBXSVF_TAP_DRPAUSE;
		break;
	case LIBXSVF_TAP_DRPAUSE:
		u->tapstate = tms ? LIBXSVF_TAP_DREXIT2 : LIBXSVF_TAP_DRPAUSE;
		break;
	case LIBXSVF_TAP_DREXIT2:
		u->tapstate = tms ? LIBXSVF_TAP_DRUPDATE : LIBXSVF_TAP_DRSHIFT;
		break;
	case LIBXSVF_TAP_DRUPDATE:
		u->tapstate = tms ? LIBXSVF_TAP_DRSELECT : LIBXSVF_TAP_IDLE;
		break;
	case LIBXSVF_TAP_IRSELECT:
		u->tapstate = tms ? LIBXSVF_TAP_RESET : LIBXSVF_TAP_IRCAPTURE;
		break;
	case LIBXSVF_TAP_IRCAPTURE:
		u->tapstate = tms ? LIBXSVF_TAP_IREXIT1 : LIBXSVF_TAP_IRSHIFT;
		break;
	case LIBXSVF_TAP_IRSHIFT:
		u->tapstate = tms ? LIBXSVF_TAP_IREXIT1 : LIBXSVF_TAP_IRSHIFT;
		break;
	case LIBXSVF_TAP_IREXIT1:
		u->tapstate = tms ? LIBXSVF_TAP_IRUPDATE : LIBXSVF_TAP_IRPAUSE;
		break;
	case LIBXSVF_TAP_IRPAUSE:
		u->tapstate = tms ? LIBXSVF_TAP_IREXIT2 : LIBXSVF_TAP_IRPAUSE;
		break;
	case LIBXSVF_TAP_IREXIT2:
		u->tapstate = tms ? LIBXSVF_TAP_IRUPDATE : LIBXSVF_TAP_IRSHIFT;
		break;
	case LIBXSVF_TAP_IRUPDATE:
		u->tapstate = tms ? LIBXSVF_TAP_DRSELECT : LIBXSVF_TAP_IDLE;
		break;
	default:
		fprintf(stderr, "Confusion in tapstate tracking -> assuming RESET state!\n");
		u->tapstate = LIBXSVF_TAP_RESET;
		break;
	}
}

int ser_recv(struct libxsvf_host *h, int block)
{
	struct udata_s *u = h->user_data;
	int rc, first = 1;
	char ch;

	fcntl(u->serial_fd, F_SETFL, block ? O_RDWR : O_RDWR|O_NONBLOCK);

	while ((rc = read(u->serial_fd, &ch, 1)) == 1)
	{
		if (ch == '\r' || ch == '\n') {
			if ((u->index1 & 0x7f) != 0x00) {
#ifdef DEBUG_SERIN
				fprintf(stderr, "ser in> %s\n", u->buffer + (u->index1 & 0x80));
#endif
				u->index2 = u->index1;
				u->index1 = (~u->index1) & 0x80;
				u->buffer[u->index1] = 0;
				if (block)
					return u->index2;
			}
			continue;
		}

		if ((u->index1 & 0x7f) == 0x7f)
			continue;

		u->buffer[u->index1++] = ch;
		u->buffer[u->index1] = 0;

		if (block && first) {
			fcntl(u->serial_fd, F_SETFL, O_RDWR|O_NONBLOCK);
			first = 0;
		}
	}

	return u->index1;
}

void ser_send(struct libxsvf_host *h, const char *data, int len)
{
	struct udata_s *u = h->user_data;
	int rc;

#ifdef DEBUG_SEROUT
	fprintf(stderr, "ser out> ");
	const char *p = data;
	while (*p) {
		if (*p == '\n')
			fprintf(stderr, "\\n");
		else if (*p == '\r')
			fprintf(stderr, "\\r");
		else if (*p == '\\')
			fprintf(stderr, "\\\\");
		else
			putc(*p, stderr);
		p++;
	}
	putc('\n', stderr);
#endif
	
	while (len > 0)
	{
		ser_recv(h, 0);

		fcntl(u->serial_fd, F_SETFL, O_RDWR);
		rc = write(u->serial_fd, data, len);
		assert(rc > 0);
		data += rc;
		len -= rc;
	}
}

void ser_printf(struct libxsvf_host *h, const char *fmt, ...)
		__attribute__ ((__format__ (__printf__, 2, 3)));

void ser_vprintf(struct libxsvf_host *h, const char *fmt, va_list ap)
{
	char *data;
	int len = vasprintf(&data, fmt, ap);
	assert(len >= 0);
	ser_send(h, data, len);
	free(data);
}

void ser_printf(struct libxsvf_host *h, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ser_vprintf(h, fmt, ap);
	va_end(ap);
}

int ser_waitfor(struct libxsvf_host *h, const char *token)
{
	struct udata_s *u = h->user_data;
	int token_len = strlen(token);
	int lastidx = -1, idx = u->index1;

	while (1) {
		while (idx != lastidx && (idx & 0x7f) >= token_len) {
			if (!memcmp(u->buffer + idx - token_len, token, token_len)) {
#ifdef DEBUG_WAITFOR
				fprintf(stderr, "token match> %.*s#%s\n", idx, u->buffer + (idx & 0x80), u->buffer + idx);
#endif
				return idx;
			}
			idx--;
		}
		lastidx = idx;

		idx = ser_recv(h, 1);
	}
}

void ser_command(struct libxsvf_host *h, const char *token, const char *fmt, ...)
{
	va_list ap;
	struct udata_s *u = h->user_data;
	int last_index2 = u->index2;

	va_start(ap, fmt);

	ser_waitfor(h, token);
	ser_vprintf(h, fmt, ap);

	while (1) {
		ser_recv(h, 1);
		if (last_index2 != u->index2)
			break;
	}

	va_end(ap);
}

static int h_setup(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	
	struct termios tio;
	memset(&tio, 0, sizeof(tio));
	tcgetattr(u->serial_fd, &tio);
	cfsetospeed(&tio, B115200);
	tcsetattr(u->serial_fd, TCSANOW, &tio);

	u->index1 = 0;
	u->index2 = 0x80;
	u->buffer[u->index1] = 0;
	u->buffer[u->index2] = 0;

	ser_printf(h, "#\n");

	ser_command(h, "HiZ>", "M\n");
	ser_command(h, ">", "6\n");
	ser_command(h, ">", "1\n");

	ser_command(h, "JTAG>", "p\n");
	ser_command(h, ">", "2\n");

	ser_command(h, "JTAG>", "]\n");
	ser_waitfor(h, "JTAG>");

	u->tapstate = LIBXSVF_TAP_IDLE;
	u->current_mode = 0;

	return 0;
}

static int h_shutdown(struct libxsvf_host *h)
{
	// struct udata_s *u = h->user_data;
	return 0;
}

static void h_udelay(struct libxsvf_host *h, long usecs, int tms, long num_tck)
{
	// struct udata_s *u = h->user_data;
}

static int h_getbyte(struct libxsvf_host *h)
{
	// struct udata_s *u = h->user_data;
	return 0;
}

static int h_pulse_tck(struct libxsvf_host *h, int tms, int tdi, int tdo, int rmask, int sync)
{
	struct udata_s *u = h->user_data;
	char cmd[64];
	int cmd_idx = 0;
	int rc = 0;
	int idx;

	if (u->current_mode == '[' && u->tapstate != LIBXSVF_TAP_IRSHIFT) {
		cmd[cmd_idx++] = ']';
		u->current_mode = 0;
	}

	if (u->current_mode == '{' && u->tapstate != LIBXSVF_TAP_DRSHIFT) {
		cmd[cmd_idx++] = '}';
		u->current_mode = 0;
	}

	if (u->tapstate == LIBXSVF_TAP_DRSHIFT && u->current_mode != '{')
	{
		cmd[cmd_idx++] = '{';
		u->current_mode = '{';
	}

	if (u->tapstate == LIBXSVF_TAP_IRSHIFT && u->current_mode != '[')
	{
		cmd[cmd_idx++] = '[';
		u->current_mode ='[';
	}

	if (tdi >= 0) {
		if (u->tapstate != LIBXSVF_TAP_DRSHIFT && u->tapstate != LIBXSVF_TAP_IRSHIFT)
			fprintf(stderr, "Shifting TDI in non-DRSHIFT, non-IRSHIFT state %d.\n", u->tapstate);
		cmd[cmd_idx++] = tdi ? '-' : '_';
		cmd[cmd_idx++] = '^';
	}

	if (tdo >= 0 || sync) {
		if (u->tapstate != LIBXSVF_TAP_DRSHIFT && u->tapstate != LIBXSVF_TAP_IRSHIFT)
			fprintf(stderr, "Sampling TDO in non-DRSHIFT, non-IRSHIFT state %d.\n", u->tapstate);
		cmd[cmd_idx++] = '.';
	}

	if (cmd_idx > 0) {
		cmd[cmd_idx++] = 0;
		ser_command(h, "JTAG>", "%s\n", cmd);
	}

	if (tdo >= 0 || sync) {
		idx = ser_waitfor(h, "DATA INPUT, STATE: ");
		rc = u->buffer[idx] == '1' ? 1 : 0;
	}

	update_tapstate(h, tms);

	return 1;
}

static void h_pulse_sck(struct libxsvf_host *h)
{
	// struct udata_s *u = h->user_data;
}

static void h_set_trst(struct libxsvf_host *h, int v)
{
	// struct udata_s *u = h->user_data;
}

static int h_set_frequency(struct libxsvf_host *h, int v)
{
	// struct udata_s *u = h->user_data;
	fprintf(stderr, "WARNING: Setting JTAG clock frequency to %d ignored!\n", v);
	return 0;
}

static void h_report_tapstate(struct libxsvf_host *h)
{
	// struct udata_s *u = h->user_data;
}

static void h_report_device(struct libxsvf_host *h, unsigned long idcode)
{
	// struct udata_s *u = h->user_data;
	printf("idcode=0x%08lx, revision=0x%01lx, part=0x%04lx, manufactor=0x%03lx\n", idcode,
			(idcode >> 28) & 0xf, (idcode >> 12) & 0xffff, (idcode >> 1) & 0x7ff);
}

static void h_report_status(struct libxsvf_host *h, const char *message)
{
	// struct udata_s *u = h->user_data;
	fprintf(stderr, "[STATUS] %s\n", message);
}

static void h_report_error(struct libxsvf_host *h, const char *file, int line, const char *message)
{
	// struct udata_s *u = h->user_data;
	fprintf(stderr, "[%s:%d] %s\n", file, line, message);
}

static void *h_realloc(struct libxsvf_host *h, void *ptr, int size, enum libxsvf_mem which)
{
	// struct udata_s *u = h->user_data;
	return realloc(ptr, size);
}

static struct udata_s u;

static struct libxsvf_host h = {
	.udelay = h_udelay,
	.setup = h_setup,
	.shutdown = h_shutdown,
	.getbyte = h_getbyte,
	.pulse_tck = h_pulse_tck,
	.pulse_sck = h_pulse_sck,
	.set_trst = h_set_trst,
	.set_frequency = h_set_frequency,
	.report_tapstate = h_report_tapstate,
	.report_device = h_report_device,
	.report_status = h_report_status,
	.report_error = h_report_error,
	.realloc = h_realloc,
	.user_data = &u
};

int main(int argc, char **argv)
{
	int rc = 0;
	
	fprintf(stderr, "\n **** This is work in progress and highly experimental! ***\n\n");

	u.serial_fd = open("/dev/ttyUSB1", O_RDWR);

	if (libxsvf_play(&h, LIBXSVF_MODE_SCAN) < 0) {
		fprintf(stderr, "Error while scanning JTAG chain.\n");
		rc = 1;
	}

	close(u.serial_fd);

	return rc;
}


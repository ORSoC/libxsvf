
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "psoc-interface.h"
#include "commands.h"
#include "../libxsvf.h"

FILE *fp = NULL;
usb_dev_handle *dh = NULL;
int error = 0, verbose = 0;

unsigned char sendbuf[64];
int sendbuf_len;

static void report_byte(int outdir, unsigned char value)
{
	fprintf(stderr, outdir ? ">> 0x%02X" : "<< 0x%02X", value);
	if ((value & CMD_BYTE_CMD_MASK) == CMD_BYTE_PULSE_CPLD)
		fprintf(stderr, " PULSE_CPLD");
	if ((value & CMD_BYTE_CMD_MASK) == CMD_BYTE_PULSE_JTAG)
		fprintf(stderr, " PULSE_JTAG");
	if ((value & CMD_BYTE_CMD_MASK) == CMD_BYTE_PULSE_CPLD || (value & CMD_BYTE_CMD_MASK) == CMD_BYTE_PULSE_JTAG)
	{
		if ((value & CMD_BYTE_PULSE_FLAG_TMS) != 0)
			fprintf(stderr, " TMS");
		if ((value & CMD_BYTE_PULSE_FLAG_TDI) != 0)
			fprintf(stderr, " TDI");
		if ((value & CMD_BYTE_PULSE_FLAG_TDO) != 0)
			fprintf(stderr, " TDO");
		if ((value & CMD_BYTE_PULSE_FLAG_CHK) != 0)
			fprintf(stderr, " CHK");
		if ((value & CMD_BYTE_PULSE_FLAG_SYN) != 0)
			fprintf(stderr, " SYN");
	}
	if ((value & CMD_BYTE_CMD_MASK) == CMD_BYTE_SYNC_CPLD)
		fprintf(stderr, " SYNC_CPLD");
	if ((value & CMD_BYTE_CMD_MASK) == CMD_BYTE_SYNC_JTAG)
		fprintf(stderr, " SYNC_JTAG");
	if ((value & CMD_BYTE_CMD_MASK) == CMD_BYTE_REPORT_CPLD)
		fprintf(stderr, " REPORT_CPLD");
	if ((value & CMD_BYTE_CMD_MASK) == CMD_BYTE_REPORT_JTAG)
		fprintf(stderr, " REPORT_JTAG");
	if ((value & CMD_BYTE_CMD_MASK) == CMD_BYTE_REPORT_CPLD || (value & CMD_BYTE_CMD_MASK) == CMD_BYTE_REPORT_JTAG)
	{
		if ((value & CMD_BYTE_REPORT_FLAG_TDO) != 0)
			fprintf(stderr, " TDO");
		if ((value & CMD_BYTE_REPORT_FLAG_ERR) != 0)
			fprintf(stderr, " ERR");
	}
	fprintf(stderr, "\n");
}

static void send_byte(unsigned char value)
{
	if (verbose > 1)
		report_byte(1, value);
	sendbuf[sendbuf_len++] = value;
	if (sendbuf_len == 64) {
		if (verbose > 1)
			fprintf(stderr, ">> buffer full -> flush\n");
		if (xpcu_psoc_send_chunk(dh, 2, sendbuf, sendbuf_len) < 0) {
			fprintf(stderr, "xpcu-prog: Send failed!\n");
			error = 1;
		}
		sendbuf_len = 0;
	}
}

static int recv_byte()
{
	unsigned char value;
	if (sendbuf_len > 0) {
		if (verbose > 1)
			fprintf(stderr, ">> sync point -> flush\n");
		if (xpcu_psoc_send_chunk(dh, 2, sendbuf, sendbuf_len) < 0) {
			fprintf(stderr, "xpcu-prog: Send failed!\n");
			error = 1;
		}
		sendbuf_len = 0;
	}
	if (xpcu_psoc_recv_chunk(dh, 6, &value, 1, NULL) < 0) {
		fprintf(stderr, "xpcu-prog: Recv failed!\n");
		error = 1;
	}
	if (verbose > 1)
		report_byte(0, value);
	return value;
}

static int h_setup(struct libxsvf_host *h)
{
	usb_find_busses();
	usb_find_devices();

	dh = xpcu_psoc_open();
	if (dh == NULL) {
		fprintf(stderr, "xpcu-prog: Can't open xpcu_psoc device!\n");
		return -1;
	}

	xpcu_psoc_upload_ihex(dh, "firmware.ihx");

	if (xpcu_psoc_claim(dh) < 0) {
		fprintf(stderr, "xpcu-prog: Can't claim xpcu_psoc device!\n");
		usb_close(dh);
		return -1;
	}

	sendbuf_len = 0;

	return 0;
}

static int h_shutdown(struct libxsvf_host *h)
{
	xpcu_psoc_release(dh);
	usb_close(dh);
	dh = NULL;
	return 0;
}

static int h_getbyte(struct libxsvf_host *h)
{
	return fgetc(fp);
}

static int h_sync(struct libxsvf_host *h)
{
	uint8_t report;
	send_byte(CMD_BYTE_SYNC_CPLD);
	report = recv_byte();
	return (report & CMD_BYTE_REPORT_FLAG_ERR) != 0 ? -1 : 0;
}

static int h_pulse_tck(struct libxsvf_host *h, int tms, int tdi, int tdo, int rmask, int sync)
{
	send_byte(CMD_BYTE_PULSE_CPLD |
			(tms ? CMD_BYTE_PULSE_FLAG_TMS : 0) | (tdi ? CMD_BYTE_PULSE_FLAG_TDI : 0) |
			(tdo ? CMD_BYTE_PULSE_FLAG_TDO : 0) | (tdo >= 0 ? CMD_BYTE_PULSE_FLAG_CHK : 0) |
			(sync ? CMD_BYTE_PULSE_FLAG_SYN : 0));
	if (sync) {
		uint8_t report = recv_byte();
		if ((report & CMD_BYTE_REPORT_FLAG_ERR) != 0)
			return -1;
		return (report & CMD_BYTE_REPORT_FLAG_TDO) != 0;
	}

	return tdo < 0 ? 1 : tdo;
}

static void h_udelay(struct libxsvf_host *h, long usecs, int tms, long num_tck)
{
	int i;
	for (i = 0; i < num_tck; i++)
		h_pulse_tck(h, tms, 0, -1, 0, 0);
	usleep(usecs);
}

static int h_set_frequency(struct libxsvf_host *h, int v)
{
	fprintf(stderr, "WARNING: Setting JTAG clock frequency to %d ignored!\n", v);
	return 0;
}

static void h_report_device(struct libxsvf_host *h, unsigned long idcode)
{
	// struct udata_s *u = h->user_data;
	printf("idcode=0x%08lx, revision=0x%01lx, part=0x%04lx, manufactor=0x%03lx\n", idcode, (idcode >> 28) & 0xf, (idcode >> 12) & 0xffff, (idcode >> 1) & 0x7ff);
}

static void h_report_status(struct libxsvf_host *h, const char *message)
{
	if (verbose > 0)
		fprintf(stderr, "[STATUS] %s\n", message);
}

static void h_report_error(struct libxsvf_host *h, const char *file, int line, const char *message)
{
	error = 1;
	fprintf(stderr, "[%s:%d] %s\n", file, line, message);
}

static void *h_realloc(struct libxsvf_host *h, void *ptr, int size, enum libxsvf_mem which)
{
	return realloc(ptr, size);
}

static struct libxsvf_host h = {
	.udelay = h_udelay,
	.setup = h_setup,
	.shutdown = h_shutdown,
	.getbyte = h_getbyte,
	.sync = h_sync,
	.pulse_tck = h_pulse_tck,
	.set_frequency = h_set_frequency,
	.report_device = h_report_device,
	.report_status = h_report_status,
	.report_error = h_report_error,
	.realloc = h_realloc
};

int main(int argc, char **argv)
{
	int opt;
	int did_something = 0;

	usb_init();

	while ((opt = getopt(argc, argv, "vcs:x:")) != -1)
	{
		if (opt == 'v')
			verbose++;
		if (opt == 's' || opt == 'x') {
			did_something = 1;
			if (!strcmp(optarg, "-"))
				fp = stdin;
			else
				fp = fopen(optarg, "rb");
			if (fp == NULL) {
				fprintf(stderr, "Can't open %s file `%s': %s\n", opt == 's' ? "SVF" : "XSVF", optarg, strerror(errno));
				error = 1;
				continue;
			}
			if (libxsvf_play(&h, opt == 's' ? LIBXSVF_MODE_SVF : LIBXSVF_MODE_XSVF) < 0) {
				fprintf(stderr, "Error while playing %s file `%s'.\n", opt == 's' ? "SVF" : "XSVF", optarg);
				error = 1;
			}
			if (strcmp(optarg, "-"))
				fclose(fp);
			fp = NULL;
		}
		if (opt == 'c') {
			did_something = 1;
			if (libxsvf_play(&h, LIBXSVF_MODE_SCAN) < 0) {
				fprintf(stderr, "Error while scanning JTAG chain.\n");
				error = 1;
			}
		}
	}

	if (!did_something) {
		error = 1;
		fprintf(stderr, "Usage: %s [ -v ] { -c | -s svf_file | -x xsvf_file } [ ... ]\n", argv[0]);
	} else if (error) {
		fprintf(stderr, "Got errors!\n");
	}

	return error;
}

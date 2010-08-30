
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "psoc-interface.h"
#include "commands.h"
#include "../libxsvf.h"

FILE *vfile_open_cpldxsvf();
FILE *vfile_open_firmware();

FILE *fp = NULL;
usb_dev_handle *dh = NULL;
int error = 0, verbose = 0;

int cpld_mode = 0;

unsigned char recv_count;
unsigned char sendbuf[64];
int sendbuf_len;

static void report_byte(int outdir, int pos, unsigned char value)
{
	fprintf(stderr, outdir ? ">> 0x%02X" : "<< 0x%02X", value);
	if (value == 0)
		fprintf(stderr, " NULL");
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
	if ((value & CMD_BYTE_CMD_MASK) == CMD_BYTE_ECHO)
		fprintf(stderr, " ECHO(%02x)", value & 0x1f);
	if (pos >= 0)
		fprintf(stderr, " <%d>\n", pos);
	else
		fprintf(stderr, "\n");
}

static void send_flush()
{
	if (xpcu_psoc_send_chunk(dh, sendbuf, sendbuf_len) < 0) {
		fprintf(stderr, "xpcu-prog: Send failed!\n");
		error = 1;
	}
	sendbuf_len = 0;
}

static void send_byte(unsigned char value)
{
	if (verbose > 1)
		report_byte(1, -1, value);
	sendbuf[sendbuf_len++] = value;
	if (sendbuf_len == 64) {
		if (verbose > 1)
			fprintf(stderr, "** buffer full -> flush %d bytes\n", sendbuf_len);
		send_flush();
	}
}

static int recv_byte()
{
	unsigned char buffer[2];
	if (sendbuf_len > 0) {
		if (verbose > 1)
			fprintf(stderr, "** sync point -> flush %d bytes\n", sendbuf_len);
		send_flush();
	}
	recv_count++;
	if (xpcu_psoc_recv_chunk(dh, buffer, 2, NULL) < 0) {
		fprintf(stderr, "xpcu-prog: Recv failed!\n");
		error = 1;
	}
	if (verbose > 1)
		report_byte(0, buffer[0], buffer[1]);
	if (buffer[0] != recv_count) {
		fprintf(stderr, "xpcu-prog: message from device is out of order! (%d vs %d)\n", buffer[0], recv_count);
	}
	return buffer[1];
}

static int h_setup(struct libxsvf_host *h)
{
	int i;

	usb_find_busses();
	usb_find_devices();

	dh = xpcu_psoc_open();
	if (dh == NULL) {
		fprintf(stderr, "xpcu-prog: Can't open xpcu_psoc device!\n");
		return -1;
	}

	FILE *f = vfile_open_firmware();
	// FILE *f = fopen("firmware.ihx", "r");
	if (f == NULL) {
		fprintf(stderr, "xpcu-prog: Can't open ihex firmware image!\n");
		usb_close(dh);
		return -1;
	}
	if (xpcu_psoc_upload_ihex(dh, f) < 0) {
		fprintf(stderr, "xpcu-prog: Can't upload ihex firmware image!\n");
		fclose(f);
		usb_close(dh);
		return -1;
	}
	fclose(f);

	if (xpcu_psoc_claim(dh) < 0) {
		fprintf(stderr, "xpcu-prog: Can't claim xpcu_psoc device!\n");
		usb_close(dh);
		return -1;
	}

	usleep(100000);
	xpcu_psoc_flush(dh);

	recv_count = 0;
	sendbuf_len = 0;

	/* test communication */
	for (i=0; i<16; i++)
		send_byte(CMD_BYTE_ECHO | i);
	send_flush();

	for (i=0; i<16; i++) {
		while (1) {
			unsigned char buf = recv_byte();
			if (buf == (CMD_BYTE_ECHO | (i & 0x1f))) {
				if (verbose > 0)
					fprintf(stderr, "com test %2d: 0x%02x ok\n", i, CMD_BYTE_ECHO | (i & 0x1f));
				break;
			}
			fprintf(stderr, "xpcu-prog: com test %2d failed: 0x%02x != 0x%02x\n", i, buf, CMD_BYTE_ECHO | (i & 0x1f));
			xpcu_psoc_release(dh);
			usb_close(dh);
			return -1;
		}
	}

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
	send_byte(cpld_mode ? CMD_BYTE_SYNC_CPLD : CMD_BYTE_SYNC_JTAG);
	report = recv_byte();
	return (report & CMD_BYTE_REPORT_FLAG_ERR) != 0 ? -1 : 0;
}

static int h_pulse_tck(struct libxsvf_host *h, int tms, int tdi, int tdo, int rmask, int sync)
{
	send_byte((cpld_mode ? CMD_BYTE_PULSE_CPLD : CMD_BYTE_PULSE_JTAG) |
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

	cpld_mode = 0;
	while ((opt = getopt(argc, argv, "vcs:x:XCJ")) != -1)
	{
		if (opt == 'v')
			verbose++;
		else if (opt == 's' || opt == 'x' || opt == 'X') {
			int cpld_mode_backup = cpld_mode;
			did_something = 1;
			if (opt == 'X') {
				cpld_mode = 1;
				fp = vfile_open_cpldxsvf();
			} else if (!strcmp(optarg, "-"))
				fp = stdin;
			else
				fp = fopen(optarg, "rb");
			if (fp == NULL) {
				fprintf(stderr, "Can't open %s file `%s': %s\n", opt == 's' ? "SVF" : "XSVF", opt == 'X' ? "CPLD XSVF" : optarg, strerror(errno));
				error = 1;
				continue;
			}
			if (libxsvf_play(&h, opt == 's' ? LIBXSVF_MODE_SVF : LIBXSVF_MODE_XSVF) < 0) {
				fprintf(stderr, "Error while playing %s file `%s'.\n", opt == 's' ? "SVF" : "XSVF", opt == 'X' ? "CPLD XSVF" : optarg);
				error = 1;
			}
			if (opt == 'X' || strcmp(optarg, "-"))
				fclose(fp);
			fp = NULL;
			cpld_mode = cpld_mode_backup;
		}
		else if (opt == 'c') {
			did_something = 1;
			if (libxsvf_play(&h, LIBXSVF_MODE_SCAN) < 0) {
				fprintf(stderr, "Error while scanning JTAG chain.\n");
				error = 1;
			}
		}
		else if (opt == 'C')
			cpld_mode = 1;
		else if (opt == 'J')
			cpld_mode = 0;
		else
			goto print_help;
	}

	if (!did_something) {
print_help:
		error = 1;
		fprintf(stderr, "\n");
		fprintf(stderr, "Usage: %s [ -v.. ] { -J | -C | -X | -c | -s svf_file | -x xsvf_file } [ ... ]\n", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "   -v      incrase verbosity\n");
		fprintf(stderr, "   -J      all following transaction are for the JTAG port (default)\n");
		fprintf(stderr, "   -C      all following transaction are for the CPLD in the cable\n");
		fprintf(stderr, "   -X      program the CPLD with the built-in default XSVF file\n");
		fprintf(stderr, "   -c      perform a bus scan and list all devices\n");
		fprintf(stderr, "   -s      execute the commands from an SVF file\n");
		fprintf(stderr, "   -x      execute the commands from an XSVF file\n");
		fprintf(stderr, "\n");
	} else if (error) {
		fprintf(stderr, "Got errors!\n");
	}

	return error;
}


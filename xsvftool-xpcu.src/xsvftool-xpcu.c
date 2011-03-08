/*
 *  xsvftool-xpcu - An (X)SVF player for the Xilinx Platform Cable USB
 *
 *  Copyright (C) 2011  RIEGL Research ForschungsGmbH
 *  Copyright (C) 2011  Clifford Wolf <clifford@clifford.at>
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

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include "libxsvf.h"
#include "fx2usb-interface.h"

#include "filedata.h"

char *correct_cksum =
#include "hardware_cksum_c.inc"
;

#define UNUSED __attribute__((unused))

/**** BEGIN: http://svn.clifford.at/tools/trunk/examples/check.h ****/

// This is to not confuse the VIM syntax highlighting
#define CHECK_VAL_OPEN (
#define CHECK_VAL_CLOSE )

#define CHECK(result, check)                                         \
   CHECK_VAL_OPEN{                                                   \
     typeof(result) _R = (result);                                   \
     if (!(_R check)) {                                              \
       fprintf(stderr, "Error from '%s' (%d %s) in %s:%d.\n",        \
               #result, (int)_R, #check, __FILE__, __LINE__);        \
       fprintf(stderr, "ERRNO(%d): %s\n", errno, strerror(errno));   \
       abort();                                                      \
     }                                                               \
     _R;                                                             \
   }CHECK_VAL_CLOSE

#define CHECK_PTR(result, check)                                         \
   CHECK_VAL_OPEN{                                                   \
     typeof(result) _R = (result);                                   \
     if (!(_R check)) {                                              \
       fprintf(stderr, "Error from '%s' (%p %s) in %s:%d.\n",        \
               #result, (void*)_R, #check, __FILE__, __LINE__);        \
       fprintf(stderr, "ERRNO(%d): %s\n", errno, strerror(errno));   \
       abort();                                                      \
     }                                                               \
     _R;                                                             \
   }CHECK_VAL_CLOSE

/**** END: http://svn.clifford.at/tools/trunk/examples/check.h ****/

FILE *file_fp = NULL;

int usb_vendor_id = 0;
int usb_device_id = 0;
char *usb_device_file = NULL;

int mode_frequency = 6000;
int mode_async_check = 0;
int mode_internal_cpld = 0;
int mode_8bit_per_cycle = 0;
int mode_hex_rmask = 0;

usb_dev_handle *fx2usb;
int internal_jtag_scan_test = 0;

int sync_count;
int tck_cycle_count;
int blocks_without_sync;

int tdo_check_period_100;
int tdo_check_thisperiod;

int rmask_bits = 0, rmask_bytes = 0;
unsigned char *rmask_data = NULL;

/* This constant are used to determine when the error status should be synced.
 * Syncinc to often would slow things down, syncinc not often enough might cause
 * errors beeing reported by far to late.
 */
#define FORCE_SYNC_AFTER_N_BLOCKS 100
#define FORCE_SYNC_MIN_PERIOD   10000
#define FORCE_SYNC_INIT_PERIOD 100000

// send larger junks to USB stack and let the kernel split it up
// #define MAXBUF() (mode_internal_cpld ? 50 : mode_8bit_per_cycle ? 500 : 1000)
#define MAXBUF() (mode_internal_cpld ? 50 : 4000)

unsigned char fx2usb_retbuf[65];
int fx2usb_retlen;

unsigned char commandbuf[4096];
int commandbuf_len;

static void shrink_8bit_to_4bit()
{
	int i;
	if ((commandbuf_len & 1) != 0)
		commandbuf[commandbuf_len++] = 0;
	for (i = 0; i<commandbuf_len; i++) {
		if ((i & 1) == 0)
			commandbuf[i >> 1] = (commandbuf[i >> 1] & 0xf0) | commandbuf[i];
		else
			commandbuf[i >> 1] = (commandbuf[i >> 1] & 0x0f) | (commandbuf[i] << 4);
	}
	commandbuf_len = commandbuf_len >> 1;
}

void fx2usb_command(const char *cmd)
{
	// fprintf(stderr, "Sending FX2USB Command: '%s' => ", cmd);
	fx2usb_send_chunk(fx2usb, 1, cmd, strlen(cmd));
	fx2usb_recv_chunk(fx2usb, 1, fx2usb_retbuf, sizeof(fx2usb_retbuf)-1, &fx2usb_retlen);
	fx2usb_retbuf[fx2usb_retlen] = 0;
	// fprintf(stderr, "'%s'\n", fx2usb_retbuf);
	
	if (strchr((char*)fx2usb_retbuf, '!') != NULL) {
		fprintf(stderr, "Internal ERROR in communication with probe: '%s' => '%s'\n", cmd, fx2usb_retbuf);
		abort();
	}
}

static int xpcu_set_frequency(struct libxsvf_host *h UNUSED, int v);
static int xpcu_pulse_tck(struct libxsvf_host *h UNUSED, int tms, int tdi, int tdo, int rmask UNUSED, int sync);

static int xpcu_setup(struct libxsvf_host *h UNUSED)
{
	sync_count = 0;
	blocks_without_sync = 0;
	commandbuf_len = 0;
	fx2usb_command("R");

	if (!mode_internal_cpld) {
		fx2usb_command("B1");
	}

	if (mode_frequency)
		xpcu_set_frequency(h, mode_frequency * 1000);

	tdo_check_period_100 = FORCE_SYNC_INIT_PERIOD * 100;
	tdo_check_thisperiod = 0;

	return 0;
}

static int xpcu_shutdown(struct libxsvf_host *h UNUSED)
{
	int rc = 0;
	if (commandbuf_len != 0) {
		fprintf(stderr, "Found %d unsynced commands in command buffer on interface shutdown!\n", commandbuf_len);
		commandbuf_len = 0;
		rc = -1;
	}
	fx2usb_command("S");
	if (fx2usb_retbuf[mode_internal_cpld ? 1 : 0] == '1') {
		fprintf(stderr, "Found pending errors in interface status on shutdown!\n");
		rc = -1;
	}
	fx2usb_command("R");
	return rc;
}

static void xpcu_udelay(struct libxsvf_host *h UNUSED, long usecs, int tms, long num_tck)
{
	struct timeval tv1, tv2;
	long rem_usecs;

	if (mode_internal_cpld)
	{
		if (!mode_8bit_per_cycle)
			shrink_8bit_to_4bit();
		unsigned char tempbuf[64];
		tempbuf[0] = 'J';
		memcpy(tempbuf+1, commandbuf, commandbuf_len);
		fx2usb_send_chunk(fx2usb, 1, tempbuf, commandbuf_len + 1);

		fx2usb_command("P");
		commandbuf_len = 0;
	}
	else
	{
		sync_count = 0x08 | ((sync_count+1) & 0x0f);
		commandbuf[commandbuf_len++] = 0x01;
		commandbuf[commandbuf_len++] = sync_count;
		if (!mode_8bit_per_cycle)
			shrink_8bit_to_4bit();
		fx2usb_send_chunk(fx2usb, 2, commandbuf, commandbuf_len);
		commandbuf_len = 0;

		char cmd[3];
		snprintf(cmd, 3, "W%x", sync_count);
		fx2usb_command(cmd);
	}

	gettimeofday(&tv1, NULL);

	while (num_tck > 0) {
		xpcu_pulse_tck(h, tms, 0, -1, 0, 0);
		num_tck--;
	}

	if (mode_internal_cpld)
	{
		if (!mode_8bit_per_cycle)
			shrink_8bit_to_4bit();
		unsigned char tempbuf[64];
		tempbuf[0] = 'J';
		memcpy(tempbuf+1, commandbuf, commandbuf_len);
		fx2usb_send_chunk(fx2usb, 1, tempbuf, commandbuf_len + 1);

		fx2usb_command("P");
		commandbuf_len = 0;
	}
	else
	{
		sync_count = 0x08 | ((sync_count+1) & 0x0f);
		commandbuf[commandbuf_len++] = 0x01;
		commandbuf[commandbuf_len++] = sync_count;
		if (!mode_8bit_per_cycle)
			shrink_8bit_to_4bit();
		fx2usb_send_chunk(fx2usb, 2, commandbuf, commandbuf_len);
		commandbuf_len = 0;

		char cmd[3];
		snprintf(cmd, 3, "W%x", sync_count);
		fx2usb_command(cmd);
	}

	while (usecs > 0) {
		gettimeofday(&tv2, NULL);
		rem_usecs = usecs - ((tv2.tv_sec - tv1.tv_sec)*1000000 + (tv2.tv_usec - tv1.tv_usec));
		if (rem_usecs <= 0)
			break;
		usleep(rem_usecs);
	}
}

static int xpcu_getbyte(struct libxsvf_host *h UNUSED)
{
	return fgetc(file_fp);
}

static int xpcu_pulse_tck(struct libxsvf_host *h UNUSED, int tms, int tdi, int tdo, int rmask, int sync)
{
	int dummy_sync = 0;

	tck_cycle_count++;

	if (tdo >= 0) {
		commandbuf[commandbuf_len++] = 0x08 | ((tdo & 1) << 2) | ((tms & 1) << 1) | ((tdi & 1) << 0);
		tdo_check_period_100 = (tdo_check_period_100 * 99) / 100 + tdo_check_thisperiod;
		tdo_check_thisperiod = 0;
	} else {
		commandbuf[commandbuf_len++] = 0x04 | ((tms & 1) << 1) | ((tdi & 1) << 0);
	}

	if (mode_async_check == 0)
	{
		if (!sync && tdo >= 0 && (blocks_without_sync > FORCE_SYNC_AFTER_N_BLOCKS || tdo_check_period_100 > FORCE_SYNC_MIN_PERIOD))
			sync = 1;
		if (!sync && !mode_internal_cpld && blocks_without_sync > 10*FORCE_SYNC_AFTER_N_BLOCKS && commandbuf_len >= (MAXBUF() - 10))
			dummy_sync = 1;
	}

	if (rmask && !sync)
		dummy_sync = 1;

	if ((dummy_sync || sync) && !mode_internal_cpld) {
		sync_count = 0x08 | ((sync_count+1) & 0x0f);
		commandbuf[commandbuf_len++] = 0x01;
		commandbuf[commandbuf_len++] = sync_count;
	}

	if (commandbuf_len >= (MAXBUF() - 4) || sync || dummy_sync) {
		if (!mode_8bit_per_cycle)
			shrink_8bit_to_4bit();
		if (mode_internal_cpld) {
			unsigned char tempbuf[64];
			tempbuf[0] = 'J';
			memcpy(tempbuf+1, commandbuf, commandbuf_len);
			fx2usb_send_chunk(fx2usb, 1, tempbuf, commandbuf_len + 1);
		} else {
			fx2usb_send_chunk(fx2usb, 2, commandbuf, commandbuf_len);
		}
		blocks_without_sync++;
		commandbuf_len = 0;
	}

	if ((sync || dummy_sync) && !mode_internal_cpld) {
		char cmd[3];
		snprintf(cmd, 3, "W%x", sync_count);
		fx2usb_command(cmd);
	}

	if (sync) {
		fx2usb_command("S");
		blocks_without_sync = 0;
	}

	if (dummy_sync) {
		fx2usb_command("P");
		blocks_without_sync = 0;
	}

	if (rmask) {
		if (rmask_bits >= 8*rmask_bytes) {
			int old_rmask_bytes = rmask_bytes;
			rmask_bytes = rmask_bytes ? rmask_bytes*2 : 64;
			rmask_data = realloc(rmask_data, rmask_bytes);
			memset(rmask_data + old_rmask_bytes, 0, rmask_bytes-old_rmask_bytes);
		}
		if (fx2usb_retbuf[mode_internal_cpld ? 5 : 4] == '1')
			rmask_data[rmask_bits/8] |= 1 << (rmask_bits%8);
		rmask_bits++;
	}

	if (sync) {
		if (fx2usb_retbuf[mode_internal_cpld ? 1 : 0] == '1')
			return -1;
		return fx2usb_retbuf[mode_internal_cpld ? 5 : 4] == '1';
	}
	return tdo < 0 ? 1 : tdo;
}

static int xpcu_sync(struct libxsvf_host *h UNUSED)
{
	if (!mode_internal_cpld) {
		sync_count = 0x08 | ((sync_count+1) & 0x0f);
		commandbuf[commandbuf_len++] = 0x01;
		commandbuf[commandbuf_len++] = sync_count;
	}

	if (!mode_8bit_per_cycle)
		shrink_8bit_to_4bit();
	if (mode_internal_cpld) {
		unsigned char tempbuf[64];
		tempbuf[0] = 'J';
		memcpy(tempbuf+1, commandbuf, commandbuf_len);
		fx2usb_send_chunk(fx2usb, 1, tempbuf, commandbuf_len + 1);
	} else {
		fx2usb_send_chunk(fx2usb, 2, commandbuf, commandbuf_len);
	}
	commandbuf_len = 0;

	if (!mode_internal_cpld) {
		char cmd[3];
		snprintf(cmd, 3, "W%x", sync_count);
		fx2usb_command(cmd);
	}

	fx2usb_command("S");
	blocks_without_sync = 0;
	if (fx2usb_retbuf[mode_internal_cpld ? 1 : 0] == '1')
		return -1;

	return 0;
}

static int xpcu_set_frequency(struct libxsvf_host *h UNUSED, int v)
{
	int freq = 24000000, delay = 0;

	if (mode_internal_cpld)
		return 0;

	if (!mode_internal_cpld) {
		sync_count = 0x08 | ((sync_count+1) & 0x0f);
		commandbuf[commandbuf_len++] = 0x01;
		commandbuf[commandbuf_len++] = sync_count;
	}

	if (!mode_8bit_per_cycle)
		shrink_8bit_to_4bit();
	if (mode_internal_cpld) {
		unsigned char tempbuf[64];
		tempbuf[0] = 'J';
		memcpy(tempbuf+1, commandbuf, commandbuf_len);
		fx2usb_send_chunk(fx2usb, 1, tempbuf, commandbuf_len + 1);
	} else {
		fx2usb_send_chunk(fx2usb, 2, commandbuf, commandbuf_len);
	}
	commandbuf_len = 0;

	if (!mode_internal_cpld) {
		char cmd[3];
		snprintf(cmd, 3, "W%x", sync_count);
		fx2usb_command(cmd);
	}

	while (delay < 250 && v < freq) {
		delay++;
		freq = 48000000 / (2*delay + 2);
	}

	if (v < freq)
		fprintf(stderr, "Requested FREQUENCY %dHz is to low! Using minimum value %dHz instead.\n", v, freq);
	else if (v-freq > 10)
		fprintf(stderr, "Requested FREQUENCY is %dHz. Using %dHz (24MHz/%d) instead.\n", v, freq, delay+1);

	char cmd[4];
	snprintf(cmd, 4, "T%02x", delay);
	fx2usb_command(cmd);

	mode_8bit_per_cycle = delay != 0;

	if (!mode_internal_cpld)
	{
		sync_count = 0x08 | ((sync_count+1) & 0x0f);
		commandbuf[commandbuf_len++] = 0x01;
		commandbuf[commandbuf_len++] = sync_count;
		if (!mode_8bit_per_cycle)
			shrink_8bit_to_4bit();
		fx2usb_send_chunk(fx2usb, 2, commandbuf, commandbuf_len);
		commandbuf_len = 0;

		char cmd[3];
		snprintf(cmd, 3, "W%x", sync_count);
		fx2usb_command(cmd);
	}

	return 0;
}

static void xpcu_report_tapstate(struct libxsvf_host *h UNUSED)
{
	// fprintf(stderr, "[%s]\n", libxsvf_state2str(h->tap_state));
}

static void xpcu_report_device(struct libxsvf_host *h UNUSED, unsigned long idcode)
{
	if (internal_jtag_scan_test != 0) {
		// CPLD should be: idcode=0x16d4a093, revision=0x1, part=0x6d4a, manufactor=0x049
		if (((idcode >> 12) & 0xffff) == 0x6d4a && ((idcode >> 1) & 0x7ff) == 0x049)
			internal_jtag_scan_test += 1;
		else
			internal_jtag_scan_test += 2;
	} else {
		printf("idcode=0x%08lx, revision=0x%01lx, part=0x%04lx, manufactor=0x%03lx\n", idcode,
				(idcode >> 28) & 0xf, (idcode >> 12) & 0xffff, (idcode >> 1) & 0x7ff);
	}
}

static void xpcu_report_status(struct libxsvf_host *h UNUSED, const char *message UNUSED)
{
	// fprintf(stderr, "[STATUS] %s\n", message);
}

static void xpcu_report_error(struct libxsvf_host *h UNUSED, const char *file, int line, const char *message)
{
	fprintf(stderr, "[%s:%d] %s\n", file, line, message);
}

static void *xpcu_realloc(struct libxsvf_host *h UNUSED, void *ptr, int size, enum libxsvf_mem which UNUSED)
{
	return realloc(ptr, size);
}

static struct libxsvf_host h = {
	.udelay = xpcu_udelay,
	.setup = xpcu_setup,
	.shutdown = xpcu_shutdown,
	.getbyte = xpcu_getbyte,
	.pulse_tck = xpcu_pulse_tck,
	.sync = xpcu_sync,
	.set_frequency = xpcu_set_frequency,
	.report_tapstate = xpcu_report_tapstate,
	.report_device = xpcu_report_device,
	.report_status = xpcu_report_status,
	.report_error = xpcu_report_error,
	.realloc = xpcu_realloc
};

const char *progname;

static void help()
{
	fprintf(stderr, "\n");
	fprintf(stderr, "A JTAG SVF/XSVF Player based on libxsvf for the Xilinx Platform Cable USB\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "xsvftool-xpcu, part of Lib(X)SVF (http://www.clifford.at/libxsvf/).\n");
	fprintf(stderr, "Copyright (C) 2011  RIEGL Research ForschungsGmbH\n");
	fprintf(stderr, "Copyright (C) 2011  Clifford Wolf <clifford@clifford.at>\n");
	fprintf(stderr, "Lib(X)SVF is free software licensed under the BSD license.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [ -L | -B ] [ -d <vendor>:<device> | -D <device_file> ] [ -f kHz ] [ -A ] [ -P ]\n", progname);
	fprintf(stderr, "       %*s { -E | -p | -s svf-file | -x xsvf-file | -c } ...\n", strlen(progname), "");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -L, -B\n");
	fprintf(stderr, "          Print RMASK bits as hex value (little or big endian)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -d <vendor>:<device>\n");
	fprintf(stderr, "          Open the device with this USB vendor and device ID.\n");
	fprintf(stderr, "          (default: 03fd:000d and 04b4:8613\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -D <device_file>\n");
	fprintf(stderr, "          Open this USB device (usually something like /dev/bus/usb/012/345)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -f kHz\n");
	fprintf(stderr, "          Run probe with the specified frequency (default=6000, max=24000)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -A\n");
	fprintf(stderr, "          Use full asynchonous error checking\n");
	fprintf(stderr, "          (very fast but error reporting might be delayed)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -P\n");
	fprintf(stderr, "          Use CPLD on probe as target device\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -p\n");
	fprintf(stderr, "          Force (re-)programming the CPLD on the probe\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -E\n");
	fprintf(stderr, "          Erase the CPLD on the probe\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -s svf-file\n");
	fprintf(stderr, "          Play the specified SVF file\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -x xsvf-file\n");
	fprintf(stderr, "          Play the specified XSVF file\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -c\n");
	fprintf(stderr, "          List devices in JTAG chain\n");
	fprintf(stderr, "\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int rc = 0;
	int gotaction = 0;
	int opt, i, j;

	int done_initialization = 0;

	progname = argc >= 1 ? argv[0] : "xsvftool-xpcu";
	while ((opt = getopt(argc, argv, "LBd:D:f:APpEs:x:c")) != -1)
	{
		if (!done_initialization && (opt == 'p' || opt == 'E' || opt == 's' || opt == 'x' || opt == 'c'))
		{
			usb_init();
			usb_find_busses();
			usb_find_devices();

			fx2usb = fx2usb_open(usb_vendor_id, usb_device_id, usb_device_file);
			if (fx2usb == NULL) {
				fprintf(stderr, "Failed to find or open USB device!\n");
				exit(1);
			}
			CHECK(fx2usb_claim(fx2usb), == 0);

			FILE *ihexf = CHECK_PTR(fmemopen(firmware_ihx, sizeof(firmware_ihx), "r"), != NULL);
			CHECK(fx2usb_upload_ihex(fx2usb, ihexf), == 0);
			CHECK(fclose(ihexf), == 0);

			i = mode_internal_cpld;
			mode_internal_cpld = 1;
			internal_jtag_scan_test = 1;
			libxsvf_play(&h, LIBXSVF_MODE_SCAN);

			if (internal_jtag_scan_test != 2) {
				fprintf(stderr, "Probe (device %s on bus %s) failed internal JTAG scan test!\n",
					usb_device(fx2usb)->filename, usb_device(fx2usb)->bus->dirname);
				exit(1);
			}
			mode_internal_cpld = i;
			internal_jtag_scan_test = 0;

			fprintf(stderr, "Connected to probe (device %s on bus %s) and passed internal JTAG scan test.\n",
				usb_device(fx2usb)->filename, usb_device(fx2usb)->bus->dirname);

			if (opt != 'p' && opt != 'E' && !mode_internal_cpld) {
				fx2usb_command("C");
				if (memcmp(correct_cksum, fx2usb_retbuf, 6)) {
					fprintf(stderr, "Mismatch in CPLD checksum (is=%.6s, should=%s): reprogramming CPLD on probe..\n",
							fx2usb_retbuf, correct_cksum);
					i = mode_internal_cpld;
					mode_internal_cpld = 1;
					file_fp = CHECK_PTR(fmemopen(hardware_svf, sizeof(hardware_svf), "r"), != NULL);
					libxsvf_play(&h, LIBXSVF_MODE_SVF);
					mode_internal_cpld = i;
					fclose(file_fp);
				}
			}

			done_initialization = 1;
		}

		switch (opt)
		{
		case 'L':
			mode_hex_rmask = 1;
			break;
		case 'B':
			mode_hex_rmask = 2;
			break;
		case 'd':
			if (usb_vendor_id || usb_device_id || usb_device_file || fx2usb)
				help();
			if (sscanf(optarg, "%x:%x", &usb_vendor_id, &usb_device_id) != 2)
				help();
			break;
		case 'D':
			if (usb_vendor_id || usb_device_id || usb_device_file || fx2usb)
				help();
			usb_device_file = strdup(optarg);
			break;
		case 'f':
			mode_frequency = atoi(optarg);
			break;
		case 'P':
			mode_internal_cpld = 1;
			break;
		case 'A':
			mode_async_check = 1;
			break;
		case 'p':
		case 'E':
			gotaction = 1;
			i = mode_internal_cpld;
			mode_internal_cpld = 1;
			if (opt == 'p') {
				file_fp = CHECK_PTR(fmemopen(hardware_svf, sizeof(hardware_svf), "r"), != NULL);
				fprintf(stderr, "(Re-)programming CPLD on the probe..\n");
			} else {
				file_fp = CHECK_PTR(fmemopen(erasecpld_svf, sizeof(erasecpld_svf), "r"), != NULL);
				fprintf(stderr, "Erasing CPLD on the probe..\n");
			}
			libxsvf_play(&h, LIBXSVF_MODE_SVF);
			mode_internal_cpld = i;
			fclose(file_fp);
			break;
		case 'x':
		case 's':
			gotaction = 1;
			if (!strcmp(optarg, "-"))
				file_fp = stdin;
			else
				file_fp = fopen(optarg, "rb");
			if (file_fp == NULL) {
				fprintf(stderr, "Can't open %s file `%s': %s\n", opt == 's' ? "SVF" : "XSVF", optarg, strerror(errno));
				rc = 1;
				break;
			}
			fprintf(stderr, "Playing %s file `%s'..\n", opt == 's' ? "SVF" : "XSVF", optarg);
			if (libxsvf_play(&h, opt == 's' ? LIBXSVF_MODE_SVF : LIBXSVF_MODE_XSVF) < 0) {
				fprintf(stderr, "Error while playing %s file `%s'.\n", opt == 's' ? "SVF" : "XSVF", optarg);
				rc = 1;
			}
			if (strcmp(optarg, "-"))
				fclose(file_fp);
			break;
		case 'c':
			gotaction = 1;
			fprintf(stderr, "Scanning JTAG chain..\n");
			if (libxsvf_play(&h, LIBXSVF_MODE_SCAN) < 0) {
				fprintf(stderr, "Error while scanning JTAG chain.\n");
				rc = 1;
			}
			break;
		default:
			help();
			break;
		}
	}

	if (!gotaction)
		help();

	if (rmask_bits > 0) {
		fprintf(stderr, "Total number of rmask bits acquired: %d\n", rmask_bits);
		if (mode_hex_rmask) {
			printf("0x");
			for (i = 0; i < rmask_bits; i += 4) {
				int val = 0;
				for (j = i; j < i + 4; j++) {
					int pos = mode_hex_rmask > 1 ? j : rmask_bits - j - 1;
					val = (val << 1) | (rmask_data[pos/8] & (1 << (pos%8)) ? 1 : 0);
				}
				printf("%x", val);
			}
		} else {
			for (i = 0; i < rmask_bits; i++)
				putchar((rmask_data[i / 8] & (1 << (i % 8))) != 0 ? '1' : '0');
		}
		putchar('\n');
	}

	if (done_initialization) {
		fx2usb_command("X");
		fx2usb_release(fx2usb);
		usb_close(fx2usb);
	}

	fprintf(stderr, "Total number of JTAG clock cycles performed: %d\n", tck_cycle_count);
	if (rc == 0)
		fprintf(stderr, "READY.\n");
	else
		fprintf(stderr, "TERMINATED WITH ERROR(s)! (see above)\n");

	return rc;
}


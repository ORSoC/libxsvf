/*
 *  Lib(X)SVF  -  A library for implementing SVF and XSVF JTAG players
 *
 *  Copyright (C) 2009  RIEGL Research ForschungsGmbH
 *  Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>
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
 *  A JTAG SVF/XSVF Player based on libxsvf for the FTDI FT232H, FT2232H and
 *  FT4232H High Speed USB to Multipurpose UART/FIFO ICs.
 *
 *  This also serves as an example program for using libxvsf with asynchonous
 *  hardware interfaces. Have a look at 'xsvftool-gpio.c' for a simple libxsvf
 *  example for synchonous interfaces (such as register mapped GPIOs).
 *
 *  IMPORTANT NOTE: You need libftdi [1] (version 0.16 or newer) installed
 *  to build this program.
 *
 *  To run it at full speed you need a version of libftdi that has been
 *  compiled with '--with-async-mode', and must uncomment all three
 *  defines ASYNC_WRITE, BACKGROUND_READ and INTERLACED_READ_WRITE below.
 *
 *  [1] http://www.intra2net.com/en/developer/libftdi/
 */

#include "libxsvf.h"

#define BUFFER_SIZE (1024*16)

// #define ASYNC_WRITE
// #define BACKGROUND_READ
// #define INTERLACED_READ_WRITE

#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <ftdi.h>
#include <math.h>
#ifdef BACKGROUND_READ
#  include <pthread.h>
#endif

struct read_job_s;
struct udata_s;
struct buffer_s;

typedef void job_handler_t(struct udata_s *u, struct read_job_s *job, unsigned char *data);

struct read_job_s {
	struct read_job_s *next;
	int data_len, bits_len;
	struct buffer_s *buffer;
	job_handler_t *handler;
	unsigned int command_id;
};

struct buffer_s {
	unsigned int tms:1;
	unsigned int tdi:1;
	unsigned int tdi_enable:1;
	unsigned int tdo:1;
	unsigned int tdo_enable:1;
	unsigned int rmask:1;
};

struct udata_s {
	FILE *f;
	struct ftdi_context ftdic;
	int buffer_size;
	struct buffer_s buffer[BUFFER_SIZE];
	struct read_job_s *job_fifo_out, *job_fifo_in;
	int last_tms;
	int last_tdo;
	int buffer_i;
	int retval_i;
	int retval[256];
	int error_rc;
	int verbose;
	int syncmode;
	int forcemode;
	int frequency;
#ifdef BACKGROUND_READ
#  ifdef INTERLACED_READ_WRITE
	int total_job_bits;
	int writer_wait_flag;
	pthread_mutex_t writer_wait_flag_mutex;
#  endif
	int reader_terminate;
	pthread_mutex_t read_write_mutex;
	pthread_cond_t read_more_cond;
	pthread_cond_t read_done_cond;
	pthread_t read_thread;
#endif
};

static FILE *dumpfile = NULL;

static void write_dumpfile(int wr, unsigned char *buf, int size, unsigned int command_id)
{
	int i;
	if (!dumpfile)
		return;
	fprintf(dumpfile, "%s[%u] %04x:", wr ? "SEND" : "RECV", command_id, size);
	for (i = 0; i < size; i++)
		fprintf(dumpfile, " %02x", buf[i]);
	fprintf(dumpfile, "\n");
}

static int my_ftdi_read_data(struct ftdi_context *ftdi, unsigned char *buf, int size, unsigned int command_id)
{
	int pos = 0;
	int poll_count = 0;
	while (pos < size) {
		int rc = ftdi_read_data(ftdi, buf+pos, size-pos);
		if (rc < 0) {
			fprintf(stderr, "[***] ftdi_read_data returned error `%s' (rc=%d).\n", ftdi_get_error_string(ftdi), rc);
			break;
		}
		// this check should only be needed for very low JTAG clock frequencies
		if (rc == 0) {
			if (++poll_count > 8) {
				fprintf(stderr, "[***] my_ftdi_read_data gives up polling <id=%u, pos=%u, size=%u>.\n", command_id, pos, size);
				break;
			}
			// fprintf(stderr, "[%d/8] my_ftdi_read_data with len=%d polling at %d..\n", poll_count, size, pos);
			usleep(4096 << poll_count);
		}
		pos += rc;
	}
	write_dumpfile(0, buf, pos, command_id);
	return pos;
}

static struct read_job_s *new_read_job(struct udata_s *u, int data_len, int bits_len, struct buffer_s *buffer, job_handler_t *handler)
{
	struct read_job_s *job = calloc(1, sizeof(struct read_job_s));
	static unsigned int command_count = 0;

	job->data_len = data_len;
	job->bits_len = bits_len;
	job->buffer = calloc(bits_len, sizeof(struct buffer_s));
	memcpy(job->buffer, buffer, bits_len*sizeof(struct buffer_s));
	job->handler = handler;
	job->command_id = command_count++;

	if (u->job_fifo_in)
		u->job_fifo_in->next = job;
	if (!u->job_fifo_out)
		u->job_fifo_out = job;
	u->job_fifo_in = job;

#ifdef BACKGROUND_READ
#  ifdef INTERLACED_READ_WRITE
	u->total_job_bits += bits_len;
#  endif
#endif

	return job;
}

static void transfer_tms_job_handler(struct udata_s *u, struct read_job_s *job, unsigned char *data)
{
	int i;
	for (i=0; i<job->bits_len; i++) {
		// seams like output is align to the MSB in the byte and is LSB first
		int bitpos = i + (8 - job->bits_len);
		int line_tdo = (*data & (1 << bitpos)) != 0 ? 1 : 0;
		if (job->buffer[i].tdo_enable && job->buffer[i].tdo != line_tdo)
			u->error_rc = -1;
		if (job->buffer[i].rmask && u->retval_i < 256)
			u->retval[u->retval_i++] = line_tdo;
		u->last_tdo = line_tdo;
	}
}

static void transfer_tms(struct udata_s *u, struct buffer_s *d, int tdi, int len)
{
	int i, rc;

	unsigned char data_command[] = {
		0x6e, len-1, tdi << 7, 0x87
	};

	for (i=0; i<len; i++)
		data_command[2] |= d[i].tms << i;
	data_command[2] |= d[len-1].tms << len;
	u->last_tms = d[len-1].tms;

	struct read_job_s *rj = new_read_job(u, 1, len, d, &transfer_tms_job_handler);

	write_dumpfile(1, data_command, sizeof(data_command), rj->command_id);
#ifdef ASYNC_WRITE
	rc = ftdi_write_data_async(&u->ftdic, data_command, sizeof(data_command));
#else
	rc = ftdi_write_data(&u->ftdic, data_command, sizeof(data_command));
#endif
	if (rc != sizeof(data_command)) {
		fprintf(stderr, "IO Error: Transfer tms write failed: %s (rc=%d/%d)\n",
				ftdi_get_error_string(&u->ftdic), rc, (int)sizeof(data_command));
		u->error_rc = -1;
	}
}

static void transfer_tdi_job_handler(struct udata_s *u, struct read_job_s *job, unsigned char *data)
{
	int i, j, k;
	int bytes = job->bits_len / 8;
	int bits = job->bits_len % 8;

	for (i=0, j=0; j<bytes; j++) {
		for (k=0; k<8; k++, i++) {
			int line_tdo = (data[j] & (1 << k)) != 0 ? 1 : 0;
			if (job->buffer[i].tdo_enable && job->buffer[i].tdo != line_tdo)
				if (!u->forcemode)
					u->error_rc = -1;
			if (job->buffer[j*8+k].rmask && u->retval_i < 256)
				u->retval[u->retval_i++] = line_tdo;
		}
	}
	for (j=0; j<bits; j++, i++) {
		int bitpos = j + (8 - bits);
		int line_tdo = (data[bytes] & (1 << bitpos)) != 0 ? 1 : 0;
		if (job->buffer[i].tdo_enable && job->buffer[i].tdo != line_tdo)
			if (!u->forcemode)
				u->error_rc = -1;
		if (job->buffer[i].rmask && u->retval_i < 256)
			u->retval[u->retval_i++] = line_tdo;
		u->last_tdo = line_tdo;
	}
}

static void transfer_tdi(struct udata_s *u, struct buffer_s *d, int len)
{
	int bytes = len / 8;
	int bits = len % 8;

	int command_len = 1;
	int data_len = 0;
	if (bytes) {
		command_len += 3 + bytes;
		data_len += bytes;
	}
	if (bits) {
		command_len += 3;
		data_len++;
	}

	int i, j, k, rc;
	unsigned char command[command_len];

	i = 0;
	if (bytes) {
		command[i++] = 0x39;
		command[i++] = (bytes-1) & 0xff;
		command[i++] = (bytes-1) >> 8;
		for (j=0; j<bytes; j++, i++) {
			command[i] = 0;
			for (k=0; k<8; k++)
				command[i] |= d[j*8+k].tdi << k;
		}
	}
	if (bits) {
		command[i++] = 0x3b;
		command[i++] = bits-1;
		command[i] = 0;
		for (j=0; j<bits; j++)
			command[i] |= d[bytes*8+j].tdi << j;
		i++;
	}
	command[i] = 0x87;
	assert(i+1 == command_len);

	struct read_job_s *rj = new_read_job(u, data_len, len, d, &transfer_tdi_job_handler);

	write_dumpfile(1, command, command_len, rj->command_id);
#ifdef ASYNC_WRITE
	rc = ftdi_write_data_async(&u->ftdic, command, command_len);
#else
	rc = ftdi_write_data(&u->ftdic, command, command_len);
#endif
	if (rc != command_len) {
		fprintf(stderr, "IO Error: Transfer tdi write failed: %s (rc=%d/%d)\n",
				ftdi_get_error_string(&u->ftdic), rc, command_len);
		u->error_rc = -1;
	}
}

static void process_next_read_job(struct udata_s *u)
{
	if (!u->job_fifo_out)
		return;

#ifdef ASYNC_WRITE
	ftdi_async_complete(&u->ftdic,1);
#endif

	struct read_job_s *job = u->job_fifo_out;
	
	u->job_fifo_out = job->next;
	if (!u->job_fifo_out)
		u->job_fifo_in = NULL;
	
	unsigned char data[job->data_len];
	if (my_ftdi_read_data(&u->ftdic, data, job->data_len, job->command_id) != job->data_len) {
		fprintf(stderr, "IO Error: FTDI/USB read failed!\n");
		u->error_rc = -1;
	} else {
		job->handler(u, job, data);
	}

#ifdef BACKGROUND_READ
#  ifdef INTERLACED_READ_WRITE
	u->total_job_bits -= job->bits_len;
#  endif
#endif
	
	free(job->buffer);
	free(job);
}

#ifdef BACKGROUND_READ
static void *reader_main(void *arg)
{
	struct udata_s *u = arg;
	pthread_mutex_lock(&u->read_write_mutex);
	while (!u->reader_terminate) {
		while (u->job_fifo_out) {
			process_next_read_job(u);
#ifdef INTERLACED_READ_WRITE
			if (u->total_job_bits <= u->buffer_size/2) {
				pthread_mutex_lock(&u->writer_wait_flag_mutex);
				int writer_is_waiting = u->writer_wait_flag;
				pthread_mutex_unlock(&u->writer_wait_flag_mutex);
				if (writer_is_waiting)
					break;
			}
#endif
		}
		pthread_cond_signal(&u->read_done_cond);
		pthread_cond_wait(&u->read_more_cond, &u->read_write_mutex);
	}
	pthread_mutex_unlock(&u->read_write_mutex);
	return NULL;
}
#endif

static void buffer_flush(struct udata_s *u)
{
#ifdef BACKGROUND_READ
#  ifdef INTERLACED_READ_WRITE
	pthread_mutex_lock(&u->writer_wait_flag_mutex);
	u->writer_wait_flag = 1;
	pthread_mutex_unlock(&u->writer_wait_flag_mutex);
	pthread_mutex_lock(&u->read_write_mutex);
	while (u->total_job_bits > u->buffer_size/2) {
		pthread_cond_wait(&u->read_done_cond, &u->read_write_mutex);
	}
#  else
	pthread_mutex_lock(&u->read_write_mutex);
	while (u->job_fifo_out) {
		pthread_cond_wait(&u->read_done_cond, &u->read_write_mutex);
	}
#  endif
#endif
	int pos = 0;
	while (pos < u->buffer_i)
	{
		struct buffer_s b = u->buffer[pos];
		if (u->last_tms != b.tms) {
			int len = u->buffer_i - pos;
			len = len > 6 ? 6 : len;
			int tdi=-1, i;
			for (i=0; i<len; i++) {
				if (!u->buffer[pos+i].tdi_enable)
					continue;
				if (tdi < 0)
					tdi = u->buffer[pos+i].tdi;
				if (tdi != u->buffer[pos+i].tdi)
					len = i;
			}
			// printf("transfer_tms <len=%d, tdi=%d>\n", len, tdi < 0 ? 1 : tdi);
			transfer_tms(u, u->buffer+pos, (tdi & 1), len);
			pos += len;
			continue;
		}
		int len = u->buffer_i - pos;
		int i;
		for (i=0; i<len; i++) {
			if (u->buffer[pos+i].tms != u->last_tms)
				len = i;
		}
		// printf("transfer_tdi <len=%d, tms=%d>\n", len, u->last_tms);
		transfer_tdi(u, u->buffer+pos, len);
		pos += len;
	}
	u->buffer_i = 0;

#ifdef BACKGROUND_READ
#  ifdef INTERLACED_READ_WRITE
	pthread_mutex_lock(&u->writer_wait_flag_mutex);
	u->writer_wait_flag = 0;
	pthread_mutex_unlock(&u->writer_wait_flag_mutex);
#  endif
	pthread_mutex_unlock(&u->read_write_mutex);
	pthread_cond_signal(&u->read_more_cond);
#else
	while (u->job_fifo_out)
		process_next_read_job(u);
#endif
}

static void buffer_sync(struct udata_s *u)
{
	buffer_flush(u);
#ifdef BACKGROUND_READ
	pthread_mutex_lock(&u->read_write_mutex);
	while (u->job_fifo_out) {
		pthread_cond_wait(&u->read_done_cond, &u->read_write_mutex);
	}
	pthread_mutex_unlock(&u->read_write_mutex);
#endif
}

static void buffer_add(struct udata_s *u, int tms, int tdi, int tdo, int rmask)
{
	u->buffer[u->buffer_i].tms = tms;
	u->buffer[u->buffer_i].tdi = tdi;
	u->buffer[u->buffer_i].tdi_enable = tdi >= 0;
	u->buffer[u->buffer_i].tdo = tdo;
	u->buffer[u->buffer_i].tdo_enable = tdo >= 0;
	u->buffer[u->buffer_i].rmask = rmask;
	u->buffer_i++;

	if (u->buffer_i >= u->buffer_size)
		buffer_flush(u);
}

static int h_setup(struct libxsvf_host *h)
{
	int device_is_amontec_jtagkey_2p = 0;

	struct udata_s *u = h->user_data;
	u->buffer_size = BUFFER_SIZE;

	if (ftdi_init(&u->ftdic) < 0)
		return -1;

	if (ftdi_set_interface(&u->ftdic, INTERFACE_A) < 0) {
		fprintf(stderr, "IO Error: Interface setup failed (set port).\n");
		ftdi_deinit(&u->ftdic);
		return -1;
	}

	// 0x0403:0xcff8 = Amontec JTAGkey2P
	if (ftdi_usb_open(&u->ftdic, 0x0403, 0xcff8) == 0) {
		device_is_amontec_jtagkey_2p = 1;
		goto found_device;
	}

	// 0x0403:0x6010 = Plain FTDI 2232H
	if (ftdi_usb_open(&u->ftdic, 0x0403, 0x6010) == 0) {
		goto found_device;
	}

	// 0x0403:0x6011 = Plain FTDI 4232H
	if (ftdi_usb_open(&u->ftdic, 0x0403, 0x6011) == 0) {
		goto found_device;
	}

	// 0x0403:0x6014 = Plain FTDI 232H
	if (ftdi_usb_open(&u->ftdic, 0x0403, 0x6014) == 0) {
		u->buffer_size = 64;
		goto found_device;
	}

	fprintf(stderr, "IO Error: Interface setup failed (can't find or can't open device).\n");
	ftdi_deinit(&u->ftdic);
	return -1;
found_device:;

#if 0
	// Older versions of libftdi don't have the TYPE_232H enum value.
	// So we simply skip this check and let BITMODE_MPSSE below fail for non-H type chips.
	if (u->ftdic.type != TYPE_232H && u->ftdic.type != TYPE_2232H && u->ftdic.type != TYPE_4232H) {
		fprintf(stderr, "IO Error: Interface setup failed (wrong chip type).\n");
		ftdi_usb_close(&u->ftdic);
		ftdi_deinit(&u->ftdic);
		return -1;
	}
#endif

#if 1
	if (ftdi_usb_reset(&u->ftdic) < 0) {
		fprintf(stderr, "IO Error: Interface setup failed (usb reset).\n");
		ftdi_usb_close(&u->ftdic);
		ftdi_deinit(&u->ftdic);
		return -1;
	}

	if (ftdi_usb_purge_buffers(&u->ftdic) < 0) {
		fprintf(stderr, "IO Error: Interface setup failed (purge buffers).\n");
		ftdi_usb_close(&u->ftdic);
		ftdi_deinit(&u->ftdic);
		return -1;
	}
#endif

	if (ftdi_set_bitmode(&u->ftdic, 0xff, BITMODE_MPSSE) < 0) {
		fprintf(stderr, "IO Error: Interface setup failed (MPSSE mode).\n");
		ftdi_usb_close(&u->ftdic);
		ftdi_deinit(&u->ftdic);
		return -1;
	}

	unsigned char plain_init_commands[] = {
		// 0x86, 0x6f, 0x17, // initial clk freq (1 kHz)
		// 0x86, 0x05, 0x00, // initial clk freq (1 MHz)
		0x86, 0x02, 0x00, // initial clk freq (2 MHz)
		0x80, 0x08, 0x0b, // initial line states
		// 0x84, // enable loopback
		0x85, // disable loopback
	};
	unsigned char amontec_init_commands[] = {
		0x86, 0x02, 0x00, // initial clk freq (2 MHz)
		0x80, 0x08, 0x1b, // initial line states
		0x85, // disable loopback
	};
	unsigned char *init_commands_p = plain_init_commands;
	int init_commands_sz = sizeof(plain_init_commands);

	if (device_is_amontec_jtagkey_2p) {
		init_commands_p = amontec_init_commands;
		init_commands_sz = sizeof(amontec_init_commands);
	}

	write_dumpfile(1, init_commands_p, init_commands_sz, 0);
	if (ftdi_write_data(&u->ftdic, init_commands_p, init_commands_sz) != init_commands_sz) {
		fprintf(stderr, "IO Error: Interface setup failed (init commands): %s\n",
				ftdi_get_error_string(&u->ftdic));
		ftdi_disable_bitbang(&u->ftdic);
		ftdi_usb_close(&u->ftdic);
		ftdi_deinit(&u->ftdic);
		return -1;
	}

	if (u->frequency > 0)
		h->set_frequency(h, u->frequency);

	u->job_fifo_out = NULL;
	u->job_fifo_in = NULL;
	u->last_tms = -1;
	u->last_tdo = -1;
	u->buffer_i = 0;
	u->error_rc = 0;

#ifdef BACKGROUND_READ
#  ifdef INTERLACED_READ_WRITE
	u->total_job_bits = 0;
	u->writer_wait_flag = 0;
	pthread_mutex_init(&u->writer_wait_flag_mutex, NULL);
#  endif
	u->reader_terminate = 0;
	pthread_mutex_init(&u->read_write_mutex, NULL);
	pthread_cond_init(&u->read_more_cond, NULL);
	pthread_cond_init(&u->read_done_cond, NULL);
	pthread_create(&u->read_thread, NULL, &reader_main, u);
#endif

	return 0;
}

static int h_shutdown(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	buffer_sync(u);
#ifdef BACKGROUND_READ
	pthread_mutex_lock(&u->read_write_mutex);
	u->reader_terminate = 1;
	pthread_cond_signal(&u->read_more_cond);
	pthread_mutex_unlock(&u->read_write_mutex);

	pthread_join(u->read_thread, NULL);
	pthread_cond_destroy(&u->read_done_cond);
	pthread_cond_destroy(&u->read_more_cond);
	pthread_mutex_destroy(&u->read_write_mutex);
#  ifdef INTERLACED_READ_WRITE
	pthread_mutex_destroy(&u->writer_wait_flag_mutex);
#  endif
#endif
	ftdi_disable_bitbang(&u->ftdic);
	ftdi_usb_close(&u->ftdic);
	ftdi_deinit(&u->ftdic);
	return u->error_rc;
}

static void h_udelay(struct libxsvf_host *h, long usecs, int tms, long num_tck)
{
	struct udata_s *u = h->user_data;
	buffer_sync(u);
	if (num_tck > 0) {
		struct timeval tv1, tv2;
		gettimeofday(&tv1, NULL);
		while (num_tck > 0) {
			buffer_add(u, tms, -1, -1, 0);
			num_tck--;
		}
		buffer_sync(u);
		gettimeofday(&tv2, NULL);
		if (tv2.tv_sec > tv1.tv_sec) {
			usecs -= (1000000 - tv1.tv_usec) + (tv2.tv_sec - tv1.tv_sec - 1) * 1000000;
			tv1.tv_usec = 0;
		}
		usecs -= tv2.tv_usec - tv1.tv_usec;
	}
	if (usecs > 0) {
		usleep(usecs);
	}
}

static int h_getbyte(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	return fgetc(u->f);
}

static int h_sync(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	buffer_sync(u);
	int rc = u->error_rc;
	u->error_rc = 0;
	return rc;
}

static int h_pulse_tck(struct libxsvf_host *h, int tms, int tdi, int tdo, int rmask, int sync)
{
	struct udata_s *u = h->user_data;
	if (u->syncmode)
		sync = 1;
	buffer_add(u, tms, tdi, tdo, rmask);
	if (sync) {
		buffer_sync(u);
		int rc = u->error_rc < 0 ? u->error_rc : u->last_tdo;
		u->error_rc = 0;
		return rc;
	}
	return u->error_rc < 0 ? u->error_rc : 1;
}

static int h_set_frequency(struct libxsvf_host *h, int v)
{
	struct udata_s *u = h->user_data;
	if (u->syncmode && v > 10000)
		v = 10000;
	unsigned char setfreq_command[] = { 0x86, 0x02, 0x00 };
	int div = fmax(ceil(12e6 / (2*v) - 1), 2);
	setfreq_command[1] = div >> 0;
	setfreq_command[2] = div >> 8;
	write_dumpfile(1, setfreq_command, sizeof(setfreq_command), 0);
#ifdef ASYNC_WRITE
	int rc = ftdi_write_data_async(&u->ftdic, setfreq_command, sizeof(setfreq_command));
#else
	int rc = ftdi_write_data(&u->ftdic, setfreq_command, sizeof(setfreq_command));
#endif
	if (rc != sizeof(setfreq_command)) {
		fprintf(stderr, "IO Error: Set frequency write failed: %s (rc=%d/%d)\n",
				ftdi_get_error_string(&u->ftdic), rc, (int)sizeof(setfreq_command));
		u->error_rc = -1;
	}
	return 0;
}

static void h_report_tapstate(struct libxsvf_host *h)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 2)
		printf("[%s]\n", libxsvf_state2str(h->tap_state));
}

static void h_report_device(struct libxsvf_host *h, unsigned long idcode)
{
	printf("idcode=0x%08lx, revision=0x%01lx, part=0x%04lx, manufactor=0x%03lx\n", idcode,
			(idcode >> 28) & 0xf, (idcode >> 12) & 0xffff, (idcode >> 1) & 0x7ff);
}

static void h_report_status(struct libxsvf_host *h, const char *message)
{
	struct udata_s *u = h->user_data;
	if (u->verbose >= 1)
		printf("[STATUS] %s\n", message);
}

static void h_report_error(struct libxsvf_host *h, const char *file, int line, const char *message)
{
	fprintf(stderr, "[%s:%d] %s\n", file, line, message);
}

static void *h_realloc(struct libxsvf_host *h, void *ptr, int size, enum libxsvf_mem which)
{
	return realloc(ptr, size);
}

static struct udata_s u = {
};

static struct libxsvf_host h = {
	.udelay = h_udelay,
	.setup = h_setup,
	.shutdown = h_shutdown,
	.getbyte = h_getbyte,
	.sync = h_sync,
	.pulse_tck = h_pulse_tck,
	.set_frequency = h_set_frequency,
	.report_tapstate = h_report_tapstate,
	.report_device = h_report_device,
	.report_status = h_report_status,
	.report_error = h_report_error,
	.realloc = h_realloc,
	.user_data = &u
};

const char *progname;

static void help()
{
	fprintf(stderr, "\n");
	fprintf(stderr, "A JTAG SVF/XSVF Player based on libxsvf for the FTDI FT232H, FT2232H and\n");
	fprintf(stderr, "FT4232H High Speed USB to Multipurpose UART/FIFO ICs.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "xsvftool-ft2232h, part of Lib(X)SVF (http://www.clifford.at/libxsvf/).\n");
	fprintf(stderr, "Copyright (C) 2009  RIEGL Research ForschungsGmbH\n");
	fprintf(stderr, "Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>\n");
	fprintf(stderr, "Lib(X)SVF is free software licensed under the ISC license.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [ -v[v..] ] [ -d dumpfile ] [ -L | -B ] [ -S ] [ -F ] \\\n", progname);
	fprintf(stderr, "      %*s [ -f freq[k|M] ] { -s svf-file | -x xsvf-file | -c } ...\n", (int)(strlen(progname)+1), "");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -v\n");
	fprintf(stderr, "          Enable verbose output (repeat for incrased verbosity)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -d dumpfile\n");
	fprintf(stderr, "          Write a logfile of all MPSSE comunication\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -L, -B\n");
	fprintf(stderr, "          Print RMASK bits as hex value (little or big endian)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -S\n");
	fprintf(stderr, "          Run in synchronous mode (slow but report errors right away)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -F\n");
	fprintf(stderr, "          Force mode (ignore all TDO mismatches)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -f freq[k|M]\n");
	fprintf(stderr, "          Set maximum frequency in Hz, kHz or MHz\n");
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
	int hex_mode = 0;
	int opt, i, j;

	progname = argc >= 1 ? argv[0] : "xsvftool-ft232h";
	while ((opt = getopt(argc, argv, "vd:LBSFf:x:s:c")) != -1)
	{
		switch (opt)
		{
		case 'v':
			u.verbose++;
			break;
		case 'd':
			if (!strcmp(optarg, "-"))
				dumpfile = stdout;
			else
				dumpfile = fopen(optarg, "w");
			if (!dumpfile) {
				fprintf(stderr, "Can't open dumpfile `%s': %s\n", optarg, strerror(errno));
				rc = 1;
			}
			break;
		case 'f':
			u.frequency = strtol(optarg, &optarg, 10);
			while (*optarg != 0) {
				if (*optarg == 'k') {
					u.frequency *= 1000;
					optarg++;
					continue;
				}
				if (*optarg == 'M') {
					u.frequency *= 1000000;
					optarg++;
					continue;
				}
				if (optarg[0] == 'H' && optarg[1] == 'z') {
					optarg += 2;
					continue;
				}
				help();
			}
			break;
		case 'x':
		case 's':
			gotaction = 1;
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
		case 'c':
			gotaction = 1;
			int old_frequency = u.frequency;
			if (u.frequency == 0)
				u.frequency = 10000;
			if (libxsvf_play(&h, LIBXSVF_MODE_SCAN) < 0) {
				fprintf(stderr, "Error while scanning JTAG chain.\n");
				rc = 1;
			}
			u.frequency = old_frequency;
			break;
		case 'L':
			hex_mode = 1;
			break;
		case 'B':
			hex_mode = 2;
			break;
		case 'S':
			if (u.frequency == 0)
				u.frequency = 10000;
			u.syncmode = 1;
			break;
		case 'F':
			u.forcemode = 1;
			break;
		default:
			help();
			break;
		}
	}

	if (!gotaction)
		help();

	if (u.retval_i) {
		if (hex_mode) {
			printf("0x");
			for (i=0; i < u.retval_i; i+=4) {
				int val = 0;
				for (j=i; j<i+4; j++)
					val = val << 1 | u.retval[hex_mode > 1 ? j : u.retval_i - j - 1];
				printf("%x", val);
			}
		} else {
			printf("%d rmask bits:", u.retval_i);
			for (i=0; i < u.retval_i; i++)
				printf(" %d", u.retval[i]);
		}
		printf("\n");
	}

	return rc;
}


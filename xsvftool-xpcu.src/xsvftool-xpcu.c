
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "libxsvf.h"
#include "fx2usb-interface.h"

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
long file_size, file_fpos;

int mode_internal_cpld = 0;
int mode_8bit_per_cycle = 0;

usb_dev_handle *fx2usb;

int sync_count = 0;

unsigned char fx2usb_retbuf[65];
int fx2usb_retlen;

void fx2usb_command(const char *cmd)
{
	fprintf(stderr, "Sending FX2USB Command: '%s' => ", cmd);
	fx2usb_send_chunk(fx2usb, 1, cmd, strlen(cmd));
	fx2usb_recv_chunk(fx2usb, 1, fx2usb_retbuf, sizeof(fx2usb_retbuf)-1, &fx2usb_retlen);
	fx2usb_retbuf[fx2usb_retlen] = 0;
	fprintf(stderr, "'%s'\n", fx2usb_retbuf);
}

static int xpcu_setup(struct libxsvf_host *h UNUSED)
{
	return 0;
}

static int xpcu_shutdown(struct libxsvf_host *h UNUSED)
{
	return 0;
}

static void xpcu_udelay(struct libxsvf_host *h UNUSED, long usecs UNUSED, int tms UNUSED, long num_tck UNUSED)
{
	/* FIXME */
	return;
}

static int xpcu_getbyte(struct libxsvf_host *h UNUSED)
{
	file_fpos++;
	if ((file_fpos % 1024) == 0 || file_fpos == file_size)
		fprintf(stderr, "\r%6.1f%%\r", (file_fpos*100.0)/file_size);
	return fgetc(file_fp);
}

unsigned char commandbuf[2048];
int commandbuf_len = 0;

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

static int xpcu_pulse_tck(struct libxsvf_host *h UNUSED, int tms, int tdi, int tdo, int rmask UNUSED, int sync)
{
	if (tdo >= 0) {
		commandbuf[commandbuf_len++] = 0x08 | ((tdo & 1) << 2) | ((tms & 1) << 1) | ((tdi & 1) << 0);
	} else {
		commandbuf[commandbuf_len++] = 0x04 | ((tms & 1) << 1) | ((tdi & 1) << 0);
	}

	if (sync && !mode_internal_cpld) {
		sync_count = 0x08 | ((sync_count+1) & 0x0f);
		commandbuf[commandbuf_len++] = 0x01;
		commandbuf[commandbuf_len++] = sync_count;
	}

	int max_commandbuf = mode_internal_cpld ? 50 : mode_8bit_per_cycle ? 1024 : 20148;
	if (commandbuf_len >= (max_commandbuf - 4) || sync) {
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
	}

	if (sync && !mode_internal_cpld) {
		char cmd[3];
		snprintf(cmd, 3, "W%x", sync_count);
		fx2usb_command(cmd);
	}

	if (sync) {
		fx2usb_command("S");
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

	if (!mode_internal_cpld) {
		char cmd[3];
		snprintf(cmd, 3, "W%x", sync_count);
		fx2usb_command(cmd);
	}

	fx2usb_command("S");
	if (fx2usb_retbuf[mode_internal_cpld ? 1 : 0] == '1')
		return -1;

	return 0;
}

static int xpcu_set_frequency(struct libxsvf_host *h UNUSED, int v)
{
	fprintf(stderr, "Ignoring FREQUENCY command (f=%dHz).\n", v);
	return 0;
}

static void xpcu_report_tapstate(struct libxsvf_host *h UNUSED)
{
	// fprintf(stderr, "[%s]\n", libxsvf_state2str(h->tap_state));
}

static void xpcu_report_device(struct libxsvf_host *h UNUSED, unsigned long idcode)
{
	printf("idcode=0x%08lx, revision=0x%01lx, part=0x%04lx, manufactor=0x%03lx\n", idcode,
			(idcode >> 28) & 0xf, (idcode >> 12) & 0xffff, (idcode >> 1) & 0x7ff);
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

int main()
{
	usb_init();
	usb_find_busses();
	usb_find_devices();

	fx2usb = CHECK_PTR(fx2usb_open(), != NULL);
	CHECK(fx2usb_claim(fx2usb), == 0);

	FILE *ihexf = CHECK_PTR(fopen("firmware.ihx", "r"), != NULL);
	CHECK(fx2usb_upload_ihex(fx2usb, ihexf), == 0);
	CHECK(fclose(ihexf), == 0);

	commandbuf_len = 0;
	mode_internal_cpld = 1;
	libxsvf_play(&h, LIBXSVF_MODE_SCAN);

	file_fp = CHECK_PTR(fopen("hardware.svf", "r"), != NULL);
	fseek(file_fp, 0L, SEEK_END);
	file_size = ftell(file_fp);
	fseek(file_fp, 0L, SEEK_SET);

	commandbuf_len = 0;
	mode_internal_cpld = 1;
	libxsvf_play(&h, LIBXSVF_MODE_SVF);
	fclose(file_fp);

	fx2usb_release(fx2usb);
	usb_close(fx2usb);

	return 0;
}


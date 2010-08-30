
#include <stdio.h>
#include <errno.h>
#include "psoc-interface.h"

usb_dev_handle *xpcu_psoc_open()
{
	struct usb_bus *b;
	struct usb_device *d;

	for (b = usb_get_busses(); b; b = b->next) {
		for (d = b->devices; d; d = d->next) {
			// The Xilinx Platform Cable USB Vendor/Device IDs
			if ((d->descriptor.idVendor == 0x03FD) && (d->descriptor.idProduct == 0x000D))
				return usb_open(d);
			// The plain CY7C68013 dev kit Vendor/Device IDs
			if ((d->descriptor.idVendor == 0x04b4) && (d->descriptor.idProduct == 0x8613))
				return usb_open(d);
		}
	}

	return NULL;
}

static int psoc_fwload_ctrl_msg(usb_dev_handle *dh, int addr, const void *data, int len)
{
	int ret = usb_control_msg(dh, 0x40, 0xA0, addr, 0, (char*)data, len, 1000);
	if (ret != len)
		fprintf(stderr, "psoc_fwload_ctrl_msg: usb_control_msg for addr=0x%04X, len=%d returned %d: %s\n", addr, len, ret, ret >= 0 ? "NO ERROR" : usb_strerror());
	return ret == len ? 0 : -1;
}

int xpcu_psoc_upload_ihex(usb_dev_handle *dh, const char *ihex_filename)
{
	uint8_t on = 1, off = 0;

	FILE *fp = fopen(ihex_filename, "r");
	if (fp == NULL) {
		fprintf(stderr, "xpcu_psoc_upload_ihex: can't open file `%s'!\n", ihex_filename);
		return -1;
	}

	// assert reset
	if (psoc_fwload_ctrl_msg(dh, 0xE600, &on, 1) < 0) {
		fprintf(stderr, "xpcu_psoc_upload_ihex: can't assert reset!\n");
		fclose(fp);
		return -1;
	}

	// parse and upload ihex file
	char line[1024];
	int linecount = 0;
	while (fgets(line, sizeof(line), fp) != NULL)
	{
		linecount++;

		if (line[0] != ':')
			continue;

		uint8_t cksum = 0;
		uint8_t ldata[512];
		int lsize = 0;

		while (sscanf(line+1+lsize*2, "%2hhx", &ldata[lsize]) == 1) {
			cksum += ldata[lsize];
			lsize++;
		}

		if (lsize < 5) {
			fprintf(stderr, "xpcu_psoc_upload_ihex: ihex line %d: record is to short!\n", linecount);
			fclose(fp);
			return -1;
		}

		if (ldata[0] != lsize-5) {
			fprintf(stderr, "xpcu_psoc_upload_ihex: ihex line %d: size does not match record length!\n", linecount);
			fclose(fp);
			return -1;
		}

		cksum -= ldata[lsize-1];
		cksum = ~cksum + 1;

		if (cksum != ldata[lsize-1]) {
			fprintf(stderr, "xpcu_psoc_upload_ihex: ihex line %d: cksum error!\n", linecount);
			fclose(fp);
			return -1;
		}

		if (psoc_fwload_ctrl_msg(dh, (ldata[1] << 8) | ldata[2], &ldata[4], ldata[0]) < 0) {
			fprintf(stderr, "xpcu_psoc_upload_ihex: ihex line %d: error in psoc communication!\n", linecount);
			fclose(fp);
			return -1;
		}
	}

	// release reset
	if (psoc_fwload_ctrl_msg(dh, 0xE600, &off, 1) < 0) {
		fprintf(stderr, "xpcu_psoc_upload_ihex: can't release reset!\n");
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}

int xpcu_psoc_claim(usb_dev_handle *dh)
{
#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
	usb_detach_kernel_driver_np(dh, 0);
#endif
	if (usb_claim_interface(dh, 0) < 0) {
		fprintf(stderr, "xpcu_psoc_claim: claiming interface 0 failed: %s!\n", usb_strerror());
		return -1;
	}
	if (usb_set_altinterface(dh, 1) < 0) {
		usb_release_interface(dh, 0);
		fprintf(stderr, "xpcu_psoc_claim: setting alternate interface 1 failed: %s!\n", usb_strerror());
		return -1;
	}
	return 0;
}

void xpcu_psoc_release(usb_dev_handle *dh)
{
	usb_release_interface(dh, 0);
}

void xpcu_psoc_flush(usb_dev_handle *dh)
{
	while (1)
	{
		unsigned char readbuf[2] = { 0, 0 };
		int ret = usb_bulk_read(dh, 1, (char*)readbuf, 2, 10);
		if (ret <= 0)
			return;
		fprintf(stderr, "Unexpected data word from device: 0x%02x 0x%02x (%d)\n", readbuf[0], readbuf[1], ret);
	}
}

int xpcu_psoc_send_chunk(usb_dev_handle *dh, const void *data, int len)
{
	int ret;
retry_write:
	ret = usb_bulk_write(dh, 1, data, len, 1000);
	if (ret == -ETIMEDOUT) {
		fprintf(stderr, "xpcu_psoc_recv_chunk: usb write timeout -> retry\n");
		xpcu_psoc_flush(dh);
		goto retry_write;
	}
	if (ret != len)
		fprintf(stderr, "xpcu_psoc_send_chunk: write of %d bytes to ep 1 returned %d: %s\n", len, ret, ret >= 0 ? "NO ERROR" : usb_strerror());
	return ret == len ? 0 : -1;
}

int xpcu_psoc_recv_chunk(usb_dev_handle *dh, void *data, int len, int *ret_len)
{
	int ret;
retry_read:
	ret = usb_bulk_read(dh, 1, data, len, 1000);
	if (ret == -ETIMEDOUT) {
		fprintf(stderr, "xpcu_psoc_recv_chunk: usb read timeout -> retry\n");
		goto retry_read;
	}
	if (ret > 0 && ret_len != NULL)
		len = *ret_len = ret;
	if (ret != len)
		fprintf(stderr, "xpcu_psoc_recv_chunk: read of %d bytes from ep 1 returned %d: %s\n", len, ret, ret >= 0 ? "NO ERROR" : usb_strerror());
	return ret == len ? 0 : -1;
}


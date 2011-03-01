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

#include <stdio.h>
#include <errno.h>
#include "fx2usb-interface.h"

usb_dev_handle *fx2usb_open()
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

static int fx2usb_fwload_ctrl_msg(usb_dev_handle *dh, int addr, const void *data, int len)
{
	int ret = usb_control_msg(dh, 0x40, 0xA0, addr, 0, (char*)data, len, 1000);
	if (ret != len)
		fprintf(stderr, "fx2usb_fwload_ctrl_msg: usb_control_msg for addr=0x%04X, len=%d returned %d: %s\n", addr, len, ret, ret >= 0 ? "NO ERROR" : usb_strerror());
	return ret == len ? 0 : -1;
}

int fx2usb_upload_ihex(usb_dev_handle *dh, FILE *fp)
{
	uint8_t on = 1, off = 0;

	// assert reset
	if (fx2usb_fwload_ctrl_msg(dh, 0xE600, &on, 1) < 0) {
		fprintf(stderr, "fx2usb_upload_ihex: can't assert reset!\n");
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
			fprintf(stderr, "fx2usb_upload_ihex: ihex line %d: record is to short!\n", linecount);
			return -1;
		}

		if (ldata[0] != lsize-5) {
			fprintf(stderr, "fx2usb_upload_ihex: ihex line %d: size does not match record length!\n", linecount);
			return -1;
		}

		cksum -= ldata[lsize-1];
		cksum = ~cksum + 1;

		if (cksum != ldata[lsize-1]) {
			fprintf(stderr, "fx2usb_upload_ihex: ihex line %d: cksum error!\n", linecount);
			return -1;
		}

		if (fx2usb_fwload_ctrl_msg(dh, (ldata[1] << 8) | ldata[2], &ldata[4], ldata[0]) < 0) {
			fprintf(stderr, "fx2usb_upload_ihex: ihex line %d: error in fx2usb communication!\n", linecount);
			return -1;
		}
	}

	// release reset
	if (fx2usb_fwload_ctrl_msg(dh, 0xE600, &off, 1) < 0) {
		fprintf(stderr, "fx2usb_upload_ihex: can't release reset!\n");
		return -1;
	}

	return 0;
}

int fx2usb_claim(usb_dev_handle *dh)
{
#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
	usb_detach_kernel_driver_np(dh, 0);
#endif
	if (usb_claim_interface(dh, 0) < 0) {
		fprintf(stderr, "fx2usb_claim: claiming interface 0 failed: %s!\n", usb_strerror());
		return -1;
	}
	if (usb_set_altinterface(dh, 1) < 0) {
		usb_release_interface(dh, 0);
		fprintf(stderr, "fx2usb_claim: setting alternate interface 1 failed: %s!\n", usb_strerror());
		return -1;
	}
	return 0;
}

void fx2usb_release(usb_dev_handle *dh)
{
	usb_release_interface(dh, 0);
}

void fx2usb_flush(usb_dev_handle *dh)
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

int fx2usb_send_chunk(usb_dev_handle *dh, int ep, const void *data, int len)
{
	int ret;
#if 0
	if (ep == 2) {
		int i;
		fprintf(stderr, "<ep2:%4d bytes> ...", len);
		for (i = len-16; i < len; i++) {
			if (i < 0)
				continue;
			fprintf(stderr, " %02x", ((unsigned char*)data)[i]);
		}
		fprintf(stderr, "\n");
	}
#endif
retry_write:
	ret = usb_bulk_write(dh, ep, data, len, 1000);
	if (ret == -ETIMEDOUT) {
		fprintf(stderr, "fx2usb_recv_chunk: usb write timeout -> retry\n");
		fx2usb_flush(dh);
		goto retry_write;
	}
	if (ret != len)
		fprintf(stderr, "fx2usb_send_chunk: write of %d bytes to ep %d returned %d: %s\n", len, ep, ret, ret >= 0 ? "NO ERROR" : usb_strerror());
	return ret == len ? 0 : -1;
}

int fx2usb_recv_chunk(usb_dev_handle *dh, int ep, void *data, int len, int *ret_len)
{
	int ret;
retry_read:
	ret = usb_bulk_read(dh, ep, data, len, 1000);
	if (ret == -ETIMEDOUT) {
		fprintf(stderr, "fx2usb_recv_chunk: usb read timeout -> retry\n");
		goto retry_read;
	}
	if (ret > 0 && ret_len != NULL)
		len = *ret_len = ret;
	if (ret != len)
		fprintf(stderr, "fx2usb_recv_chunk: read of %d bytes from ep %d returned %d: %s\n", len, ep, ret, ret >= 0 ? "NO ERROR" : usb_strerror());
	return ret == len ? 0 : -1;
}


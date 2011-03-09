/*
 *  xsvftool-xpcu - An (X)SVF player for the Xilinx Platform Cable USB
 *
 *  Copyright (C) 2011  RIEGL Research ForschungsGmbH
 *  Copyright (C) 2011  Clifford Wolf <clifford@clifford.at>
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
 */

#ifndef FX2USB_INTERFACE_H
#define FX2USB_INTERFACE_H

#include <usb.h>
#include <stdio.h>

usb_dev_handle *fx2usb_open(int vendor_id, int device_id, char *dev);
int fx2usb_upload_ihex(usb_dev_handle *dh, FILE *fp);
int fx2usb_claim(usb_dev_handle *dh);
void fx2usb_release(usb_dev_handle *dh);

void fx2usb_flush(usb_dev_handle *dh);
int fx2usb_send_chunk(usb_dev_handle *dh, int ep, const void *data, int len);
int fx2usb_recv_chunk(usb_dev_handle *dh, int ep, void *data, int len, int *ret_len);

#endif /* FX2USB_INTERFACE_H */


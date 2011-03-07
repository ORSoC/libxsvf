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


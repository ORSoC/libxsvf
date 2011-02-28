
#ifndef FX2USB_INTERFACE_H
#define FX2USB_INTERFACE_H

#include <usb.h>
#include <stdio.h>

usb_dev_handle *fx2usb_open();
int fx2usb_upload_ihex(usb_dev_handle *dh, FILE *fp);
int fx2usb_claim(usb_dev_handle *dh);
void fx2usb_release(usb_dev_handle *dh);

void fx2usb_flush(usb_dev_handle *dh);
int fx2usb_send_chunk(usb_dev_handle *dh, int ep, const void *data, int len);
int fx2usb_recv_chunk(usb_dev_handle *dh, int ep, void *data, int len, int *ret_len);

#endif /* FX2USB_INTERFACE_H */


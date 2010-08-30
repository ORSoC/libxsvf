
#ifndef PSOC_INTERFACE_H
#define PSOC_INTERFACE_H

#include <usb.h>
#include <stdio.h>

usb_dev_handle *xpcu_psoc_open();
int xpcu_psoc_upload_ihex(usb_dev_handle *dh, FILE *fp);
int xpcu_psoc_claim(usb_dev_handle *dh);
void xpcu_psoc_release(usb_dev_handle *dh);

void xpcu_psoc_flush(usb_dev_handle *dh);
int xpcu_psoc_send_chunk(usb_dev_handle *dh, const void *data, int len);
int xpcu_psoc_recv_chunk(usb_dev_handle *dh, void *data, int len, int *ret_len);

#endif /* PSOC_INTERFACE_H */


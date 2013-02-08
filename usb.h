/*
 * usb.h
 *
 *  Created on: Feb 7, 2013
 *      Author: brent
 */

#ifndef USB_H_
#define USB_H_


void UsbSendPacket(uint8_t *packet, int len);

int UsbPoll(int blinkLeds);

void UsbStart(void);

#endif /* USB_H_ */

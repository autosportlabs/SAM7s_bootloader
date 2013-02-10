/*
 * bootloader.h
 *
 *  Created on: Feb 7, 2013
 *      Author: brent
 */

#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#include <stdint.h>
#include "at91sam7s512.h"

#define GPIO_LED_1		AT91C_PIO_PA1
#define GPIO_LED_2		AT91C_PIO_PA2
#define GPIO_LED_3		AT91C_PIO_PA31
#define GPIO_USB_PU		AT91C_PIO_PA16
#define GPIO_BUTTON		AT91C_PIO_PA4

#define WDT_HIT()		AT91C_BASE_WDTC->WDTC_WDCR = 0xa5000001
#define LOW(x)	 		AT91C_BASE_PIOA->PIO_CODR = (x)
#define HIGH(x)	 		AT91C_BASE_PIOA->PIO_SODR = (x)
#define GETBIT(x) 		(AT91C_BASE_PIOA->PIO_ODSR & (x)) ? 1:0
#define SETBIT(x, y) 	(y) ? (HIGH(x)):(LOW(x))
#define INVBIT(x) 		SETBIT((x), !(GETBIT(x)))
#define BUTTON_PRESS()	!(AT91C_BASE_PIOA->PIO_PDSR & GPIO_BUTTON)


#define USB_D_PLUS_PULLUP_OFF() { \
		AT91C_BASE_PIOA->PIO_PER = GPIO_USB_PU; \
		AT91C_BASE_PIOA->PIO_OER = GPIO_USB_PU; \
		HIGH(GPIO_USB_PU); \
	}
#define USB_D_PLUS_PULLUP_ON(){ \
		AT91C_BASE_PIOA->PIO_PER = GPIO_USB_PU; \
		AT91C_BASE_PIOA->PIO_OER = GPIO_USB_PU; \
		LOW(GPIO_USB_PU); \
	}

#define LED_1_ON()		LOW(GPIO_LED_1)
#define LED_1_OFF()		HIGH(GPIO_LED_1)
#define LED_1_INV()		INVBIT(GPIO_LED_1)
#define LED_2_ON()		LOW(GPIO_LED_2)
#define LED_2_OFF()		HIGH(GPIO_LED_2)
#define LED_2_INV()		INVBIT(GPIO_LED_2)
#define LED_3_ON()		LOW(GPIO_LED_3)
#define LED_3_OFF()		HIGH(GPIO_LED_3)
#define LED_3_INV()		INVBIT(GPIO_LED_3)

#define MC_FLASH_COMMAND_PAGEN(x)				((x)<<8)
#define RST_CONTROL_KEY							(0xa5<<24)
#define MC_FLASH_COMMAND_KEY					((0x5a)<<24)

#define UDP_CSR_BYTES_RECEIVED(x)				(((x) >> 16) & 0x7ff)
#define UDP_INTERRUPT_ENDPOINT(x)				(1<<(x))

#define TRUE 1
#define FALSE 0

// called from UsbPoll if data is available.
void UsbPacketReceived(uint8_t *packet, int len);

#endif /* BOOTLOADER_H_ */

#include "at91sam7s512.h"
#include "bootloader.h"
#include "usb_cmd.h"
#include "usb.h"

unsigned int start_addr, end_addr, bootrom_unlocked;
extern unsigned int _bootrom_start, _bootrom_end, _flash_start, _flash_end, _osimage_entry;

static void setupHardware( void )
{

    // First set up all the I/O pins; GPIOs configured directly, other ones
    // just need to be assigned to the appropriate peripheral.

    // Kill all the pullups, especially the one on USB D+; leave them for
    // the unused pins, though.


	AT91C_BASE_PIOA->PIO_PER = GPIO_BUTTON;
    AT91C_BASE_PIOA->PIO_PPUDR =
    	GPIO_USB_PU			|
		GPIO_LED_1			|
		GPIO_LED_2			|
		GPIO_LED_3;

    AT91C_BASE_PIOA->PIO_OER =
    	GPIO_LED_1			|
		GPIO_LED_2			|
		GPIO_LED_3;

	// PIO controls the following pins
    AT91C_BASE_PIOA->PIO_PER =
    	GPIO_USB_PU			|
		GPIO_LED_1			|
		GPIO_LED_2			|
		GPIO_LED_3;

    USB_D_PLUS_PULLUP_OFF();
    LED_1_OFF();
    LED_2_OFF();
    LED_3_OFF();


	// enable main oscillator and set startup delay
    AT91C_BASE_PMC->PMC_MOR = 0x00000601;
//        AT91C_CKGR_MOSCEN |
//        PMC_MAIN_OSC_STARTUP_DELAY(8);

	// wait for main oscillator to stabilize
	while ( !(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MOSCS) )
		;

    // PLL output clock frequency in range  80 - 160 MHz needs CKGR_PLL = 00
    // PLL output clock frequency in range 150 - 180 MHz needs CKGR_PLL = 10
    // PLL output is MAINCK * multiplier / divisor = 16Mhz * 12 / 2 = 96Mhz
    AT91C_BASE_PMC->PMC_PLLR = 0x00191C05;
//    	PMC_PLL_DIVISOR(2) |
//		PMC_PLL_COUNT_BEFORE_LOCK(0x50) |
//		PMC_PLL_FREQUENCY_RANGE(0) |
//		PMC_PLL_MULTIPLIER(12) |
//		PMC_PLL_USB_DIVISOR(1);

	// wait for PLL to lock
	while ( !(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_LOCK) )
		;

	// datasheet recommends that this register is programmed in two operations
	// when changing to PLL, program the prescaler first then the source
    AT91C_BASE_PMC->PMC_MCKR = 0x00000007;

	// wait for main clock ready signal
	while ( !(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY) )
		;

	// set the source to PLL
//    AT91C_BASE_PMC->PMC_MCKR = 0x00000007 | AT91C_PMC_CSS_PLL_CLK;

	// wait for main clock ready signal
	//while ( !(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY) )
		//;

	/* When using the JTAG debugger the hardware is not always initialised to
	the correct default state.  This line just ensures that this does not
	cause all interrupts to be masked at the start. */
	AT91C_BASE_AIC->AIC_EOICR = 0;

	//======================================

	/* Enable reset-button */
	AT91C_BASE_RSTC->RSTC_RMR = (0xA5000000 | AT91C_RSTC_URSTEN);

    // Set the PLL USB Divider
    AT91C_BASE_CKGR->CKGR_PLLR |= AT91C_CKGR_USBDIV_1 ;

    // enable system clock and USB clock
    AT91C_BASE_PMC->PMC_SCER = AT91C_PMC_PCK | AT91C_PMC_UDP;

	// enable the clock to the following peripherals
    AT91C_BASE_PMC->PMC_PCER =
		(1<<AT91C_ID_PIOA)	|
		(1<<AT91C_ID_ADC)	|
		(1<<AT91C_ID_SPI)	|
		(1<<AT91C_ID_SSC)	|
		(1<<AT91C_ID_PWMC)	|
		(1<<AT91C_ID_UDP);

	return;
 }

static void Fatal(void)
{
    for(;;);
}

void UsbPacketReceived(uint8_t *packet, int len)
{
    int i, dont_ack=0;
    UsbCommand *c = (UsbCommand *)packet;
    volatile uint32_t *p;

    if(len != sizeof(*c)) {
        Fatal();
    }

    switch(c->cmd) {
        case CMD_DEVICE_INFO:
            dont_ack = 1;
            c->cmd = CMD_DEVICE_INFO;
            c->arg[0] = DEVICE_INFO_FLAG_BOOTROM_PRESENT | DEVICE_INFO_FLAG_CURRENT_MODE_BOOTROM |
                DEVICE_INFO_FLAG_UNDERSTANDS_START_FLASH;
            //if(common_area.flags.osimage_present) c->arg[0] |= DEVICE_INFO_FLAG_OSIMAGE_PRESENT;
            UsbSendPacket(packet, len);
            break;

        case CMD_SETUP_WRITE:
            /* The temporary write buffer of the embedded flash controller is mapped to the
             * whole memory region, only the last 8 bits are decoded.
             */
            p = (volatile uint32_t *)&_flash_start;
            for(i = 0; i < 12; i++) {
                p[i+c->arg[0]] = c->d.asDwords[i];
            }
            break;

        case CMD_FINISH_WRITE:
            p = (volatile uint32_t *)&_flash_start;
            for(i = 0; i < 4; i++) {
                p[i+60] = c->d.asDwords[i];
            }

            /* Check that the address that we are supposed to write to is within our allowed region */
            if( ((c->arg[0]+AT91C_IFLASH_PAGE_SIZE-1) >= end_addr) || (c->arg[0] < start_addr) ) {
                /* Disallow write */
                dont_ack = 1;
                c->cmd = CMD_NACK;
                UsbSendPacket(packet, len);
            } else {
                /* Translate address to flash page and do flash, update here for the 512k part */
                AT91C_BASE_EFC0->EFC_FCR = MC_FLASH_COMMAND_KEY |
                    MC_FLASH_COMMAND_PAGEN((c->arg[0]-(int)&_flash_start)/AT91C_IFLASH_PAGE_SIZE) |
                    AT91C_MC_FCMD_START_PROG;
            }

            uint32_t sr;

            while(!((sr = AT91C_BASE_EFC0->EFC_FSR) & AT91C_MC_FRDY))
                ;
            if(sr & (AT91C_MC_LOCKE | AT91C_MC_PROGE)) {
        	    dont_ack = 1;
                    c->cmd = CMD_NACK;
                    UsbSendPacket(packet, len);
            }
            break;

        case CMD_HARDWARE_RESET:
            USB_D_PLUS_PULLUP_OFF();
            AT91C_BASE_RSTC->RSTC_RCR = RST_CONTROL_KEY | AT91C_RSTC_PROCRST;
            break;

        case CMD_START_FLASH:
            if(c->arg[2] == START_FLASH_MAGIC) bootrom_unlocked = 1;
            else bootrom_unlocked = 0;
            {
                int prot_start = (int)&_bootrom_start;
                int prot_end = (int)&_bootrom_end;
                int allow_start = (int)&_flash_start;
                int allow_end = (int)&_flash_end;
                int cmd_start = c->arg[0];
                int cmd_end = c->arg[1];

                /* Only allow command if the bootrom is unlocked, or the parameters are outside of the protected
                 * bootrom area. In any case they must be within the flash area.
                 */
                if( (bootrom_unlocked || ((cmd_start >= prot_end) || (cmd_end < prot_start)))
                    && (cmd_start >= allow_start) && (cmd_end <= allow_end) ) {
                    start_addr = cmd_start;
                    end_addr = cmd_end;
                } else {
                    start_addr = end_addr = 0;
                    dont_ack = 1;
                    c->cmd = CMD_NACK;
                    UsbSendPacket(packet, len);
                }
            }
            break;

        default:
            Fatal();
            break;
    }

    if(!dont_ack) {
        c->cmd = CMD_ACK;
        UsbSendPacket(packet, len);
    }
}

static void flash_mode(int externally_entered)
{
	start_addr = 0;
	end_addr = 0;
	bootrom_unlocked = 0;

	UsbStart();
	for(;;) {
		WDT_HIT();

		UsbPoll(TRUE);

		/*
		if(!externally_entered && !BUTTON_PRESS()) {
			// Perform a reset to leave flash mode
			USB_D_PLUS_PULLUP_OFF();
			LED_B_ON();
			AT91C_BASE_RSTC->RSTC_RCR = RST_CONTROL_KEY | AT91C_RSTC_PROCRST;
			for(;;);
		}
		if(externally_entered && BUTTON_PRESS()) {
			// Let the user's button press override the automatic leave
			externally_entered = 0;
		}
		*/
	}
}


int main(void)
{
    setupHardware();
    if(BUTTON_PRESS()) {
    	flash_mode(0);
    } else {
    	asm("bx %0\n" : : "r" ( ((int)&_osimage_entry)) );
    }
}

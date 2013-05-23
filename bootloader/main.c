#include "at91sam7s512.h"
#include "bootloader.h"
#include "usb_cmd.h"
#include "usb.h"

unsigned int start_addr, end_addr, bootrom_unlocked;
extern unsigned int _bootrom_start, _bootrom_end, _flash_start, _flash_end, _osimage_entry;

#define FATAL_ERROR_HARDWARE	1
#define FATAL_ERROR_FLASH	 	2
#define FATAL_ERROR_COMMAND  	3
#define FATAL_ERROR_UNKNOWN		4

static void fatalError(int type){

	int count;
	int pause = 5000000;
	int flash = 1000000;

	switch (type){
		case FATAL_ERROR_HARDWARE:
		case FATAL_ERROR_FLASH:
		case FATAL_ERROR_COMMAND:
			count = type;
			break;
		default:
			count = FATAL_ERROR_UNKNOWN;
			break;
	}

	while(1){
		for (int c = 0; c < count; c++){
			LED_1_ON();
			LED_2_ON();
			LED_3_ON();
			for (int i=0;i<flash;i++){}
			LED_1_OFF();
			LED_2_OFF();
			LED_3_OFF();
			for (int i=0;i<flash;i++){}
		}
		for (int i=0;i<pause;i++){}
	}
}

static void setupPorts(){
    // set up all the I/O pins; GPIOs configured directly, other ones
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
    AT91C_BASE_PMC->PMC_PCER = (1<<AT91C_ID_PIOA);
}

static void setupHardware( void )
{
    //* Set MCK at 48 054 850
    // 1 Enabling the Main Oscillator:
    // SCK = 1/32768 = 30.51 uSecond
    // Start up time = 8 * 6 / SCK = 56 * 30.51 = 1,46484375 ms
    AT91C_BASE_PMC->PMC_MOR = (( AT91C_CKGR_OSCOUNT & (0x06 <<8) | AT91C_CKGR_MOSCEN ));
    // Wait the startup time
    while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MOSCS));
    // 2 Checking the Main Oscillator Frequency (Optional)
    // 3 Setting PLL and divider:
    // - div by 14 Fin = 1.3165 =(18,432 / 14)
    // - Mul 72+1: Fout = 96.1097 =(3,6864 *73)
    // for 96 MHz the error is 0.11%
    // Field out NOT USED = 0
    // PLLCOUNT pll startup time estimate at : 0.844 ms
    // PLLCOUNT 28 = 0.000844 /(1/32768)
    AT91C_BASE_PMC->PMC_PLLR = ((AT91C_CKGR_DIV & 14 ) | (AT91C_CKGR_PLLCOUNT & (28<<8)) | (AT91C_CKGR_MUL & (72<<16)));

    // Wait the startup time
    while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_LOCK));
    while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY));
    // 4. Selection of Master Clock and Processor Clock
    // select the PLL clock divided by 2
    AT91C_BASE_PMC->PMC_MCKR = AT91C_PMC_PRES_CLK_2 ;
    while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY));
    AT91C_BASE_PMC->PMC_MCKR |= AT91C_PMC_CSS_PLL_CLK ;
    while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY));

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


void UsbPacketReceived(uint8_t *packet, int len)
{
    int i, dont_ack=0;
    UsbCommand *c = (UsbCommand *)packet;
    volatile uint32_t *p;

    if(len != sizeof(*c)) {
        fatalError(FATAL_ERROR_COMMAND);
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
            fatalError(FATAL_ERROR_COMMAND);
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
	}
}


int main(void)
{
	setupPorts();
    if(BUTTON_PRESS()) {
        setupHardware();
    	flash_mode(0);
    } else {
    	asm("bx %0\n" : : "r" ( ((int)&_osimage_entry)) );
    }
}

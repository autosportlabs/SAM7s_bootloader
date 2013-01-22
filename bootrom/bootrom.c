//-----------------------------------------------------------------------------
// The bootrom proper; the piece of code that runs from RAM and can program
// flash. If we are loaded over JTAG, then we go directly into RAM; otherwise
// we are copied from some location in flash into RAM by the routines in
// fromflash.c. In any case, we start up the USB driver and either load
// new code (if the downloader connects) or give up waiting and jump to
// the application.
//
// Jonathan Westhues, split Aug 14 2005, public release May 26 2006
//-----------------------------------------------------------------------------
#include <bootrom.h>

static void ConfigClocks(void)
{
    volatile int i;

    // we are using a 16 MHz crystal as the basis for everything
    PMC_SYS_CLK_ENABLE = PMC_SYS_CLK_PROCESSOR_CLK | PMC_SYS_CLK_UDP_CLK;

    PMC_PERIPHERAL_CLK_ENABLE =
        (1<<PERIPH_PIOA) |
        (1<<PERIPH_ADC) |
        (1<<PERIPH_SPI) |
        (1<<PERIPH_SSC) |
        (1<<PERIPH_PWMC) |
        (1<<PERIPH_UDP);

    PMC_MAIN_OSCILLATOR = PMC_MAIN_OSCILLATOR_ENABLE |
        PMC_MAIN_OSCILLATOR_STARTUP_DELAY(0x40);

    // TODO: THIS DEPENDS ON THE CRYSTAL FREQUENCY THAT YOU CHOOSE. Make
    // the ARM run at some reasonable speed, and make the USB peripheral
    // run at exactly 48 MHz.

    // minimum PLL clock frequency is 80 MHz in range 00 (96 here so okay)
    PMC_PLL = PMC_PLL_DIVISOR(2) | PMC_PLL_COUNT_BEFORE_LOCK(0x40) |
        PMC_PLL_FREQUENCY_RANGE(0) | PMC_PLL_MULTIPLIER(12) |
        PMC_PLL_USB_DIVISOR(1);

    // let the PLL spin up, plenty of time
    for(i = 0; i < 100; i++)
        ;

    PMC_MASTER_CLK = PMC_CLK_SELECTION_SLOW_CLOCK | PMC_CLK_PRESCALE_DIV_2;

    for(i = 0; i < 100; i++)
        ;

    PMC_MASTER_CLK = PMC_CLK_SELECTION_PLL_CLOCK | PMC_CLK_PRESCALE_DIV_2;

    for(i = 0; i < 100; i++)
        ;
}

static void Fatal(void)
{
    for(;;);
}

void UsbPacketReceived(BYTE *packet, int len)
{
    int i;
    UsbCommand *c = (UsbCommand *)packet;
    volatile DWORD *p;

    if(len != sizeof(*c)) {
        Fatal();
    }

    switch(c->cmd) {
        case CMD_DEVICE_INFO:
            break;

        case CMD_SETUP_WRITE:
            p = (volatile DWORD *)0;
            for(i = 0; i < 12; i++) {
                p[i+c->ext1] = c->d.asDwords[i];
            }
            break;

        case CMD_FINISH_WRITE:
            p = (volatile DWORD *)0;
            for(i = 0; i < 4; i++) {
                p[i+60] = c->d.asDwords[i];
            }

            MC_FLASH_COMMAND = MC_FLASH_COMMAND_KEY |
                MC_FLASH_COMMAND_PAGEN(c->ext1/FLASH_PAGE_SIZE_BYTES) |
                FCMD_WRITE_PAGE;
            while(!(MC_FLASH_STATUS & MC_FLASH_STATUS_READY))
                ;
            break;

        case CMD_HARDWARE_RESET:
            break;

        default:
            Fatal();
            break;
    }

    c->cmd = CMD_ACK;
    UsbSendPacket(packet, len);
}

void Bootrom(void)
{
    //------------
    // First set up all the I/O pins; GPIOs configured directly, other ones
    // just need to be assigned to the appropriate peripheral.

    // Kill all the pullups, especially the one on USB D+; leave them for
    // the unused pins, though.
    PIO_NO_PULL_UP_ENABLE =     (1 << GPIO_USB_PU)          | 
                                (1 << GPIO_LED);

    PIO_OUTPUT_ENABLE =         (1 << GPIO_LED);

    PIO_ENABLE =                (1 << GPIO_USB_PU)          |
                                (1 << GPIO_LED);

    USB_D_PLUS_PULLUP_OFF();
    LED_OFF();

    LED_ON();

    // Configure the flash that we are running out of.
    MC_FLASH_MODE = MC_FLASH_MODE_FLASH_WAIT_STATES(0) | 
        MC_FLASH_MODE_MASTER_CLK_IN_MHZ(48);

    // Careful, a lot of peripherals can't be configured until the PLL clock
    // comes up; you write to the registers but it doesn't stick.
    ConfigClocks();

    UsbStart();

    // Borrow a PWM unit for my real-time clock
    PWM_ENABLE = PWM_CHANNEL(0);
    // 48 MHz / 1024 gives 46.875 kHz
    PWM_CH_MODE(0) = PWM_CH_MODE_PRESCALER(10);
    PWM_CH_DUTY_CYCLE(0) = 0;
    PWM_CH_PERIOD(0) = 0xffff;

    WORD start = (SWORD)PWM_CH_COUNTER(0);

    for(;;) {
        WORD now = (SWORD)PWM_CH_COUNTER(0);

        if(UsbPoll()) {
            // It did something; reset the clock that would jump us to the
            // applications.
            start = now;
        }

        WDT_HIT();

        if((SWORD)(now - start) > 30000) {
            USB_D_PLUS_PULLUP_OFF();

            // This is a function call to 0x00002001, which is equal to
            // (1 << 13) | 1. The application is at address 0x00002000, and
            // we want to set the LSb in order to switch in to Thumb mode
            // when we bx. 
            //
            // I would have thought that I could write this as a cast to
            // a function pointer and a call, but I can't seem to get that
            // to work. I also can't figure out how to make the assembler
            // load pc relative a constant in flash, thus the ugly way
            // to specify the address.
            asm("mov r3, #1\n");
            asm("lsl r3, r3, #13\n");
            asm("mov r4, #1\n");
            asm("orr r3, r4\n");
            asm("bx r3\n");
        }
    }
}


This is a simple bootloader for AT91SAM7{S,X}{64,128,256} processors. It
permits you to download new code to the device over USB. This bootrom is
installed at address 0x00000000 in the ARM, which means that it is the
first piece of code to execute after reset. At startup, it waits for
a few seconds to see if the downloader is trying to connect. If not,
then the bootloader gives up control and jumps to your program. If the
downloader is trying to connect, then the bootloader receives the new
program over USB and writes it into flash.

All of the ARM-side code is written for the GNU toolchain (arm-elf-gcc
and friends). The downloader, which runs under Windows, is written for
the Microsoft Visual C++ compiler, version 6.

It is necessary to use some method other than the bootloader to program
the bootloader into the ARM for the first time. I have done this using
JTAG. The bootrom build process builds forjtag.s19, which I download
into the AT91SAM7S's RAM. I have used a Wiggler-compatible JTAG dongle,
which works with Macraigor's OCD Commander tools. Any JTAG tools will do,
however. I then run the bootrom from RAM (by starting it over JTAG),
and download the bootrom into flash over USB, using the copy of the
bootloader that I had downloaded into RAM.

The process, when bringing up a board for the first time, is therefore
this:

    * connect to board over JTAG
    * use JTAG to download forjtag.s19 to RAM (address 0x00200000)
    * start PC-side download: run `usbdl bootrom bootrom.s19'
    * use JTAG to start ARM running from address 0x00200000
    * wait for bootrom to finish downloading

It would also be possible to use Atmel's SAM-BA bootloader to load this
(more capable; see below) bootloader. This means that you could load
this bootloader without ever using JTAG, so there would be no need to
bring those pins to a header.

                 *                *                *

The bootrom resides on the ARM; it is built from the following files:

    bootrom/Makefile            -- makefile to build all bootrom files

    bootrom/at91sam7sXXX.h      -- register definitions etc. for the part

    bootrom/bootrom.h           -- includes specific to the bootrom

    bootrom/fromflash.c         -- the bootrom programs the flash, so it
                                   has to run out of RAM; this file
                                   contains the routines (that execute
                                   from flash) to load the bootrom from
                                   RAM to flash and jump to it

    bootrom/flash-reset.s       -- the code that goes at 0x00000000, to
                                   set up all the reset etc. vectors
                                   and jump to the first line of C
                                   code that executes

    bootrom/usb.c               -- a USB driver

    bootrom/ldscript-flash      --
    bootrom/ldscript-ram-jtag   -- linker scripts for the programmed-in-
                                   flash and loaded-over-JTAG versions
                                   of the bootrom
        
    bootrom/myhw.h              -- defines specific to your hardware
                                   (GPIO assignments)

    bootrom/ram-reset.s         -- a simple C runtime to use when
                                   loading the bootrom over JTAG

    bootrom/merge-srec.pl       -- a script to relocate the RAM image of
                                   the bootrom to the area in flash
                                   where it will actually be programmed
                                   and stored

    bootrom/srecswap.pl         -- a tool to endian-swap S records

The following files are created by the bootrom build process:

    bootrom/bootrom.s19         -- a flash image to program into the chip

    bootrom/forjtag.s19         -- a RAM image to download over JTAG when
                                   loading the bootrom for the first time

    bootrom/forjtag-swapped.s19 -- same thing, endian-swapped (I have found
                                   some sort of a bug in OCD Command, but
                                   I cannot reproduce it reliably; if
                                   forjtag.s19 does not work, then try the
                                   swapped version)

The downloader runs on the PC; it is built from the following files:

    loader/Makefile             -- makefile to build the downloader

    loader/usbdl.cpp            -- the downloader

    loader/include/*            -- DDK-only headers that are required to
                                   do USB things (the downloader is
                                   dynamically linked, so you don't need
                                   the .libs)

The following file is created by the downloader build process:

    loader/usbdl.exe            -- the downloader

I also include a sample `application' in app/.

                 *                *                *

I assume that your hardware is connected like this:

-----------------+
                 |
                 |    1.5 kohm
    a GPIO line  +----\/\/\/\/-----+
                 |                 |                      +--------------
                 |                 |        27 ohm        |
             DP  +-----------------+------\/\/\/\/\/------+
                 |                                        |
                 |                                        |
 AT91SAM7Sxxx    |                                        | USB B/mini-B
                 |                                        |
                 |                          27 ohm        |
             DM  +------------------------\/\/\/\/\/------+
                 |                                        |
                 |

The bootrom needs to know where that GPIO line is, so that it can add
or remove the 1.5k pull-up by either tri-stating that GPIO or driving
it high.  Tell it in myhw.h. It is necessary to have some way to do this
if you want to be able to re-enumerate with different VID/PID/descriptors
after control passes from the bootrom to the application.

The bootrom is written for a 16 MHz crystal. It is necessary to change the
ConfigClocks() routine (in bootrom.c and fromflash.c) if you are using
a different speed crystal. Not all crystal frequencies are acceptable;
you have to be able to produce a (48 +/- 0.1%) MHz USB clock, given the
maximum operating frequency of the PLL.

                 *                *                *

The downloader recognizes the following command line options:

    usbdl load  app.s19         -- load the application from app.s19
    usbdl bootrom bootrom.s19   -- load the bootrom from bootrom.s19

It is possible to use the bootrom to load a new bootrom, even when the
existing bootrom is running from flash. Of course, if something goes
wrong while doing this then you will have to reload the bootrom
over JTAG (or SAM-BA).
    
                 *                *                *

The memory map of the ARM's flash must be as follows:
    
    0x00000000                  -- bootrom
    0x00002000                  -- start of application

The bootrom does the equivalent of a C function call to the start of
your application; the stack will already be set up.

Minor changes to the software are necessary for an AT91SAM7S64,
which has a different flash page size. Look at bootrom/bootrom.c,
and usbdl/usbdl.cpp. By default, the AT91SAM7X parts boot from ROM,
not flash. In order to use this bootloader, it is necessary to
flip general-purpose non-volatile memory bit 2, but otherwise it
all works the same.

                 *                *                *

Atmel provides a bootloader called SAM-BA but as far as I can tell it
is not what I want. SAM-BA takes full control of the processor: there
is no provision for it to jump to a separate `application' after a few
seconds. It would be possible to use their bootloader to download any kind
of program to the device, but in doing so that program must wipe SAM-BA
out (or else SAM-BA would never let it execute). This seems like it would
be rather inconvenient for development; you would have to fiddle with
the test pins to reload SAM-BA every time you wanted to download new code.

                 *                *                *

I initially wrote this bootloader for my proxmark{ii,3} devices, which are
based around AT91SAM7S parts. I have tested the code with an AT91SAM7S64,
AT91SAM7S128, and AT91SAM7S256. With modifications to set GPNVM bit 2,
I have tested this code on an AT91SAM7X256.

The PID/VID are random numbers. If you intend to use this in a product,
then you should at least choose different random numbers.

I am not really sure how long it is necessary to wait before passing
control to the application, if we wish to reliably detect when the
downloader is trying to connect. The values that I have now seem to work,
but if you have trouble then increase that delay.

THIS SOFTWARE IS IN THE PUBLIC DOMAIN. It may therefore be used in any
application, commercial or non-commercial, with or without attribution
to the author. NO WARRANTY IS EXPRESSED OR IMPLIED.

Jonathan Westhues, May 2006 (updated Jun 2007)
user jwesthues, at host cq.cx


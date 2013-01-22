# This is for nmake, not gnumake, for no particular reason.

CC	  = arm-elf-gcc
AS	  = arm-elf-as
LD	  = arm-elf-ld
OBJCOPY = arm-elf-objcopy

OBJDIR  = obj

INCLUDE = -I../include

INCLUDES = ../include/*.h

CFLAGS  = -O6 -c $(INCLUDE) -Wall

OBJ =   $(OBJDIR)/start.o \

# This seems terribly ugly, but I haven't figured out how to include libgcc
# without specifying its path explicitly. I need to include libgcc, because
# that is there the divide subroutines are defined, but I don't want the
# C runtime.
INSTALLDIR = C:/gnuarm/

all: osimage.s19

$(OBJDIR)/osimage.s19: $(OBJ) $(OBJCOMMON)
	@echo obj/osimage.s19
	@$(LD) -g -Tldscript -o $(OBJDIR)/osimage.elf $(OBJ) $(INSTALLDIR)/lib/gcc/arm-elf/3.4.1/interwork/libgcc.a
	@$(OBJCOPY) -Osrec --srec-forceS3 $(OBJDIR)/osimage.elf $(OBJDIR)/osimage.s19

osimage.s19: $(OBJDIR)/osimage.s19
	@echo osimage.s19
	@cat $(OBJDIR)/osimage.s19 > osimage.s19

$(OBJ): $(@B).c $(INCLUDES)
	@echo $(@B).c
	@$(CC) $(CFLAGS) -mthumb -mthumb-interwork $(@B).c -o $(OBJDIR)/$(@B).o

clean:
	del /q obj\*.o
	del /q obj\*.elf
	del /q obj\*.s19
	del /q *.s19

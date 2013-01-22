
LIBS   = kernel32.lib user32.lib advapi32.lib uuid.lib setupapi.lib

usbdl.exe: usbdl.cpp ../include/usb_cmd.h
	cl $(LIBS) usbdl.cpp

clean:
	rm usbdl.exe

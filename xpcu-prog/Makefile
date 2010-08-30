
CFLAGS = -MD -Wall -Os -ggdb $(shell libusb-config --cflags)
LDLIBS = $(shell libusb-config --libs) ../libxsvf.a

all: xpcu-prog firmware.ihx

xpcu-prog: xpcu-prog.o psoc-interface.o

firmware.ihx: firmware.c commands.h
	sdcc -mmcs51 firmware.c

clean:
	rm -f firmware.???
	rm -f xpcu-prog
	rm -f *.o *.d core

-include *.d


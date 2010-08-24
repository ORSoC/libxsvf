
CFLAGS = -MD -Wall -Os -ggdb $(shell libusb-config --cflags)
LDLIBS = $(shell libusb-config --libs) ../libxsvf.a

all: xpcu-prog firmware.ihx

xpcu-prog: xpcu-prog.o psoc-interface.o

firmware.ihx: firmware.c
	sdcc -mmcs51 firmware.c

clean:
	rm -f firmware.???
	rm -f xpcu-prog
	rm -f *.o core

-include *.d



CFLAGS = -MD -Wall -Os -ggdb $(shell libusb-config --cflags)
LDLIBS = $(shell libusb-config --libs) ../libxsvf.a

all: xpcu-prog firmware.ihx

xpcu-prog: xpcu-prog.o psoc-interface.o vfile_firmware.o vfile_cpldxsvf.o

genvfile: genvfile.o

vfile_firmware.c: firmware.ihx genvfile
	./genvfile firmware < firmware.ihx | fmt -s > vfile_firmware.c

vfile_cpldxsvf.c: cpld-prog.xsvf genvfile
	./genvfile cpldxsvf < cpld-prog.xsvf | fmt -s > vfile_cpldxsvf.c

firmware.ihx: firmware.c commands.h
	sdcc -mmcs51 firmware.c

clean:
	rm -f vfile_*.c
	rm -f firmware.???
	rm -f genvfile
	rm -f xpcu-prog
	rm -f *.o *.d core

-include *.d



LIBXSVFDIR=$(HOME)/Work/libxsvf-31

CC = gcc
CFLAGS = -Wall -Wextra -Werror -Os -ggdb -I$(LIBXSVFDIR) -MD
LDFLAGS = -L$(LIBXSVFDIR)
LDLIBS = -lusb -lreadline -lxsvf

SDCC = sdcc
SDCFLAGS = -mmcs51 --xram-loc 0x2000

all: xsvftool-xpcu hardware.svf firmware.ihx

xsvftool-xpcu: xsvftool-xpcu.o fx2usb-interface.o

hardware.svf: hardware.sh hardware.ucf hardware.v
	bash hardware.sh

firmware.ihx: firmware.c
	cpp -MD -MF $(basename $<).d -MT $(basename $<).ihx -o /dev/null $<
	$(SDCC) $(SDCFLAGS) $<

firmware.ihx: gpifprog_fixed.c
gpifprog_fixed.c: gpifprog.c
	sed 's/ xdata / /g;' < $< > $@

clean:
	rm -f firmware.asm firmware.lnk firmware.lst firmware.map
	rm -f firmware.mem firmware.rel firmware.rst firmware.sym
	rm -f hardware.bld hardware_build.xml hardware.chk hardware.cmd
	rm -f hardware.log hardware.lso hardware.mfd hardware.ngc hardware.ngd
	rm -f hardware_ngdbuild.xrpt hardware.ngr hardware.pad hardware_pad.csv
	rm -f hardware.pnx hardware.prj hardware.rpt hardware.svf hardware.syr
	rm -f hardware.vm6 hardware.xml hardware.xst hardware_xst.xrpt
	rm -f hardware.cxt hardware.gyd hardware.jed _impactbatch.log tmperr.err
	rm -rf hardware_html xilinx xlnx_auto_0_xdb _xmsgs
	rm -f gpifprog_fixed.c core *.o *.d
	rm -f firmware.ihx xsvftool-xpcu

-include *.d


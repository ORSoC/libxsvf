#
#  xsvftool-xpcu - An (X)SVF player for the Xilinx Platform Cable USB
#
#  Copyright (C) 2011  RIEGL Research ForschungsGmbH
#  Copyright (C) 2011  Clifford Wolf <clifford@clifford.at>
#  
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  
#    1. Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#    2. Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#    3. The names of the authors may not be used to endorse or promote
#       products derived from this software without specific prior
#       written permission.
#  
#  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
#  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
#  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#

LIBXSVFDIR=..

CC = gcc
CFLAGS = -Wall -Wextra -Werror -Os -ggdb -I$(LIBXSVFDIR) -MD
LDFLAGS = -L$(LIBXSVFDIR)
LDLIBS = -lusb -lreadline -lxsvf

SDCC = sdcc
SDCFLAGS = -mmcs51 --xram-loc 0x2000

all: xsvftool-xpcu hardware.svf firmware.ihx

xsvftool-xpcu: xsvftool-xpcu.o fx2usb-interface.o $(LIBXSVFDIR)/libxsvf.a
	$(CC) $(LDFLAGS) xsvftool-xpcu.o fx2usb-interface.o $(LDLIBS) -o $@

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


# Lib(X)SVF  -  A library for implementing SVF and XSVF JTAG players
#
# Copyright (C) 2009  RIEGL Research ForschungsGmbH
# Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
#   1. Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#   3. The names of the authors may not be used to endorse or promote
#      products derived from this software without specific prior
#      written permission.
# 
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

#AR = ppc_6xx-ar
#RANLIB = ppc_6xx-ranlib
#CC = ppc_6xx-gcc
#CFLAGS += -DXSVFTOOL_RLMS_VLINE

AR = ar
RANLIB = ranlib
CC = gcc

CFLAGS += -Wall -Os -ggdb -MD
#CFLAGS += -Wextra -Wno-unused-parameter -Werror

help:
	@echo ""
	@echo "Usage:"
	@echo ""
	@echo "  $(MAKE) libxsvf.a"
	@echo "                .... build only the library"
	@echo ""
	@echo "  $(MAKE) xsvftool"
	@echo "                .... build the library and xsvftool"
	@echo ""
	@echo "  $(MAKE) jtag-ft2232h"
	@echo "                .... build the library and jtag-ft2232h"
	@echo ""
	@echo "  $(MAKE) all"
	@echo "                .... build the library and all examples"
	@echo ""

all: libxsvf.a xsvftool jtag-ft2232h

libxsvf.a: tap.o statename.o memname.o svf.o xsvf.o scan.o play.o xsvftool.o
	rm -f libxsvf.a
	$(AR) qc $@ $^
	$(RANLIB) $@

xsvftool: libxsvf.a xsvftool.o

jtag-ft2232h: LDLIBS+=-lftdi
jtag-ft2232h: LDFLAGS+=-pthread
jtag-ft2232h.o: CFLAGS+=-pthread
jtag-ft2232h: libxsvf.a jtag-ft2232h.o

clean:
	rm -f xsvftool jtag-ft2232h libxsvf.a *.o *.d

-include *.d


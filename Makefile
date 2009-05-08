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

#CC = ppc_6xx-gcc
#CFLAGS += -DXSVFTOOL_RLMS_VLINE

CFLAGS += -Wall -Os -ggdb -MD
#CFLAGS += -Wextra -Wno-unused-parameter -Werror

xsvftool: tap.o statename.o memname.o svf.o xsvf.o scan.o play.o xsvftool.o

clean:
	rm -f xsvftool *.o *.d

-include *.d


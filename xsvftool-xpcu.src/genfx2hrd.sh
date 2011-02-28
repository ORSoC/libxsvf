#!/bin/bash

ORIG_HDR_DIR="$HOME/.wine/drive_c/Cypress/USB/Target/Inc/"

{ echo "// **** Auto-generated using genfx2hrd.sh from CY3684 EZ USB FX2LP Development Kit headers"
sed -r '
	# OLD: EXTERN xdata volatile BYTE CPUCS             _AT_ 0xE600;
	# NEW: xdata at 0xE600 volatile BYTE CPUCS;
	s/^EXTERN xdata volatile BYTE ([^ ]+)( +)_AT_ +([^ ;]+);/xdata at \3 volatile BYTE \1;\2/;

	# OLD: sfr IOA     = 0x80;
	# NEW: sfr at 0x80 IOA;
	s/^sfr ([^ ]+)( +)= ([^ ;]+);/sfr at \3 \1;\2/;

	# OLD: sbit PA0    = 0x80 + 0;
	# NEW: sbit at (0x80 + 0) IOA;
	s/^( *)sbit ([^ ]+)( +)= ([^;]+);/\1sbit at (\4) \2;\3/;
' < "$ORIG_HDR_DIR/fx2regs.h"; } > fx2regs.h

# hotfix line ending in Fx2.h
{ echo "// **** Auto-generated using genfx2hrd.sh from CY3684 EZ USB FX2LP Development Kit headers"
sed -r 's,\\ *,\\,;' < "$ORIG_HDR_DIR/Fx2.h"; } > fx2.h

# we do not have intrins.h but it seams quite clear what _nop_() does..
{ echo "// **** Auto-generated using genfx2hrd.sh from CY3684 EZ USB FX2LP Development Kit headers"
sed 's,^#include "intrins.h",#define _nop_() do { _asm nop; _endasm; } while(0),' < "$ORIG_HDR_DIR/fx2sdly.h"; } > fx2sdly.h


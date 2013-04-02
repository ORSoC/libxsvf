#!/bin/bash
#
#  xsvftool-xpcu - An (X)SVF player for the Xilinx Platform Cable USB
#
#  Copyright (C) 2011  RIEGL Research ForschungsGmbH
#  Copyright (C) 2011  Clifford Wolf <clifford@clifford.at>
#  
#  Permission to use, copy, modify, and/or distribute this software for any
#  purpose with or without fee is hereby granted, provided that the above
#  copyright notice and this permission notice appear in all copies.
#  
#  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
#  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
#  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

. /opt/Xilinx/14.2/ISE_DS/settings64.sh

cat > hardware.prj <<- EOT
        verilog work "hardware.v"
EOT

cat > hardware.lso <<- EOT
        work
EOT

cat > hardware.xst <<- EOT
	set -tmpdir "xilinx/projnav.tmp"
	set -xsthdpdir "xilinx"
	run
	-ifn hardware.prj
	-ifmt mixed
	-ofn hardware
	-ofmt NGC
	-p xbr
	-top top
	-opt_mode Speed
	-opt_level 1
	-iuc NO
	-lso hardware.lso
	-keep_hierarchy YES
	-netlist_hierarchy as_optimized
	-rtlview Yes
	-hierarchy_separator /
	-bus_delimiter <>
	-case maintain
	-verilog2001 YES
	-fsm_extract YES -fsm_encoding Auto
	-safe_implementation No
	-mux_extract YES
	-resource_sharing YES
	-iobuf YES
	-pld_mp YES
	-pld_xp YES
	-pld_ce YES
	-wysiwyg NO
	-equivalent_register_removal YES
EOT

cat > hardware.cmd <<- EOT
	setMode -bs
	setCable -port svf -file "hardware.svf"
	addDevice -p 1 -file "hardware.jed"
	Erase -p 1 
	Program -p 1 -e -v 
	Verify -p 1 
	quit
EOT

cat > erasecpld.cmd <<- EOT
	setMode -bs
	setCable -port svf -file "erasecpld.svf"
	addDevice -p 1 -file "hardware.jed"
	Erase -p 1 
	quit
EOT

set -ex

mkdir -p xilinx/projnav.tmp/
xst -ifn "hardware.xst" -ofn "hardware.syr"

mkdir -p xilinx/_ngo/
ngdbuild -dd xilinx/_ngo -uc hardware.ucf -p xc2c256-VQ100-6 hardware.ngc hardware.ngd

cpldfit -p xc2c256-7-VQ100 -ofmt verilog -optimize density -htmlrpt -loc on -slew fast -init low \
		-inputs 32 -pterms 28 -unused keeper -terminate keeper -iostd LVCMOS18 hardware.ngd

hprep6 -i hardware

impact -batch hardware.cmd
sed -i '\,^// Date:, d;' hardware.svf

impact -batch erasecpld.cmd
sed -i '\,^// Date:, d;' erasecpld.svf


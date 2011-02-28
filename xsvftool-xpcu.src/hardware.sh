#!/bin/bash

. /opt/Xilinx/12.4/ISE_DS/settings32.sh

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

set -ex

mkdir -p xilinx/projnav.tmp/
xst -ifn "hardware.xst" -ofn "hardware.syr"

mkdir -p xilinx/_ngo/
ngdbuild -dd xilinx/_ngo -uc hardware.ucf -p xc2c256-VQ100-6 hardware.ngc hardware.ngd

cpldfit -p xc2c256-7-VQ100 -ofmt verilog -optimize density -htmlrpt -loc on -slew fast -init low \
		-inputs 32 -pterms 28 -unused keeper -terminate keeper -iostd LVCMOS18 hardware.ngd

hprep6 -i hardware

impact -batch hardware.cmd


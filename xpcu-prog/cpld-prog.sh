#!/bin/bash

cat > cpld-prog.prj <<- EOT
	verilog work "cpld-prog.v"
EOT

cat > cpld-prog.lso <<- EOT
	work
EOT

cat > cpld-prog.xst <<- EOT
	set -tmpdir "xilinx/projnav.tmp"
	set -xsthdpdir "xilinx"
	run
	-ifn cpld-prog.prj
	-ifmt mixed
	-ofn cpld-prog
	-ofmt NGC
	-p xbr
	-top cpldprog
	-opt_mode Speed
	-opt_level 1
	-iuc NO
	-lso cpld-prog.lso
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

set -ex

mkdir -p xilinx/projnav.tmp/
xst -ifn "cpld-prog.xst" -ofn "cpld-prog.syr"

mkdir -p xilinx/_ngo/
ngdbuild -dd xilinx/_ngo -uc cpld-prog.ucf -p xc2c256-VQ100-6 cpld-prog.ngc cpld-prog.ngd

cpldfit -p xc2c256-7-VQ100 -ofmt verilog -optimize density -htmlrpt -loc on -slew fast -init low \
	-inputs 32 -pterms 28 -unused keeper -terminate keeper -iostd LVCMOS18 cpld-prog.ngd

hprep6 -i cpld-prog


#!/bin/bash

ISE11DIR="/opt/Xilinx/11.3/ISE"
. "$ISE11DIR"/settings32.sh

dev2dev() {
	lsusb -d "$1" | sed -r 's,^Bus ([0-9]+) Device ([0-9]+).*,/dev/bus/usb/\1/\2,'
}

v() {
	echo "+ $*"
	"$@"
}

batchfile=$( mktemp )
trap 'rm "$batchfile"' 0

cat << EOT > "$batchfile"
setMode -bs
setCable -port usb21
identify
EOT

# v ./xsvftool-xpcu -E

v fxload -t fx2 -D $( dev2dev 04b4:8613; ) -I "$ISE11DIR"/bin/lin/xusb_emb.hex

for x in 0 1 2 3 4 5 6 7; do
	lsusb -d "03fd:0008" && break
	echo "Waiting for probe to re-enumerate.."
	sleep 1
done

v impact -batch "$batchfile"


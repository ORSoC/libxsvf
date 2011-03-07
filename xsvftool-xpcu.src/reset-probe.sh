#!/bin/bash

ISEDIR="/opt/Xilinx/11.3/ISE"

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
# identify
quit
EOT

v fxload -t fx2 -D $( dev2dev 04b4:8613; ) -I "$ISEDIR"/bin/lin/xusb_emb.hex

for x in 0 1 2 3 4 5 6 7; do
	lsusb -d "03fd:0008" && break
	echo "Waiting for probe to re-enumerate.."
	sleep 1
done

echo -n "Waiting for probe to settle.."
for x in 1 2 3 4 5; do echo -n .; sleep 1; done; echo

v strace -o x -f "$ISEDIR"/bin/lin/impact -batch "$batchfile"


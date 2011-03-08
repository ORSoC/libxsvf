#!/bin/bash
#
# Reset probe to Xilinx Firmware using 'impact'
# (see README file for details)
#

ISEDIR="/opt/Xilinx/11.3/ISE"

finddev() {
	for id in 03fd:0009 03fd:000d 03fd:000f 04b4:8613; do lsusb -d $id; done | \
		head -n 1 | sed -r 's,^Bus ([0-9]+) Device ([0-9]+).*,/dev/bus/usb/\1/\2,'
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

v fxload -t fx2 -D $(finddev) -I "$ISEDIR"/bin/lin/xusb_emb.hex

for x in 0 1 2 3 4 5 6 7; do
	lsusb -d "03fd:0008" && break
	echo "Waiting for probe to re-enumerate.."
	sleep 1
done

echo -n "Waiting for probe to settle.."
for x in 1 2 3 4 5; do echo -n .; sleep 1; done; echo

v "$ISEDIR"/bin/lin/impact -batch "$batchfile"


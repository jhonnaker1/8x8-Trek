#!/bin/sh
# Copy the game's data files onto Xemu's default SD card.
#
# mtools refuses the card until the FAT32 BPB has non-zero CHS geometry, which
# Xemu's formatter leaves at zero. Those two fields are legacy and unused by
# FAT32-LBA -- the Hypervisor reads by LBA -- so patching them to 63/255 makes
# the card readable by standard tools and changes nothing about how the MEGA65
# sees it. Done once, idempotent.
set -e
IMG="$HOME/Library/Application Support/xemu-lgb/mega65/mega65.img"
python3 - "$IMG" <<'PY'
import sys
p=sys.argv[1]; OFF=1048576
f=open(p,'r+b'); f.seek(OFF); b=bytearray(f.read(512))
if int.from_bytes(b[24:26],'little')==0:
    b[24:26]=(63).to_bytes(2,'little'); b[26:28]=(255).to_bytes(2,'little')
    f.seek(OFF); f.write(bytes(b)); print("patched legacy CHS geometry for mtools")
f.close()
PY
RC=$(mktemp); printf 'drive z: file="%s" offset=1048576\n' "$IMG" > "$RC"
export MTOOLSRC="$RC"
for f in "$@"; do mcopy -o "$f" z:/ ; done
mdir z: | tail -8
rm -f "$RC"

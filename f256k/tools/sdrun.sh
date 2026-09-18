#!/bin/bash
# Put a PGZ on a WORKING COPY of an F256 SD image and boot MAME into it.
# The original image is never written -- it is the user's, and a rig that
# edits what it measures is the failure this project keeps re-learning.
# Usage: sdrun.sh <mame_dir> <sdcard.img> <file.pgz> <name>
set -e
MAME_DIR="$1"; SDCARD="$2"; PGZ="$3"; NAME="$4"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp "$SDCARD" "$WORK/sd.img"
DISK="$(hdiutil attach -nomount "$WORK/sd.img" 2>/dev/null | head -1 | awk '{print $1}')"
diskutil mount "${DISK}s1" >/dev/null
VOL="$(diskutil info "${DISK}s1" | awk -F': +' '/Mount Point/{print $2}')"
cp "$PGZ" "$VOL/$NAME.pgz"
diskutil unmount "${DISK}s1" >/dev/null
hdiutil detach "$DISK" >/dev/null 2>&1
echo ">>> at the SuperBASIC prompt:  /- $NAME"
"$MAME_DIR/mame" f256k -rompath "$MAME_DIR" -window -harddisk "$WORK/sd.img"

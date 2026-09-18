#!/bin/bash
# Put a PGZ on a WORKING COPY of an F256 SD image and boot MAME into it.
# The original image is never written -- it is the user's, and a rig that
# edits what it measures is the failure this project keeps re-learning.
#
# MTOOLS, NOT hdiutil. The FAT partition is written in place with mcopy at the
# partition offset (7ms), instead of attaching the image as a device and asking
# diskutil to mount it (seconds, needs disk arbitration, and leaves a device
# node behind if the run dies). Nothing else about the path changes: FoenixMCP
# still reads the same FAT and pexec still loads the same file.
#
# Usage: sdrun.sh <mame_dir> <sdcard.img> <file.pgz> <name> [script.lua]
#   With script.lua the run is AUTOMATED: -nothrottle and the script drives it.
#   Without, the machine is LEFT RUNNING at the prompt for a person, throttled.
set -e
MAME_DIR="$1"; SDCARD="$2"; PGZ="$3"; NAME="$4"; LUA="$5"

command -v mcopy >/dev/null || { echo "sdrun: mtools not found (brew install mtools)" >&2; exit 1; }

# The FAT partition's byte offset, read from the MBR rather than assumed: entry
# 1's LBA is a little-endian u32 at $1C6. A hard-coded 2048 is right for this
# image and wrong for the next one.
OFF=$(python3 - "$SDCARD" <<'PY'
import sys, struct
with open(sys.argv[1], 'rb') as f:
    f.seek(0x1C6)
    print(struct.unpack('<I', f.read(4))[0] * 512)
PY
)

# MAME writes cfg/, nvram/ and snap/ into its working directory -- which is this
# port's SOURCE ROOT. Two of those turned up staged for a commit once. Send all
# three to build/ rather than adding a .gitignore line per directory as each
# one appears.
# Anchored to the PORT ROOT, not the caller's cwd -- a relative path here puts
# the emulator's droppings wherever the script happened to be invoked from.
B="$(cd "$(dirname "$0")/.." && pwd)/build"
MAME="$MAME_DIR/mame f256k -rompath $MAME_DIR -window -skip_gameinfo"
MAME="$MAME -cfg_directory $B/cfg -nvram_directory $B/nvram"
MAME="$MAME -snapshot_directory $B/snap"

WORK="$(mktemp -d)"
cp "$SDCARD" "$WORK/sd.img"
mcopy -o -i "$WORK/sd.img@@$OFF" "$PGZ" "::$NAME.pgz"

# EXTRA FILES RIDE ALONG, as "local/path:DESTNAME" in $SD_EXTRA. The overlay
# build needs OVERLAYS.BIN beside the program, and a run that copies only the
# PGZ tests the half of the port that does not need it.
for e in $SD_EXTRA; do
    mcopy -o -i "$WORK/sd.img@@$OFF" "${e%%:*}" "::${e##*:}"
done

# $SD_WAV records the emulated audio. -nothrottle does not distort it: MAME
# writes the wav in EMULATED time, so a run at eleven times speed still yields
# a recording whose frequencies are the machine's.
WAV=""
[ -n "$SD_WAV" ] && WAV="-wavwrite $SD_WAV"

if [ -n "$LUA" ]; then
    trap 'rm -rf "$WORK"' EXIT
    $MAME -nothrottle $WAV -harddisk "$WORK/sd.img" -autoboot_script "$LUA"
else
    # LEAVE IT RUNNING, and leave the image behind with it -- a machine in an
    # interesting state is the point, and deleting its disk out from under it
    # is not. The working copy is in a mktemp dir the OS reaps.
    echo ">>> at the SuperBASIC prompt:  /- $NAME"
    $MAME -harddisk "$WORK/sd.img"
fi

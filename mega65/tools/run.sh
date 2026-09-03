#!/bin/sh
# Headless run + screenshot. THIS IS THE THING THAT WAS MISSING ALL ALONG.
#
#   mega65/tools/run.sh build/egatrek.prg out.png [seconds]
#
# Xemu flushes -screenshot and -dumpscreen on SIGTERM, so a program with no
# exit path is still capturable. What made this work at last:
#
#   * -sdimg @mega65.img -- Xemu's OWN default card, in its prefs directory,
#     which it fdisk/formats itself the first time it creates one. Three
#     hand-built images failed before this: a bare FAT32, an MBR+FAT32 the
#     Hypervisor would not CHDIR into, and a blank card offered to a format
#     utility that lives ON the card. sdcontent.c in Xemu's source is the
#     authority: TWO partitions (type 0x0C FAT32 and type 0x41 MEGA65 system)
#     plus a fixed disk signature 837dcba6.
#   * A card Xemu made needs no ONBOARDing, so nothing waits for a human.
#   * The PRG must load at $2001 to be detected as BASIC and AUTO-RUN. An ELF
#     linked straight to a .prg loads at $457F, is not recognised, and Xemu
#     just types a SYS line and waits -- which looks exactly like a hang.
set -e
PRG="${1:?usage: run.sh prg out.png [seconds]}"
OUT="${2:?}"
SECS="${3:-20}"
ROM="$HOME/Library/Application Support/xemu-lgb/mega65/MEGA65.ROM"
XM="$HOME/xemu/bin/xmega65"
rm -f "$OUT"
"$XM" -rom "$ROM" -sdimg @mega65.img -prgmode 65 -prg "$PRG" \
      -besure -headless -screenshot "$OUT" >/dev/null 2>&1 &
PID=$!
sleep "$SECS"
kill -TERM "$PID" 2>/dev/null || true
sleep 2
ls -l "$OUT"

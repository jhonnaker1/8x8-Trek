#!/bin/sh
# q800.sh APP.bin OUTDIR "STEPS"
#
# Boot QEMU's Quadra 800 on System 7.6.1 with APP as the only Startup Item,
# record its sound, and run STEPS against the QEMU monitor. The rig is
# commodore-uno/mac's (its memory notes it): MAME's macii hangs at start-up
# whenever a Lua script is attached, so colour testing is done here instead.
#
# STEPS is a space-separated list, run in order:
#   wait:N        sleep N seconds of host time
#   shot:NAME     screendump OUTDIR/NAME.png
#   key:K         QEMU sendkey K (ret, up, down, backspace, a, shift-a, ...)
# The run ends with `quit` down the monitor, which is how QEMU is stopped
# (unlike Hatari, nothing here needs a gentler exit).
#
# THE SYSTEM DISK IS A CLONE, on the Retro68 volume because the internal disk
# is short of space (Jamie, 2026-11-26): `cp -c` of mactest/s761.raw, which
# APFS makes instantly and which costs only what is written. The application
# is copied into ":System Folder:Startup Items:" with hfsutils, replacing
# whatever was there, so it starts by itself about 90 s after power-on.
set -e
APP=$1; OUT=$2; STEPS=$3
RIG=${RIG:-/Volumes/Retro68/egatrek-test}
B=${RETRO68:-/Volumes/Retro68/Retro68-build/toolchain}/bin
ROM=${ROM:-$HOME/qemu_m68k/Quadra800.rom}
DEPTH=${DEPTH:-8}
mkdir -p "$OUT"

# hfsutils keeps its state in $HOME/.hcwd; keep it off the real home.
HOME=$RIG; export HOME
$B/hmount "$RIG/s761.raw" 1 >/dev/null
# A name can hold spaces ("EGA Trek"), so read the listing a line at a time.
$B/hls -1 ":System Folder:Startup Items:" 2>/dev/null | while IFS= read -r f; do
  $B/hdel ":System Folder:Startup Items:$f"
done
$B/hcopy -m "$APP" ":System Folder:Startup Items:"
$B/humount >/dev/null

SOCK="$RIG/qmon.sock"
rm -f "$SOCK" "$OUT/audio.wav"
qemu-system-m68k -M q800 -bios "$ROM" -m 64 -g 640x480x$DEPTH \
  -drive file="$RIG/q800-pram.img",format=raw,if=mtd \
  -device scsi-hd,scsi-id=0,drive=hd0 \
  -drive file="$RIG/s761.raw",media=disk,format=raw,if=none,id=hd0 \
  -audiodev wav,id=snd0,path="$OUT/audio.wav" -machine audiodev=snd0 \
  -display none -monitor unix:"$SOCK",server,nowait >"$OUT/qemu.log" 2>&1 &
QPID=$!

mon() { printf '%s\n' "$1" | nc -U "$SOCK" >/dev/null 2>&1 || true; }

i=0
while [ ! -S "$SOCK" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i+1)); done

for s in $STEPS; do
  case $s in
    wait:*) sleep "${s#wait:}" ;;
    shot:*) mon "screendump $OUT/${s#shot:}.png -f png"; sleep 1 ;;
    key:*)  mon "sendkey ${s#key:}"; sleep 0.4 ;;
  esac
done
mon quit
wait $QPID 2>/dev/null || true
ls "$OUT"

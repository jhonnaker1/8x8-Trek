#!/bin/sh
# q800play.sh APP.bin -- boot QEMU's Quadra 800 for a PERSON to play.
#
# The same clone of System 7.6.1 as tools/q800.sh, but with the sound on the
# Mac's speakers and the screen in Screen Sharing over VNC: QEMU's own Cocoa
# window shows only the bottom-left quarter on a Retina display, which is
# what commodore-uno/mac found, and Screen Sharing is how Jamie played UNO.
#
# The application goes into Startup Items (so it starts by itself) AND onto
# the disk's top level (so the Finder shows its icon, and it can be started
# again after quitting). If "not shut down properly" comes up first, press
# Return. Quit QEMU by closing nothing -- run `tools/q800play.sh stop`.
set -e
RIG=${RIG:-/Volumes/Retro68/egatrek-test}
B=${RETRO68:-/Volumes/Retro68/Retro68-build/toolchain}/bin
ROM=${ROM:-$HOME/qemu_m68k/Quadra800.rom}
SOCK="$RIG/qmon-play.sock"

if [ "$1" = stop ]; then
  printf 'quit\n' | nc -U "$SOCK" >/dev/null 2>&1 || true
  exit 0
fi
APP=$1

HOME=$RIG; export HOME
$B/hmount "$RIG/s761.raw" 1 >/dev/null
# A name can hold spaces ("EGA Trek"), so read the listing a line at a time.
$B/hls -1 ":System Folder:Startup Items:" 2>/dev/null | while IFS= read -r f; do
  $B/hdel ":System Folder:Startup Items:$f"
done
$B/hdel ":EGA Trek" 2>/dev/null || true
$B/hcopy -m "$APP" ":System Folder:Startup Items:"
$B/hcopy -m "$APP" ":EGA Trek"
$B/humount >/dev/null

rm -f "$SOCK"
qemu-system-m68k -M q800 -bios "$ROM" -m 64 -g 640x480x8 \
  -drive file="$RIG/q800-pram.img",format=raw,if=mtd \
  -device scsi-hd,scsi-id=0,drive=hd0 \
  -drive file="$RIG/s761.raw",media=disk,format=raw,if=none,id=hd0 \
  -audiodev coreaudio,id=snd0 -machine audiodev=snd0 \
  -display none -object secret,id=vncpw,data=egatrek \
  -vnc 127.0.0.1:1,password-secret=vncpw \
  -monitor unix:"$SOCK",server,nowait &
sleep 2
open "vnc://:egatrek@127.0.0.1:5901"
wait

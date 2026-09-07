#!/bin/sh
# Time a byte-at-a-time CBDOS read of OVERLAYS.BIN off a D81.
set -e
ROM="$HOME/Library/Application Support/xemu-lgb/mega65/MEGA65.ROM"
SECS="${1:-70}"
rm -f build/probe_read.png
"$HOME/xemu/bin/xmega65" -rom "$ROM" -sdimg @mega65.img -prgmode 65 \
    -8 "$PWD/build/read.d81" -prg "$PWD/build/probe_read.prg" \
    -besure -headless -screenshot build/probe_read.png >/dev/null 2>&1 &
PID=$!
sleep "$SECS"
kill -TERM "$PID" 2>/dev/null || true
sleep 2
ls -l build/probe_read.png

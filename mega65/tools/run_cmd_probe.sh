#!/bin/sh
# Probe 4 against a FRESH D81, then read the image back. The directory is the
# check on the probe's own story: after an overwrite there must be exactly ONE
# TREKSAVE, not two.
set -e
ROM="$HOME/Library/Application Support/xemu-lgb/mega65/MEGA65.ROM"
D81="$PWD/build/cmd.d81"
rm -f "$D81" build/probe_cmd.png
c1541 -format "probe,01" d81 "$D81" >/dev/null 2>&1
"$HOME/xemu/bin/xmega65" -rom "$ROM" -sdimg @mega65.img -prgmode 65 \
    -8 "$D81" -prg "$PWD/build/probe_cmd.prg" \
    -besure -headless -screenshot build/probe_cmd.png >/dev/null 2>&1 &
PID=$!
sleep 30
kill -TERM "$PID" 2>/dev/null || true
sleep 2
echo "--- D81 directory ---"
c1541 -attach "$D81" -list 2>&1 | grep -v "OPENCBM\|recognised\|attached\|detached"

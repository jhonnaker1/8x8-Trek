#!/bin/sh
# Run one probe_dos variant against a FRESH D81 on drive 8, then read the image
# back. The screen says what the KERNAL reported; the D81 says what happened.
#   tools/run_dos_probe.sh 1|2|3
set -e
C="${1:?usage: run_dos_probe.sh cfg}"
ROM="$HOME/Library/Application Support/xemu-lgb/mega65/MEGA65.ROM"
D81="$PWD/build/probe$C.d81"
rm -f "$D81" "build/probe_dos$C.png"
c1541 -format "probe,01" d81 "$D81" >/dev/null 2>&1
"$HOME/xemu/bin/xmega65" -rom "$ROM" -sdimg @mega65.img -prgmode 65 \
    -8 "$D81" -prg "$PWD/build/probe_dos$C.prg" \
    -besure -headless -screenshot "build/probe_dos$C.png" >/dev/null 2>&1 &
PID=$!
sleep 16
kill -TERM "$PID" 2>/dev/null || true
sleep 2
echo "--- D81 directory after cfg $C ---"
c1541 -attach "$D81" -list 2>&1 | grep -v OPENCBM

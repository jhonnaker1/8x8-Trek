#!/usr/bin/env python3
"""Run a Plus/4 PRG and decode the 40x25 text screen at $0C00.

Screen CODES, not PETSCII: 0-31 are @A-Z[..], 32-63 are ASCII punctuation and
digits, 64+ are the graphics half. Decoded the same way the C64 port's boot.py
does, and WITHOUT masking bit 7 -- masking it is how a screenshot of a working
title screen came back blank on the C64 (instrument #22).
"""
import os, subprocess, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import vice_mon

prg = sys.argv[1]
vice = subprocess.Popen(
    ["xplus4", "-binarymonitor", "-binarymonitoraddress", "ip4://127.0.0.1:6502",
     "-autostart", prg], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(float(sys.argv[2]) if len(sys.argv) > 2 else 8.0)

mon = vice_mon.Mon()
scr = mon.mem_get(0x0C00, 1000)
col = mon.mem_get(0x0800, 1000)

def ch(c):
    if c < 32:  return chr(64 + c)
    if c < 64:  return chr(c)
    return "#"                      # the graphics half: borders and blocks
live = sum(1 for c in scr if c != 32)
hues = sorted({b & 0x0F for b, c in zip(col, scr) if c != 32})
print("%d non-space cells, hues in use: %s" % (live, hues))
for r in range(25):
    print("%2d|%s|" % (r, "".join(ch(c) for c in scr[r*40:(r+1)*40])))
mon.resume()
vice.terminate()

#!/usr/bin/env python3
"""Load the game, then JUMP INTO IT DIRECTLY, taking BASIC out of the picture.

`SYS 4110` reports ?SYNTAX ERROR because the program is entered, dies, and
control falls back into BASIC's parser mid-statement -- so the error is the
program's and BASIC is only the messenger. Setting PC through the monitor
removes the messenger: whatever happens next is the program alone.
"""
import os, struct, subprocess, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import vice_mon

CMD_REGISTERS_AVAILABLE = 0x83
CMD_REGISTERS_SET       = 0x32

def registers(mon):
    """id -> name, from the emulator rather than from a table I typed."""
    rtype, err, p = mon.cmd(CMD_REGISTERS_AVAILABLE, bytes([0x00]))
    n = struct.unpack("<H", p[:2])[0]
    out, off = {}, 2
    for _ in range(n):
        size = p[off]
        rid, bits, nlen = p[off+1], p[off+2], p[off+3]
        name = p[off+4:off+4+nlen].decode("latin1")
        out[name.upper()] = rid
        off += size + 1
    return out

def set_pc(mon, rid, value):
    body = bytes([0x00]) + struct.pack("<H", 1) + \
           bytes([3, rid]) + struct.pack("<H", value)
    return mon.cmd(CMD_REGISTERS_SET, body)

d64 = "build/egatrek-plus4.d64"
pc  = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x1055
v = subprocess.Popen(["xplus4", "-binarymonitor",
                      "-binarymonitoraddress", "ip4://127.0.0.1:6502",
                      "-8", d64, "-autostart", d64],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(40)                       # load, RUN, fail -- the image is resident
mon = vice_mon.Mon(check=False)
regs = registers(mon)
print("registers the emulator offers: %s" % ", ".join(sorted(regs)))
print("setting PC = $%04X" % pc)
set_pc(mon, regs["PC"], pc)
# ONE CONNECTION, NOT TWO. Opening a second monitor socket after resuming
# timed out -- the emulator serves one at a time, and the failure looked like
# a crashed program rather than a busy port.
mon.resume()
time.sleep(12)
scr = mon.mem_get(0x0C00, 1000)
col = mon.mem_get(0x0800, 1000)
def ch(c):
    return chr(64 + c) if c < 32 else (chr(c) if c < 64 else "#")
live = sum(1 for c in scr if c != 32)
hues = sorted({b & 0x0F for b, c in zip(col, scr) if c != 32})
print("%d non-space cells, hues %s" % (live, hues))
for r in range(25):
    t = "".join(ch(c) for c in scr[r*40:(r+1)*40]).rstrip()
    print("%2d|%s|" % (r, t))
mon.resume(); v.terminate()

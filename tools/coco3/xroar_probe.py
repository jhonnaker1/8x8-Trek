#!/usr/bin/env python3
"""Run a CoCo 3 probe under XRoar and read its report -- and its SCREEN.

THE SECOND EMULATOR. MAME is the house rig for this target, and for the MMU
question that was the problem: the suspect was MAME's own GIME model, and a
suspect cannot be its own alibi. This runs the same probe under XRoar and
prints the same bytes, so the two can be compared directly.

XRoar has no headless screenshot, so the report comes back over the GDB remote
protocol -- `m<addr>,<len>` is the whole client.

Three traps, each of which cost a run:

  * ONE CONNECTION PER SESSION, and connecting HALTS the machine. The stub
    stops answering once the first client disconnects, so everything happens
    down a single socket; and the target is stopped the moment that socket
    opens, so `c` has to be sent before the emulator moves at all.

  * DO NOT DRIVE IT THROUGH BASIC. `-type` parses its own backslash escapes,
    so a real newline in the argument is swallowed and the whole script
    arrives as one line -- Jamie watched BASIC answer ?OM ERROR to it, and the
    retry dropped the E off EXEC. `-run FILE` injects the segments and jumps
    to the exec address. No CLEAR is needed either: a probe sets its own stack
    and never returns.

  * -no-ratelimit, AND DUMP THE SCREEN. At real CoCo speed a probe full of
    failing reads takes minutes, which reads exactly like wedged; polling
    tells the two apart cheaply. And the screen is what caught the other two
    traps -- without it this rig reported ten plausible bytes from a probe
    that had never been reached.

Leaves the machine running, as every rig here does.

    xroar_probe.py DISK.dsk PROBE.BIN [report_addr] [count]
"""
import socket, subprocess, sys, time

REPORT = 0x3F00
DONE   = 0x5A          # the completion marker every probe here writes last


def pkt(data):
    return ("$%s#%02x" % (data, sum(data.encode()) & 0xFF)).encode()


class GDB:
    """Just enough of the remote protocol to ask one question."""

    def __init__(self, host="127.0.0.1", port=65520, timeout=15.0):
        self.s = socket.create_connection((host, port), timeout=timeout)
        self.s.settimeout(timeout)
        self.buf = b""

    def _recv(self):
        while True:
            i = self.buf.find(b"$")
            j = self.buf.find(b"#", i + 1) if i >= 0 else -1
            if i >= 0 and j >= 0 and len(self.buf) >= j + 3:
                body, self.buf = self.buf[i + 1:j].decode("latin1"), self.buf[j + 3:]
                self.s.sendall(b"+")
                return body
            chunk = self.s.recv(4096)
            if not chunk:
                raise EOFError("gdb stub closed")
            self.buf += chunk

    def cmd(self, data):
        self.s.sendall(pkt(data))
        return self._recv()

    def go(self):
        self.s.sendall(pkt("c"))

    def halt(self):
        self.s.sendall(b"\x03")
        return self._recv()

    def read(self, addr, n):
        r = self.cmd("m%x,%x" % (addr, n))
        if not r or r.startswith("E"):
            raise RuntimeError("read $%04X failed: %r" % (addr, r))
        return bytes.fromhex(r)


def vdg(b):
    """VDG screen code -> printable. $00-$1F is @A-Z[..], $20-$3F is ASCII."""
    v = b & 0x3F
    return chr(0x40 + v) if v < 0x20 else chr(v)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__.strip().splitlines()[-1].strip())
    dsk, binary = sys.argv[1], sys.argv[2]
    addr = int(sys.argv[3], 16) if len(sys.argv) > 3 else REPORT
    n    = int(sys.argv[4], 16) if len(sys.argv) > 4 else 16

    subprocess.run(["pkill", "-f", "xroar -machine coco3"])
    time.sleep(1)
    log = open("/tmp/xroar-probe.log", "w")
    subprocess.Popen(["xroar", "-machine", "coco3", "-ram", "128",
                      "-load-fd0", dsk, "-run", binary,
                      "-gdb", "-no-ratelimit"], stdout=log, stderr=log)
    time.sleep(3)

    g = GDB()
    g.cmd("?")                      # attached; the machine is already halted
    rep, last, elapsed = None, None, 0.0
    while elapsed < 180.0:
        g.go()
        time.sleep(10.0)
        elapsed += 10.0
        g.halt()
        rep = g.read(addr, n)
        line = " ".join("%02X" % b for b in rep)
        if line != last:
            print("%5.0fs  %s" % (elapsed, line), flush=True)
            last = line
        if rep[-1] == DONE:
            break
    else:
        print("NEVER COMPLETED -- wedged, not slow", flush=True)

    scr = g.read(0x0400, 512)
    print("\n-- screen --")
    for row in range(16):
        print("|" + "".join(vdg(b) for b in scr[row * 32:(row + 1) * 32]) + "|")

    g.go()                          # LEAVE IT RUNNING
    print("\nleft running; pkill -f 'xroar -machine coco3'")
    return 0 if rep and rep[-1] == DONE else 1


if __name__ == "__main__":
    sys.exit(main())

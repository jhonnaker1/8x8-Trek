#!/usr/bin/env python3
"""Does 2K under the C64's KERNAL ROM survive KERNAL DISK I/O? Item 59.

Not "can I write to $E000" -- of course I can, writes go to RAM whatever is
banked in. The question is whether the bytes are still there after the port
does the thing it cannot avoid doing: loading a file through the KERNAL, which
runs ROM code at $E000-$FFFF with the candidate buffer sitting underneath it.

Reasoning says the KERNAL keeps its variables at $0200-$03FF and never stores
under itself. Reasoning has been wrong often enough here to be worth a run.
"""
import os, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
C64  = os.path.dirname(HERE)
ROOT = os.path.dirname(C64)
sys.path.insert(0, os.path.join(ROOT, "tools"))

D64 = os.path.join(C64, "build", "probe.d64")
REPORT = 0x0334


def main():
    if not os.path.exists(D64):
        sys.exit("logcheck: missing %s -- run `make probe`" % D64)

    vice = subprocess.Popen(
        ["x64sc", "-binarymonitor",
         "-binarymonitoraddress", "ip4://127.0.0.1:6502",
         "-autostart", D64 + ":logprobe"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(14)
        import vice_mon
        mon = vice_mon.Mon()
        # Wait for the stamp rather than a fixed time -- a LOAD off an
        # emulated 1541 is not instant and guessing the number is how three
        # tools on this project reported a timeout as a hang.
        r = None
        for _ in range(40):
            r = mon.mem_get(REPORT, 8)
            if r[7] == 0x5A:
                break
            time.sleep(0.5)
    finally:
        vice.send_signal(signal.SIGKILL)
        vice.wait()

    if r is None or r[7] != 0x5A:
        print("logcheck: the probe did not finish -- report %s"
              % " ".join("%02X" % b for b in (r or [])))
        return 1
    if r[0] != 0xA1:
        print("logcheck: the probe never reached main()")
        return 1

    names = {0xC1: "survived", 0xE1: "CORRUPTED"}
    print("  2K written under the KERNAL, read back before any disk I/O : %s"
          % names.get(r[1], "?%02X" % r[1]))
    print("  ... and again AFTER a KERNAL LOAD                          : %s"
          % {0xC2: "survived", 0xE2: "CORRUPTED"}.get(r[2], "?%02X" % r[2]))
    print("  the LOAD itself ended at $%02X%02X (non-zero = it really ran)"
          % (r[5], r[6]))
    if r[1] != 0xC1 or r[2] != 0xC2:
        print("  first bad offset: $%02X%02X" % (r[3], r[4]))
        print("logcheck: RAM under the KERNAL is NOT a home for the message log")
        return 1
    print("logcheck: 2,048 bytes under the KERNAL survive a KERNAL LOAD -- "
          "item 59 has an answer")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Both pages of the 40-column console, as PNGs, for a person to judge.

ITEM 57 STEP TWO. The straight left/right split is implemented and none of the
three message-placement answers is -- so what these pictures show is the
QUESTION, not an answer. Is typing an order on the tactical page and reading
its result on the chart page acceptable, or does the BADGE have to give way to
a two-box message strip? That is a taste call about the console Jamie cares
most about, and it wants a picture rather than arithmetic.

WHICH PAGE IS POKED, NOT TIMED. `page40` is read out of the ELF with llvm-nm
-- never a hardcoded address, it moves with every build -- and set from the
monitor, so both pictures come from one run and neither depends on guessing
when a redraw landed. The .map was tried first; its columns are not addresses.

AND use_vicii=1. vice_mon's default screenshot is the VDC's 80-column screen,
which is what every other check in this port looks at and is NOT where this
driver draws. A shot of the wrong chip is a blank screen, and reads exactly
like a driver that never ran.
"""
import os, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
C128 = os.path.dirname(HERE)
ROOT = os.path.dirname(C128)
sys.path.insert(0, os.path.join(ROOT, "tools"))

PRG = os.path.join(C128, "build", "console40.prg")
NM = os.path.expanduser("~/llvm-mos/bin/llvm-nm")


def page_symbol():
    out = subprocess.run([NM, PRG + ".elf"], capture_output=True, text=True).stdout
    for line in out.splitlines():
        f = line.split()
        if len(f) == 3 and f[2] == "page40":
            return int(f[0], 16)
    sys.exit("shot40: page40 is not in the ELF -- run `make vid40`")


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(C128, "build", "c40")
    if not os.path.exists(PRG):
        sys.exit("shot40: missing %s -- run `make vid40`" % PRG)
    addr = page_symbol()
    print("  page40 at $%04X" % addr)

    vice = subprocess.Popen(
        ["x128", "-binarymonitor",
         "-binarymonitoraddress", "ip4://127.0.0.1:6502", "-autostart", PRG],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(14)
        import vice_mon
        mon = vice_mon.Mon()
        for page, name in ((0, "tactical"), (1, "chart")):
            mon.mem_set(addr, bytes([page]))
            time.sleep(1.5)
            path = "%s-%s.png" % (out, name)
            w, h = vice_mon.screenshot(mon, path, use_vicii=1)
            print("  page %d %-8s %s  %dx%d" % (page, name, path, w, h))
    finally:
        vice.send_signal(signal.SIGKILL)
        vice.wait()
    return 0


if __name__ == "__main__":
    sys.exit(main())

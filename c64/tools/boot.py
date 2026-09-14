#!/usr/bin/env python3
"""Boot the C64 disk in VICE, screenshot it, AND LEAVE IT RUNNING.

LEAVING IT RUNNING IS THE POINT, not an oversight. Jamie, on the first tool
that got a machine somewhere interesting and then killed it: "your script kills
it too fast." A tool that reaches a state a person would want to look at hands
the machine over. --kill is for automation, and nothing else.

The screenshot comes off the VIC-II, which on this machine is the only chip
there is -- but vice_mon defaults to use_vicii=0, the C128's VDC, because that
is what every other check in this project looks at. A shot of the wrong chip is
a blank image and reads exactly like a driver that never ran, which has already
cost this project a debugging round.

THE SCREEN RAM CELL COUNT IS PRINTED BESIDE THE PICTURE, and that is the fix
for instrument #19: a black PNG was taken of a screen holding 364 stable cells,
and I reported the screen as blank when Jamie had watched it work. Screen RAM is
the thing itself; a display grab is one more instrument between me and it.
"""
import argparse, os, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
C64 = os.path.dirname(HERE)
ROOT = os.path.dirname(C64)
sys.path.insert(0, os.path.join(ROOT, "tools"))

SCREEN = 0x0400          # where c128/src/vic.c points the video matrix
CELLS = 40 * 25


# The console's borders, from c128/src/layout.h. WITHOUT THESE the whole frame
# printed as question marks and the panels were unreadable -- which matters,
# because this dump is the instrument that sees past a screenshot.
BOX = {64: "-", 93: "|", 112: "+", 110: "+", 109: "+", 125: "+",
       107: "+", 115: "+", 114: "+", 113: "+", 91: "+"}


def decode(cells):
    """Screen codes to something readable, WITHOUT MASKING BIT 7.

    The first version of this did `c &= 0x7F` to fold reverse video away, and
    it deleted the entire title screen. G_BLOCK is 160 -- a REVERSE SPACE --
    and the banner letters and the Lexington's artwork are built out of
    nothing else, so masking turned all of them into code 32 and printed a
    blank screen over a screen that was drawing perfectly.

    IT WAS THE LIVE-CELL COUNT THAT CAUGHT IT: 286 cells against a dump
    showing about 160 characters. That count exists because of instrument #19,
    where a black PNG was reported as a blank screen Jamie had just watched
    work. It has now paid for itself on the first screen of the next port.

    So: 160 prints as a solid #, other reverse codes as lowercase, and
    anything unrecognised as ? rather than a guess.
    """
    out = ""
    for c in cells:
        rev = c & 0x80
        b = c & 0x7F
        if c == 160:
            out += "#"
        elif b in BOX:
            out += BOX[b]
        elif 1 <= b <= 26:
            out += chr(96 + b) if rev else chr(64 + b)
        elif b == 32:
            out += " "
        elif 33 <= b <= 63:
            out += chr(b)
        elif b == 0:
            out += "@"
        else:
            out += "?"
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--disk", default=os.path.join(C64, "build", "egatrek-c64.d64"))
    ap.add_argument("--out", default=os.path.join(C64, "build", "boot.png"))
    ap.add_argument("--wait", type=float, default=20.0,
                    help="seconds to let the machine load and draw")
    ap.add_argument("--kill", action="store_true",
                    help="shut VICE down afterwards (automation only)")
    ap.add_argument("--poke", action="append", default=[],
                    help="ADDR=BYTE, applied before the shot (e.g. kb_inject)")
    a = ap.parse_args()

    if not os.path.exists(a.disk):
        sys.exit("boot: no %s -- run `make d64`" % a.disk)

    vice = subprocess.Popen(
        ["x64sc", "-binarymonitor",
         "-binarymonitoraddress", "ip4://127.0.0.1:6502",
         "-autostart", a.disk + ":trek64"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(a.wait)
        import vice_mon
        mon = vice_mon.Mon()

        for p in a.poke:
            addr, val = p.split("=")
            mon.mem_set(int(addr, 0), bytes([int(val, 0)]))
            time.sleep(0.5)

        scr = mon.mem_get(SCREEN, CELLS)
        live = sum(1 for b in scr if b not in (32, 0))
        w, h = vice_mon.screenshot(mon, a.out, use_vicii=1)
        print("  %s  %dx%d" % (a.out, w, h))
        print("  screen RAM: %d live cells of %d" % (live, CELLS))

        print("  --- screen $0400 ---")
        for row in range(25):
            print("  |" + decode(scr[row * 40:row * 40 + 40]) + "|")
    finally:
        if a.kill:
            vice.send_signal(signal.SIGKILL)
            vice.wait()
        else:
            print("  VICE IS STILL RUNNING (pid %d) -- play with it, then close"
                  " the window. --kill if you wanted it gone." % vice.pid)
    return 0


if __name__ == "__main__":
    sys.exit(main())

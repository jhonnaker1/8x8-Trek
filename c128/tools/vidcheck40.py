#!/usr/bin/env python3
"""The VIC-IIe 40-column driver, checked in BYTES rather than by eye.

WHY BOTH THIS AND A PICTURE. `src/vidtest40.c` draws a screen for a person to
look at, because three things here fail in ways that look like a working
screen: the wrong character-set bank renders every letter as the wrong letter,
a transposed colour is a legal colour, and a wrong stride puts everything
somewhere plausible. A person catches those. But a person also gets them
wrong in the other direction -- the corner blocks in the first passing run
looked yellow to me and are light green -- so the bytes settle it.

WHAT IT PROVED ON ITS FIRST RUN, and it is the reason the driver has an
odd-looking line in it: `$D018 = $14` IS NOT ENOUGH ON A C128. The KERNAL's
own screen editor IRQ re-asserts $D018 every frame from VM1 at $0A2C, so the
write lands and is undone before anyone sees it. The symptom was every letter
lowercase; the register read back $17 twice with the machine running. The
80-column driver never meets this because it deliberately leaves the character
base alone.

Run `make vid40` first, or use `make vidcheck40` which does.
"""
import os, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
C128 = os.path.dirname(HERE)
ROOT = os.path.dirname(C128)
sys.path.insert(0, os.path.join(ROOT, "tools"))

PRG = os.path.join(C128, "build", "vidtest40.prg")

# egavic.h's table. Held here as an independent copy ON PURPOSE: a check that
# imports the thing it is checking proves only that a file equals itself.
WANT = [0, 6, 5, 3, 2, 4, 9, 15, 11, 14, 13, 3, 10, 4, 7, 1]

# "EGA TREK" in screen codes, which vidtest40 writes at 0,0. The ARMED CHECK:
# if the program never ran, colour RAM still holds whatever the KERNAL left
# and every comparison below would be against a screen nobody drew.
BANNER = [5, 7, 1, 32, 20, 18, 5, 11]


def main():
    if not os.path.exists(PRG):
        sys.exit("vidcheck40: missing %s -- run `make vid40`" % PRG)

    vice = subprocess.Popen(
        ["x128", "-binarymonitor",
         "-binarymonitoraddress", "ip4://127.0.0.1:6502", "-autostart", PRG],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(14)                      # KERNAL boot, autostart, run
        import vice_mon
        mon = vice_mon.Mon()
        scr = mon.mem_get(0x0400, 1000)
        col = mon.mem_get(0xD800, 1000)
        d018 = mon.mem_get(0xD018, 1)[0]
        clk = mon.mem_get(0xD030, 1)[0]
    finally:
        vice.send_signal(signal.SIGKILL)
        vice.wait()

    if list(scr[:8]) != BANNER:
        print("vidcheck40: $0400 does not hold EGA TREK -- the program did "
              "not run, so nothing below would be a measurement")
        print("   got: %s" % " ".join("%02X" % b for b in scr[:8]))
        return 1

    bad = 0

    # THE CHARACTER SET BANK, which is the one that cost a run. Bits 1-3 are
    # the character base; 010 is $1000, uppercase/graphics. Bit 0 reads as 1.
    if (d018 & 0x0E) != 0x04:
        print("  $D018 = %02X -- character base is NOT $1000 "
              "(uppercase/graphics); letters will be wrong" % d018)
        bad += 1
    else:
        print("  $D018 = %02X   uppercase/graphics, and it SURVIVED the "
              "KERNAL's editor IRQ" % d018)

    # 1 MHz. At 2MHz the VIC-IIe cannot fetch coherently and there is no
    # picture at all -- the opposite of what the 80-column driver wants.
    if clk & 0x01:
        print("  $D030 = %02X -- 2MHz, the VIC-IIe has no picture at 2x" % clk)
        bad += 1
    else:
        print("  $D030 = %02X   1MHz, which is what the VIC-IIe needs" % clk)

    # EACH SECTION COUNTS ITS OWN. These summaries used to be gated on the
    # running total, so the first failure SUPPRESSED the pass line of every
    # later section -- and a reader could not tell whether the colours had
    # been checked and were fine, or not checked at all. Caught by breaking
    # the $D018 write on purpose and reading the whole output instead of
    # grepping it, which is the lesson of instrument #17. This is the second
    # time an accumulating counter has hidden a result on this project; the
    # first was tools/check_colours.py the same day.
    wrong = 0
    for i in range(16):
        got = col[5 * 40 + 3 + i * 2] & 0x0F
        if got != WANT[i]:
            print("  EGA %-2d -> VIC %-2d, want %-2d  WRONG" % (i, got, WANT[i]))
            wrong += 1
    print("  all 16 EGA colours map as egavic.h says" if not wrong
          else "  %d of 16 colours WRONG" % wrong)
    bad += wrong

    # THE STRIDE. Forty columns per row is the one number that, if wrong, puts
    # everything somewhere plausible and breaks nothing visibly.
    wrong = 0
    for label, off in (("0,24", 24 * 40), ("39,24", 24 * 40 + 39), ("39,0", 39)):
        if scr[off] != 160 or (col[off] & 0x0F) != 13:
            print("  corner %s: screen %d colour %d -- stride is wrong"
                  % (label, scr[off], col[off] & 0x0F))
            wrong += 1
    print("  corners at 0,24 / 39,24 / 39,0 -- the stride is 40" if not wrong
          else "  %d of 3 corners WRONG" % wrong)
    bad += wrong

    if bad:
        print("vidcheck40: %d wrong" % bad)
        return 1
    print("vidcheck40: the VIC-IIe driver draws what it is told")
    return 0


if __name__ == "__main__":
    sys.exit(main())

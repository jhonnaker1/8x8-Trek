#!/usr/bin/env python3
"""How much of the 64K does this port want, and what is left?

The staging rule every port here starts with: link the whole game RESIDENT and
READ THE OVERFLOW, before writing a driver that a failed budget would make
pointless. The numbers below are the machine's, not an estimate.
"""
import os, sys

ORG = 0x2800          # where the program loads, from the Makefile's --org
LOG = 0x2000          # 2,048, from src/gimelog.c -- LOW RAM now
LOG_END = LOG + 2048
STACK = 1024          # what cmoc's crt assumes, reserved below the vector page
VECTORS = 0xFE00      # MC3's RAM vector page; $FEF7 is the sound driver's slot
IO = 0xFF00           # the I/O page: nothing of ours may reach it

# THE SCREEN IS NOT UP HERE ANY MORE. It used to be modelled as `LOG - 4000`,
# a 4,000-byte tenant wedged under the message log, and that was true until
# the day it was measured that low RAM is safe -- it lives at $1000 now, below
# the load address, and costs this budget nothing. See src/gimevid.c and
# src/lowinit.c. The window size is no longer a hardcoded 4,096 either; it is
# whatever the caller reserved.


def main():
    path = sys.argv[1]
    # With --window, the ceiling is the overlay window rather than the screen:
    # the resident image has to end below it.
    win = None
    if "--window" in sys.argv:
        win = int(sys.argv[sys.argv.index("--window") + 1], 0)
    winlen = 0
    if "--window-len" in sys.argv:
        winlen = int(sys.argv[sys.argv.index("--window-len") + 1], 0)
    ceiling = win if win else (VECTORS - STACK)
    n = os.path.getsize(path)
    # A DECB binary carries a 5-byte header per segment and a 5-byte trailer.
    body = n - 10
    end = ORG + body
    print("  program   $%04X..$%04X   %d bytes" % (ORG, end - 1, body))
    print("  screen    $1000..$1F9F   4000 bytes   (low RAM, below $%04X)" % ORG)
    if win:
        # THE CEILING, NOT THE LOG. This derived the window's length from the
        # log's address, which was directly above it until the log moved into
        # low RAM -- and then printed a NEGATIVE size, $E600..$1FFF, without
        # anything noticing. The window now runs to the stack reserve.
        wl = winlen if winlen else (VECTORS - STACK - win)
        print("  window    $%04X..$%04X   %d bytes" % (win, win + wl - 1, wl))
    print("  log       $%04X..$%04X   2048 bytes   (low RAM, below $%04X)"
          % (LOG, LOG_END - 1, ORG))
    print("  stack     $%04X..$%04X   %d bytes" % (VECTORS - STACK,
                                                   VECTORS - 1, STACK))
    if end > ceiling:
        print("  OVERLAP   the image runs %d bytes PAST $%04X"
              % (end - ceiling, ceiling))
        print("early: it does not fit. The pool, the overlays or the screen "
              "position has to move.")
        return 1
    print("  free      $%04X..$%04X   %d bytes" % (end, ceiling - 1, ceiling - end))
    print("early: IT FITS, with %d bytes spare" % (ceiling - end))
    return 0


if __name__ == "__main__":
    sys.exit(main())

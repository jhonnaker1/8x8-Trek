#!/usr/bin/env python3
"""How much of the 64K does this port want, and what is left?

The staging rule every port here starts with: link the whole game RESIDENT and
READ THE OVERFLOW, before writing a driver that a failed budget would make
pointless. The numbers below are the machine's, not an estimate.
"""
import os, sys

ORG = 0x2800          # where the program loads, from the Makefile's --org
LOG = 0xF200          # 2,048, from src/gimelog.c
SCREEN = LOG - 4000   # the 80x25 text buffer, from src/gimevid.c
SCREEN_END = LOG
IO = 0xFF00           # the I/O page: nothing of ours may reach it


def main():
    path = sys.argv[1]
    # With --window, the ceiling is the overlay window rather than the screen:
    # the resident image has to end below it.
    win = None
    if "--window" in sys.argv:
        win = int(sys.argv[sys.argv.index("--window") + 1], 0)
    ceiling = win if win else SCREEN
    n = os.path.getsize(path)
    # A DECB binary carries a 5-byte header per segment and a 5-byte trailer.
    body = n - 10
    end = ORG + body
    print("  program   $%04X..$%04X   %d bytes" % (ORG, end - 1, body))
    print("  screen    $%04X..$%04X   %d bytes" % (SCREEN, SCREEN_END - 1,
                                                   SCREEN_END - SCREEN))
    if win:
        print("  window    $%04X..$%04X   4096 bytes" % (win, win + 4095))
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

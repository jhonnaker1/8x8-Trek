#!/usr/bin/env python3
"""How much of the 64K does this port want, and what is left?

The staging rule every port here starts with: link the whole game RESIDENT and
READ THE OVERFLOW, before writing a driver that a failed budget would make
pointless. The numbers below are the machine's, not an estimate.
"""
import os, sys

ORG = 0x2800          # where the program loads, from the Makefile's --org
SCREEN = 0xE000       # the 80x25 text buffer, from src/gimevid.c
SCREEN_END = SCREEN + 80 * 25 * 2
IO = 0xFF00           # the I/O page: nothing of ours may reach it


def main():
    path = sys.argv[1]
    n = os.path.getsize(path)
    # A DECB binary carries a 5-byte header per segment and a 5-byte trailer.
    body = n - 10
    end = ORG + body
    print("  program   $%04X..$%04X   %d bytes" % (ORG, end - 1, body))
    print("  screen    $%04X..$%04X   %d bytes" % (SCREEN, SCREEN_END - 1,
                                                   SCREEN_END - SCREEN))
    if end > SCREEN:
        print("  OVERLAP   the program runs %d bytes INTO the screen buffer"
              % (end - SCREEN))
        print("early: it does not fit. The pool, the overlays or the screen "
              "position has to move.")
        return 1
    print("  free      $%04X..$%04X   %d bytes between them"
          % (end, SCREEN - 1, SCREEN - end))
    print("  and       $%04X..$%04X   %d bytes above the screen"
          % (SCREEN_END, IO - 1, IO - SCREEN_END))
    print("early: it fits, with %d bytes spare below the screen"
          % (SCREEN - end))
    return 0


if __name__ == "__main__":
    sys.exit(main())

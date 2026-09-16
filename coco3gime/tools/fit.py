#!/usr/bin/env python3
"""Does the WHOLE PORT fit in the address space -- image, bss and all?

THE INSTRUMENT THIS PORT NEEDED FROM THE START. Three budget answers were
published and all three were wrong, every one of them because two quantities
share a name: the FILE SIZE, the RESIDENT IMAGE, the image PLUS BSS, and the
RUN-TIME EXTENT including the fixed tenants are four different numbers, and
only the fourth decides anything.

So this reads build_ovl.py's own report -- which knows the image and asks the
linker for bss_start/bss_end -- and adds the tenants that live at fixed
addresses and appear in no map at all: the overlay window, the 80x25 screen,
the message log and the stack.
"""
import re, subprocess, sys

WINDOW_MIN = 2560     # the largest overlay image, rounded up to a page
import os
WIDTH      = int(os.environ.get("WIDTH", "80"))
# THE SCREEN IS NOT COUNTED HERE ANY MORE, and that is a measurement, not an
# economy. It lives at $1000, in the low RAM below the load address, which
# lowram.c reported as fatal and was wrong about: what broke its disk was
# vdc_init writing INIT0 = $00 and clearing MC2, the WD1773's chip select.
# src/lowbisect.c fills and restores every 512-byte block of $0200..$27FF with
# the disk reading STOR_OK throughout, and src/lowinit.c reads a file with the
# full 80-column mode set and all 4,000 bytes written at $1000.
SCREEN     = 0                # $1000..$1F9F, below the image, not above it
LOG        = 2048     # ui.c: LOG_SLOTS 32 x LOG_STRIDE 64, at $F200
STACK      = 1024     # what cmoc's crt assumes
IO         = 0xFF00


def main():
    out = subprocess.run(["make", "overlays", "WIDTH=%d" % WIDTH],
                         capture_output=True, text=True).stdout
    res = re.search(r"resident\s+(\d+) bytes, \$([0-9A-F]{4})\.\.\$([0-9A-F]{4})", out)
    bss = re.search(r"bss\s+\$([0-9A-F]{4})\.\.\$([0-9A-F]{4}), (\d+) bytes", out)
    img = re.search(r"largest image (\d+) bytes", out)
    if not (res and bss):
        print("fit: build_ovl.py did not report an image and a bss range.")
        print(out[-600:])
        return 1

    image = int(res.group(1))
    top = int(bss.group(2), 16) + 1
    biggest = int(img.group(1)) if img else 0
    window = (biggest + 0xFF) & ~0xFF

    print("  resident image   %6d   $%s..$%s" % (image, res.group(2), res.group(3)))
    print("  bss              %6s   $%s..$%s" % (bss.group(3), bss.group(1), bss.group(2)))
    print("  image + bss end  $%04X" % top)
    print()
    print("  window           %6d   (largest image %d, to a page)" % (window, biggest))
    print("  screen           %6d   (%dx25, two bytes a cell)" % (SCREEN, WIDTH))
    print("  message log      %6d" % LOG)
    print("  stack            %6d" % STACK)
    need = window + SCREEN + LOG + STACK
    have = IO - top
    print("  ------------------------")
    print("  still to place   %6d" % need)
    print("  room to $FF00    %6d" % have)
    if need > have:
        print("\nfit: OVER BY %d bytes" % (need - have))
        return 1
    print("\nfit: IT FITS, %d bytes spare" % (have - need))
    return 0


if __name__ == "__main__":
    sys.exit(main())

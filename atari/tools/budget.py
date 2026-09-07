#!/usr/bin/env python3
"""How far off does EGA Trek link on this machine, and what would close it?

THE SCOPE ENDS WITH AN INSTRUCTION -- "Measure it properly before committing:
link the whole game early" -- and this is that measurement, kept runnable so
the number moves as code moves rather than being quoted from a note.

It expects the link to FAIL at this stage and reports by how much. That is the
point: the answer is information about how much has to be paged, not a build
that has to succeed yet.
"""
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve()
ATARI = HERE.parents[1]
NM = str(pathlib.Path.home() / "llvm-mos/bin/llvm-nm")
ELF = ATARI / "build" / "early.elf"

BASE, TOP = 0x3000, 0xC000          # $C000 up is the XL's OS ROM


def window_size():
    """From atari.ld, so this file never carries a second copy of it."""
    m = re.search(r"__ovl_size\s*=\s*(0x[0-9a-f]+)", (ATARI / "atari.ld").read_text())
    if not m:
        sys.exit("budget: no __ovl_size in atari.ld")
    return int(m.group(1), 16)


def main():
    win = window_size()
    resident_space = (TOP - BASE) - win

    build = subprocess.run(["make", "-C", str(ATARI), "build/early.elf"],
                           capture_output=True, text=True)
    over = {}
    for m in re.finditer(r"section '(\S+)' will not fit in region '(\w+)': "
                         r"overflowed by (\d+) bytes", build.stderr + build.stdout):
        sec, region, n = m.group(1), m.group(2), int(m.group(3))
        if region == "ram":
            over[sec] = n
        elif region == "window":
            over.setdefault("WINDOW:" + sec, n)

    print("atari/vbxe budget -- the game with every seam STUBBED")
    print("  address space   $%04X..$%04X       %6d bytes" % (BASE, TOP - 1, TOP - BASE))
    print("  overlay window  %6d bytes" % win)
    print("  for resident    %6d bytes" % resident_space)
    print()

    wins = {k: v for k, v in over.items() if k.startswith("WINDOW:")}
    if wins:
        print("  AN OVERLAY DOES NOT FIT THE WINDOW:")
        for k, v in sorted(wins.items()):
            print("    %-16s over by %d" % (k[7:], v))
        print()

    if not over:
        """READ FROM THE MAP, NOT FROM nm. The link emits an XEX through
           OUTPUT_FORMAT, not an ELF, so llvm-nm has nothing to read -- it
           returned an empty symbol list and this printed 'resident ends
           $0000, 44544 bytes spare', which is a nonsense that looks like
           good news. The map is the only thing that knows."""
        txt = (ATARI / "build" / "early.map").read_text()
        def sym(name):
            m = re.search(r"^\s+([0-9a-f]+)\s+[0-9a-f]+\s+\d+\s+\d+\s+"
                          + re.escape(name) + r"\s*=", txt, re.M)
            return int(m.group(1), 16) if m else None
        end = sym("__heap_start") or sym("__bss_end")
        if end is None:
            sys.exit("budget: no end symbol in build/early.map")
        code_end = sym("__data_end") or end
        window = BASE + resident_space
        print("  IT LINKS.")
        print("    code+rodata      $%04X..$%04X   %6d bytes" % (BASE, code_end, code_end - BASE))
        print("    writable data    $%04X..$%04X   %6d bytes" % (code_end, end, end - code_end))
        print("    spare below the window          %6d bytes" % (window - end))
        print()
        print("  AND THAT IS WITH NO DRIVERS. The X16's video, input, storage,")
        print("  far-memory, overlay and sound layers measure 4,539 bytes, so")
        print("  the real position is about %d bytes SHORT." % (4539 - (window - end)))
        return

    worst = max(v for k, v in over.items() if not k.startswith("WINDOW:")) if \
        any(not k.startswith("WINDOW:") for k in over) else 0
    if worst:
        print("  RESIDENT IS OVER BY %d BYTES." % worst)
        print("  That much more must move into overlays than the C128 already")
        print("  moves -- and this is BEFORE any driver is written, so the real")
        print("  gap is larger. For scale, the C128 fits the same game in")
        print("  37,823 bytes using eleven 4,096-byte overlays.")


main()

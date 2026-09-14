#!/usr/bin/env python3
"""What the C64 build actually costs, printed from the ELF and the ld script.

READ OUT OF c64.ld, NEVER TYPED HERE. The MEGA65's Makefile printed a free
figure 512 bytes too generous for as long as it existed, because the limit was
a number in the recipe and the linker script had moved -- and free space is
exactly the thing those figures get consulted for.
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
C64 = os.path.dirname(HERE)
READELF = os.path.expanduser("~/llvm-mos/bin/llvm-readelf")


def regions():
    """name -> (origin, length), from the MEMORY block of c64.ld."""
    text = open(os.path.join(C64, "c64.ld")).read()
    out = {}
    for m in re.finditer(r"^\s*(\w+)\s*\((?:rw|rwx)\)\s*:\s*ORIGIN\s*=\s*"
                         r"(0x[0-9A-Fa-f]+)\s*,\s*LENGTH\s*=\s*(0x[0-9A-Fa-f]+)",
                         text, re.M):
        out[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    return out


def sections(elf):
    """name -> (addr, size) for the allocated sections."""
    out = {}
    txt = subprocess.check_output([READELF, "--section-headers", elf], text=True)
    for line in txt.splitlines():
        m = re.match(r"\s*\[\s*\d+\]\s+(\.\S+)\s+\S+\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)",
                     line)
        if m:
            out[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    return out


def main():
    elf, prg = sys.argv[1], sys.argv[2]
    reg, sec = regions(), sections(elf)

    ram_o, ram_n = reg["ram"]
    win_o, win_n = reg["window"]

    # The highest byte any resident section reaches. .noinit is included on
    # purpose: it is not in the file, but it IS in the address space, and the
    # message log lives there.
    top = max(a + n for name, (a, n) in sec.items()
              if not name.startswith(".ovl") and not name.startswith(".zp")
              and a >= ram_o)
    used, free = top - ram_o, (ram_o + ram_n) - top

    print("  c64: resident $%04X..$%04X, %d bytes of %d, %d free"
          % (ram_o, top - 1, used, ram_n, free))
    print("       PRG %d bytes on disk; log and bss are above it in RAM"
          % os.path.getsize(prg))
    print("       stack guard $%04X..$%04X (%d bytes) then the window at $%04X"
          % (ram_o + ram_n, win_o - 1, win_o - (ram_o + ram_n), win_o))
    if free < 0:
        sys.exit("report_size: the resident image is %d bytes OVER the region"
                 % -free)
    return 0


if __name__ == "__main__":
    sys.exit(main())

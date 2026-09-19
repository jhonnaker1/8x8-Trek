#!/usr/bin/env python3
"""Static checks on the F256K build. No emulator: `make ports` runs this on
every port and a verify that boots a machine makes that gate unusable.

WHAT IT CHECKS, and every one of these is a fault this project has actually
shipped somewhere:

  * WHERE __stack POINTS. Nothing else does. The Plus/4's soft stack sat
    inside .text and the machine answered ?SYNTAX ERROR; it sat under a ROM
    and reads came back as ROM. Three homes, all of them addresses somebody
    believed were free.
  * THAT .lowbss IS BRACKETED BY THE SYMBOLS THAT ZERO IT. .bss lives at
    $0400 here, outside anything the C runtime clears, so vdc_init() zeroes
    __low_bss_start..__low_bss_end by hand. If those drift apart from the
    section, globals come up holding FoenixMCP's leftovers -- which is not
    hypothetical: the keyboard suite once read pexec's copy of the filename
    out of an uncleared array.
  * THAT NO TWO SECTIONS OVERLAP. The low region holds three.
  * THAT EVERY OVERLAY FITS THE WINDOW, and how much room is left.
  * THE PGZ ITSELF: two segments at the addresses the linker script says, and
    an entry point equal to _start. A wrong entry gives a file that loads and
    never runs, silently.
  * THE OVERLAY STAMP. The images are linked WITH the resident half, so
    yesterday's OVERLAYS.BIN beside today's program jumps into the middle of
    some other function with no error anywhere. That cost the MEGA65 an
    afternoon and two wrong diagnoses.
  * STRINGS.DAT'S COUNT AGAINST STR_COUNT. The binary carries STR_COUNT as an
    immediate, so a file from an older tree blanks every label in the game and
    says nothing.
"""
import os, re, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.dirname(HERE)
ROOT = os.path.dirname(PORT)
NM = os.path.expanduser("~/llvm-mos/bin/llvm-nm")
READELF = os.path.expanduser("~/llvm-mos/bin/llvm-readelf")

ELF  = os.path.join(PORT, "build", "egatrek.elf")
PGZ  = os.path.join(PORT, "build", "egatrek.pgz")
OVL  = os.path.join(PORT, "build", "egatrek.ovl")
STRS = os.path.join(PORT, "build", "strings.dat")

RAM_LO, RAM_HI = 0x2000, 0xA000        # $2000..$9FFF
LOW_LO, LOW_HI = 0x0400, 0x2000
WINDOW, WIN_SIZE = 0xA000, 0x2000

fails = []
def check(ok, msg):
    if not ok:
        fails.append(msg)
    return ok

def sections():
    out = subprocess.run([READELF, "-S", ELF], capture_output=True, text=True).stdout
    secs = {}
    for m in re.finditer(r"\[\s*\d+\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", out):
        name, typ, addr, off, size = m.groups()
        if name.startswith("."):
            secs[name] = (int(addr, 16), int(size, 16), typ)
    return secs

def symbols():
    out = subprocess.run([NM, ELF], capture_output=True, text=True).stdout
    syms = {}
    for ln in out.splitlines():
        f = ln.split()
        if len(f) == 3:
            syms[f[2]] = int(f[0], 16)
    return syms

def main():
    for p in (ELF, PGZ, OVL, STRS):
        if not os.path.exists(p):
            sys.exit("verify_f256: %s is missing -- run `make game disk` first" % p)

    secs, sym = sections(), symbols()
    text = secs.get(".text", (0, 0, ""))
    data = secs.get(".data", (0, 0, ""))
    resident = text[1] + data[1]
    spare = (RAM_HI - RAM_LO) - resident

    print("  resident            $%04X..$%04X, %d bytes, %d spare"
          % (RAM_LO, RAM_LO + resident - 1, resident, spare))
    check(spare >= 0, "resident overflows $2000..$9FFF by %d bytes" % -spare)
    check(text[0] >= RAM_LO, ".text starts at $%04X, below $2000" % text[0])

    # -- the low region, and where the stack points ------------------------
    lowr = secs.get(".lowrodata", (0, 0, ""))
    lowb = secs.get(".lowbss", (0, 0, ""))
    lows = secs.get(".lowstack", (0, 0, ""))
    print("  lowram              $%04X..$%04X: rodata %d, bss %d"
          % (LOW_LO, LOW_HI - 1, lowr[1], lowb[1]))

    stack = sym.get("__stack")
    bottom = sym.get("__stack_bottom")
    check(stack is not None and bottom is not None, "__stack or __stack_bottom is missing")
    if stack is not None and bottom is not None:
        print("  soft stack          $%04X..$%04X, %d bytes reserved, grows down"
              % (bottom, stack - 1, stack - bottom))
        # THE CHECK THE PLUS/4 DID NOT HAVE.
        check(LOW_LO < stack <= LOW_HI,
              "__stack is $%04X, outside the low region $%04X..$%04X" % (stack, LOW_LO, LOW_HI))
        check(bottom >= lowb[0] + lowb[1],
              "the soft stack starts inside .lowbss")
        check(stack - bottom >= 512,
              "only %d bytes reserved for the soft stack" % (stack - bottom))

    bs, be = sym.get("__low_bss_start"), sym.get("__low_bss_end")
    check(bs is not None and be is not None, "the low-bss zeroing symbols are missing")
    if bs is not None and lowb[1]:
        check(bs <= lowb[0] and be >= lowb[0] + lowb[1],
              "vdc_init zeroes $%04X..$%04X but .lowbss is $%04X..$%04X"
              % (bs, be, lowb[0], lowb[0] + lowb[1]))

    # -- nothing may overlap ----------------------------------------------
    spans = [(n, s[0], s[0] + s[1]) for n, s in secs.items()
             if s[1] and s[0] and not n.startswith(".ovl_") and n != ".comment"]
    spans.sort(key=lambda x: x[1])
    for (n1, _, e1), (n2, s2, _) in zip(spans, spans[1:]):
        check(e1 <= s2, "%s ends at $%04X, past the start of %s at $%04X" % (n1, e1, n2, s2))

    # -- the overlays ------------------------------------------------------
    ovs = {n: s for n, s in secs.items() if n.startswith(".ovl_")}
    check(len(ovs) > 0, "no overlay sections at all")
    biggest, bname = 0, ""
    for n, (addr, size, _) in sorted(ovs.items()):
        check(addr == WINDOW, "%s runs at $%04X, not the window $%04X" % (n, addr, WINDOW))
        check(size <= WIN_SIZE, "%s is %d bytes, the window is %d" % (n, size, WIN_SIZE))
        check(size > 0, "%s is EMPTY -- nothing is annotated for it, or it was all inlined" % n)
        if size > biggest:
            biggest, bname = size, n
    print("  largest overlay     %s, %d of %d, %d spare in the window"
          % (bname, biggest, WIN_SIZE, WIN_SIZE - biggest))

    # -- the PGZ container -------------------------------------------------
    blob = open(PGZ, "rb").read()
    check(blob[:1] == b"Z", "the PGZ does not start with 'Z'")
    at, segs = 1, []
    while at + 6 <= len(blob):
        addr = int.from_bytes(blob[at:at+3], "little")
        size = int.from_bytes(blob[at+3:at+6], "little")
        at += 6
        if size == 0:
            segs.append((addr, 0))
            break
        segs.append((addr, size))
        at += size
    check(at == len(blob), "the PGZ has %d trailing bytes" % (len(blob) - at))
    check(len(segs) >= 2, "the PGZ has no terminating segment")
    entry = segs[-1][0]
    check(entry == sym.get("_start"),
          "the PGZ entry is $%04X but _start is $%04X" % (entry, sym.get("_start", 0)))
    addrs = sorted(a for a, n in segs[:-1])
    check(addrs == [LOW_LO, RAM_LO],
          "the PGZ loads at %s, expected [$0400, $2000]" % ["$%04X" % a for a in addrs])
    print("  pgz                 %d bytes, %d segment(s), entry $%04X"
          % (len(blob), len(segs) - 1, entry))

    # -- the overlay images and their stamp --------------------------------
    img = open(OVL, "rb").read()
    check(img[:4] == b"TOVL", "OVERLAYS.BIN has no TOVL header")
    count, stamp = struct.unpack("<HH", img[4:8])
    check(count == len(ovs), "OVERLAYS.BIN holds %d images, the ELF has %d sections" % (count, len(ovs)))
    check(stamp == (sym.get("ovl_anchor", 0) & 0xFFFF),
          "OVERLAYS.BIN's stamp is $%04X, ovl_anchor is $%04X -- the images are from a DIFFERENT LINK"
          % (stamp, sym.get("ovl_anchor", 0)))
    sizes = struct.unpack("<%dH" % count, img[8:8 + 2 * count])
    check(8 + 2 * count + sum(sizes) == len(img),
          "OVERLAYS.BIN's directory says %d bytes, the file is %d"
          % (8 + 2 * count + sum(sizes), len(img)))
    print("  overlay images      %d, %d bytes, stamp $%04X" % (count, len(img), stamp))

    # -- STRINGS.DAT against the header the program was compiled from -------
    want = None
    for ln in open(os.path.join(ROOT, "c128", "src", "strdata.h")):
        m = re.match(r"#define\s+STR_COUNT\s+(\d+)", ln)
        if m:
            want = int(m.group(1))
    got = int.from_bytes(open(STRS, "rb").read(2), "little")
    check(want is not None and got == want,
          "STRINGS.DAT holds %d strings, STR_COUNT is %s -- every label would come out blank"
          % (got, want))
    print("  strings             %d, matching STR_COUNT" % got)

    if fails:
        for f in fails:
            print("verify_f256: FAIL -- %s" % f)
        return 1

    # THE LINE tools/check_ports.py ACTUALLY SURFACES, and its shape is not
    # decoration: that gate prints only lines containing "verify:" and one of
    # its KEEP words. Without this the port sits in the list showing NOTHING,
    # which is the failure its own comment warns about -- "a port that joins
    # without surfacing its numbers is in the list without being in the check".
    # Found by joining and reading the gate's output rather than assuming it.
    print("f256k verify: resident $%04X..$%04X, %d bytes spare; "
          "lowram $%04X..$%04X; soft stack %d reserved; "
          "largest overlay %d of %d"
          % (RAM_LO, RAM_LO + resident - 1, spare, LOW_LO, LOW_HI - 1,
             (stack - bottom) if stack and bottom else 0, biggest, WIN_SIZE))
    print("verify_f256: all checks pass")
    return 0

if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""The checks the compiler and the test suite structurally cannot make.

WHAT THIS DELIBERATELY DOES NOT CHECK: the overlay call graph, the key table,
the panel geometry, the string-pool ceiling. Those live in shared code, and
`make -C c128 verify` already checks them against the same sources -- a second
copy here would be a second thing to keep in step. This file checks what is
TRUE OF THIS MACHINE ONLY, which is the memory map.

Every limit is read out of c64.ld or off the disk. Nothing is typed twice.
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
C64 = os.path.dirname(HERE)
LD = os.path.join(C64, "c64.ld")
ELF = os.path.join(C64, "build", "trek64.elf")
PRG = os.path.join(C64, "build", "trek64.prg")
READELF = os.path.expanduser("~/llvm-mos/bin/llvm-readelf")

fails = []


def check(ok, msg):
    print("  %-5s %s" % ("ok" if ok else "FAIL", msg))
    if not ok:
        fails.append(msg)


def ld_text():
    return open(LD).read()


def regions():
    out = {}
    for m in re.finditer(r"^\s*(\w+)\s*\((?:rw|rwx)\)\s*:\s*ORIGIN\s*=\s*"
                         r"(0x[0-9A-Fa-f]+)\s*,\s*LENGTH\s*=\s*(0x[0-9A-Fa-f]+)",
                         ld_text(), re.M):
        out[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    return out


def sections():
    out = {}
    txt = subprocess.check_output([READELF, "--section-headers", ELF], text=True)
    for line in txt.splitlines():
        m = re.match(r"\s*\[\s*\d+\]\s+(\.\S+)\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)",
                     line)
        if m:
            out[m.group(1)] = dict(addr=int(m.group(2), 16),
                                   off=int(m.group(3), 16),
                                   size=int(m.group(4), 16))
    return out


def segments():
    """(vaddr, paddr, memsz) per PT_LOAD -- this is where LMA != VMA shows up."""
    out = []
    txt = subprocess.check_output([READELF, "--program-headers", ELF], text=True)
    for m in re.finditer(r"LOAD\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)"
                         r"\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)", txt):
        out.append((int(m.group(1), 16), int(m.group(2), 16), int(m.group(4), 16)))
    return out


def main():
    reg, sec = regions(), sections()
    ram_o, ram_n = reg["ram"]
    win_o, win_n = reg["window"]

    # 1. THE LOAD ADDRESS. A PRG that says anything but $0801 does not
    #    auto-start, and the symptom is a READY prompt rather than an error.
    d = open(PRG, "rb").read()
    load = d[0] | (d[1] << 8)
    check(load == ram_o,
          "PRG loads at $%04X, the start of the program region" % load)

    # 2. THE RESIDENT IMAGE FITS, .noinit included. The log is 2K of .noinit
    #    and is not in the file, so a check against the PRG's size alone would
    #    pass with the log hanging over the stack guard.
    top = max(v["addr"] + v["size"] for k, v in sec.items()
              if not k.startswith(".ovl") and not k.startswith(".zp")
              and v["addr"] >= ram_o)
    check(top <= ram_o + ram_n,
          "resident top $%04X is inside the region (ends $%04X, %d free)"
          % (top, ram_o + ram_n - 1, ram_o + ram_n - top))

    # 3. THE STACK GUARD IS REAL. __stack sits at the window and grows DOWN,
    #    so the gap between the region's end and the window is all the room it
    #    has. 64 bytes shipped on the C128 in v0.9.0 and was overrun by 79 --
    #    straight into the overlay window, which then loaded 4K of code over
    #    the live return addresses.
    stack = int(re.search(r"__stack\s*=\s*(0x[0-9A-Fa-f]+)", ld_text()).group(1), 16)
    guard = stack - (ram_o + ram_n)
    check(stack == win_o, "__stack is $%04X, the foot of the window" % stack)
    check(guard >= 256, "stack guard is %d bytes (>= 256)" % guard)

    # 4. EVERY OVERLAY RUNS AT THE WINDOW AND FITS IT.
    ovl = {k: v for k, v in sec.items() if k.startswith(".ovl_")}
    check(len(ovl) == 11, "eleven overlay sections (%d)" % len(ovl))
    bad = [k for k, v in ovl.items() if v["addr"] != win_o]
    check(not bad, "every overlay runs at $%04X" % win_o)
    big = [(k, v["size"]) for k, v in ovl.items() if v["size"] > win_n]
    check(not big, "every overlay fits %d bytes (largest %d)"
          % (win_n, max(v["size"] for v in ovl.values())))

    # 5. NO TWO OVERLAY IMAGES SHARE A PLACE IN THE FILE. When they did on the
    #    C128, objcopy dumped the same image under two names and the game ran
    #    the hall of fame when it asked for the evaluation.
    offs = sorted(v["off"] for v in ovl.values())
    check(len(set(offs)) == len(offs), "eleven distinct file offsets")

    # 6. .data NEEDS NO COPY ROUTINE. c64.ld aliases c_readonly and
    #    c_writeable to the same region, so VMA must equal LMA -- and if it
    #    ever stops doing so, the build needs -lcopy-data or .data is NEVER
    #    INITIALISED. That is not a theoretical failure: it cost the C128 port
    #    its sound for a day, silently, with a map that looked correct.
    dat = sec.get(".data")
    if dat and dat["size"]:
        seg = [s for s in segments() if s[0] <= dat["addr"] < s[0] + s[2]]
        same = all(v == p for v, p, _ in seg) if seg else False
        check(same, ".data at $%04X has LMA == VMA, so no copy routine is needed"
              % dat["addr"])
    else:
        check(True, ".data is empty")

    # 7. THE FAR STORE FITS. $E000..$FFF9 holds STRINGS.DAT and MUSIC.DAT; the
    #    top six bytes are the RAM interrupt vectors c64mem.c writes, and are
    #    NOT the store's. A pool that overflows would load truncated, and the
    #    game would draw the last few hundred strings as whatever follows.
    far_base = int(re.search(r"#define FAR_BASE\s+(0x[0-9A-Fa-f]+)",
                             open(os.path.join(C64, "src", "c64mem.c")).read()).group(1), 16)
    far_limit = int(re.search(r"#define FAR_LIMIT\s+(0x[0-9A-Fa-f]+)",
                              open(os.path.join(C64, "src", "c64mem.c")).read()).group(1), 16)
    room = far_limit - far_base
    have = 0
    for f in ("strings.dat", "music.dat"):
        p = os.path.join(C64, "build", f)
        if os.path.exists(p):
            have += os.path.getsize(p)
    check(have <= room,
          "far store $%04X..$%04X holds %d bytes of %d (%d spare)"
          % (far_base, far_limit - 1, have, room, room - have))

    # 8. THE FAR STORE DOES NOT REACH THE VECTORS. Stated separately from 7
    #    because it is a different claim: 7 is about today's files, this is
    #    about the constant. If FAR_LIMIT is ever raised to $10000 the store
    #    would be allowed to write over its own NMI vector, and the failure
    #    would be a RESTORE keypress during a string fetch -- once, rarely,
    #    with nothing pointing back here.
    check(far_limit <= 0xFFFA,
          "FAR_LIMIT $%04X leaves the six RAM vectors at $FFFA alone" % far_limit)

    # 9. MUSIC.DAT IS THE COMPOSITION, NOT A COPY OF SOMETHING PRG-HEADERED.
    #    This port shipped a 414-byte music.dat for its whole short life -- the
    #    C128's music.pdat, two bytes of load address and all -- because a make
    #    rule did not name its source and a stale file looked up to date. The
    #    far store came back two bytes long and nothing else complained.
    #    Compared by CONTENT against the C128's, which is where the composition
    #    is generated; tools/make_music.py output is the only music that may
    #    ship (see NOTES.md on why gen_music.py output never can).
    ours = os.path.join(C64, "build", "music.dat")
    theirs = os.path.join(os.path.dirname(C64), "c128", "build", "music.dat")
    if os.path.exists(ours) and os.path.exists(theirs):
        a, b = open(ours, "rb").read(), open(theirs, "rb").read()
        check(a == b, "music.dat is byte-identical to the composition (%d bytes)"
              % len(a))

    if fails:
        print("verify_c64: %d check(s) failed" % len(fails))
        return 1
    print("verify_c64: the C64 memory map is sound")
    return 0


if __name__ == "__main__":
    sys.exit(main())

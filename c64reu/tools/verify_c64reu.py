#!/usr/bin/env python3
"""The checks the compiler and the test suite structurally cannot make.

THE C64'S CHECKS, ON THIS PORT'S LINK, PLUS THREE OF ITS OWN. The memory map
is ../c64's with a different overlay list, so its parsers are imported from
../c64/tools/verify_c64.py and pointed at c64reu.ld and build/trekreu.elf
rather than copied. What only this port can get wrong:

  * THE THUNKS. Every slot the patch filled must be `jsr ovl_far`, an id this
    build has, and the START of a function in that overlay -- read back out
    of the linked ELF, so a patch that wrote the wrong bytes cannot pass on
    the strength of its own report.
  * THE IDS. Every .ovl_* section must have an id in core/overlay.h and the
    count must be OVL_COUNT, because src/c64reu.c loads by id and the disk
    carries one file per section.
  * THE REU. Twenty 4K images must fit the smallest REU made, a 128K 1700.

`make ports` runs this. It needs no emulator; `make reucheck` is the one that
plays the game.
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.dirname(HERE)
ROOT = os.path.dirname(PORT)
sys.path.insert(0, os.path.join(ROOT, "c64", "tools"))
sys.path.insert(0, HERE)
import verify_c64 as v                    # noqa: E402
import reu_thunks                         # noqa: E402

v.LD = os.path.join(PORT, "c64reu.ld")
v.ELF = os.path.join(PORT, "build", "trekreu.elf")
v.PRG = os.path.join(PORT, "build", "trekreu.prg")
check = v.check

REU_SMALLEST = 128 * 1024
IMAGE = 0x1000
JSR = 0x20


def main():
    reg, sec = v.regions(), v.sections()
    ram_o, ram_n = reg["ram"]
    win_o, win_n = reg["window"]
    ld = open(v.LD).read()

    d = open(v.PRG, "rb").read()
    load = d[0] | (d[1] << 8)
    check(load == ram_o, "PRG loads at $%04X, the start of the program region" % load)

    top = max(s["addr"] + s["size"] for k, s in sec.items()
              if not k.startswith(".ovl") and not k.startswith(".zp")
              and s["addr"] >= ram_o)
    check(top <= ram_o + ram_n,
          "resident $%04X..$%04X, %d of %d used, %d free below the stack guard"
          % (ram_o, top - 1, top - ram_o, ram_n, ram_o + ram_n - top), surface=True)

    stack = int(re.search(r"__stack\s*=\s*(0x[0-9A-Fa-f]+)", ld).group(1), 16)
    guard = stack - (ram_o + ram_n)
    check(stack == win_o, "__stack is $%04X, the foot of the window" % stack)
    check(guard >= 256, "soft stack guard %d bytes at $%04X" % (guard, ram_o + ram_n),
          surface=True)

    # THE OVERLAYS, AND THEIR IDS.
    ovl = {k: s for k, s in sec.items() if k.startswith(".ovl_")}
    ids = reu_thunks.overlay_ids(sorted(k[5:] for k in ovl))
    count = len(ids)
    check(sorted(ids.values()) == list(range(count)),
          "%d overlays, ids 0..%d with none missing" % (count, count - 1))
    check(not [k for k, s in ovl.items() if s["addr"] != win_o],
          "every overlay runs at $%04X" % win_o)
    big_n = max(s["size"] for s in ovl.values())
    check(big_n <= win_n, "largest overlay %d of %d, %d spare"
          % (big_n, win_n, win_n - big_n), surface=True)
    offs = [s["off"] for s in ovl.values()]
    check(len(set(offs)) == len(offs), "%d distinct file offsets" % len(offs))
    check(count * IMAGE <= REU_SMALLEST,
          "%d images of 4K need %dK of REU; the smallest, a 1700, has 128K"
          % (count, count * IMAGE // 1024))

    # THE THUNKS, read back out of the link.
    elf = reu_thunks.Elf(v.ELF)
    syms = {s["name"]: s for s in elf.syms}
    t0, t1 = syms["__ovl_thunks"]["value"], syms["__ovl_thunks_end"]["value"]
    far = syms["ovl_far"]["value"]
    text = syms["__ovl_thunks"]["shndx"]
    secname = {i: s["nm"] for i, s in enumerate(elf.sh)}
    starts = {}
    for s in elf.syms:
        if s["type"] == reu_thunks.STT_FUNC and secname.get(s["shndx"], "").startswith(".ovl_"):
            starts[(secname[s["shndx"]][5:], s["value"])] = s["name"]
    by_id = {i: n for n, i in ids.items()}
    used, wrong = 0, []
    for a in range(t0, t1, reu_thunks.THUNK):
        o = elf.at(text, a)
        b = bytes(elf.b[o:o + reu_thunks.THUNK])
        if b == bytes(reu_thunks.THUNK):
            continue                          # an unfilled slot: BRK x 6
        used += 1
        op, lo, hi, oid, blo, bhi = struct.unpack("<6B", b)
        target = blo | bhi << 8
        if op != JSR or (lo | hi << 8) != far:
            wrong.append("$%04X is not jsr ovl_far" % a)
        elif oid not in by_id:
            wrong.append("$%04X names overlay %d, which this build has not" % (a, oid))
        elif (by_id[oid], target) not in starts:
            wrong.append("$%04X calls $%04X, not the start of a function in %s"
                         % (a, target, by_id[oid]))
    check(not wrong, "thunks %d of %d slots, each jsr ovl_far to a function "
          "start in its overlay" % (used, (t1 - t0) // reu_thunks.THUNK), surface=True)
    for w in wrong:
        print("        " + w)

    # .data needs no copy routine: the C64's link script's reason, unchanged.
    dat = sec.get(".data")
    if dat and dat["size"]:
        seg = [s for s in v.segments() if s[0] <= dat["addr"] < s[0] + s[2]]
        check(bool(seg) and all(a == b for a, b, _ in seg),
              ".data at $%04X has LMA == VMA, so no copy routine is needed" % dat["addr"])

    # THE FAR STORE is the C64's c64mem.c, holding this port's strings.
    mem = open(os.path.join(ROOT, "c64", "src", "c64mem.c")).read()
    far_base = int(re.search(r"#define FAR_BASE\s+(0x[0-9A-Fa-f]+)", mem).group(1), 16)
    far_limit = int(re.search(r"#define FAR_LIMIT\s+(0x[0-9A-Fa-f]+)", mem).group(1), 16)
    room = far_limit - far_base
    have = sum(os.path.getsize(os.path.join(PORT, "build", f))
               for f in ("strings.dat", "music.dat")
               if os.path.exists(os.path.join(PORT, "build", f)))
    check(have <= room, "far store $%04X..$%04X holds %d of %d, %d spare"
          % (far_base, far_limit - 1, have, room, room - have), surface=True)
    check(far_limit <= 0xFFFA,
          "FAR_LIMIT $%04X leaves the six RAM vectors at $FFFA alone" % far_limit)

    ours = os.path.join(PORT, "build", "music.dat")
    theirs = os.path.join(ROOT, "c128", "build", "music.dat")
    if os.path.exists(ours) and os.path.exists(theirs):
        a, b = open(ours, "rb").read(), open(theirs, "rb").read()
        check(a == b, "music.dat is byte-identical to the composition (%d bytes)" % len(a))

    if v.fails:
        print("verify_c64reu: %d check(s) failed" % len(v.fails))
        return 1
    print("verify_c64reu: the C64 + REU memory map is sound")
    return 0


if __name__ == "__main__":
    sys.exit(main())

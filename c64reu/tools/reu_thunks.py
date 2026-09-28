#!/usr/bin/env python3
"""Patch the REU build's cross-overlay calls into thunks, in the linked ELF.

    reu_thunks.py build/trekreu.elf

WHY AFTER THE LINK. The REU build lets one overlay call another -- see
src/reuovl.s -- but the compiler emits a plain `jsr f` for every call, and
under LTO most of the callees are static functions that no source-level
trick can rename. The linker knows exactly where every such call is, and
with -Wl,--emit-relocs it leaves that knowledge in the ELF. So this reads
the relocations, finds every JSR or JMP from one section into a function in
a DIFFERENT overlay (or from resident code into any overlay), gives each
distinct callee a six-byte thunk in __ovl_thunks, and rewrites the call's
operand to point at the thunk. Calls inside one overlay stay direct.

EVERY WINDOW ADDRESS BELONGS TO TWENTY FUNCTIONS AT ONCE -- each overlay
starts at $C000 -- so an address cannot say which function a call reaches.
A relocation can: it names a symbol, and the symbol names its SECTION. That
is the whole reason this works from relocations and not from a disassembly;
tools/overlay_check.py records what the address-based view got wrong.

JUMP TABLES ARE LEFT ALONE: a switch's table sits in resident .rodata and
points into the middle of its own function, which only reads it while that
function -- and so its overlay -- is running.

IT REFUSES, rather than guessing, when:
  * any reference to an overlay function other than a JSR/JMP comes from
    outside that overlay -- a function pointer would bypass the thunk and
    run whatever image the window holds;
  * a call's operand does not already hold the address its relocation
    predicts -- then this script has misread the ELF, and patching would
    make that worse;
  * there are more callees than __ovl_thunks has slots.

The overlay ids come from core/overlay.h through the C preprocessor, the
same definitions src/c64reu.c indexes its name table by, so the thunks and
the loader cannot disagree about which image is which.
"""
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
DEFINES = ["-DTREK_OVERLAYS", "-DTREK_OVL_ENEMY", "-DTREK_OVL_MOVE",
           "-DTREK_OVL_REU"]

SHT_SYMTAB, SHT_RELA = 2, 4
STT_FUNC = 2
R_MOS_ADDR16, R_MOS_ADDR16_LO, R_MOS_ADDR16_HI = 3, 4, 5
JSR, JMP = 0x20, 0x4C
THUNK = 6


def overlay_ids(names):
    """{name: id}, evaluated by the preprocessor from core/overlay.h."""
    src = '#include "core/overlay.h"\n' + "".join(
        "@%s@ OVL_%s\n" % (n, n.upper()) for n in names)
    out = subprocess.run(["cc", "-E", "-P", "-I", ROOT] + DEFINES + ["-"],
                         input=src, capture_output=True, text=True, cwd=ROOT)
    if out.returncode:
        sys.exit("reu_thunks: preprocessing core/overlay.h failed:\n" + out.stderr)
    ids = {}
    for m in re.finditer(r"@(\w+)@\s+(.+)", out.stdout):
        expr = m.group(2).strip()
        if not re.fullmatch(r"[0-9()+ ]+", expr):
            sys.exit("reu_thunks: OVL_%s is not a constant here: %r"
                     % (m.group(1).upper(), expr))
        ids[m.group(1)] = eval(expr)
    missing = [n for n in names if n not in ids]
    if missing:
        sys.exit("reu_thunks: no id in core/overlay.h for %s" % missing)
    if len(set(ids.values())) != len(ids):
        sys.exit("reu_thunks: two overlays share an id: %s" % ids)
    return ids


class Elf(object):
    def __init__(self, path):
        self.path = path
        self.b = bytearray(open(path, "rb").read())
        if self.b[:4] != b"\x7fELF" or self.b[4] != 1 or self.b[5] != 1:
            sys.exit("reu_thunks: %s is not a little-endian ELF32" % path)
        shoff, = struct.unpack_from("<I", self.b, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", self.b, 0x2E)
        self.sh = []
        for i in range(shnum):
            f = struct.unpack_from("<10I", self.b, shoff + i * shentsize)
            self.sh.append(dict(name=f[0], type=f[1], flags=f[2], addr=f[3],
                                offset=f[4], size=f[5], link=f[6], info=f[7],
                                entsize=f[9]))
        strtab = self.sh[shstrndx]
        for s in self.sh:
            s["nm"] = self.cstr(strtab["offset"] + s["name"])
        self.syms = []
        for s in self.sh:
            if s["type"] == SHT_SYMTAB:
                names = self.sh[s["link"]]["offset"]
                for o in range(s["offset"], s["offset"] + s["size"], 16):
                    nm, val, sz, info, other, shndx = struct.unpack_from(
                        "<IIIBBH", self.b, o)
                    self.syms.append(dict(name=self.cstr(names + nm), value=val,
                                          size=sz, type=info & 0xF, shndx=shndx))

    def cstr(self, o):
        return bytes(self.b[o:self.b.index(0, o)]).decode()

    def relocs(self):
        """(site section index, r_offset, type, symbol, addend) for all RELA."""
        for s in self.sh:
            if s["type"] != SHT_RELA:
                continue
            for o in range(s["offset"], s["offset"] + s["size"], 12):
                off, info, add = struct.unpack_from("<IIi", self.b, o)
                yield s["info"], off, info & 0xFF, self.syms[info >> 8], add

    def at(self, secidx, addr):
        """File offset of VMA `addr` inside section `secidx`."""
        s = self.sh[secidx]
        if not s["addr"] <= addr < s["addr"] + s["size"]:
            raise ValueError("$%04X is outside %s" % (addr, s["nm"]))
        return s["offset"] + addr - s["addr"]


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__.strip().splitlines()[2])
    elf = Elf(sys.argv[1])
    idx = {s["nm"]: i for i, s in enumerate(elf.sh)}
    ovl = {i: s["nm"][5:] for i, s in enumerate(elf.sh)
           if s["nm"].startswith(".ovl_")}
    ids = overlay_ids(sorted(ovl.values()))

    def sym(name):
        hits = [s for s in elf.syms if s["name"] == name]
        if len(hits) != 1:
            sys.exit("reu_thunks: expected one symbol %s, found %d"
                     % (name, len(hits)))
        return hits[0]

    far = sym("ovl_far")["value"]
    t0, t1 = sym("__ovl_thunks")["value"], sym("__ovl_thunks_end")["value"]
    text = sym("__ovl_thunks")["shndx"]

    # Every function in every overlay, by (section, start address).
    starts = {}
    for s in elf.syms:
        if s["type"] == STT_FUNC and s["shndx"] in ovl and s["size"]:
            starts[(s["shndx"], s["value"])] = s["name"]

    sites, bad, tables = {}, [], 0
    for site_sec, off, rtype, s, add in elf.relocs():
        tsec = s["shndx"]
        if tsec not in ovl or site_sec == tsec:
            continue                      # not into an overlay, or intra
        if not (elf.sh[site_sec]["flags"] & 2):
            continue                      # SHF_ALLOC: not a loaded section
        target = s["value"] + add
        fn = starts.get((tsec, target))
        where = "%s+$%04X" % (elf.sh[site_sec]["nm"], off)
        if fn is None:
            # A SWITCH'S JUMP TABLE: llvm-mos puts it in resident .rodata,
            # as split low/high byte tables pointing into the middle of the
            # function that switches. Only that function reads it, while it
            # runs, so its overlay is the one in the window -- the disk build
            # has depended on exactly this since the first overlay. From
            # CODE, a reference into the middle of another section's function
            # has no such story, and is refused.
            if not (elf.sh[site_sec]["flags"] & 4):     # SHF_EXECINSTR
                tables += 1
                continue
            bad.append("%s jumps into the middle of %s at $%04X"
                       % (where, elf.sh[tsec]["nm"], target))
            continue
        if rtype != R_MOS_ADDR16:
            bad.append("%s takes the address of %s (%s) -- a pointer would "
                       "bypass the thunk" % (where, fn, elf.sh[tsec]["nm"]))
            continue
        fo = elf.at(site_sec, off)
        op = elf.b[fo - 1]
        if op not in (JSR, JMP):
            bad.append("%s holds the address of %s in something other than a "
                       "JSR or JMP (opcode $%02X)" % (where, fn, op))
            continue
        have = elf.b[fo] | elf.b[fo + 1] << 8
        if have != target:
            bad.append("%s: the operand is $%04X, the relocation says $%04X "
                       "-- this script has misread the ELF" % (where, have, target))
            continue
        sites.setdefault((tsec, target, fn), []).append((site_sec, off))
    if bad:
        sys.exit("reu_thunks: REFUSING TO PATCH\n  " + "\n  ".join(bad))

    slots = (t1 - t0) // THUNK
    callees = sorted(sites, key=lambda k: (ids[ovl[k[0]]], k[2]))
    if len(callees) > slots:
        sys.exit("reu_thunks: %d callees need thunks and __ovl_thunks has %d "
                 "slots -- raise NTHUNKS in src/reuovl.s" % (len(callees), slots))

    ncalls = 0
    for k, (tsec, target, fn) in enumerate(callees):
        thunk = t0 + k * THUNK
        o = elf.at(text, thunk)
        elf.b[o:o + THUNK] = bytes([JSR, far & 0xFF, far >> 8,
                                    ids[ovl[tsec]], target & 0xFF, target >> 8])
        for site_sec, off in sites[(tsec, target, fn)]:
            fo = elf.at(site_sec, off)
            elf.b[fo:fo + 2] = bytes([thunk & 0xFF, thunk >> 8])
            ncalls += 1
    open(elf.path, "wb").write(elf.b)
    print("reu_thunks: %d thunks of %d, %d calls patched; %d jump-table "
          "entries left alone" % (len(callees), slots, ncalls, tables))
    for tsec, target, fn in callees:
        print("    %-7s %2d  %s" % (ovl[tsec], ids[ovl[tsec]], fn))


if __name__ == "__main__":
    main()

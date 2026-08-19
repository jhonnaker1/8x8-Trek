#!/usr/bin/env python3
"""Disassemble 16-bit code out of the original EGA Trek executable.

Binary Ninja Cloud loads this file flat, with the MZ header unparsed and
almost nothing marked as code, so every function has to be defined by hand
through the UI. This does the same job locally and scriptably.

Addresses here are RAW FILE OFFSETS, which is also what Binary Ninja shows
for this file (confirmed against four known strings), so they can be pasted
straight into its go-to box.

Far calls encode an unrelocated seg:off pair. The load module starts
LOAD_BASE bytes into the file, so the file offset of a far target is
(seg << 4) + off + LOAD_BASE. Those get annotated inline.

    python3 tools/dis16.py <addr> [count]
"""
import glob
import os
import sys

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_16
except ImportError:
    # capstone is not installable system-wide here (PEP 668), so it lives in
    # a venv. Glob for site-packages rather than pinning a Python version.
    venv = os.environ.get("CAPSTONE_VENV", "")
    hits = glob.glob(os.path.join(venv, "lib", "python*", "site-packages")) if venv else []
    if not hits:
        sys.exit("capstone not found. Create a venv, pip install capstone, "
                 "and point CAPSTONE_VENV at it.")
    sys.path.insert(0, hits[0])
    from capstone import Cs, CS_ARCH_X86, CS_MODE_16

EXE = "reference/EGATREK_unpacked.exe"
LOAD_BASE = 1248 * 16          # MZ header size, stripped at load time
RUNTIME_LO = 0x2239A + LOAD_BASE   # Turbo Pascal runtime band, see tp_labels.csv
RUNTIME_HI = 0x2899F + LOAD_BASE


def labels():
    out = {}
    try:
        for line in open("tp_labels.csv"):
            name, _lm, fo = line.strip().split(",")
            if name != "name":
                out[int(fo, 16)] = name
    except OSError:
        pass
    return out


def run(start, count=80):
    code = open(EXE, "rb").read()
    known = labels()
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    md.detail = False
    shown = 0
    for ins in md.disasm(code[start:start + count * 8], start):
        note = ""
        # 9a = call far ptr16:16, ea = jmp far. Resolve to a file offset.
        if ins.bytes[0] in (0x9A, 0xEA):
            off = int.from_bytes(ins.bytes[1:3], "little")
            seg = int.from_bytes(ins.bytes[3:5], "little")
            tgt = (seg << 4) + off + LOAD_BASE
            name = known.get(tgt)
            if name:
                note = "  ; -> %s  (runtime)" % name
            elif RUNTIME_LO <= tgt <= RUNTIME_HI:
                note = "  ; -> 0x%06X  (runtime, unnamed)" % tgt
            else:
                note = "  ; -> 0x%06X  GAME CODE" % tgt
        print("0x%06X  %-22s %-30s%s"
              % (ins.address, ins.bytes.hex(' '), ins.mnemonic + " " + ins.op_str, note))
        shown += 1
        if shown >= count:
            break
        if ins.mnemonic in ("ret", "retf"):
            break


if __name__ == "__main__":
    run(int(sys.argv[1], 0), int(sys.argv[2]) if len(sys.argv) > 2 else 80)

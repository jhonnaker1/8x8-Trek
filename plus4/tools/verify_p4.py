#!/usr/bin/env python3
"""Everything about the Plus/4 build that can be checked without a machine.

A PORT WITHOUT A `verify` MAKES `make ports` TRUE AND MEANINGLESS -- the CoCo 3
had none for its whole life and the gate reported it green. These are the
claims plus4.ld makes that a linker cannot check for itself.
"""
import re, sys

PRG, MAP = "build/trek4.prg", "build/trek4.map"
LOAD, ROM, WINDOW = 0x1001, 0x8000, 0xCC00

def sections(path):
    out = {}
    for line in open(path):
        m = re.match(r"\s*([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+\d+\s+(\.\S+)\s*$", line)
        if m and m.group(4) not in out:
            out[m.group(4)] = (int(m.group(1), 16), int(m.group(3), 16))
    return out

def main():
    bad = []
    # A GATE THAT CANNOT FIND THE BUILD MUST SAY SO, NOT TRACEBACK. The first
    # version raised FileNotFoundError when a link had failed, which prints a
    # stack trace and -- through a pipe -- can still look like a pass. Check
    # the instrument is armed before reading it.
    try:
        d = open(PRG, "rb").read()
        s = sections(MAP)
    except OSError as e:
        print("verify_p4: nothing to check -- %s" % e)
        print("verify_p4: did the link fail? this gate reports RED for that.")
        return 1
    if len(d) < 16 or not s:
        print("verify_p4: %s is %d bytes and the map has %d sections -- "
              "an empty or partial build" % (PRG, len(d), len(s)))
        return 1

    load = d[0] | (d[1] << 8)
    print("  load address        $%04X" % load)
    if load != LOAD:
        bad.append("PRG loads at $%04X, not $%04X" % (load, LOAD))

    # THE BASIC HEADER'S SYS IS TEXT AND THE ENTRY IS ARITHMETIC. Nothing ties
    # them together but this check: change the header's length and the number
    # inside it silently stops pointing at the code.
    hdr = d[2:14]
    sysno = int(bytes(hdr[5:9]).decode("latin1"))
    entry = LOAD + len(hdr)
    print("  BASIC header SYS    %d, entry $%04X (%d)" % (sysno, entry, entry))
    if sysno != entry:
        bad.append("header says SYS %d, the code starts at %d" % (sysno, entry))

    # AND WHAT IS AT THAT ADDRESS, WHICH THE CHECK ABOVE CANNOT SEE. The SYS
    # number matching `load + header length` is arithmetic, and it stayed true
    # while $100D held the first byte of kernal_load_raw -- RUN jumped into the
    # middle of the KERNAL banking assembly and nothing started. The entry must
    # be a JMP, and it must go where the C runtime actually begins.
    off = 2 + len(hdr)
    if d[off] != 0x4C:
        bad.append("the SYS address holds $%02X, not a JMP ($4C) -- RUN would "
                   "land in whatever section was placed there" % d[off])
    else:
        target = d[off + 1] | (d[off + 2] << 8)
        runtime = min((v[0] for n, v in s.items()
                       if n in (".text",) and v[1]), default=0)
        print("  entry               JMP $%04X" % target)
        if runtime and target != runtime:
            bad.append("the entry jumps to $%04X but .text starts at $%04X"
                       % (target, runtime))

    # THE WHOLE MEMORY-MAP ARGUMENT IN TWO LINES. Above $8000 the ROM hides the
    # program during a KERNAL call; .lowtext makes the call and .lowbss holds
    # its operands, so both must stay visible or the call returns into shadow.
    for name in (".lowtext", ".lowbss"):
        if name not in s or not s[name][1]:
            bad.append("%s is absent -- the KERNAL window has nowhere to live" % name)
            continue
        vma, size = s[name]
        end = vma + size
        print("  %-18s $%04X..$%04X" % (name, vma, end - 1))
        if end > ROM:
            bad.append("%s ends $%04X, above $%04X -- hidden during a KERNAL call"
                       % (name, end, ROM))

    resident = [(n, v) for n, v in s.items()
                if v[1] and n not in (".lowtext", ".lowbss") and not n.startswith(".ovl")]
    top = max(v[0] + v[1] for _, v in resident)
    print("  resident top        $%04X" % top)
    print("  window              $%04X" % WINDOW)
    if top > WINDOW:
        bad.append("the resident image reaches $%04X, into the overlay window at $%04X"
                   % (top, WINDOW))
    else:
        print("  spare               %d bytes" % (WINDOW - top))

    for b in bad:
        print("verify_p4: %s" % b)
    if bad:
        return 1
    print("verify_p4: %d checks, all pass" % (3 + len(resident)))
    return 0

if __name__ == "__main__":
    sys.exit(main())

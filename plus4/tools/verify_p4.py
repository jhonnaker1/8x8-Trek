#!/usr/bin/env python3
"""Everything about the Plus/4 build that can be checked without a machine.

A PORT WITHOUT A `verify` MAKES `make ports` TRUE AND MEANINGLESS -- the CoCo 3
had none for its whole life and the gate reported it green. These are the
claims plus4.ld makes that a linker cannot check for itself.
"""
import os, re, subprocess, sys

PRG, MAP = "build/trek4.prg", "build/trek4.map"
ELF = "build/trek4.elf"
NM  = os.path.expanduser("~/llvm-mos/bin/llvm-nm")
LOAD, ROM, WINDOW = 0x1001, 0x8000, 0xCC00

def sections(path):
    out = {}
    for line in open(path):
        m = re.match(r"\s*([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+\d+\s+(\.\S+)\s*$", line)
        if m and m.group(4) not in out:
            out[m.group(4)] = (int(m.group(1), 16), int(m.group(3), 16))
    return out

def symbols():
    """__stack COMES FROM THE BINARY NOW, not from a regex over plus4.ld.

    It used to be `__stack = 0x0500` -- a literal this gate could scrape out
    of the link script. It is a section-derived symbol now, so the script no
    longer states an address anywhere and scraping it would find nothing and
    report a PASS for a build with no stack at all. Ask the ELF."""
    out = {}
    for line in subprocess.run([NM, ELF], capture_output=True, text=True).stdout.splitlines():
        f = line.split()
        if len(f) == 3 and re.fullmatch(r"[0-9a-fA-F]+", f[0]):
            out[f[2]] = int(f[0], 16)
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
    hdr = d[2:15]
    sysno = int(bytes(hdr[6:10]).decode("latin1"))
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

    # THE SOFT STACK MUST NOT BE INSIDE A LOADED SECTION, and nothing but this
    # check can see it. __stack was $7F00 while .text ran $1055..$9908: the
    # stack grew down into the program's own code and killed it on the first
    # call. The link succeeded, every size fitted, and the failure surfaced as
    # `?SYNTAX ERROR` from BASIC.
    sym = symbols()
    stk, bot = sym.get("__stack"), sym.get("__stack_bottom")
    if stk is None or bot is None:
        bad.append("the binary defines no __stack/__stack_bottom -- plus4.ld "
                   "must reserve a .stack section, not nominate an address")
    else:
        print("  __stack             $%04X (%d bytes, $%04X..$%04X)"
              % (stk, stk - bot, bot, stk - 1))

        # THE STACK MUST BE LINKER-OWNED, and this is the check that says so.
        # Every previous home was an address somebody believed was free, and
        # the last one -- $0500 down into $0400 -- sat on CHRGET, INDSUB and
        # the IND* fetch routines the ROM calls. A reserved section cannot be
        # wrong about what is free; an address can, and was, three times.
        if bot < LOAD:
            bad.append("__stack_bottom $%04X is below the load address -- the "
                       "stack is at a nominated address, not in a reserved "
                       "section" % bot)
        if stk > ROM:
            bad.append("__stack $%04X is above $%04X -- a write there passes "
                       "through the ROM and A READ RETURNS ROM" % (stk, ROM))
        # THE SENTINEL FILL IS UNROLLED TO A FIXED NUMBER OF PAGES in
        # p4bank.c. If .stack grows and that fill does not, the top of the
        # stack is never filled and the high-water reading silently
        # under-reports -- the measurement would break quietly, which is the
        # one failure this project keeps meeting.
        if (stk - bot) != 0x800:
            bad.append(".stack is %d bytes but p4bank.c's sentinel fills "
                       "2048 -- keep them equal" % (stk - bot))
        if stk - bot < 0x100:
            bad.append("__stack is only %d bytes; the deepest path other ports "
                       "measure is 184" % (stk - bot))
        for n, (vma, size) in s.items():
            # NOT-LOADED SECTIONS ARE NOT MEMORY. `.symtab`, `.strtab` and
            # `.shstrtab` sit at VMA 0 in the map and cover thousands of
            # bytes; counting them made this check report `__stack $0800 is
            # inside .symtab ($0000..$193F)` the moment the stack moved below
            # the program, which is a FALSE POSITIVE on the one layout that
            # fixes the startup fault. A gate that cries wolf about a correct
            # build is worse than no gate. Nothing in this port loads at 0.
            if vma == 0 or n in (".stack",):
                continue
            # __stack IS THE EXCLUSIVE TOP -- the first push is at __stack-1,
            # so testing __stack itself flagged the section that starts right
            # above the reserved stack and failed a correct build. Test the
            # bytes the stack actually occupies: __stack_bottom..__stack-1.
            lo, hi = bot, stk - 1
            if size and not n.startswith(".ovl") and lo <= vma + size - 1 and vma <= hi:
                bad.append("the stack ($%04X..$%04X) overlaps %s ($%04X..$%04X)"
                           % (lo, hi, n, vma, vma + size - 1))

    for b in bad:
        print("verify_p4: %s" % b)
    if bad:
        return 1
    # THE SUMMARY LINES ARE IN THE SHARED SHAPE ON PURPOSE. tools/check_ports.py
    # surfaces a passing port's numbers by splitting each line on "verify:" and
    # matching its KEEP list -- so "verify_p4:" does NOT match, and this port
    # would have joined the gate printing nothing but `ok`. The numbers are the
    # half of that gate that catches figures going stale while everything still
    # compiles; a port that shows none of them is in the list without being in
    # the check.
    print("plus4 verify: resident $%04X..$%04X, %d bytes spare below the window "
          "at $%04X" % (LOAD, top, WINDOW - top, WINDOW))
    if stk is not None and bot is not None:
        print("plus4 verify: soft stack $%04X..$%04X, %d bytes reserved "
              "(measured demand 50)" % (bot, stk - 1, stk - bot))
    print("verify_p4: %d checks, all pass" % (3 + len(resident)))
    return 0

if __name__ == "__main__":
    sys.exit(main())

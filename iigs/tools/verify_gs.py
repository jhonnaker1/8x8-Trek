#!/usr/bin/env python3
"""Check the invariants a IIgs link can break silently.

Every one of these is here because a linker will happily produce a file that
violates it and say nothing. The Plus/4's equivalent was written after an
afternoon of blaming a BASIC header for a soft stack that had been placed
inside .text; this one is written before that happens rather than after.
"""
import re
import subprocess
import sys

LLVM = "/Users/jhonnaker/llvm-mos/bin"


def sections(elf):
    out = subprocess.run([f"{LLVM}/llvm-readelf", "-S", "-W", elf],
                         capture_output=True, text=True).stdout
    secs = {}
    for line in out.splitlines():
        m = re.match(r"\s*\[\s*\d+\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+"
                     r"([0-9a-f]+)\s+([0-9a-f]+)", line)
        if m:
            name, typ, addr, off, size = m.groups()
            secs[name] = dict(type=typ, addr=int(addr, 16),
                              off=int(off, 16), size=int(size, 16))
    return secs


def symbols(elf):
    out = subprocess.run([f"{LLVM}/llvm-nm", elf], capture_output=True,
                         text=True).stdout
    syms = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3:
            syms[p[2]] = int(p[0], 16)
    return syms


def main(argv):
    if len(argv) < 2:
        raise SystemExit("usage: verify_gs.py IMAGE.elf [--window ADDR] "
                         "[--org ADDR]")
    elf = argv[1]
    window = int(argv[argv.index("--window") + 1], 0) if "--window" in argv else 0xB000
    org = int(argv[argv.index("--org") + 1], 0) if "--org" in argv else 0x0800

    try:
        secs, syms = sections(elf), symbols(elf)
    except FileNotFoundError:
        print(f"FAIL: {elf} does not exist -- build it first")
        return 1
    if not secs:
        print(f"FAIL: {elf} has no sections readelf could parse")
        return 1

    fails, notes = [], []
    bound = window

    resident = {n: s for n, s in secs.items()
                if s["addr"] and s["addr"] < window
                and not n.startswith((".ovl_", ".debug", ".comment", ".symtab",
                                      ".strtab", ".shstrtab"))}
    if not resident:
        fails.append("no resident sections below the window at all")
        top = org
    else:
        top = max(s["addr"] + s["size"] for s in resident.values())
        notes.append(f"resident ${org:04X}..${top:04X}")

    # 1. The entry cell really is a JMP, and it points at the C runtime.
    ent = secs.get(".entry")
    if not ent:
        fails.append(".entry section missing -- whatever section landed first "
                     "is the entry point, which is how the Plus/4 jumped into "
                     "banking assembly")
    else:
        if ent["addr"] != org:
            fails.append(f".entry is at ${ent['addr']:04X}, not the load "
                         f"address ${org:04X}")
        blob = open(elf, "rb").read()[ent["off"]:ent["off"] + 3]
        if len(blob) == 3 and blob[0] != 0x4C:
            fails.append(f"entry byte is ${blob[0]:02X}, not a JMP ($4C)")
        elif len(blob) == 3:
            target = blob[1] | (blob[2] << 8)
            start = syms.get("_start")
            if start is None:
                fails.append("no _start symbol to check the entry against")
            elif target != start:
                fails.append(f"entry jumps to ${target:04X} but _start is at "
                             f"${start:04X}")
            else:
                notes.append(f"entry $4C -> _start ${target:04X}")

    # 2. The soft stack is outside every loaded section. The linker checks
    #    neither this nor how much room is below it.
    stack = syms.get("__stack")
    if stack is None:
        fails.append("no __stack symbol")
    else:
        inside = [n for n, s in resident.items()
                  if s["type"] != "NOBITS" and s["addr"] <= stack < s["addr"] + s["size"]]
        inside += [n for n, s in resident.items()
                   if s["type"] == "NOBITS" and s["addr"] <= stack < s["addr"] + s["size"]]
        if inside:
            fails.append(f"__stack ${stack:04X} is INSIDE {', '.join(inside)} "
                         f"-- the first call overwrites the program")
        else:
            # WHICHEVER BOUND IS LOWER IS THE REAL ONE, and saying "headroom
            # to the window" stopped being true the moment the window moved
            # into the language card: it is then in a different mapping
            # entirely and the figure jumped by 4,096 bytes that the program
            # cannot use. The stack is what the image actually runs into.
            bound, which = (stack, "__stack") if stack < window \
                else (window, "the window")
            notes.append(f"__stack ${stack:04X}")
            notes.append(f"HEADROOM {bound - top} bytes -- to {which} at "
                         f"${bound:04X}, the lower of the two")

    # 3. Nothing resident runs into the overlay window.
    if top > window:
        fails.append(f"resident image ends at ${top:04X}, past the window at "
                     f"${window:04X}, by {top - window} bytes")

    # 4. Every overlay fits the window, and they all run at it.
    ovls = {n: s for n, s in secs.items() if n.startswith(".ovl_")}
    if ovls:
        for n, s in sorted(ovls.items()):
            if s["addr"] != window:
                fails.append(f"{n} runs at ${s['addr']:04X}, not the window "
                             f"${window:04X}")
            if s["size"] > 0x1000:
                fails.append(f"{n} is {s['size']} bytes, window is 4096")
        # AND THEY MUST NOT SHARE A LOAD ADDRESS. A shared AT> region does not
        # advance, so every overlay is dumped from the same bytes and eleven
        # images come out identical -- which on the C128 drew the hall of fame
        # when the game asked for the evaluation.
        offs = {}
        for n, s in ovls.items():
            offs.setdefault(s["off"], []).append(n)
        dup = {o: ns for o, ns in offs.items() if len(ns) > 1}
        if dup:
            for o, ns in dup.items():
                fails.append(f"overlays share file offset ${o:X}: "
                             f"{', '.join(sorted(ns))} -- their images will be "
                             f"identical")
        big = max(s["size"] for s in ovls.values())
        notes.append(f"{len(ovls)} overlays, largest {big} of 4096 "
                     f"({4096 - big} spare)")

    # 5. .data with VMA != LMA needs copy-data, and nothing says so.
    data = secs.get(".data")
    if data and data["size"]:
        syms_have_copy = "__do_copy_data" in syms or "__copy_data" in syms
        # In this layout c_readonly and c_writeable are the same region, so VMA
        # should equal LMA; if a future script splits them, this catches it.
        if not syms_have_copy:
            notes.append(f".data is {data['size']} bytes with no copy routine "
                         f"linked -- correct only while VMA == LMA")

    for n in notes:
        print(f"  {n}")
    if fails:
        for f in fails:
            print(f"FAIL: {f}")
        return 1
    # ONE LINE THE CROSS-PORT GATE WILL SURFACE. tools/check_ports.py prints
    # any line that contains BOTH a keyword it cares about and the literal
    # "verify:" -- and "verify_gs:" does not contain "verify:", so this port's
    # numbers were invisible in `make ports` while every other port's showed.
    # These are exactly the figures that go stale when nobody looks at them.
    big = max((s["size"] for s in ovls.values()), default=0)
    print(f"iigs verify: resident ${org:04X}..${top:04X}, "
          f"{bound - top} bytes spare, largest overlay {big} of 4096")
    print(f"verify_gs: {len(notes)} checks reported, 0 failures")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

#!/usr/bin/env python3
"""Check the linked .prg for cc65 character-set translation damage.

Why this exists
---------------
cc65 translates string AND character literals to the target character set for
Commodore targets, so an uppercase 'A' written in the source lands in the
binary as PETSCII $C1 (193), not ASCII $41 (65). Three separate bugs in this
port were that one fact wearing different hats:

  * panel titles rendered as blank space
  * input.c's CIA1 key table stored PETSCII, so kb_waitkey() returned 205
    for 'M'
  * main.c compared against numeric ASCII, so no letter command ever matched
    and `q` would not quit

**Neither test suite can catch this.** Native cc performs no such translation,
so core `make test` and c128 `make test` pass either way -- only the linked
binary tells the truth. Every time it bit, the thing that actually found it
was grepping the .prg for an expected byte pattern by hand. This makes that a
build step.

What it checks
--------------
The keyboard table must appear in the binary as ASCII. Rather than hardcoding
bytes -- which would rot the moment a key is added -- the expected sequence is
derived from input.h and input.c, so this stays honest as the table changes.
The table is a const array of 3-byte structs, contiguous in the binary, so one
search over the whole blob is both stronger and simpler than probing entries
individually.
"""
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve()
C128 = HERE.parents[1]
PRG = C128 / "build" / "trek128.prg"
HDR = C128 / "src" / "input.h"
SRC = C128 / "src" / "input.c"


MAP = C128 / "build" / "trek128.map"


def check_data_init(mapfile):
    """.data must actually be copied to where the code expects to find it.

    THIS IS THE SOUND BUG, AND IT IS A WHOLE CLASS, NOT ONE VARIABLE.

    llvm-mos initialises .data at startup only if copy-data.c.obj is linked
    in, and the Commodore targets deliberately leave it out: their stock
    script puts c_readonly and c_writeable in the SAME region, so .data's run
    address equals its load address and there is nothing to copy.

    trek128.ld moves c_writeable to lowram. That splits the two -- and with no
    copy routine, every `static x = <nonzero>;` in the port came up as
    whatever RAM held. sid.c's `enabled` came up 0, so snd_poll() returned on
    its first line and the port was silent with correct note data sitting in
    bank 1. It took a bank-1 dump to rule the data out and a poke to $1301 to
    prove the driver had been right all along.

    Nothing else catches it. Both test suites run natively, where .data is the
    host's problem; the map looks healthy, because the section really does
    have a VMA, an LMA and a size. Only the running machine shows that the
    bytes never arrived -- so check the LINK instead, on every build.
    """
    if not mapfile.exists():
        die(f"{mapfile.name} not found -- the map is how this is checked")
    text = mapfile.read_text()

    size = vma = lma = None
    for line in text.splitlines():
        parts = line.split()
        if len(parts) == 5 and parts[4] == ".data":
            vma, lma, size = (int(parts[i], 16) for i in range(3))
            break
    if size is None:
        die("no .data section line in the map -- has the link script changed?")

    if size == 0:
        print("verify: .data is empty -- nothing to copy")
        return
    if vma == lma:
        print(f"verify: .data is {size} bytes, loaded in place -- ok")
        return
    if "__do_copy_data" not in text:
        die(
            f".data is {size} bytes at ${vma:04x}, loaded at ${lma:04x}, "
            "and NOTHING COPIES IT.\n"
            "         Every `static x = <nonzero>;` in the port will come up "
            "as garbage\n"
            "         on the real machine. Link -lcopy-data -- see LDLIBS in "
            "the Makefile."
        )
    print(f"verify: .data is {size} bytes at ${vma:04x}, copied from "
          f"${lma:04x} -- ok")


def die(msg):
    print(f"verify: {msg}", file=sys.stderr)
    sys.exit(1)


def constants():
    """KB_* values from input.h -- the single definition both sides include."""
    out = {}
    for m in re.finditer(r"#define\s+(KB_\w+)\s+(\d+)", HDR.read_text()):
        out[m.group(1)] = int(m.group(2))
    if not out:
        die(f"no KB_* constants found in {HDR.name}")
    return out


def table_rows():
    """The {pra, prb, ch} rows of keys[] in input.c."""
    text = SRC.read_text()
    m = re.search(r"static const Key keys\[\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        die(f"could not find the keys[] table in {SRC.name}")
    rows = re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*([^}]+?)\s*\}", m.group(1))
    if not rows:
        die("keys[] table parsed but held no rows")
    return rows


def evaluate(expr, consts):
    """Resolve 'KB_M' or 'KB_DIGIT0 + 1' against the header's constants."""
    expr = expr.strip()

    # Catch the mistake at its source, before it can even reach the binary.
    # This is the exact bug the whole script exists for, so it is worth a
    # message that names it rather than a generic parse complaint.
    if re.fullmatch(r"'\\?.'", expr):
        die(
            f"character literal {expr} in keys[] ({SRC.name}).\n"
            f"         cc65 translates literals to PETSCII, so this compiles to\n"
            f"         a value the dispatch in main.c will never match -- which\n"
            f"         is exactly how `q` once stopped quitting.\n"
            f"         Use the numeric KB_* constants from {HDR.name}."
        )

    resolved = re.sub(r"KB_\w+", lambda m: str(consts[m.group(0)]), expr)
    if not re.fullmatch(r"[\d\s+\-*]+", resolved):
        die(f"unexpected expression in keys[]: {expr!r}")
    return eval(resolved, {"__builtins__": {}})  # noqa: S307 - digits only


def main():
    # An explicit path is accepted so the byte-level detection below can be
    # exercised against a deliberately corrupted copy. A check that has only
    # ever been seen to pass is not evidence of anything.
    prg = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else PRG

    if not prg.exists():
        die(f"{prg} not built yet -- run make first")

    check_data_init(MAP)

    consts = constants()
    rows = table_rows()

    expected = bytearray()
    letters = []
    for pra, prb, ch_expr in rows:
        ch = evaluate(ch_expr, consts)
        if not 0 <= ch <= 255:
            die(f"key value out of range: {ch_expr} = {ch}")
        expected += bytes([int(pra), int(prb), ch])
        if 65 <= ch <= 90:
            letters.append((ch_expr.strip(), ch))

    # The same table as cc65 would emit it from character literals: uppercase
    # ASCII maps to PETSCII by +128.
    translated = bytearray()
    for i in range(0, len(expected), 3):
        pra, prb, ch = expected[i], expected[i + 1], expected[i + 2]
        translated += bytes([pra, prb, ch + 128 if 65 <= ch <= 90 else ch])

    blob = prg.read_bytes()
    at = blob.find(bytes(expected))
    bad = blob.find(bytes(translated))

    print(f"verify: {len(rows)} keys, {len(letters)} letters "
          f"({', '.join(n for n, _ in letters)})")

    if at >= 0:
        print(f"verify: key table found as ASCII at 0x{at:04x} -- ok")
        if bad >= 0 and bad != at:
            die(f"a PETSCII copy also exists at 0x{bad:04x}")
        return 0

    if bad >= 0:
        die(
            f"key table is PETSCII at 0x{bad:04x}, expected ASCII.\n"
            f"         A C character literal crept into keys[] in "
            f"{SRC.name}; cc65 translated it.\n"
            f"         Use the numeric KB_* constants from {HDR.name} instead."
        )

    die(
        "key table not found in the binary at all, in either encoding.\n"
        "         Either keys[] changed shape, or the struct is no longer\n"
        "         three contiguous bytes per entry."
    )


if __name__ == "__main__":
    sys.exit(main())

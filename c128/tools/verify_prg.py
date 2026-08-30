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
import subprocess
import sys
from pathlib import Path

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


def check_overlays(mapfile):
    """Every overlay must run at the window and load from its OWN address.

    THIS IS A REAL BUG THAT SHIPPED FOR TEN MINUTES, and it was silent at
    every stage. Overlays deliberately share a RUN address -- that is the
    whole idea -- and the first version gave them all one staging region to
    take their LOAD addresses from. lld does not advance a region's pointer
    for a section that also carries an explicit run address, so both overlays
    came out at load address $10000, occupied the same bytes of the ELF, and
    llvm-objcopy dumped the SAME image twice under two names. The link was
    silent, the extraction was silent, the disk looked right, and the game
    drew the hall of fame when it asked for the evaluation.

    So: same VMA is required, distinct LMAs are required, and neither is
    something a human will notice in a map file.
    """
    text = mapfile.read_text()
    ovl, window = [], None
    for line in text.splitlines():
        parts = line.split()
        if len(parts) == 5 and parts[4].startswith(".ovl_"):
            vma, lma, size = (int(parts[i], 16) for i in range(3))
            ovl.append((parts[4], vma, lma, size))
        elif len(parts) == 5 and parts[4] == "window" or (
                len(parts) >= 3 and parts[-1] == "__ovl_start"):
            pass
    if not ovl:
        print("verify: no overlays in this build")
        return

    vmas = {v for _, v, _, _ in ovl}
    if len(vmas) != 1:
        die("overlays do not share one run address: "
            + ", ".join("%s at $%04x" % (n, v) for n, v, _, _ in ovl))

    seen = {}
    for name, _, lma, _ in ovl:
        if lma in seen:
            die(f"{name} and {seen[lma]} both LOAD from ${lma:04x}, so they are\n"
                f"         the same bytes of the ELF and llvm-objcopy will dump\n"
                f"         one image twice. Give each overlay its own staging\n"
                f"         region in trek128.ld.")
        seen[lma] = name

    win = max(s for _, _, _, s in ovl)
    print("verify: %d overlays at $%04x, distinct load addresses, largest %d bytes"
          % (len(ovl), ovl[0][1], win))


MSG_W  = 40    # layout.h
MSG_PAD = 2    # msg_box() draws inside the border


def check_message_widths():
    """A message the panel cannot hold is TRUNCATED, and it does not say so.

    msg_box() clamps the text to MSG_W - 2 - len(department) and writes what
    fits. Nothing warns, at build time or at run time. Two shipped that way:

      COMPUTER: with the full command list, 61 characters against 28, so the
        player was shown "M W L T D R S E Q C F A SND" and would reasonably
        conclude that MSGS, SAVE, INFO, HAIL, SHUP, SHDN and MAX do not exist.
        A half-list is worse than no list. Found by Jamie playing it.

      ENGINEERING: IMPULSE ENGINES TOO DAMAGED, 27 against 25, losing "ED".

    The panel draws ONE line per box. The original's boxes hold two and wrap,
    which is a real difference and is written up in NOTES.md -- but with the
    two strings above shortened, nothing in this port needs the second line,
    so the wrap is a feature to build rather than a bug to fix.
    """
    src = SRC.read_text() + (C128 / "src" / "main.c").read_text() \
        + (C128 / "src" / "ui.c").read_text()
    strings = {}
    for ln in (C128 / "src" / "strings.txt").read_text().splitlines():
        if ln[:1].isdigit():
            i, _, t = ln.partition("\t")
            strings[int(i)] = t

    bad = []
    for d, t in re.findall(r"ui_message\(S\(S_(\d+)\),\s*S\(S_(\d+)\)\)", src):
        dept, text = strings[int(d)], strings[int(t)]
        room = MSG_W - MSG_PAD - len(dept)
        if len(text) > room:
            bad.append((dept, text, room))

    if bad:
        lines = "\n".join(
            '         %s%s\n             %d chars, %d fit, shown as "%s"'
            % (d, t, len(t), r, t[:r]) for d, t, r in bad)
        die("message text that the panel will silently truncate:\n" + lines)
    print("verify: every ui_message fits the panel -- ok")


# put_u16 takes a uint16_t, so five digits. The callers that pass a percentage
# are not special-cased: a bound that depends on the caller is a bound nobody
# maintains.
# put_quad is three: both indices are quadrant numbers, bounded by GAL_DIM
# and not by the caller, which is the whole reason it exists as a helper.
COMPOSE_FIXED = {"put_u16": 5, "put_sector": 5, "put_tenths_str": 7,
                 "put_quad": 3}


def check_linebuf():
    """No composed line may overrun the buffer it is composed in.

    FOUND 2026-08-28, on the real machine. `linebuf` was 32 bytes and the
    laser-overheat line is forty:

        "LASERS OVERHEAT. NOW AT " + "88" + "% EFFICIENCY."

    The eight bytes past the end land on log_count, log_head and panel_slot,
    which sit immediately after it in .bss -- so overheating the banks
    silently corrupted the MESSAGE LOG's bookkeeping, and the panel then drew
    a garbage row with a garbage stardate. Three other compositions were over
    the line too and had simply not been triggered in a session.

    Nothing could have caught it earlier. Both test suites run on the host,
    where the same overrun lands on host padding and does nothing; the width
    check above only reads `ui_message(S(a), S(b))`, two literals, and every
    one of these is built at runtime out of pieces.

    NOTHING HERE IS HARDCODED. Widths are derived from the sources -- a
    helper's own `return` statements, a name table's own initialiser -- so
    editing a string cannot leave a stale number behind. An expression this
    cannot bound STOPS THE BUILD rather than being guessed at, because an
    under-count is exactly the failure the check exists to catch.
    """
    src = {}
    for rel in ("c128/src/main.c", "c128/src/ui.c", "core/planet.c"):
        f = C128.parent / rel
        if f.exists():
            src[rel] = f.read_text()
    blob = "\n".join(src.values())

    strings = {}
    for ln in (C128 / "src" / "strings.txt").read_text().splitlines():
        if ln[:1].isdigit():
            i, _, t = ln.partition("\t")
            strings[int(i)] = t

    def sid(name):
        m = re.match(r"S_(\d+)$", name.strip())
        return int(m.group(1)) if m else None

    def array_widths(name):
        """Every width an element of `name[]` can have, from its initialiser."""
        m = re.search(r"\b" + re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\}",
                      blob, re.S)
        if not m:
            return None
        body = m.group(1)
        ids = [strings[int(i)] for i in re.findall(r"\bS_(\d+)\b", body)
               if int(i) in strings]
        lits = [t.encode().decode("unicode_escape")
                for t in re.findall(r'"((?:[^"\\]|\\.)*)"', body)]
        vals = ids + lits
        return max(len(v) for v in vals) if vals else None

    def fn_widths(name, seen):
        """The widest thing `name()` can return, from its own returns."""
        m = re.search(r"\b" + re.escape(name) + r"\s*\([^)]*\)\s*\{", blob)
        if not m:
            return None
        i, depth = m.end() - 1, 0
        for j in range(i, len(blob)):
            if blob[j] == "{":
                depth += 1
            elif blob[j] == "}":
                depth -= 1
                if depth == 0:
                    body = blob[i:j]
                    break
        else:
            return None
        outs = [width(e, name, seen) for e in re.findall(r"return\s+(.+?);", body)]
        outs = [o for o in outs if o is not None]
        return max(outs) if outs else None

    def width(expr, where, seen=()):
        expr = expr.strip()
        if expr in seen:            # a helper that returns a call to itself
            return 0
        seen = tuple(seen) + (expr,)
        m = re.fullmatch(r"S\(S_(\d+)\)", expr)
        if m:
            return len(strings[int(m.group(1))])
        m = re.fullmatch(r'"((?:[^"\\]|\\.)*)"', expr)
        if m:
            return len(m.group(1).encode().decode("unicode_escape"))
        # A ternary: the wider arm.
        if "?" in expr:
            arms = [len(strings[int(a)]) for a in re.findall(r"S\(S_(\d+)\)", expr)]
            if arms:
                return max(arms)
        # S(table[i]) and table[i], both resolved from the initialiser.
        m = re.fullmatch(r"S\(\s*(\w+)\s*\[.*\]\s*\)", expr) \
            or re.fullmatch(r"(\w+)\s*\[.*\]", expr)
        if m:
            w = array_widths(m.group(1))
            if w is not None:
                return w
        m = re.match(r"(\w+)\s*\(", expr)
        if m:
            w = fn_widths(m.group(1), seen)
            if w is not None:
                return w
        die("verify: cannot bound the width of `%s` (in %s).\n"
            "  Teach check_linebuf to resolve it -- do not guess. Guessing is\n"
            "  how the linebuf overrun of 2026-08-28 got in." % (expr, where))

    bad = []
    narrow = []          # composed lines the PANEL will truncate
    for rel in ("c128/src/main.c", "c128/src/ui.c"):
        text = src.get(rel, "")
        m = re.search(r"static char linebuf\[(\d+)\]", text)
        if not m:
            continue
        cap = int(m.group(1))
        lines = text.splitlines()
        total, start = 0, None
        for n, ln in enumerate(text.splitlines(), 1):
            # A switch arm starts a fresh composition: the arms are
            # alternatives, not pieces of one line.
            if re.match(r"\s*(case\b|default\s*:)", ln):
                total, start = 0, None
            if "linebuf" not in ln:
                continue
            if re.search(r"\blinebuf\s*\[[^\]]*\]\s*=\s*0\s*;", ln):
                if start is not None and total + 1 > cap:
                    bad.append((rel, start, n, total + 1, cap))
                # AND THE PANEL, which is the tighter of the two bounds and was
                # not checked at all until 2026-08-29. check_message_widths()
                # above reads `ui_message(S(a), S(b))` only -- two literals --
                # so every line BUILT at runtime was bounded by the buffer it
                # was composed in and by nothing else. The buffer is 64 bytes
                # and the panel holds 40 minus the department label, so a line
                # could pass the check and still be cut on screen. Two were:
                # the hail's own "STARBASE IN 6,6 responds." shipped truncated,
                # and the delayed reply added the same day was caught here
                # rather than by looking at the VDC.
                if start is not None:
                    for look in lines[n:n + 6]:
                        d = re.search(r"ui_message\(\s*S\(S_(\d+)\)\s*,"
                                      r"\s*linebuf\s*\)", look)
                        if not d:
                            continue
                        dept = strings[int(d.group(1))]
                        room = MSG_W - MSG_PAD - len(dept)
                        if total > room:
                            narrow.append((rel, start, n, dept, total, room))
                        break
                total, start = 0, None
                continue
            m = re.search(r"=\s*put_str\(\s*linebuf(?:\s*\+\s*\w+)?\s*,\s*(.+)\)\s*;", ln)
            if m:
                if start is None:
                    start = n
                total += width(m.group(1), "%s:%d" % (rel, n))
                continue
            m = re.search(r"=\s*(put_u16|put_sector|put_tenths_str|put_quad)\(\s*linebuf", ln)
            if m:
                if start is None:
                    start = n
                total += COMPOSE_FIXED[m.group(1)]
                continue
            if re.search(r"linebuf\s*\[\s*\w+\+\+\s*\]\s*=", ln):
                if start is None:
                    start = n
                total += 1

    if bad:
        die("verify: composed lines overrun the buffer they are built in:\n"
            + "\n".join(
                "         %s:%d-%d needs %d bytes, linebuf holds %d"
                % (f, a, b, t, c) for f, a, b, t, c in bad))
    print("verify: every composed line fits linebuf -- ok")

    if narrow:
        die("composed lines the message panel will silently truncate:\n"
            + "\n".join(
                "         %s:%d-%d under %s\n"
                "             needs %d columns, %d fit"
                % (f, a, b, d.strip(), t, r) for f, a, b, d, t, r in narrow))
    print("verify: every composed line fits the panel -- ok")


OBJDUMP = str(Path.home() / "llvm-mos/bin/llvm-objdump")


def check_overlay_calls():
    """No overlay may call INTO another overlay, or load one.

    THERE IS ONE WINDOW. Code in .ovl_X runs at $AFC0 and so does code in
    .ovl_Y, so a call from one to the other lands on whatever is loaded rather
    than on what it names -- and an ovl_load() from inside an overlay
    overwrites the very code making the call. Neither is a link error, neither
    fails a test, and both die on the machine.

    FOUND THE HARD WAY 2026-08-29: fire_one_torpedo was moved to an overlay on
    its own, and it did `ovl_load(OVL_MSGS); report_nova(dmg)` to reach a
    callee an earlier pass had put in the msgs window. It built, it verified,
    all three suites passed, and it would have crashed the first time a torpedo
    hit a star. Caught by reading the call graph, which is not a thing to rely
    on twice.

    AN ADDRESS CANNOT NAME A SECTION HERE -- every overlay starts at $AFC0, so
    $AFC0 belongs to ten different functions at once. The question is not "who
    owns this address" but "does the CALLING section own it": a call inside
    .ovl_X to a window address is fine exactly when some symbol of .ovl_X
    covers it. The first draft of this check asked the first question and
    reported a call that was perfectly correct.
    """
    elf = C128 / "build" / "trek128.elf"
    if not elf.exists():
        return

    syms = subprocess.run([OBJDUMP, "--syms", str(elf)],
                          capture_output=True, text=True).stdout
    spans, lo, hi = {}, None, None
    for ln in syms.splitlines():
        m = re.match(r"^([0-9a-f]{8})\s+\S*\s+F\s+(\.ovl_\w+)\s+([0-9a-f]{8})\s+(\S+)", ln)
        if not m:
            continue
        a0, sec, sz, nm = int(m.group(1), 16), m.group(2), int(m.group(3), 16), m.group(4)
        spans.setdefault(sec, []).append((a0, a0 + sz, nm))
        lo = a0 if lo is None else min(lo, a0)
        hi = max(hi or 0, a0 + sz)
    if not spans:
        return

    # ovl_load is RESIDENT, so its address is unambiguous -- and a call to it
    # from inside a window is always fatal.
    loader = None
    for ln in syms.splitlines():
        m = re.match(r"^([0-9a-f]{8})\s+\S*\s+F\s+\.text\s+[0-9a-f]{8}\s+ovl_load$", ln)
        if m:
            loader = int(m.group(1), 16)

    stray, loads = [], []
    for sec, own in sorted(spans.items()):
        d = subprocess.run([OBJDUMP, "-d", "--section=" + sec, str(elf)],
                           capture_output=True, text=True).stdout
        here = None
        for ln in d.splitlines():
            m = re.match(r"^\s*([0-9a-f]+)\s+<(\S+)>:", ln)
            if m:
                here = m.group(2)
                continue
            m = re.search(r"\b(jsr|jmp)\s+\$([0-9a-f]{4})\b", ln)
            if not m:
                continue
            tgt = int(m.group(2), 16)
            if loader is not None and tgt == loader:
                loads.append((sec, here))
                continue
            if not (lo <= tgt < hi):
                continue                                  # resident: fine
            if not any(a0 <= tgt < a1 for a0, a1, _ in own):
                elsewhere = sorted({s2 for s2, o2 in spans.items()
                                    for a0, a1, _ in o2 if a0 <= tgt < a1} - {sec})
                stray.append((sec, here, "$%04x" % tgt, ", ".join(elsewhere) or "nothing"))

    if loads:
        die("an overlay calls ovl_load, which overwrites the code making the\n"
            "         call. Move the callee into this window instead:\n"
            + "\n".join("         %s:%s" % l for l in loads))
    if stray:
        die("overlay code calls a window address its own section does not\n"
            "         own -- it will land on whatever is loaded:\n"
            + "\n".join("         %s:%s -> %s (lives in %s)" % t for t in stray))
    print("verify: no overlay calls out of its own window -- ok")


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
    check_overlays(MAP)
    check_message_widths()
    check_linebuf()
    check_overlay_calls()

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

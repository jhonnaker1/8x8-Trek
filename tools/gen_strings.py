#!/usr/bin/env python3
"""Pool the prose: move string literals out of the C128 binary.

The C128 has about 42K for code and read-only data TOGETHER, and 4,088 bytes
of it were literals. This lifts them into a file that is streamed into bank 1
at startup, leaving the binary with IDs.

WHAT IT REWRITES, AND WHAT IT WILL NOT TOUCH. Only literals that appear as a
direct argument to one of DRAW_FNS below. That rule exists because a literal
in a static initialiser -- `static const char *const rank_name[] = {"...",}`
-- cannot become a function call, and a script that rewrote those would
produce code that does not compile, or worse, compiles and returns the wrong
buffer. Anything else is left exactly as it is.

THE LIST IS THE SOURCE OF TRUTH. c128/src/strings.txt holds id and text, and
it IS committed, so a clone with no reference/ can still build STRINGS.DAT and
get a game with words in it.

WHAT THIS PROSE ACTUALLY IS, corrected 2026-09-02. This used to say "unlike the
music, this prose is the port's own wording and not Anderson's". **That is not
true and was never checked.** Of 326 pooled strings, 177 appear verbatim in
EGATREK.EXE and 33 match character for character including his capitalisation
-- the mixed-case entries are the tell, since this port writes in caps.

Most are interface labels a port cannot rename (COMMUNICATIONS, ENGINEERING,
U.S.S. LEXINGTON) and score-sheet rows that are the measured rubric. But
roughly forty-seven are whole sentences of Anderson's writing.

**KEPT, deliberately -- Jamie's call, 2026-09-02**, on the grounds that
rewriting them changes the feel of the game, which is the thing the port exists
to preserve. Replacements were drafted and read before the decision.

So the honest position is: the BRIEFING and the MUSIC are held to "none of
Anderson's", and the message pool is not. Do not describe the pool as the
port's own wording.

The first run extracts literals from the sources and writes the list. Later
runs READ the list and only add literals that are new, because by then the
sources hold S(S_n) and there is nothing left to extract. An earlier version
rebuilt the list from the sources every time and wiped the pool on its second
run -- the ids must outlive the extraction that created them.
"""
import os
import re
import sys

SRCS = ["c128/src/main.c", "c128/src/ui.c"]
HDR  = "c128/src/strdata.h"
BLOB = "c128/build/strings.dat"
LIST = "c128/src/strings.txt"      # the source of truth, and COMMITTED

# Literals are pooled ONLY when they are a direct argument to one of these.
DRAW_FNS = ("ui_message", "ui_dialog_line", "ui_dialog_ask", "ui_dialog_ask_esc",
            "ui_dialog_open", "scr_puts", "ui_confirm",
            # Added 2026-08-26, when the image overflowed and the 1,564 bytes
            # still in .rodata had to come out. All three take a const char*
            # and copy or draw it immediately, so a rotating pool slot is
            # safe: only one pooled string is ever alive per call.
            "put_str", "ev_row", "gauge_row", "ask_yes")

# NEVER pool a name the loader itself needs. "STRINGS.DAT" is read before the
# pool exists, and "MUSIC.DAT" and "TREK.SCR" are opened by code that must not
# depend on it. None of them is an argument to a DRAW_FN, which is what keeps
# them out -- do not add far_load, plat_open or hof_* to that list.

# Lowered from 10 to 6 on 2026-08-26, and MEASURED rather than reasoned: the
# 10 was a guess that short strings cost more in call overhead than they save.
# They do not. A pooled string costs two bytes of index and turns a
# load-address into a load-immediate-plus-call, which is one or two bytes at
# the site; a six-character literal is seven bytes of .rodata. The department
# prefixes alone -- "HELM: ", "COMMS: ", "SCIENCE: " -- were 60 bytes sitting
# above the old floor.
#
# Below six it really does stop paying, and the remaining literals at that
# length are single characters and separators.
MINLEN = 6
MAXLEN = 63          # STR_MAX - 1 in core/strpool.h


def find_calls(src):
    """Yield (start, end) spans of each argument list belonging to a DRAW_FN."""
    for m in re.finditer(r'\b(' + "|".join(DRAW_FNS) + r')\s*\(', src):
        depth, i = 1, m.end()
        while i < len(src) and depth:
            if src[i] == '(':
                depth += 1
            elif src[i] == ')':
                depth -= 1
            elif src[i] == '"':                       # skip over string bodies
                i += 1
                while i < len(src) and src[i] != '"':
                    i += 2 if src[i] == '\\' else 1
            i += 1
        yield m.end(), i - 1


def load_list():
    """Existing id -> text, or empty on the first run."""
    out = {}
    if os.path.exists(LIST):
        for ln in open(LIST):
            ln = ln.rstrip("\n")
            if not ln or ln.startswith("#"):
                continue
            i, _, t = ln.partition("\t")
            out[int(i)] = t
    return out


def main():
    known = load_list()
    texts = {t: i for i, t in known.items()}

    # Pass one: collect. Comments are stripped first so prose in them is not
    # mistaken for code.
    for path in SRCS:
        src = open(path).read()
        bare = re.sub(r'/\*.*?\*/', lambda m: " " * len(m.group(0)), src, flags=re.S)
        for a, b in find_calls(bare):
            for lit in re.finditer(r'"((?:[^"\\]|\\.)*)"', bare[a:b]):
                t = lit.group(1)
                if MINLEN <= len(t) <= MAXLEN and t not in texts:
                    texts[t] = None

    # Ids already issued keep their number for ever; new ones are appended.
    ids = {t: i for t, i in texts.items() if i is not None}
    nxt = (max(ids.values()) + 1) if ids else 0
    for t in sorted(k for k, v in texts.items() if v is None):
        ids[t] = nxt
        nxt += 1

    with open(LIST, "w") as f:
        f.write("# GENERATED by tools/gen_strings.py, and COMMITTED.\n")
        f.write("# id<TAB>text. Ids are permanent -- never renumber them.\n")
        for t, i in sorted(ids.items(), key=lambda kv: kv[1]):
            f.write("%d\t%s\n" % (i, t))

    # Pass two: rewrite, in the same spans only.
    changed = 0
    for path in SRCS:
        src = open(path).read()
        bare = re.sub(r'/\*.*?\*/', lambda m: " " * len(m.group(0)), src, flags=re.S)
        out, last = [], 0
        for a, b in find_calls(bare):
            for lit in re.finditer(r'"((?:[^"\\]|\\.)*)"', bare[a:b]):
                t = lit.group(1)
                if t not in ids:
                    continue
                s, e = a + lit.start(), a + lit.end()
                out.append(src[last:s])
                out.append("S(S_%d)" % ids[t])
                last = e
                changed += 1
        out.append(src[last:])
        open(path, "w").write("".join(out))

    # The ids, and the blob. Each string is stored NUL terminated, and the
    # index is a flat table of 16-bit offsets so a lookup is one far read.
    off, offsets, text = 0, [], bytearray()
    for t in sorted(ids, key=lambda k: ids[k]):
        offsets.append(off)
        b = t.encode("ascii", "replace") + b"\0"
        text += b
        off += len(b)

    with open(HDR, "w") as f:
        f.write("/* GENERATED by tools/gen_strings.py -- do not edit.\n"
                "   The text AND its offset table are in STRINGS.DAT on the game\n"
                "   disk; only the ids are in the binary. */\n")
        f.write("#ifndef STRDATA_H\n#define STRDATA_H\n\n")
        f.write("#define STR_COUNT %d\n\n" % len(ids))
        for t in sorted(ids, key=lambda k: ids[k]):
            f.write("#define S_%-4d %3d   /* %s */\n"
                    % (ids[t], ids[t], t[:46].replace("*/", "*\\/")))
        f.write("\n#endif\n")

    # THE OFFSET TABLE SHIPS IN THE FILE, NOT IN THE BINARY.
    #
    # It used to be `const unsigned int str_offset[STR_COUNT]` in a generated
    # strdata.c -- 600 bytes of read-only data in the resident image, which is
    # the tightest pool in the build. Moving the prose out and leaving the
    # index behind only ever half-finished the job.
    #
    # It could not simply be DECLARED in far memory: bank 1 is filled by
    # loading a file into it, and a static initialiser table has no way to get
    # there. So the table travels with the text it indexes, in the same file
    # and the same single KERNAL LOAD:
    #
    #     +0                  count, 16-bit little endian
    #     +2                  count 16-bit offsets, little endian
    #     +2 + 2*count        the text, each string NUL terminated
    #
    # THE COUNT IS A GUARD, and it is why the file leads with it. A disk built
    # from an older tree has no header at all, and the first two bytes of the
    # text would read as an offset -- every string would come back as noise
    # from a random place in the store. str_load() compares this against the
    # compiled STR_COUNT and refuses a pool that does not match, which turns
    # that into the failure the pool already had a plan for: no words, and a
    # game that still plays.
    blob = bytearray()
    blob += len(ids).to_bytes(2, "little")
    for o in offsets:
        blob += o.to_bytes(2, "little")
    blob += text

    # Pad so a full-width read of the LAST string cannot run off the end of
    # the pool -- S() reads STR_MAX bytes at a time and then looks for the
    # terminator, which is much cheaper than a far read per character.
    blob += b"\0" * 64

    os.makedirs("c128/build", exist_ok=True)
    open(BLOB, "wb").write(blob)
    print("gen_strings: %d strings, %d bytes of text -> %s" % (len(ids), len(blob), BLOB))
    print("gen_strings: rewrote %d call sites; the index is %d bytes IN THE FILE, "
          "0 in the binary" % (changed, len(ids) * 2))


if __name__ == "__main__":
    main()

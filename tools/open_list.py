#!/usr/bin/env python3
"""Check that THE OPEN LIST's header agrees with the list under it.

WHY THIS EXISTS. The header has been wrong five separate ways on this project,
and every time the cause was the same: STATUS LIVING SOMEWHERE THE COUNT CANNOT
SEE IT.

  * it read `0 open of 25` while two unreleased ports carried work;
  * items 32, 33 and 34 were each used TWICE, by two batches on one day;
  * item 27 was struck through as a whole line with "Sound and input remain
    stubs" in bold INSIDE it, so re-deriving counted it closed -- and a port
    that could not answer a prompt was handed over as playable;
  * "the seventh port is not released" sat in PROSE for a week, so nothing
    counted it and nothing could close it;
  * and on 2026-09-13 five items were fixed, said CLOSED in their bodies, and
    kept unstruck headings -- so a hand count said one open, the formatting
    said seven, and the truth was two.

Re-deriving by reading is what produced every one of those. This does it
mechanically instead: an item is OPEN if its number is not followed by `~~`,
and the header must say so. It also catches a number used twice, which is how
the 32/33/34 collision survived being correct-by-count.

Run from `make ports`. It reads one file and needs no toolchain.
"""
import os, re, sys

HERE  = os.path.dirname(os.path.abspath(__file__))
NOTES = os.path.join(os.path.dirname(HERE), "NOTES.md")


def main():
    s = open(NOTES).read()
    try:
        i = s.index("## THE OPEN LIST")
    except ValueError:
        sys.exit("open_list: no '## THE OPEN LIST' heading in NOTES.md")
    j = s.index("\n## ", i + 10)
    body = s[i:j]

    seen, dup = {}, []
    for m in re.finditer(r"^  (\d+)\.\s+(~~)?", body, re.M):
        n, closed = int(m.group(1)), bool(m.group(2))
        if n in seen:
            dup.append(n)
        seen[n] = closed

    open_items = sorted(n for n, c in seen.items() if not c)
    raised = max(seen) if seen else 0

    hm = re.search(r"\((\d+) open of (\d+) raised\)", body)
    if not hm:
        sys.exit("open_list: the heading does not carry '(N open of M raised)'")
    said_open, said_raised = int(hm.group(1)), int(hm.group(2))

    bad = []
    if dup:
        bad.append("numbers used twice: %s" % sorted(set(dup)))
    if said_open != len(open_items):
        bad.append("header says %d open, the list has %d: %s"
                   % (said_open, len(open_items), open_items))
    if said_raised != raised:
        bad.append("header says %d raised, the highest number is %d"
                   % (said_raised, raised))

    if bad:
        print("open_list: THE HEADER AND THE LIST DISAGREE")
        for b in bad:
            print("    " + b)
        print("    an item is OPEN if its number is not followed by ~~;")
        print("    a body that says CLOSED under an unstruck heading is not closed")
        return 1

    print("open_list: %d open of %d raised -- %s"
          % (len(open_items), raised, open_items if open_items else "empty"))
    return 0


sys.exit(main())

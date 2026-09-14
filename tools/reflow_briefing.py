#!/usr/bin/env python3
"""c128/src/briefing.txt (80 columns) -> briefing40.txt (40), for item 61.

THE BRIEFING IS NOT UNIFORM PROSE, which is why this is a tool and not a
sed. Twelve pages carrying centred headers, right-aligned "Page N of 12"
footers, ordinary paragraphs at an indent of 2, a specifications table with
dot leaders at 5, and deeper-indented continuation blocks at 11 and 12. Each
wants different treatment, and re-wrapping the dot-leader table as prose would
turn a table into a sentence.

RE-WRAPPING ROUGHLY DOUBLES THE LINE COUNT, so it also RE-PAGINATES and
RENUMBERS. A tool that reflowed without repaginating would produce pages the
viewer scrolls off the bottom of, and footers that lie about how many there
are.

THE COMMAND CONSOLE PAGE IS REPLACED, NOT REFLOWED. Its 80-column text opens
"Nine panels, all live, all the time. You will not need to ask for any of
them" -- which is FALSE at forty columns, where the console is two halves and
C switches between them. Reflowing a false sentence just makes it narrower.
The replacement lives in c128/src/briefing40-console.txt, as prose in a text
file rather than strings in a script.
"""
import os, re, sys, textwrap

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "c128", "src", "briefing.txt")
CONSOLE = os.path.join(ROOT, "c128", "src", "briefing40-console.txt")
OUT = os.path.join(ROOT, "c128", "src", "briefing40.txt")

WIDTH = 38          # drawn at x=1, so column 39 is the last usable one
# TWENTY, NOT TWENTY-TWO. Each page carries its own blank line and "Page N of
# M" footer, so a 22-row budget for the BODY produces a 24-row page --
# verify_prg's BRIEF_ROWS is 22 for the whole thing and said so. Two checks
# measuring different things is worse than one, and the disagreement is the
# only reason this was caught.
PAGE  = 20
DOTS  = re.compile(r"\s\.{3,}\s")


def wrap(text, indent, hang):
    return textwrap.wrap(text, WIDTH, initial_indent=indent,
                         subsequent_indent=hang, break_on_hyphens=False,
                         break_long_words=False) or [indent.rstrip()]


def reflow_page(page):
    """One 80-column page -> a list of 40-column lines, footer stripped."""
    out, para, pind = [], [], None

    def flush():
        nonlocal para, pind
        if para:
            text = " ".join(l.strip() for l in para)
            # FLAT FOR PROSE, HANGING FOR THE DEEP BLOCKS. The first version
            # hung every paragraph two columns further in, which turned
            # ordinary prose into what looked like a bulleted list.
            if pind <= 2:
                ind = hang = "  "
            else:
                ind, hang = "   ", "     "
            out.extend(wrap(text, ind, hang))
            para, pind = [], None

    for line in page.split("\n"):
        stripped = line.strip()
        ind = len(line) - len(line.lstrip()) if stripped else 0

        if not stripped:
            flush(); out.append(""); continue
        if ind >= 44:                      # "Page N of 12" -- regenerated later
            continue
        if ind >= 20:                      # a centred header
            flush(); out.append(stripped.center(WIDTH).rstrip()); continue
        if DOTS.search(line):              # the specifications table
            flush()
            label, value = DOTS.split(line.strip(), 1)
            # " " + label + " " + dots + " " + value  ==  3 + L + V + dots.
            # The first version budgeted one column too few and produced
            # 39-character rows; the tool's own width check caught it.
            dots = WIDTH - 3 - len(label) - len(value)
            if dots >= 1:
                out.append(" " + label + " " + "." * dots + " " + value)
            else:
                # Too wide for one line: label, then the value under it.
                out.extend(wrap(label, " ", " "))
                out.extend(wrap(value, "      ", "      "))
            continue
        para.append(line)
        pind = ind if pind is None else min(pind, ind)
    flush()
    while out and not out[-1]:
        out.pop()
    return out


def paginate(pages):
    """Split reflowed pages at PAGE lines, never inside a paragraph if it can
    be helped -- a break at a blank line reads as a break, one mid-sentence
    reads as a bug."""
    final = []
    for body in pages:
        # THE HEADER GOES ON EVERY CONTINUATION. Each page of the original
        # carries the ship's name and its section title; a split that dropped
        # them produced an orphan page of prose with nothing saying what it
        # was about -- and the first one to land there was the PRESS C
        # instruction, which is the whole point of the 40-column briefing.
        # The header is the first two lines BY CONSTRUCTION -- reflow_page and
        # the console override both emit a centred pair followed by a blank.
        # Detecting it by indentation does not work: a centred line and a
        # prose line both start with spaces, which is why the first attempt
        # found no header at all and carried nothing.
        head = (body[:2] if len(body) > 2 and body[0].strip()
                and body[1].strip() and not body[2].strip() else [])
        while len(body) > PAGE:
            cut = PAGE
            while cut > PAGE - 6 and body[cut - 1].strip():
                cut -= 1
            if cut <= PAGE - 6:
                cut = PAGE
            final.append(body[:cut])
            rest = body[cut:]
            while rest and not rest[0].strip():
                rest.pop(0)
            body = (head + [""] + rest) if head and rest else rest
        final.append(body)
    return final


def main():
    src = open(SRC).read()
    console = open(CONSOLE).read().rstrip("\n").split("\n")

    pages = []
    for page in src.split("\f"):
        if "THE COMMAND CONSOLE" in page:
            pages.append(console)          # replaced, not reflowed
        else:
            pages.append(reflow_page(page))

    pages = paginate(pages)
    total = len(pages)

    body = []
    for i, p in enumerate(pages, 1):
        p = list(p)
        foot = "Page %d of %d" % (i, total)
        p.append("")
        p.append(" " * max(0, WIDTH - len(foot)) + foot)
        body.append("\n".join(p))
    text = "\f".join(body) + "\n"
    open(OUT, "w").write(text)

    lines = [l for pg in text.split("\f") for l in pg.split("\n")]
    wide = [l for l in lines if len(l) > WIDTH]
    # Measured the way verify_prg measures it: the WHOLE page, footer included.
    tall = [i + 1 for i, pg in enumerate(text.split("\f"))
            if len(pg.strip("\n").split("\n")) > PAGE + 2]
    print("  %s: %d pages, %d lines, longest %d"
          % (os.path.relpath(OUT, ROOT), total, len(lines),
             max(len(l) for l in lines)))
    if wide:
        print("  %d line(s) WIDER than %d:" % (len(wide), WIDTH))
        for l in wide[:5]:
            print("     %2d  %s" % (len(l), l))
        return 1
    if tall:
        print("  page(s) taller than %d rows: %s" % (PAGE, tall))
        return 1
    print("  every line fits %d columns and every page fits %d rows "
          "including its footer" % (WIDTH, PAGE + 2))
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Attribute a colour to every message site in the original.

MEASURED.md, "Message colour is PER SITE", read the MECHANISM in 2026-09-02:
there is no department colour table. Each site calls BGI `SetColor`
(fn 0x029960 -- the immediate is the `mov ax, N` before it) and then the
message routine (fn 0x021CC2), which adds one rule of its own: **if the colour
is 10 it uses 15 instead**. Attributing the 145 sites to the nearest preceding
SetColor gave ten distinct colours -- but the TABLE was never written down,
only the count and four worked examples.

This writes the table. For each far call to the message routine it walks
backwards for the nearest far call to SetColor, takes the immediate, applies
the 10->15 rule, and reports it with the string the site pushes so the result
can be matched to this port's own messages by TEXT rather than by address.

    python3 tools/msg_colours.py [--csv]

Needs reference/, which is gitignored -- see the README on why.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(HERE, "reference", "EGATREK_unpacked.exe")
LOAD_BASE = 1248 * 16
MSG_FN = 0x021CC2          # the message routine
SETCOLOR_FN = 0x029960     # BGI SetColor
BACK = 96                  # how far back a site's SetColor may sit


def calls(data):
    """Every call and where it goes, FAR AND NEAR.

    The first version read only far calls (`9A off off seg seg`) and found 57
    message sites against the 145 MEASURED.md records, missing colours 1, 2, 4
    and 11 entirely. Code in the same segment as the message routine reaches it
    with a NEAR call (`E8 rel16`), whose target is simply the next instruction
    plus a signed displacement -- and file offsets work directly for those,
    because a near call cannot leave the segment. Having the earlier reading's
    COUNT written down is the only reason the gap was visible.
    """
    out = []
    n = len(data)
    for i in range(n - 5):
        b0 = data[i]
        if b0 == 0x9A:
            off = data[i + 1] | (data[i + 2] << 8)
            seg = data[i + 3] | (data[i + 4] << 8)
            out.append((i, (seg << 4) + off + LOAD_BASE))
        elif b0 == 0xE8:
            rel = data[i + 1] | (data[i + 2] << 8)
            if rel >= 0x8000:
                rel -= 0x10000
            out.append((i, i + 3 + rel))
    return out


def main():
    if not os.path.exists(EXE):
        # SKIP, DO NOT FAIL, on a tree without reference/ -- the same rule
        # tools/check_tables.py follows. The licence keeps reference/ out of
        # this repo, so a clone that cannot run this is the normal case.
        print("msg_colours: no reference/ in this clone -- skipped")
        return
    data = open(EXE, "rb").read()
    allcalls = calls(data)
    by_addr = {}
    for at, tgt in allcalls:
        by_addr.setdefault(tgt, []).append(at)

    sites = sorted(by_addr.get(MSG_FN, []))
    colours = sorted(by_addr.get(SETCOLOR_FN, []))
    print("msg_colours: %d message sites, %d SetColor calls"
          % (len(sites), len(colours)), flush=True)
    if not sites:
        sys.exit("msg_colours: found no calls to $%05X -- has the address "
                 "drifted? MEASURED.md has the provenance." % MSG_FN)

    cset = set(colours)
    rows, unattributed = [], 0
    for at in sites:
        # nearest preceding SetColor, and the `mov ax, N` (B8 nn nn) in front
        # of it. Both must be inside BACK bytes or the site is left unclaimed
        # rather than given a neighbour's colour.
        col = None
        for back in range(1, BACK):
            if at - back in cset:
                j = at - back
                for b2 in range(3, 12):
                    if data[j - b2] == 0xB8:
                        col = data[j - b2 + 1] | (data[j - b2 + 2] << 8)
                        break
                break
        if col is None:
            unattributed += 1
        else:
            if col == 10:
                col = 15            # the message routine's one rule
            rows.append((at, col))

    # THE STRING EACH SITE PUSHES, so the result can be matched to this port's
    # messages by TEXT rather than by address. The site does
    # `mov di,imm16 / push cs / push di`, so the string lives in the site's OWN
    # code segment -- and the segment base is what has to be solved. Turbo
    # Pascal strings are length-prefixed, which makes a candidate base
    # checkable: base+off must hold a length byte followed by that many
    # printable characters. MEASURED.md's worked example (base 0x009310,
    # offset 0x4d5e) round-trips to "NAVIGATION: Not adjacent to planet.",
    # which is how this was validated before being trusted.
    def pascal_at(k):
        if not (0 <= k < len(data) - 1):
            return None
        n = data[k]
        if not (6 <= n <= 80) or k + 1 + n > len(data):
            return None
        t = data[k + 1:k + 1 + n]
        if not all(32 <= c < 127 for c in t):
            return None
        txt = t.decode("ascii")
        return txt if any(c.isalpha() for c in txt) else None

    def pushed_off(at):
        """`mov di,imm16` (BF) within 16 bytes before the call."""
        for b2 in range(3, 18):
            if data[at - b2] == 0xBF:
                return data[at - b2 + 1] | (data[at - b2 + 2] << 8)
        return None

    def solve_bases(sites):
        """One base per SEGMENT, by consensus -- not per site by proximity.

        THE FIRST VERSION CHOSE, FOR EACH SITE INDEPENDENTLY, whichever
        candidate base put the string nearest the code. It disagreed with the
        one site whose answer is written down: MEASURED.md records $0E315 as
        "NAVIGATION: Not adjacent to planet." and proximity gave "Planet
        settlers found...". An offset lands on something that passes for a
        Pascal string at many bases, so a per-site test cannot separate them.

        MEASURED.md's own method is to solve a base from DISJOINT SPANS -- a
        real segment base explains MANY sites at once, a coincidence explains
        one. So: collect every candidate base per site, count how many sites
        each base can serve, and give each site the most-supported base that
        works for it.
        """
        cand = {}
        for at in sites:
            off = pushed_off(at)
            if off is None:
                continue
            for base in range(max(0, at - 65535) & ~0xF, at + 1, 16):
                if pascal_at(base + off):
                    cand.setdefault(at, []).append(base)
        support = {}
        for at, bases in cand.items():
            for b in bases:
                support[b] = support.get(b, 0) + 1
        out = {}
        for at, bases in cand.items():
            best = max(bases, key=lambda b: (support[b], -abs(b - at)))
            out[at] = (best, support[best])
        return out

    solved = solve_bases([at for at, _ in rows])
    texts, weak = {}, 0
    for at, c in rows:
        if at in solved:
            base, sup = solved[at]
            t = pascal_at(base + pushed_off(at))
            if t:
                texts[at] = t
                if sup < 3:
                    weak += 1
    print("msg_colours: text recovered for %d of %d attributed sites "
          "(%d from a base only 1-2 sites support -- treat those as unread)"
          % (len(texts), len(rows), weak))
    # THE CHECK THAT DECIDES WHETHER ANY OF THIS IS EVIDENCE. MEASURED.md
    # records $0E315 as "NAVIGATION: Not adjacent to planet."
    known = texts.get(0x0E315, "")
    ok = known.startswith("NAVIGATION: Not adjacent")
    print("msg_colours: $0E315 reads %r -- %s"
          % (known, "AGREES with MEASURED.md" if ok else "DISAGREES, so the "
             "base solving is still wrong and nothing here is evidence"))

    hist = {}
    for _, c in rows:
        hist[c] = hist.get(c, 0) + 1
    print("msg_colours: attributed %d, unclaimed %d" % (len(rows), unattributed))
    print("msg_colours: colours in use -- %s"
          % ", ".join("%d x%d" % (c, n) for c, n in sorted(hist.items())))
    if "--csv" in sys.argv:
        for at, c in rows:
            print("%05X,%d,%s" % (at, c, texts.get(at, "")))


main()

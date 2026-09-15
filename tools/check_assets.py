#!/usr/bin/env python3
"""RUNNING.md counts its own assets. Make it count them correctly.

TWICE NOW. Item 63: the line said eleven and the release had twelve, because
adding the C64 meant changing "seven machines" to "eight" and the OTHER number
was left alone. One release later, adding the ST, I wrote "Fourteen assets"
and there are THIRTEEN. A count that only a person checks is a count that
drifts, and this one is generated straight onto the release page.

The arithmetic is not hard and that is the point: NINE artefacts, one per
machine, plus a .txt beside each of the FOUR that are bare disk images and
have nowhere to put a README inside. The four .zip ports carry theirs as
egatrek-<port>/README.txt.
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNNING = os.path.join(ROOT, "RUNNING.md")
WORDS = {1:"one",2:"two",3:"three",4:"four",5:"five",6:"six",7:"seven",
         8:"eight",9:"nine",10:"ten",11:"eleven",12:"twelve",13:"thirteen",
         14:"fourteen",15:"fifteen",16:"sixteen",17:"seventeen",18:"eighteen"}


def ports():
    """RELEASE_PORTS, out of the root Makefile -- never a list typed here."""
    mk = open(os.path.join(ROOT, "Makefile")).read()
    m = re.search(r"^RELEASE_PORTS\s*=\s*(.+)$", mk, re.M)
    if not m:
        sys.exit("check_assets: no RELEASE_PORTS in the root Makefile")
    return m.group(1).split()


def main():
    ps = ports()
    # A port ships a .txt beside its artefact exactly when it has a
    # README-release.txt AND its artefact is a bare image rather than a zip.
    bare = []
    for p in ps:
        d = os.path.join(ROOT, p, "build")
        if not os.path.isdir(d):
            continue
        for f in os.listdir(d):
            if re.fullmatch(r"egatrek-%s\.(d64|d81|atr)" % p, f):
                bare.append(p)
    assets = len(ps) + len(bare)

    text = open(RUNNING).read()
    m = re.search(r"^(\w+) assets, (\w+) machines\.", text, re.M)
    if not m:
        print("check_assets: RUNNING.md does not open by counting itself -- "
              "that sentence IS the check, so its absence is a failure")
        return 1
    said_a, said_m = m.group(1).lower(), m.group(2).lower()
    want_a, want_m = WORDS.get(assets, str(assets)), WORDS.get(len(ps), str(len(ps)))

    ok = True
    if said_a != want_a:
        print("check_assets: RUNNING.md says %r assets; there are %d (%s) -- "
              "%d artefacts + %d .txt beside the bare images (%s)"
              % (said_a, assets, want_a, len(ps), len(bare), ", ".join(bare)))
        ok = False
    if said_m != want_m:
        print("check_assets: RUNNING.md says %r machines; RELEASE_PORTS has %d"
              % (said_m, len(ps)))
        ok = False
    if not ok:
        return 1
    print("check_assets: RUNNING.md says %s assets, %s machines -- %d artefacts "
          "+ %d .txt, and RELEASE_PORTS agrees" % (said_a, said_m, len(ps), len(bare)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

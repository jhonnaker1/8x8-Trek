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
         14:"fourteen",15:"fifteen",16:"sixteen",17:"seventeen",18:"eighteen",
         19:"nineteen",20:"twenty",21:"twenty-one",22:"twenty-two",
         23:"twenty-three",24:"twenty-four",25:"twenty-five"}
# THE TABLE STOPPED AT EIGHTEEN, and v0.22.0 was the first release with more
# assets than that: the MSX2 made nineteen, RUNNING.md said "Nineteen", and
# this failed saying 'nineteen' was not 19 -- because it could not spell 19.


def ports():
    """RELEASE_PORTS, out of the root Makefile -- never a list typed here."""
    mk = open(os.path.join(ROOT, "Makefile")).read()
    m = re.search(r"^RELEASE_PORTS\s*=\s*(.+)$", mk, re.M)
    if not m:
        sys.exit("check_assets: no RELEASE_PORTS in the root Makefile")
    return m.group(1).split()


def bare_images(ps):
    """Which ports ship a BARE disk image, read out of their `release:` rules.

    THE FIRST VERSION LOOKED IN build/, AND THAT MADE THE CHECK DEPEND ON
    HAVING BUILT. `make release-clean` wipes every build/ -- which is exactly
    when a release is being cut and exactly when this check matters most --
    and it then found zero bare images and failed with "there are 9". It would
    have failed the same way on a fresh clone.
    A check that only passes on a tree somebody has already built is not
    checking the repository, it is checking the last thing that ran.
    """
    out = []
    for p in ps:
        mk = os.path.join(ROOT, p, "Makefile")
        if not os.path.exists(mk):
            continue
        if re.search(r"egatrek-%s\.(d64|d81|atr)\b" % p, open(mk).read()):
            out.append(p)
    return out


def main():
    ps = ports()
    # A port ships a .txt beside its artefact exactly when that artefact is a
    # bare image -- a .d64, .d81 or .atr has nowhere to put a README inside.
    bare = bare_images(ps)
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
    # THE SECOND SENTENCE COUNTS THEM AGAIN AND NOTHING CHECKED IT. Adding the
    # Plus/4 left "Four assets are bare disk images ... The six .zip assets"
    # stale while the opening line -- the only one this tool read -- was
    # correct and green. A count that a check does not reach drifts exactly
    # like one no tool ever touched; the shape is the target, not the sentence.
    n_bare, n_zip = len(bare), len(ps) - len(bare)
    m2 = re.search(r"^\*\*(\w+) assets are bare disk images", text, re.M)
    m3 = re.search(r"The (\w+) `\.zip` assets carry theirs inside", text)
    for got, want, what in ((m2, n_bare, "bare disk images"),
                            (m3, n_zip, "`.zip` assets")):
        if not got:
            print("check_assets: RUNNING.md no longer states how many %s there "
                  "are -- that sentence is part of the count" % what)
            ok = False
        elif got.group(1).lower() != WORDS.get(want, str(want)):
            print("check_assets: RUNNING.md says %r %s; there are %d (%s)"
                  % (got.group(1), what, want, WORDS.get(want, str(want))))
            ok = False

    if not ok:
        return 1
    print("check_assets: RUNNING.md says %s assets, %s machines -- %d artefacts "
          "+ %d .txt (%d bare, %d zip), and RELEASE_PORTS agrees"
          % (said_a, said_m, len(ps), len(bare), n_bare, n_zip))
    return 0


if __name__ == "__main__":
    sys.exit(main())

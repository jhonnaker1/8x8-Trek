#!/usr/bin/env python3
"""Is the EGATrek.app on the NeXT's shared folder built from THIS tree?

    python3 tools/checkbuild.py CARD

The release packs a program the host cannot compile, so it must be sure of
what it is packing. Three things, and each fails by name:

  1. THE DEPLOYED SOURCES ARE THE REPOSITORY'S. Every file `make deploy`
     copies is compared byte for byte with the one here: an edit made since
     the last deploy means the NeXT built something else.
  2. THE PROGRAM IS NEWER THAN EVERY SOURCE IT WAS BUILT FROM: a deploy made
     after the last build on the NeXT means the binary is the old one.
  3. THE DATA INSIDE THE WRAPPER IS build/'s: a stale STRINGS.DAT is the
     STR_COUNT coupling, every label blank and no error anywhere.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(HERE)


def shared():
    mk = open(os.path.join(HERE, "Makefile")).read()
    m = re.search(r"^SHARED\s*=\s*((?:.*\\\n)*.*)$", mk, re.M)
    return m.group(1).replace("\\\n", " ").split()


def ours():
    out = ["next/Makefile.next", "next/EGATrek.iconheader", "next/EGATrek.tiff"]
    for d, _, fs in os.walk(os.path.join(HERE, "src")):
        for f in fs:
            if not f.startswith("."):
                out.append(os.path.relpath(os.path.join(d, f), ROOT))
    return out


def main():
    card = os.path.join(sys.argv[1], "egatrek")
    app = os.path.join(card, "next", "EGATrek.app")
    prog = os.path.join(app, "EGATrek")
    bad = []
    if not os.path.exists(prog):
        sys.exit("checkbuild: no %s -- `make deploy`, then on the NeXT, in "
                 "/Net/nfs/egatrek/next: make -f Makefile.next" % prog)
    built = os.path.getmtime(prog)
    files = shared() + ours()
    for rel in files:
        here = os.path.join(ROOT, rel)
        there = os.path.join(card, rel)
        if not os.path.exists(there):
            bad.append("%s was never deployed" % rel)
        elif open(here, "rb").read() != open(there, "rb").read():
            bad.append("%s differs from the deployed copy -- `make deploy` "
                       "and build again on the NeXT" % rel)
        elif os.path.getmtime(there) > built:
            bad.append("%s was deployed after EGATrek was built -- build "
                       "again on the NeXT" % rel)
    for f in ("STRINGS.DAT", "MUSIC.DAT", "BRIEF.TXT"):
        a, b = os.path.join(HERE, "build", f), os.path.join(app, f)
        if not os.path.exists(b) or open(a, "rb").read() != open(b, "rb").read():
            bad.append("EGATrek.app/%s is not build/%s" % (f, f))
    if bad:
        for m in bad:
            print("checkbuild: FAIL -- " + m)
        return 1
    print("checkbuild: EGATrek.app (%d bytes) is built from this tree's %d "
          "files, and carries this tree's data" % (os.path.getsize(prog),
                                                   len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

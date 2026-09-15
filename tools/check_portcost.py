#!/usr/bin/env python3
"""The README's port-cost arithmetic, checked against the tree.

"What a sub-80 port actually entails" quotes three numbers -- the shared line
count, what the C64 wrote, and how many seam functions there are. THEY ARE THE
WHOLE POINT OF THE SECTION, and this project has just spent a sweep deleting
figures that had drifted out of three other READMEs. A number in prose with no
check behind it is the shape that goes stale; this is the check.
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
README = os.path.join(ROOT, "README.md")

SHARED = ["c128/src/ui.c", "c128/src/main.c", "c128/src/layout40.c",
          "c128/src/strpool.c", "c128/src/input.c", "c128/src/sid.c",
          "c128/src/storage.c", "c128/src/overlay.c", "c128/src/vic.c",
          "c128/src/music_data.c", "core/trek.c", "core/planet.c",
          "core/hof.c", "core/serial.c"]
OWN_C = ["c64/src/c64mem.c", "c64/src/c64log.c"]
OWN_ALL = OWN_C + ["c64/c64.ld", "c64/Makefile",
                   "c64/tools/verify_c64.py", "c64/tools/report_size.py"]
# vdc.h declares fifteen; vdc_reg_read/vdc_reg_write are the 8563's own and
# nothing outside vdc.c calls them, so a port with no VDC fills thirteen.
HEADERS = {"core/storage.h": 5, "core/farmem.h": 3, "core/overlay.h": 2,
           "c128/src/vdc.h": 15, "c128/src/input.h": 2, "c128/src/sid.h": 9}
NOT_NEEDED = 2

def lines(paths):
    return sum(sum(1 for _ in open(os.path.join(ROOT, p), encoding="utf-8",
                                   errors="replace")) for p in paths)

def fns(h):
    src = open(os.path.join(ROOT, h), encoding="utf-8").read()
    return len(set(re.findall(r"^[a-z][a-z0-9_ ]*\**\s+\**([a-z_0-9]+)\s*\(",
                              src, re.M)))

def main():
    text = open(README, encoding="utf-8").read()
    bad = []

    def want(n, what):
        if ("%s" % n) in text or ("{:,}".format(n)) in text:
            print("  ok    %-42s %s" % (what, "{:,}".format(n)))
        else:
            print("  FAIL  %-42s %s NOT IN README.md" % (what, "{:,}".format(n)))
            bad.append(what)

    want(lines(SHARED), "shared lines the C64 links")
    want(lines(OWN_C), "C the C64 wrote")
    want(lines(OWN_ALL), "everything the C64 wrote")

    declared = {h: fns(h) for h in HEADERS}
    for h, n in HEADERS.items():
        if declared[h] != n:
            print("  FAIL  %-42s declares %d, this tool expects %d"
                  % (h, declared[h], n))
            bad.append(h)
    total = sum(declared.values()) - NOT_NEEDED
    want(total, "seam functions a non-VDC port must fill")

    if bad:
        print("check_portcost: %d figure(s) in README.md are stale" % len(bad))
        return 1
    print("check_portcost: the README's port-cost arithmetic matches the tree")
    return 0

if __name__ == "__main__":
    sys.exit(main())

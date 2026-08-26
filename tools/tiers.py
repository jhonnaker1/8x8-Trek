#!/usr/bin/env python3
"""Provenance audit: what in the core is measured, and what is still a guess.

Every #define in core/trek.h and core/planet.h carries a marker on ITS OWN
LINE, `/*@TIER*/`. That placement is the whole point. The tiers used to live
in the prose above a constant, and a scan for them attributed the nearest
preceding paragraph -- so it reported ENEMY_BASE as fitted and PLANET_MIN as
provisional when neither was, and the output could not be trusted for the one
question it exists to answer.

TIERS, strongest first:

  BINARY       read out of the original's code with tools/dis16.py. An exact
               constant of Anderson's, not a sample.
  CONFIRMED    stated in the manual, or read unambiguously off the screen.
  MEASURED     measured off the original running under DOSBox.
  FITTED       chosen to match readings, but not uniquely determined.
  DERIVED      taken from the FORTRAN ancestor (reference/sst2k). Anderson
               rewrote formulas and dropped rules, so DERIVED is a hypothesis.
  PROVISIONAL  a guess, kept only because something has to be there.
  ID           an identifier or enumeration, not a measurement of anything.

Fails if any #define is untagged, so a new constant cannot arrive without a
provenance.
"""
import re
import sys

FILES = ("core/trek.h", "core/planet.h")
ORDER = ("BINARY", "CONFIRMED", "MEASURED", "FITTED", "DERIVED", "PROVISIONAL", "ID")
SKIP = {"TREK_H", "PLANET_H"}
SOFT = ("FITTED", "DERIVED", "PROVISIONAL")


def main():
    found, missing = {t: [] for t in ORDER}, []
    for path in FILES:
        try:
            src = open(path).read()
        except OSError:
            print("tiers: cannot read %s" % path, file=sys.stderr)
            return 1
        for line in src.split("\n"):
            m = re.match(r"#define\s+([A-Z_0-9]+)", line)
            if not m or m.group(1) in SKIP:
                continue
            t = re.search(r"/\*@([A-Z]+)\*/", line)
            if not t:
                missing.append(m.group(1))
            elif t.group(1) in found:
                found[t.group(1)].append(m.group(1))
            else:
                print("tiers: %s has unknown tier %s" % (m.group(1), t.group(1)),
                      file=sys.stderr)
                return 1

    if missing:
        print("tiers: %d constant(s) with no provenance marker:" % len(missing),
              file=sys.stderr)
        for n in missing:
            print("    %s" % n, file=sys.stderr)
        return 1

    for t in ORDER:
        print("tiers: %-12s %3d" % (t, len(found[t])))
    soft = [n for t in SOFT for n in found[t]]
    print("tiers: %d constants NOT measured or read:" % len(soft))
    for t in SOFT:
        for n in sorted(found[t]):
            print("    %-11s %s" % (t, n))
    return 0


if __name__ == "__main__":
    sys.exit(main())

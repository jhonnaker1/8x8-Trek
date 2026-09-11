#!/usr/bin/env python3
"""A variable used in a rule's prerequisites must be defined ABOVE that rule.

THIS HAS NOW BITTEN THREE TIMES AND IS INVISIBLE EVERY TIME.

Make expands a rule's prerequisites AS IT PARSES THE LINE. A variable defined
further down the file expands to NOTHING, so

    verify: $(GAME_OUT) nolto        with GAME_OUT defined 20 lines below

is really `verify: nolto` -- the rule still runs, still prints, and still
passes. Nothing fails. What you lose is the dependency:

  * c128/Makefile, 2026-09-09: `verify: $(RES) $(MAP)` silently depended on
    neither, so the checks that read them did not run.
  * atari/Makefile, 2026-09-10: `verify: $(GAME_OUT)` depended on nothing, and
    `make verify` on a clean tree failed because nothing had built the game.
  * atari and c128, same day: `nolto: $(GAME_SRC)` and `nolto: $(SRC) $(HDR)`.
    THIS IS THE QUIET ONE. Those objects are what `make verify` reads to check
    overlay rule 4 -- the rule whose violation crashed three ports at once --
    and they were not rebuilt when a source or a header changed. A stale object
    passing a check is worse than a missing one.

The first two were found by accident. The third was found by this script, in
the same minute it was written.

Deliberately narrow: only prerequisites, only simple `$(NAME)` references, only
a definition LATER in the same file. It cannot see includes or recursion, and
it is not trying to -- it catches one mistake that costs an afternoon.
"""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DEFINE = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*)\s*[:?+]?=')
# A rule line: a target list, a colon that is not `:=`, then prerequisites.
RULE = re.compile(r'^([^\t=#:]+):(?!=)(.*)$')
REF = re.compile(r'\$\(([A-Za-z_][A-Za-z0-9_]*)\)')


def check(path):
    lines = open(os.path.join(HERE, path)).read().splitlines()

    first = {}
    for n, line in enumerate(lines, 1):
        m = DEFINE.match(line)
        if m and m.group(1) not in first:
            first[m.group(1)] = n

    bad = []
    for n, line in enumerate(lines, 1):
        if line.startswith("\t") or line.lstrip().startswith("#"):
            continue
        m = RULE.match(line)
        if not m:
            continue
        for var in REF.findall(m.group(2)):
            at = first.get(var)
            if at and at > n:
                bad.append((n, m.group(1).strip(), var, at))
    return bad


COMPILES = re.compile(r'\$\((?:[A-Z0-9_]*CC|CC)\)|clang|gcc|\bas\b|vasm|vlink')


def check_relink(path):
    """A rule that COMPILES must list the Makefile among its prerequisites.

    Editing a Makefile changes the flags, the source list, the linker script
    name -- everything about the output -- and make cannot see that unless the
    Makefile is a prerequisite. Four of this project's five ports did not list
    it, and the cost was not theoretical: on 2026-09-10 an Atari Makefile edit
    left the game unrelinked, a test ran for twenty minutes against a binary
    that was not the one on disk, and the finding it produced had to be thrown
    away because the symbols came from a different link.

    Only the C128 had it right, which is why nothing had gone wrong there.
    """
    raw = open(os.path.join(HERE, path)).read().splitlines()
    # JOIN BACKSLASH CONTINUATIONS FIRST. The first version of this check read
    # one physical line, so a prerequisite list wrapped onto a second line was
    # invisible -- and it reported the very rule that had just been fixed.
    lines, i = [], 0
    while i < len(raw):
        line = raw[i]
        while line.endswith("\\") and i + 1 < len(raw):
            i += 1
            line = line[:-1] + " " + raw[i].strip()
        lines.append(line)
        i += 1
    bad, target, prereqs, n_at = [], None, "", 0
    for n, line in enumerate(lines, 1):
        if line.startswith("\t"):
            if target and COMPILES.search(line) and "Makefile" not in prereqs:
                bad.append((n_at, target))
                target = None            # report each rule once
            continue
        m = RULE.match(line)
        if m and not line.lstrip().startswith("#"):
            target, prereqs, n_at = m.group(1).strip(), m.group(2), n
    return bad


def main():
    files = sorted(glob.glob(os.path.join(HERE, "Makefile"))
                   + glob.glob(os.path.join(HERE, "*", "Makefile")))
    bad = 0
    for f in files:
        rel = os.path.relpath(f, HERE)
        for n, target, var, at in check(rel):
            bad += 1
            print("%s:%d: `%s:` uses $(%s), which is not defined until line %d"
                  % (rel, n, target, var, at))
            print("          Make expands prerequisites as it PARSES this "
                  "line, so $(%s) is EMPTY here and" % var)
            print("          the rule does not depend on it. Move the "
                  "definition above line %d." % n)
    # THE RELINK CHECK REPORTS; IT DOES NOT FAIL THE BUILD. Every game link
    # rule is fixed, and thirty-odd probe and smoke rules are not -- failing
    # `all` on those in the middle of other work is how a check gets deleted
    # rather than obeyed. A wall of warnings is not a report either, so this
    # prints a COUNT and the shipped rules only. The rest are open list item 19.
    soft = []
    for f in files:
        rel = os.path.relpath(f, HERE)
        soft += [(rel, n, t) for n, t in check_relink(rel)]
    if soft:
        print("check_makefiles: %d rule(s) compile without depending on their "
              "Makefile" % len(soft))
        print("          (probe and smoke builds; every game link rule now "
              "does -- see open list item 19)")

    if bad:
        print("\ncheck_makefiles: %d rule(s) silently depending on nothing"
              % bad)
        return 1
    print("check_makefiles: %d Makefiles, no variable used before it is defined"
          % len(files))
    return 0


sys.exit(main())

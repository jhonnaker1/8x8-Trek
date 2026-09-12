#!/usr/bin/env python3
"""Enforce core/overlay.h's rules on the CoCo 3's whole-file overlays.

WHAT IT READS: the link map, which says exactly which symbols ended up in the
window, and the GENERATED ASSEMBLY, which says exactly who calls them. Neither
is a guess about source text -- a `JSR _hof_offer` in a .s file is a call into
the window whatever the C looked like.

THE RULES IT CHECKS, from core/overlay.h:

  2. An overlay calls RESIDENT code only, never another overlay. They share
     one window, so the callee's image is not there.
  4. Only a declared caller may call into a window, and it must have loaded
     the right image. The load/call pairing cannot be proved from a call
     graph, so what is enforced is the checkable half: NOTHING CALLS INTO A
     WINDOW EXCEPT THE DECLARED PAIRS BELOW. That is rule 3's answer -- one
     entry per overlay through a resident stub that loads and calls together
     -- and it is what makes "forgot to load it" impossible rather than
     merely unlikely.

  And the corollary: an overlay may not call ovl_load, because it would page
  itself out mid-call.

THE C128 LEARNED THIS THE EXPENSIVE WAY. trek_score() was resident and its
whole body was an overlay function, called one statement after a different
image had been swapped in; the first game anyone played to the end dropped
the machine into its monitor. All three ports carried it.
"""
import os, re, sys

# (caller symbol, overlay symbol it may reach) -- every pair a deliberate,
# reviewed decision, exactly as tools/overlay_check.py does for the C128.
PAIRED = set()


def load_map(path, ovl_objects):
    """Symbols in the window, and which object each came from."""
    win_syms, all_syms = {}, {}
    sects = {}
    for line in open(path):
        m = re.match(r'Section: (\S+) \(([^)]*)\) load at ([0-9A-Fa-f]+), '
                     r'length ([0-9A-Fa-f]+)', line.strip())
        if m:
            sects.setdefault(m.group(1), []).append(
                (int(m.group(3), 16), int(m.group(4), 16), m.group(2)))
        m = re.match(r'Symbol: (\S+) \(([^)]*)\) = ([0-9A-Fa-f]+)', line.strip())
        if m:
            all_syms[m.group(1)] = (int(m.group(3), 16), m.group(2))

    ranges = []
    for name, lst in sects.items():
        if name.startswith("ovl"):
            for ad, ln, obj in lst:
                ranges.append((ad, ad + ln, os.path.basename(obj)))
    for sym, (ad, obj) in all_syms.items():
        base = os.path.basename(obj)
        for lo, hi, rowner in ranges:
            if base == rowner and lo <= ad < hi:
                win_syms[sym] = base
                break
    return win_syms


def calls_in(spath):
    """Every symbol this assembly calls. lwasm/cmoc emit JSR and LBSR."""
    out = set()
    for line in open(spath):
        m = re.search(r'\b(?:JSR|LBSR|JMP|LBRA)\s+(_\w+)', line)
        if m:
            out.add(m.group(1))
    return out


def main():
    mappath, intdir = sys.argv[1], sys.argv[2]
    ovl_srcs = sys.argv[3:]
    ovl_objs = {os.path.basename(s)[:-2] + ".o" for s in ovl_srcs}

    win = load_map(mappath, ovl_objs)
    if not win:
        sys.exit("overlay_check: the map lists NO symbols in a window -- "
                 "the parser is broken, which reports success it has not earned")

    bad, reach = [], {}
    for s in sorted(os.listdir(intdir)):
        if not s.endswith(".s"):
            continue
        obj = s[:-2] + ".o"
        called = calls_in(os.path.join(intdir, s))
        is_ovl = obj in ovl_objs

        for sym in sorted(called):
            if sym == "_ovl_load" and is_ovl:
                bad.append("%s is an OVERLAY and calls ovl_load -- it would "
                           "page itself out mid-call" % s)
            if sym not in win:
                continue
            owner = win[sym]
            if is_ovl:
                if owner != obj:
                    bad.append("%s (overlay) calls %s, which lives in %s -- "
                               "two overlays cannot share the window"
                               % (s, sym, owner))
            else:
                reach.setdefault(s, set()).add((sym, owner))

    print("overlay_check: %d symbols in the window" % len(win))
    if reach:
        print("resident code reaching into a window:")
        for s in sorted(reach):
            for sym, owner in sorted(reach[s]):
                ok = (s[:-2], sym) in PAIRED
                print("    %-22s -> %-24s (%s)%s"
                      % (s, sym, owner, "" if ok else "   UNDECLARED"))
    undeclared = sum(1 for s in reach for sym, _ in reach[s]
                     if (s[:-2], sym) not in PAIRED)
    for b in bad:
        print("overlay_check: FAIL -- " + b)
    if bad:
        return 1
    if undeclared:
        print("\noverlay_check: %d UNDECLARED call sites into a window.\n"
              "Each needs a load paired with it and a line in PAIRED, or the\n"
              "function moved. Rule 4 is not satisfied." % undeclared)
        return 1
    print("overlay_check: rules 2 and 4 hold")
    return 0


sys.exit(main())

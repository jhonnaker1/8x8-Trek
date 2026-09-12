#!/usr/bin/env python3
"""Enforce core/overlay.h's rules on the CoCo 3's overlays, PER FUNCTION.

WHAT IT READS: the GENERATED ASSEMBLY, which says which section each function
ended up in and exactly who it calls, and the LINK MAP, which says where each
symbol actually landed. Neither is a guess about source text -- a `JSR
_hof_offer` in a .s file is a call into the window whatever the C looked like.

IT USED TO WORK PER FILE, and that stopped being true. When this port could
only page whole translation units, "is this an overlay?" was a question about
an object file. Now that functions are lifted out of `SECTION code`
individually, ONE ui.s spans resident code and six different overlays, and a
file-level answer is wrong in both directions: it would clear a real
two-overlay collision inside ui.o and flag ui_hall_of_fame calling hof_offer,
which are in the same image.

THE RULES, from core/overlay.h:

  2. An overlay calls RESIDENT code only, never a DIFFERENT overlay. They
     share one window, so the other image is not there.
  4. Only a declared caller may call into a window. The load/call pairing
     cannot be proved from a call graph, so what is enforced is the checkable
     half: a resident function that calls into overlay X must ITSELF call
     load_x(). That is rule 3's answer -- the load and the call in one
     resident function -- and it makes "forgot to load it" impossible rather
     than merely unlikely. Genuine cross-function pairs are listed in PAIRED,
     one line each, as deliberate reviewed decisions.

  And the corollary: an overlay may not call ovl_load, because it would page
  itself out mid-call.

THE C128 LEARNED THIS THE EXPENSIVE WAY. trek_score() was resident and its
whole body was an overlay function, called one statement after a different
image had been swapped in; the first game anyone played to the end dropped the
machine into its monitor. All three ports carried it.
"""
import os, re, sys

# (calling function, symbol it may reach) -- pairs where the load() is NOT in
# the calling function, each a deliberate decision with its reason.
PAIRED = {
    # main()'s run_turn does `if (trek_events_due()) load_events();` and the
    # call itself is one frame deeper, inside trek_run_events. core/overlay.h
    # names this pairing explicitly: the guard and the load are together, and
    # trek_run_events is only ever reached through it.
    ("trek_run_events", "_run_events"),
}


def asm_sections(intdir):
    """(function, file) -> section. KEYED BY FILE, NOT BY NAME ALONE.

    TWO DIFFERENT STATIC FUNCTIONS CAN SHARE A NAME. c128/src/main.c and
    core/serial.c both define `static put_quad`, and cmoc emits `_put_quad`
    for both -- file-local in each object, so the linker is right and never
    complains. A dict keyed on the bare name is not: serial.s is read after
    main.s and silently overwrote it, and the checker then reported main.c's
    resident put_quad as living in serial.c's overlay. A false FAIL is not
    the harmless direction -- it is the one that gets a real check deleted."""
    where = {}
    for fname in sorted(os.listdir(intdir)):
        if not fname.endswith(".s"):
            continue
        sect = None
        for line in open(os.path.join(intdir, fname), errors="replace"):
            m = re.match(r'\s+SECTION\s+(\S+)', line)
            if m:
                sect = m.group(1)
            m = re.match(r'^_([A-Za-z_]\w*)\s+EQU\s+\*\s*$', line)
            if m:
                where[(m.group(1), fname)] = sect
    return where


def asm_calls(intdir):
    """(function, source) -> symbols it calls. Tracks the enclosing function
    by cmoc's own `_NAME EQU *` / `funcsize_NAME` brackets."""
    calls, cur, fn = {}, None, None
    for fname in sorted(os.listdir(intdir)):
        if not fname.endswith(".s"):
            continue
        fn = None
        for line in open(os.path.join(intdir, fname), errors="replace"):
            m = re.match(r'^_([A-Za-z_]\w*)\s+EQU\s+\*\s*$', line)
            if m:
                fn = m.group(1)
                calls.setdefault((fn, fname), set())
                continue
            if fn and re.match(r'^funcsize_%s\s+EQU' % re.escape(fn), line):
                fn = None
                continue
            m = re.search(r'\b(?:JSR|LBSR|JMP|LBRA)\s+(_\w+)', line)
            if m and fn:
                calls[(fn, fname)].add(m.group(1))
    return calls


def window_symbols(mappath):
    """Symbols the LINK actually placed in the window. Every overlay section
    loads at the same address, so this says IN a window, not WHICH -- the
    .s says which, and the two are cross-checked below."""
    lo = hi = None
    for line in open(mappath):
        m = re.match(r'Section: (ovl\w*) \([^)]*\) load at ([0-9A-Fa-f]+), '
                     r'length ([0-9A-Fa-f]+)', line.strip())
        if m:
            a, n = int(m.group(2), 16), int(m.group(3), 16)
            lo = a if lo is None else min(lo, a)
            hi = a + n if hi is None else max(hi, a + n)
    if lo is None:
        return set(), None, None
    syms = set()
    for line in open(mappath):
        m = re.match(r'Symbol: (\S+) \([^)]*\) = ([0-9A-Fa-f]+)', line.strip())
        if m and lo <= int(m.group(2), 16) < hi:
            syms.add(m.group(1))
    return syms, lo, hi


def main():
    mappath, intdir = sys.argv[1], sys.argv[2]
    where = asm_sections(intdir)
    calls = asm_calls(intdir)
    winsyms, lo, hi = window_symbols(mappath)

    if lo is None:
        sys.exit("overlay_check: the map lists NO overlay section -- "
                 "the parser is broken, which reports success it has not earned")

    ovl_of = {k: v for k, v in where.items() if v and v.startswith("ovl")}
    defined_in = {}
    for (fn, fname) in where:
        defined_in.setdefault(fn, []).append(fname)

    def overlay_of(callee, from_file):
        """Which overlay a called symbol is in, resolved FILE-LOCALLY FIRST --
        a static callee is the one defined in the calling file."""
        if (callee, from_file) in where:
            return ovl_of.get((callee, from_file))
        homes = defined_in.get(callee, [])
        if len(homes) == 1:
            return ovl_of.get((callee, homes[0]))
        return None        # ambiguous across files and not local: not ours
    if not ovl_of:
        sys.exit("overlay_check: no function is in an overlay section -- "
                 "the split did nothing and every rule below is vacuous")

    # CROSS-CHECK THE TWO SOURCES BEFORE TRUSTING EITHER. The assembly says
    # what was asked for; the map says what happened. A function marked for an
    # overlay that did not land in the window means the splitter emitted a
    # section the linker ignored -- it would stay resident and every rule
    # below would pass while the window paid for nothing.
    astray = sorted(f for (f, _) in ovl_of if "_" + f not in winsyms)
    if astray:
        sys.exit("overlay_check: %d function(s) put in an overlay section did "
                 "NOT land in the window: %s" % (len(astray), ", ".join(astray[:6])))

    bad, reach = [], []
    for (fn, src), called in sorted(calls.items()):
        mine = ovl_of.get((fn, src))
        for sym in sorted(called):
            if sym == "_ovl_load" and mine:
                bad.append("%s is in overlay %s and calls ovl_load -- it "
                           "would page itself out mid-call" % (fn, mine))
            callee = sym[1:]
            theirs = overlay_of(callee, src)
            if not theirs:
                continue
            if mine:
                if mine != theirs:
                    bad.append("%s (%s) calls %s (%s) -- two overlays cannot "
                               "share the window" % (fn, mine, sym, theirs))
            else:
                reach.append((fn, src, sym, theirs))

    # Rule 4's checkable half: the resident caller must load it itself.
    undeclared = []
    for fn, src, sym, theirs in reach:
        if (fn, sym) in PAIRED:
            continue
        loads = {c for c in calls[(fn, src)] if c.startswith("_load_")}
        if not loads:
            undeclared.append((fn, src, sym, theirs, "loads nothing"))

    print("overlay_check: %d functions in %d overlays, %d symbols in the "
          "window at $%04X" % (len(ovl_of), len(set(ovl_of.values())),
                               len(winsyms), lo))
    for b in bad:
        print("overlay_check: FAIL -- " + b)
    for fn, src, sym, theirs, why in undeclared:
        print("    %-24s -> %-24s (%s)  %s" % (fn, sym, theirs, why))
    if bad:
        return 1
    if undeclared:
        print("\noverlay_check: %d call site(s) reach a window from a resident\n"
              "function that loads nothing. Each needs its load_x() in the same\n"
              "function, or a reviewed line in PAIRED. Rule 4 is not satisfied."
              % len(undeclared))
        return 1
    print("overlay_check: rules 2 and 4 hold")
    return 0


sys.exit(main())

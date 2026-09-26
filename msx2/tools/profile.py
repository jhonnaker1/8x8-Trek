#!/usr/bin/env python3
"""Map tools/profile.tcl's PC samples to functions.

Labels come from the linker's RELOCATED LISTINGS (build/*.rst, made with
-Wl-u), which carry every label -- static functions too -- at its final
address; the map adds the runtime library's globals. A sample is charged to
the nearest label at or below its PC. A PC in page 0 with page 0 on slot 0
is the BIOS ROM, not the game, whatever address it shares with our code.

Usage: profile.py profile.txt build/probe.map build/*.rst"""
import collections, re, sys

prof, mapf, rsts = sys.argv[1], sys.argv[2], sys.argv[3:]
labels = {}
for r in rsts:
    for l in open(r, errors="replace"):
        m = re.match(r"^\s+([0-9A-F]{8})\s+\d+\s+(_[A-Za-z_0-9]+)::?\s*$", l)
        if m:
            labels[int(m.group(1), 16)] = m.group(2)
for l in open(mapf):
    m = re.match(r"^\s+([0-9A-F]{8})\s+(_[A-Za-z_0-9]+)\s", l)
    if m and int(m.group(1), 16) not in labels:
        labels[int(m.group(1), 16)] = m.group(2)
addrs = sorted(labels)
import bisect
def name(pc):
    i = bisect.bisect_right(addrs, pc) - 1
    return labels[addrs[i]] if i >= 0 else "?"

samples = [l.split() for l in open(prof)]
c = collections.Counter()
for t, pc, slot in samples:
    pc, slot = int(pc, 16), int(slot)
    if pc < 0x4000 and slot == 0:
        c["[BIOS ROM, page 0]"] += 1
    elif pc >= 0xDB06:
        c["[DOS / system, page 3]"] += 1
    elif 0x4000 <= pc < 0x8000 and slot != 3:
        c["[ROM in page 1 -- disk / Nextor]"] += 1
    else:
        c[name(pc)] += 1
n = len(samples)
t0, t1 = float(samples[0][0]), float(samples[-1][0])
print("%d samples over %.2fs of emulated time" % (n, t1 - t0))
for f, k in c.most_common(25):
    print("  %5.1f%%  %6d  %s" % (100.0 * k / n, k, f))

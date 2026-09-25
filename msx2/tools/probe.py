#!/usr/bin/env python3
"""Report the whole game's footprint against the MSX-DOS TPA.

Reads the linker map for where the image ENDS -- the last byte of any area --
rather than summing sizes, so gaps and ordering cannot fool it. The stubs are
subtracted and named, because they are what the unwritten drivers replace."""
import re, sys
mapf, stubs_rel, tpa_top = sys.argv[1], sys.argv[2], int(sys.argv[3], 0)
end = 0
for line in open(mapf):
    m = re.match(r'^(_\w+)\s+([0-9A-F]{8})\s+([0-9A-F]{8})\s', line)
    if m and not m.group(1).startswith('_HEADER'):
        a, n = int(m.group(2), 16), int(m.group(3), 16)
        if n: end = max(end, a + n)
stubs = 0
for line in open(stubs_rel, errors='replace'):
    m = re.match(r'^A (\S+) size (\S+)', line)
    if m: stubs += int(m.group(2), 16)
used = end - 0x100
print("  image $0100..$%04X          %6d bytes" % (end - 1, used))
print("  of which unwritten stubs    %6d" % stubs)
print("  MSX-DOS TPA $0100..$%04X    %6d   AS THE SHELL -- under COMMAND2\n"
      "                                        it is 1,280 less (make shell)" % (tpa_top - 1, tpa_top - 0x100))
left = tpa_top - end
print("  LEFT for the unwritten drivers and the stack: %d bytes%s" %
      (left, "" if left > 0 else "  -- OVER"))

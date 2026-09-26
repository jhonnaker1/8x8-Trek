#!/usr/bin/env python3
"""`make verify` for the MSX2: STATIC, and it prints the numbers.

Two failures here would be silent on the machine, so they are checked where
they cost nothing:

  * THE BUDGET. crt0.s refuses to start unless ($0006) clears the image end by
    STACK_RESERVE. As the shell ($0006) is $DB06 on every machine measured; if
    a build ever leaves less than the reserve under that, the game would
    refuse on the very machine it was made for. So the arithmetic is checked
    here against the same numbers, read from the same places: the linker map
    for the image end, crt0.s for the reserve, the Makefile's TPA_TOP.
  * THE STRING COUNT. strpool.c refuses a STRINGS.DAT whose header disagrees
    with the STR_COUNT compiled in, and the game then plays with every label
    blank and no error. The two are generated together; this checks they
    were.

Usage: verify_msx2.py probe.map crt0.s strdata.h strings.dat TPA_TOP"""
import re, sys

mapf, crt0, strdata, dat, tpa = sys.argv[1:6]
tpa = int(tpa, 0)

end = 0
for line in open(mapf):
    m = re.match(r'^(_\w+)\s+([0-9A-F]{8})\s+([0-9A-F]{8})\s', line)
    if m and not m.group(1).startswith('_HEADER'):
        a, n = int(m.group(2), 16), int(m.group(3), 16)
        if n: end = max(end, a + n)
m = re.search(r'^STACK_RESERVE\s*=\s*(\d+)', open(crt0).read(), re.M)
if not m:
    sys.exit("verify: no STACK_RESERVE in %s" % crt0)
reserve = int(m.group(1))
left = tpa - end
bad = []
print("verify: image $0100..$%04X, %d free as the shell, %d reserved for the stack (208 measured), %d spare"
      % (end - 1, left, reserve, left - reserve))
if left < reserve:
    bad.append("the image leaves %d under ($0006) = $%04X and crt0 needs %d -- the game would refuse to start"
               % (left, tpa, reserve))

m = re.search(r'#define\s+STR_COUNT\s+(\d+)', open(strdata).read())
want = int(m.group(1))
b = open(dat, 'rb').read(2)
got = b[0] | (b[1] << 8)
print("verify: STRINGS.DAT holds %d strings, the binary expects %d" % (got, want))
if got != want:
    bad.append("STRINGS.DAT disagrees with STR_COUNT -- the game would play with every label blank")

if bad:
    sys.exit("verify: FAILED\n  " + "\n  ".join(bad))

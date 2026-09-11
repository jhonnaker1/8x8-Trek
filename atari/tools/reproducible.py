#!/usr/bin/env python3
"""Link the game N times and say whether they all agree.

THE ATARI LINK IS NOT REPRODUCIBLE and this turns that from an invisible
property into a reported one.

    make reproducible

N IS EIGHT, NOT TWO, AND THAT IS A REPAIR RATHER THAN A SETTING. This file
used to link TWICE. Measured over thirty links on 2026-09-11 the split is
19/11 -- so two links agree about 53% of the time, and the old check reported
"reproducible THIS TIME" MORE OFTEN THAN NOT about a build that never is.
A test that samples twice cannot see a coin. At eight links the chance of a
false all-agree is 0.63^8 + 0.37^8, about 2.5%.

It poisons the other direction too: every "I tried X and it did not help"
below was re-run at TWELVE links each on 2026-09-11, because four links
agreeing by chance is 18% and a two-link verdict on a candidate fix is worth
nothing at all.

WHAT IT IS, as narrowly as it has been pinned (2026-09-11):

  * Two distinct binaries, differing by exactly 20 bytes in exactly ONE
    function -- `ui_draw_position`, 243 or 263. Every other symbol identical.
  * NOT the LTO optimiser. With `-Wl,--save-temps` every bitcode stage --
    preopt, internalize, opt, precodegen -- is byte-identical across runs.
    Only the codegen output, `*.lto.o`, differs. The optimiser is exonerated.
  * NOT register allocation. Dumping the function after all 148 machine
    passes, everything through regalloc is byte-identical; the FIRST pass
    whose output differs is Prologue/Epilogue Insertion.
  * WHAT DIFFERS is where one local lives: a zero-page static-stack slot at
    $E5, or callee-saved `__rc24`/`__rc25` -- which PEI must then save and
    restore, and that pair of save/restores is the 20 bytes.
  * THE ZERO PAGE IS EXACTLY SATURATED: .zp.data 3 + .zp.bss 66 + .zp 27 = 96
    bytes in a 96-byte region, $A0..$FF. Taking FOUR bytes off it fails the
    link. So the zero-page slots are contested, and which function wins one is
    not decided the same way twice.
  * It happens only inside lld. The same module, codegenned by a standalone
    `clang -c -fno-lto -x ir`, is identical 12 runs out of 12.

WHAT DOES NOT HELP, each re-verified at twelve links: `--threads=1`,
`--lto-partitions=1`, `--lto-O0`/`O1`/`O3`, `--lto-CGO1`/`CGO3`, and
`-enable-ipra=false` (a no-op here -- the same two hashes as baseline). The
machine outliner was ruled out in 2026-09-10's pass.
`-enable-shrink-wrap=false` makes it WORSE: four variants instead of two.

AND ONE THING THAT WORKS AND IS STILL NOT A FIX. Splitting the build into
three -- `--lto-emit-llvm` to combined bitcode (deterministic, 8/8), then
`clang -c -fno-lto -x ir` to an object (deterministic, 12/12), then a plain
link -- is reproducible 10 runs out of 10. It costs about 950 bytes, and the
reason is not overhead: compiling the post-LTO module as an ordinary
translation unit loses llvm-mos's zero-page allocation ENTIRELY. `.zp.data`,
`.zp.bss` and `.zp` all go to zero, 96 bytes of zero page unused. That is not
a reproducible build of this program; it is a worse compiler.

WHY IT MATTERS ENOUGH TO CHECK. A binary you cannot reproduce is a bug report
you cannot pin. It cost an hour on 2026-09-10: a resident figure moved by 20
bytes between builds, was read as a stale link, and a correct finding was
retracted on the strength of it.
"""
import collections
import hashlib
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LINKS = int(os.environ.get("REPRO_LINKS", "8"))


def sizes(mapfile):
    out = {}
    for line in open(mapfile):
        f = line.split()
        if len(f) == 5 and ":(.text." in f[4]:
            out[f[4].split(":(.text.")[1].rstrip(")")] = int(f[2], 16)
    return out


def link(tag):
    xex = "/tmp/repro-%s.xex" % tag
    mp = "/tmp/repro-%s.map" % tag
    r = subprocess.run(["make", "-C", HERE, "--no-print-directory",
                        "GAME_OUT=" + xex, "-B", "game"],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit("reproducible: the link failed:\n"
                 + r.stdout[-800:] + r.stderr[-800:])
    built = os.path.join(HERE, "build", "trekatari.map")
    if os.path.exists(built):
        subprocess.run(["cp", built, mp])
    return hashlib.md5(open(xex, "rb").read()).hexdigest(), mp


def main():
    seen = collections.OrderedDict()          # md5 -> [count, mapfile]
    for i in range(LINKS):
        h, mp = link(str(i))
        if h in seen:
            seen[h][0] += 1
        else:
            seen[h] = [1, mp]
    for h, (n, _) in seen.items():
        print("reproducible: %2d of %d  %s" % (n, LINKS, h))

    if len(seen) == 1:
        chance = 100 * (0.63 ** LINKS + 0.37 ** LINKS)
        print("reproducible: all %d links agree -- THIS TIME." % LINKS)
        print("              At the measured 19/11 split, %d agreeing by "
              "chance is about %.1f%%." % (LINKS, chance))
        print("              Agreeing is not proof. DISAGREEING is.")
        return 0

    print("reproducible: %d DISTINCT BINARIES -- open list item 19."
          % len(seen))
    maps = [m for _, (_, m) in seen.items()]
    base = sizes(maps[0])
    for m in maps[1:]:
        other = sizes(m)
        for k in sorted(set(base) | set(other)):
            if base.get(k) != other.get(k):
                print("              %-24s %s vs %s"
                      % (k, base.get(k), other.get(k)))
    print("              Both are valid and pass every check, so this reports "
          "a known property")
    print("              rather than failing on it. The header says how far "
          "it has been pinned.")
    return 0


sys.exit(main())

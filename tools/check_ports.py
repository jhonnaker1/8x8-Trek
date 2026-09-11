#!/usr/bin/env python3
"""Build and verify every port, and REPORT THE EXIT STATUS OF EACH.

WHY THIS EXISTS. Checking five ports by hand means five shell lines, and the
convenient shape of that line is `cd x && make verify 2>&1 | tail`. THE PIPE
REPORTS TAIL'S STATUS, NOT MAKE'S. A failed build prints its error, scrolls
past, and reads as a pass -- which is how `make test` went two days without
compiling on this project, and how tools/budget.py printed "IT LINKS, 327 bytes
spare" over a failed link, twice in one session. I did it twice more on
2026-09-10 while testing a check written to catch exactly this.

So the fix is not discipline, it is not having to be disciplined: one command
that runs each port's own gate, captures the return code, and says PASS or FAIL
per port with a non-zero exit if any failed. There is no pipe to get wrong.

    make ports          every port
    make ports P=atari  just one

It runs each port's REAL gate rather than a summary of its own: `make verify`
where a port has one, `make` where it does not (the Amiga has no overlays to
verify). Nothing here knows what "verified" means for a given machine -- that
knowledge stays in the port.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (directory, make target). The Amiga has no verify: with no overlays, no
# window and no staging regions there is nothing for one to check, so its
# build IS its gate. Saying that here beats inventing an empty verify there.
PORTS = (
    ("c128",   "verify"),
    ("x16",    "verify"),
    ("mega65", "verify"),
    ("atari",  "verify"),
    ("amiga",  None),
)

# Lines worth surfacing from a passing run: the numbers that go stale when
# nobody looks at them. A port that stops printing one is not a failure here,
# but it is the thing this project has had to re-derive by hand four times.
KEEP = ("resident", "free", "spare", "largest overlay", "lowram", "soft stack")


def run(port, target):
    cmd = ["make"] + ([target] if target else [])
    p = subprocess.run(cmd, cwd=os.path.join(HERE, port),
                       capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def main():
    only = None
    for a in sys.argv[1:]:
        if a.startswith("P="):
            only = a[2:]
        elif a:
            only = a

    todo = [(d, t) for d, t in PORTS if only in (None, d)]
    if not todo:
        sys.exit("check_ports: no port named %r -- have %s"
                 % (only, ", ".join(d for d, _ in PORTS)))

    bad = []
    for port, target in todo:
        code, out = run(port, target)
        what = "make " + (target or "(build)")
        if code:
            bad.append(port)
            print("FAIL  %-7s %-14s exit %d" % (port, what, code))
            tail = [l for l in out.splitlines() if l.strip()][-12:]
            for l in tail:
                print("        | " + l)
        else:
            print("ok    %-7s %-14s" % (port, what))
            for l in out.splitlines():
                low = l.lower()
                if any(k in low for k in KEEP) and "verify:" in low:
                    print("        " + l.split("verify:", 1)[1].strip())

    print()
    if bad:
        print("check_ports: %d of %d FAILED -- %s"
              % (len(bad), len(todo), ", ".join(bad)))
        return 1
    print("check_ports: %d of %d ok" % (len(todo), len(todo)))
    return 0


sys.exit(main())

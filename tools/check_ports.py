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

It runs from `all`, so it is not something anybody has to remember -- which was
the whole complaint about the first version. A port whose cross compiler is not
installed is SKIPPED, naming the variable and the path it looked for; only a
port that could have been built and was not counts as a failure.

It runs each port's REAL gate rather than a summary of its own: `make verify`
where a port has one, `make` where it does not (the Amiga has no overlays to
verify). Nothing here knows what "verified" means for a given machine -- that
knowledge stays in the port.
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (directory, make target, the Makefile variable naming its compiler).
#
# The Amiga has no verify: with no overlays, no window and no staging regions
# there is nothing for one to check, so its build IS its gate. Saying that here
# beats inventing an empty verify there.
#
# The third field is what lets this run from `all` on a machine with none of
# the five cross compilers. It is a VARIABLE NAME, not a path, because the path
# is the port's business and differs per port -- ask the repo, not the
# filesystem.
PORTS = (
    ("c128",   "verify", "MOSCC"),
    ("x16",    "verify", "X16CC"),
    ("mega65", "verify", "M65CC"),
    ("atari",  "verify", "ATARICC"),
    ("amiga",  None,     "CC"),
)

# Lines worth surfacing from a passing run: the numbers that go stale when
# nobody looks at them. A port that stops printing one is not a failure here,
# but it is the thing this project has had to re-derive by hand four times.
KEEP = ("resident", "free", "spare", "largest overlay", "lowram", "soft stack")


def compiler(port, var):
    """Where this port's Makefile says its compiler is, expanded by make.

    ASKING THE REPO RATHER THAN THE PATH. Every port names its compiler
    differently and resolves it through its own variables ($(LLVM_MOS),
    $(AMIGA_TOOLCHAIN), $(HOME)), so the only honest way to learn the path is
    to let that Makefile expand it. `-f -` appends a rule read from stdin,
    which needs no cooperation from the port.
    """
    rule = "trek_print_cc:\n\t@echo $(%s)\n" % var
    p = subprocess.run(["make", "-C", os.path.join(HERE, port),
                        "--no-print-directory", "-f", "Makefile", "-f", "-",
                        "trek_print_cc"],
                       input=rule, capture_output=True, text=True)
    return p.stdout.strip() if p.returncode == 0 else ""


def have(path):
    if not path:
        return False
    return os.path.exists(path) or bool(shutil.which(path))


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

    todo = [(d, t, v) for d, t, v in PORTS if only in (None, d)]
    if not todo:
        sys.exit("check_ports: no port named %r -- have %s"
                 % (only, ", ".join(d for d, _, _ in PORTS)))

    bad, skipped = [], []
    for port, target, var in todo:
        # A MISSING CROSS COMPILER IS A SKIP, NOT A FAILURE, and that
        # distinction is what lets this run from `all` on every build. Five
        # toolchains are installed on the machine this was written on; a fresh
        # clone has none, and a gate that turns `make` red for a stranger is a
        # gate that gets deleted. A port whose compiler IS present gets no
        # such mercy.
        cc = compiler(port, var)
        if not have(cc):
            skipped.append(port)
            print("skip  %-7s no %s at %s" % (port, var, cc or "(unset)"))
            continue
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
    ran = len(todo) - len(skipped)
    if skipped:
        print("check_ports: %d ok, %d skipped for a missing toolchain (%s)"
              % (ran, len(skipped), ", ".join(skipped)))
    else:
        print("check_ports: %d of %d ok" % (ran, len(todo)))
    return 0


sys.exit(main())

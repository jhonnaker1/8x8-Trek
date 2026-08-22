#!/usr/bin/env python3
"""Bisect NOTES.md open item 2: what stops this port returning to BASIC?

Builds c128/test/exit_bisect.c one stage at a time, runs each under x128 with
the binary monitor open, and then asks BASIC to do arithmetic.

WHY ARITHMETIC AND NOT A SCREENSHOT. A wedged C128 still shows READY -- it was
printed before the program was RUN, and nothing erases it. Liveness is not
enough either: the KERNAL's 60Hz IRQ keeps bumping the jiffy clock even when
BASIC is dead, so tools/vice_mon.py's `live` check would say yes to a machine
that will never accept another command. Typing `print 6*7` and finding "42" in
40-column screen RAM proves the editor took the keys AND that BASIC evaluated
them. 42 cannot come from the echo of what was typed.

    python3 tools/exit_bisect.py          # all stages
    python3 tools/exit_bisect.py 2        # just one
"""
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vice_mon
from vice_mon import Mon, CMD_KEYBOARD_FEED, screenshot

ROOT   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD  = os.path.join(ROOT, "c128", "build")
SHOTS  = os.path.join(BUILD, "bisect")

STAGES = {
    0: "control -- nothing but return 0",
    1: "+ 2MHz switch (D030 bit 0)",
    2: "+ VDC register writes (vdc_init/vdc_shutdown)",
    3: "+ CIA1 matrix scan (SEI, poke PRA, read PRB, CLI)",
}

VIC_SCREEN = 0x0400
VIC_LEN    = 1000


def build(stage):
    out = os.path.join(BUILD, "exit_bisect%d.prg" % stage)
    src = [os.path.join(ROOT, "c128", "test", "exit_bisect.c")]
    if stage >= 2:
        src.append(os.path.join(ROOT, "c128", "src", "vdc.c"))
    cmd = ["cl65", "-t", "c128", "-O", "-DSTAGE=%d" % stage, "-o", out] + src
    subprocess.run(cmd, check=True, cwd=ROOT)
    return out


def feed(mon, text):
    """VICE's KEYBOARD_FEED: one length byte, then the text. This goes into the
    KERNAL's keyboard buffer, which is exactly what we want here -- we are
    testing the KERNAL and BASIC, not the port's own CIA scanning."""
    # UPPERCASE ascii, deliberately. VICE feeds these bytes as PETSCII, and
    # PETSCII $50 is the 'P' BASIC's parser wants; lowercase ascii $70 lands in
    # the shifted range, which DISPLAYS as an uppercase P on the C128's boot
    # charset and then earns a ?SYNTAX ERROR. That error was itself the first
    # proof the machine was alive, but it is not the signal we want to assert.
    payload = text.encode("ascii")
    mon.cmd(CMD_KEYBOARD_FEED, bytes([len(payload)]) + payload)
    mon.resume()


def find_answer(mem, want=b"42"):
    """Screen codes for digits are their ASCII values, so "42" is 34 32."""
    return mem.find(want) >= 0


def run_stage(stage, keep=False):
    prg = build(stage)
    proc = subprocess.Popen(
        ["x128", "-binarymonitor", "-binarymonitoraddress", "ip4://127.0.0.1:6502",
         "-autostart", prg],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(12)          # autostart loads, types RUN, and the stage runs
        mon = Mon()
        before = mon.mem_get(VIC_SCREEN, VIC_LEN)
        screenshot(mon, os.path.join(SHOTS, "stage%d-before.png" % stage), use_vicii=1)

        feed(mon, "PRINT 6*7\r")
        time.sleep(2.5)

        after = mon.mem_get(VIC_SCREEN, VIC_LEN)
        screenshot(mon, os.path.join(SHOTS, "stage%d-after.png" % stage), use_vicii=1)
        mon.resume()

        alive = find_answer(after)
        changed = before != after
        return alive, changed
    finally:
        if not keep:
            proc.terminate()
            proc.wait(timeout=10)


def main():
    os.makedirs(SHOTS, exist_ok=True)
    wanted = [int(a) for a in sys.argv[1:]] or sorted(STAGES)
    results = []
    for stage in wanted:
        print("\n=== stage %d: %s ===" % (stage, STAGES[stage]))
        alive, changed = run_stage(stage)
        verdict = "BASIC ALIVE" if alive else ("screen moved, no answer"
                                               if changed else "WEDGED")
        print("  -> %s" % verdict)
        results.append((stage, alive, changed))

    print("\n%-6s %-46s %s" % ("stage", "adds", "returns to BASIC"))
    for stage, alive, changed in results:
        print("%-6d %-46s %s"
              % (stage, STAGES[stage], "yes" if alive else
                 ("partial" if changed else "NO")))


if __name__ == "__main__":
    main()

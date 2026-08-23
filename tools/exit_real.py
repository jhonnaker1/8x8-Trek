#!/usr/bin/env python3
"""Does the REAL port get all the way out? Regression test for the whole
end-of-game path: quit, evaluation, hall of fame, "Play Again?", and BASIC.

tools/exit_bisect.py clears the three suspects that note named, one at a time,
in a program small enough that nothing else can be blamed. This one is the
other half: it builds the whole port, plays it through to a quit with
kb_inject, and then asks BASIC to do arithmetic. 42 on the 40-column screen is
the only proof that counts -- see the long comment at the end of main().

It plays TWO games on purpose. The first answers YES at the play-again prompt,
which must land back on the title screen with setup asked again; the second
answers NO, which must reach BASIC. Answering NO is now the only way out, so
NOTES item 2 is still covered -- it just goes through one more door.

Needs the debug build, because scripted input is what plays it to the quit.
That build differs from the release binary by one byte of state and the poll
that reads it; the exit path is identical.
"""
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vice_mon import Mon, CMD_KEYBOARD_FEED, screenshot, symbol

ROOT  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "c128", "build")
SHOTS = os.path.join(BUILD, "bisect")
PRG   = os.path.join(BUILD, "trek128-exit.prg")
MAP   = os.path.join(BUILD, "trek128-exit.map")

# Kept in step with c128/Makefile by hand. It drifted once already: adding the
# SID driver broke this script and not the build, because the build has the
# list and this has a copy.
SRC = ["src/main.c", "src/vdc.c", "src/layout.c", "src/ui.c", "src/input.c",
       "src/sid.c", "src/music_data.c", "../core/trek.c"]

# title, briefing=no, restore=no, name, level, password, then Q and the two
# end-of-game screens. RETURN alone answers "no" to the Y/N prompts because
# ask_yes() only counts an explicit Y.
# (keystroke, seconds to wait after it, screenshot name or None).
# Generous waits, and a checkpoint shot at each screen boundary: kb_inject is a
# single byte with no handshake -- vice_mon's own notes say a read-back of it
# lies -- so poking faster than the port polls silently loses characters. The
# first attempt used a flat 0.45s and desynced somewhere in the title, which
# looked exactly like the game ignoring Q.
SCRIPT = (
    # ---- game one, answered YES ----
    [("\r", 4.0, "step1-title")] +
    [(c, 1.0, None) for c in "\r\rKIRK\r3\rX\r"] +
    [("", 3.0, "step2-setup")] +
    [(c, 1.2, None) for c in "Q\r"] +          # quit
    [("Y", 1.5, None)] +                       # ... and confirm it
    [(c, 1.5, None) for c in "\r\r"] +         # evaluation, hall of fame
    [("", 2.0, "step3-playagain")] +           # the prompt itself
    [("Y", 3.0, "step4-title-again")] +        # YES goes back to the title
    # ---- game two, answered NO ----
    [(c, 1.0, None) for c in "\r\r\rSPOCK\r2\rQ\r"] +
    [("", 2.5, "step5-setup2")] +
    [(c, 1.2, None) for c in "Q\r"] +
    [("Y", 1.5, None)] +
    [(c, 1.5, None) for c in "\r\r"] +
    [("", 2.0, "step6-playagain2")] +
    [("N", 3.0, "step7-exit")])


def build():
    subprocess.run(["cl65", "-t", "c128", "-O", "-DTREK_DEBUG_INPUT",
                    "-m", MAP, "-o", PRG] + SRC,
                   check=True, cwd=os.path.join(ROOT, "c128"))


def main():
    os.makedirs(SHOTS, exist_ok=True)
    build()
    addr, path = symbol("kb_inject", maps=(MAP,))
    print("kb_inject = $%04X" % addr)

    proc = subprocess.Popen(
        ["x128", "-binarymonitor", "-binarymonitoraddress", "ip4://127.0.0.1:6502",
         "-autostart", PRG],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(14)
        mon = Mon()
        screenshot(mon, os.path.join(SHOTS, "real-title.png"))

        for ch, wait, shot in SCRIPT:
            if ch:
                mon.mem_set(addr, bytes([ord(ch)]))
            time.sleep(wait)
            if shot:
                screenshot(mon, os.path.join(SHOTS, shot + ".png"))
                print("  checkpoint %s" % shot)

        screenshot(mon, os.path.join(SHOTS, "real-vdc-after.png"))
        before = mon.mem_get(0x0400, 1000)
        mon.cmd(CMD_KEYBOARD_FEED, bytes([10]) + b"PRINT 6*7\r")
        mon.resume()
        time.sleep(2.5)
        after = mon.mem_get(0x0400, 1000)
        screenshot(mon, os.path.join(SHOTS, "real-vic-after.png"), use_vicii=1)
        mon.resume()

        print("BASIC alive:", "YES" if after.find(b"42") >= 0 else
              ("no (screen moved)" if before != after else "NO -- WEDGED"))
    finally:
        proc.terminate()
        proc.wait(timeout=10)


if __name__ == "__main__":
    main()

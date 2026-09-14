#!/usr/bin/env python3
"""Drive the 40-column game past setup and photograph the REAL console.

The pictures from `make shot40` are an empty frame drawn by a test program.
This one boots the actual game off the actual disk -- overlays, string pool,
SID and all -- types its way through the title and setup, and captures the
console with real content in it. That is the only thing that proves the
shared UI renders at 40 columns; everything before it proved geometry.

kb_inject IS THE ONLY WAY IN. VICE's KEYBOARD_FEED fills the KERNAL's buffer
and c128/src/input.c scans the CIA1 matrix directly, so fed keys never reach
the port. kb_inject is a byte the port polls, compiled in only under
-DTREK_DEBUG_INPUT -- hence a separate disk, which `make d64-40-debug` builds.

THE ADDRESS IS READ FROM THIS BUILD'S MAP, never hardcoded: it has moved four
times in the 80-column build's life, and a stale address injects into
whatever now lives there and reports nothing wrong.
"""
import os, re, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
C128 = os.path.dirname(HERE)
ROOT = os.path.dirname(C128)
sys.path.insert(0, os.path.join(ROOT, "tools"))

D64 = os.path.join(C128, "build", "trek128-40-debug.d64")
MAP = os.path.join(C128, "build", "trek128-40-debug.map")
SETTLE = 0.45

# Title, then skip the briefing, then take every setup default. Screenshots
# are taken BETWEEN steps so a sequence that goes wrong says where.
# READ OFF THE SCREEN, not guessed. The first attempt sent RETURN at "RESTORE
# A SAVED GAME (Y/N)?" -- a Y/N prompt ignores it, so every key after that
# queued against a prompt that would never take them and the run looked like
# kb_inject had stopped working. Screenshotting BETWEEN steps is what showed
# which prompt was actually on screen.
# EVERY PROMPT IS A LINE EDITOR, INCLUDING THE Y/N ONES -- type the answer and
# press RETURN. README-release.txt says so in as many words and I still sent
# bare "n" first, which echoed on screen and submitted nothing; the keys after
# it then landed on prompts they were not meant for. Reading the screen
# between steps is what showed the N sitting there with the cursor still after
# it. A prompt that ECHOES is not a prompt that has ACCEPTED.
STEPS = [("title",       ""),
         ("briefing-q",  "\r"),    # past the title
         ("no-briefing", "n\r"),   # WILL YOU REQUIRE A BRIEFING (Y/N)?
         ("no-restore",  "n\r"),   # RESTORE A SAVED GAME (Y/N)?
         ("named",       "\r"),    # PLEASE ENTER YOUR NAME: (default)
         ("level",       "1\r"),   # COMMAND LEVEL (1-5)
         ("console",     "ABC\r")] # SELF-DESTRUCT PASSWORD -- then the game


def kb_inject_addr():
    for line in open(MAP):
        f = line.split()
        if len(f) == 5 and f[4] == "kb_inject":
            return int(f[0], 16)
    sys.exit("shot40game: kb_inject not in %s -- is this a DEBUG build?" % MAP)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(C128, "build")
    for p in (D64, MAP):
        if not os.path.exists(p):
            sys.exit("shot40game: missing %s -- run `make d64-40-debug`" % p)
    addr = kb_inject_addr()
    print("  kb_inject at $%04X" % addr)

    vice = subprocess.Popen(
        ["x128", "-binarymonitor",
         "-binarymonitoraddress", "ip4://127.0.0.1:6502", "-autostart", D64],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(24)                      # boot, autostart, load the pool
        import vice_mon
        mon = vice_mon.Mon()
        for name, keys in STEPS:
            for ch in keys:
                mon.mem_set(addr, bytes([ord(ch)]))
                time.sleep(SETTLE)
            time.sleep(1.2)
            path = os.path.join(out, "g40-%s.png" % name)
            w, h = vice_mon.screenshot(mon, path, use_vicii=1)
            # AND THE SCREEN RAM, because the PNG has lied. On 2026-09-13 the
            # console capture came back BLACK while $0400 held 364 non-blank
            # cells, stable across twenty seconds -- Jamie was watching the
            # emulator window and saw the console perfectly well. A display
            # grab is one more instrument; screen RAM is the thing itself.
            scr = mon.mem_get(0x0400, 1000)
            live = sum(1 for b in scr if b not in (32, 0))
            print("  %-16s %-42s  %4d live cells" % (name, path, live))
    finally:
        vice.send_signal(signal.SIGKILL)
        vice.wait()
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Drive the C64 game past setup and photograph the REAL console.

The C128's tools/shot40game.py, pointed at x64sc and this port's debug disk.
Everything it learned applies here unchanged and is not restated: kb_inject is
the only way in (input.c scans the CIA1 matrix, so VICE's KEYBOARD_FEED never
reaches it), EVERY PROMPT IS A LINE EDITOR including the Y/N ones, screens are
waited for by watching screen RAM STOP CHANGING rather than by sleeping, and
the screen text is printed beside every PNG because a display grab has lied
about a console that was working.

IT LEAVES VICE RUNNING BY DEFAULT. --kill is for automation. Jamie: "your
script kills it too fast."
"""
import os, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
C64 = os.path.dirname(HERE)
ROOT = os.path.dirname(C64)
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, HERE)
from boot import decode

D64 = os.path.join(C64, "build", "trek64-debug.d64")
MAP = os.path.join(C64, "build", "trek64-debug.map")
SETTLE = 0.45
POLL = 0.4
QUIET = 13        # 13 polls x 0.4s: the screen must be still for ~5 seconds

STEPS = [("title",       ""),
         ("briefing-q",  "\r"),
         ("no-briefing", "n\r"),
         ("no-restore",  "n\r"),
         ("named",       "\r"),
         ("level",       "1\r"),
         ("console",     "ABC\r")]


def kb_inject_addr():
    for line in open(MAP):
        f = line.split()
        if len(f) == 5 and f[4] == "kb_inject":
            return int(f[0], 16)
    sys.exit("play: kb_inject not in %s -- is this a DEBUG build?" % MAP)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    kill = "--kill" in sys.argv
    show = "--quiet" not in sys.argv

    # --steps NAME=KEYS,NAME=KEYS,... replaces the default run. `\r` in the
    # keys is a RETURN. Every prompt in this game is a LINE EDITOR, including
    # the Y/N ones, so an answer is the letter AND a return -- a bare "n"
    # echoes and submits nothing, which is how the first drive of the C128
    # build looked like kb_inject had stopped working.
    global STEPS
    for a in sys.argv[1:]:
        if a.startswith("--steps="):
            STEPS = [(p.split("=", 1)[0], p.split("=", 1)[1].replace("\\r", "\r"))
                     for p in a[len("--steps="):].split(",")]
    out = args[0] if args else os.path.join(C64, "build")
    for p in (D64, MAP):
        if not os.path.exists(p):
            sys.exit("play: missing %s -- run `make d64-debug`" % p)
    addr = kb_inject_addr()
    print("  kb_inject at $%04X" % addr)

    vice = subprocess.Popen(
        ["x64sc", "-binarymonitor",
         "-binarymonitoraddress", "ip4://127.0.0.1:6502",
         "-autostart", D64 + ":trek64"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(12)
        import vice_mon
        mon = vice_mon.Mon()

        def screen():
            return mon.mem_get(0x0400, 1000)

        def live(s=None):
            s = screen() if s is None else s
            return sum(1 for b in s if b not in (32, 0))

        def settle_screen(was=None, timeout=30.0):
            """Wait for the screen to CHANGE, then to stop changing.

            Settling alone is not enough and this port proved it on its first
            run: after the last setup answer the screen is already stable --
            it is the setup screen -- while the machine is loading four
            overlays off a 1541 to draw the console. Three agreeing polls
            arrive in a second, the shot is taken of the OLD screen, and the
            console reads as "never drew". The screen bench hit exactly this
            and grew the same fix.

            AND WAITING FOR *A* CHANGE IS NOT ENOUGH EITHER, which is the
            second half of the same lesson and cost a second run. Typing
            "ABC" at the password prompt CHANGES THE SCREEN -- the prompt
            echoes it -- so a detector watching for any change fires on the
            echo, settles on the setup screen, and photographs it while the
            console is still four overlay loads away. The console arrived
            2.5 seconds later and a separate diagnostic saw it at 532 cells.

            So the rule is QUIET FOR FIVE SECONDS, not "three polls agree".
            An echo is a change followed immediately by stillness; a screen
            transition on this machine is a change followed by more changes
            as each overlay lands.
            """
            t0 = time.time()
            if was is not None:
                while time.time() - t0 < timeout and live() == was:
                    time.sleep(0.3)
            last, stable = -1, 0
            while time.time() - t0 < timeout:
                n = live()
                stable = stable + 1 if n == last else 0
                last = n
                if stable >= QUIET:
                    return n
                time.sleep(POLL)
            return last

        settle_screen()
        for name, keys in STEPS:
            before = live()
            for ch in keys:
                mon.mem_set(addr, bytes([ord(ch)]))
                time.sleep(SETTLE)
            n = settle_screen(before if keys else None)
            s = screen()
            if keys and n == before:
                print("  %-16s THE SCREEN DID NOT CHANGE (%d cells both sides)"
                      % (name, n))
            path = os.path.join(out, "c64-%s.png" % name)
            vice_mon.screenshot(mon, path, use_vicii=1)
            print("  %-16s %-38s %4d live cells" % (name, os.path.basename(path), live(s)))
            if show:
                for row in range(25):
                    print("    |" + decode(s[row * 40:row * 40 + 40]) + "|")
                print()
    finally:
        if kill:
            vice.send_signal(signal.SIGKILL)
            vice.wait()
        else:
            print("  VICE IS STILL RUNNING (pid %d) -- the C64 console is on"
                  " its screen." % vice.pid)
            print("  Play it. Type HELP at CMD: for the order list, C to swap"
                  " console pages.")
            print("  Close the window when you are done, or: kill %d" % vice.pid)
    return 0


if __name__ == "__main__":
    sys.exit(main())

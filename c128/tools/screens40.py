#!/usr/bin/env python3
"""Dump every 40-column screen as text, from ONE emulator run.

THE CYCLE THIS REPLACES. Looking at the hall of fame used to mean booting the
game, typing through six setup prompts, opening the self-destruct dialog,
entering a password and pressing RETURN three times -- about a hundred seconds
of emulator for one look at one layout, and the same again for the next
screen. Jamie: "can't you just make test builds to just display the screen you
are working on?" NOTES.md has said the same thing to itself since 2026-09-08.

It pokes `screen40` and reads screen RAM back as text. TEXT, not a PNG,
deliberately: a layout question is answered by which column a thing starts in,
and a picture makes me squint at that while a dump makes it countable. The PNG
capture has also lied once already (instrument #19).
"""
import os, re, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
C128 = os.path.dirname(HERE)
ROOT = os.path.dirname(C128)
sys.path.insert(0, os.path.join(ROOT, "tools"))

D64 = os.path.join(C128, "build", "screens40.d64")
ELF = os.path.join(C128, "build", "screens40.prg.elf")
NM  = os.path.expanduser("~/llvm-mos/bin/llvm-nm")

SCREENS = ["console (tactical)", "console (chart page)", "hall of fame",
           "detailed evaluation", "top secret memo", "state of repair",
           "previous messages", "info panel", "planet list", "play again",
           "title screen", "dialog (wrapping)", "setup"]


def symbol(name):
    out = subprocess.run([NM, ELF], capture_output=True, text=True).stdout
    for line in out.splitlines():
        f = line.split()
        if len(f) == 3 and f[2] == name:
            return int(f[0], 16)
    sys.exit("screens40: %s is not in the ELF" % name)


def as_text(scr):
    def ch(b):
        b &= 0x7F
        if b == 32: return " "
        if 1 <= b <= 26: return chr(64 + b)
        if 48 <= b <= 63: return chr(b)
        return "."
    return ["".join(ch(scr[r * 40 + c]) for c in range(40)) for r in range(25)]


def main():
    want = [int(a) for a in sys.argv[1:] if a.isdigit()] or list(range(len(SCREENS)))
    for p in (D64, ELF):
        if not os.path.exists(p):
            sys.exit("screens40: missing %s -- run `make screens40`" % p)
    addr = symbol("screen40")
    kb = symbol("kb_inject")        # to release screens that wait for a key

    vice = subprocess.Popen(
        ["x128", "-binarymonitor",
         "-binarymonitoraddress", "ip4://127.0.0.1:6502", "-autostart", D64],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(16)                      # boot, autostart, load the pool
        import vice_mon
        mon = vice_mon.Mon()
        for n in want:
            # WAIT FOR IT TO CHANGE, *THEN* FOR IT TO SETTLE. Waiting only
            # for "stable" concluded instantly against the screen that was
            # already there and dumped the console under the heading "hall of
            # fame" -- a settle test passes trivially before the work starts.
            before = bytes(mon.mem_get(0x0400, 400))
            mon.mem_set(addr, bytes([n]))
            changed = False
            for _ in range(40):
                time.sleep(0.3)
                if bytes(mon.mem_get(0x0400, 400)) != before:
                    changed = True
                    break
            if not changed:
                # SAY SO. The first version printed the PREVIOUS screen under
                # the new screen's heading when a screen did not draw -- which
                # is a caption lying about a picture, the same failure as
                # instrument #19. A screen that does not draw is a finding.
                print("=== %d  %s ===" % (n, SCREENS[n] if n < len(SCREENS) else "?"))
                print("    THE SCREEN DID NOT CHANGE -- this entry point drew "
                      "nothing. Not dumping the previous screen under its name.")
                print()
                continue
            last, stable = -1, 0
            for _ in range(40):
                time.sleep(0.3)
                live = sum(1 for b in mon.mem_get(0x0400, 1000) if b not in (32, 0))
                stable = stable + 1 if live == last else 0
                last = live
                if stable >= 3:
                    break
            scr = mon.mem_get(0x0400, 1000)
            live = sum(1 for b in scr if b not in (32, 0))
            name = SCREENS[n] if n < len(SCREENS) else "?"
            png = os.path.join(C128, "build", "s40-%02d.png" % n)
            vice_mon.screenshot(mon, png, use_vicii=1)
            print("=== %d  %s   (%d live cells, %s) ===" % (n, name, live, png))
            for i, line in enumerate(as_text(scr)):
                if line.strip():
                    print("%2d |%s|" % (i, line.rstrip()))
            print()
            # RELEASE IT. Most of these end in `while (kb_waitkey() != RETURN)`,
            # so without a key the machine stays inside this screen and every
            # later poke is ignored -- which reads as "the screen did not
            # change" and is really "it never left the last one".
            for key in ("\r", "N", "\r"):
                mon.mem_set(kb, bytes([ord(key)]))
                time.sleep(0.35)
    finally:
        vice.send_signal(signal.SIGKILL)
        vice.wait()
    return 0


if __name__ == "__main__":
    sys.exit(main())

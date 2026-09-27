#!/usr/bin/env python3
"""Play one scripted game on a C64 disk and record every screen.

    reucheck.py DISK.d64 MAP OUT.json [--reu] [--realtime] [--cmd=NAME=KEYS ...]
    reucheck.py --compare A.json B.json

THE ORACLE FOR THE REU BUILD. Run it on the disk build and on the REU build
with the same script and the recordings must agree screen for screen --
characters AND colours. The two builds are the same C; only where the code
sits and how it gets into the window differ, so ANY difference is the overlay
manager's: a thunk into the wrong image, a register lost across a swap, a
window not put back.

THE GALAXY IS PINNED, because otherwise two runs are two different games.
The seed is setup_seed(kb_entropy) at the moment the password line ends, and
kb_entropy counts poll passes, so it depends on timing. input.c returns an
injected key BEFORE it counts, so writing kb_entropy and the final RETURN
while the machine is STOPPED -- two raw monitor writes and then one resume --
gives both builds the same seed with no change to the game.

THE MACHINE RUNS IN WARP. Nothing here depends on emulated time any more,
and a disk build loading an overlay per command is slow at 1x. A key is only
sent once the previous one has been taken (kb_inject reads back 0), rather
than after a sleep, which warp would make meaningless.
"""
import hashlib
import json
import os
import signal
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, HERE)
import vice_mon                       # noqa: E402
from boot import decode               # noqa: E402

SEED_ENTROPY = 0x1234
QUIET = 3.0          # seconds of an unchanged screen that end a step
STEP_TIMEOUT = 120.0
KEY_TIMEOUT = 30.0   # a key not taken in this long: the machine has stopped

# (name, keys). "\r" is RETURN and "\e" is ESC (KB_ESC, 27). "@" is where the
# pinned seed goes in, and it must be the RETURN that ends the password line.
STEPS = [
    ("title",       ""),
    ("briefing-q",  "\r"),
    ("no-briefing", "n\r"),
    ("no-restore",  "n\r"),
    ("named",       "\r"),
    ("level",       "1\r"),
    ("console",     "ABC@"),
]

# THE GAME IT PLAYS, chosen to reach every one of the seven REU overlays and
# the nested calls between them: the chart and panels (view, panel), a warp
# speed and two moves (nav, time), the enemy turns they provoke (turn, enemy),
# a laser kill (laser), a torpedo whose fire_one_torpedo in OVL_REPAIR calls
# trek_fire_torpedo in OVL_TORP, docking (cmds), and the modal screens. Every
# dismissal is the key that screen asks for -- a space where it wants RETURN
# shifts every later keystroke by one, and the run stops testing anything.
GAME = [
    ("chart",    "C\r"),         ("back",     " "),
    ("repair",   "R\r"),         ("r-back",   "\r"),
    ("info",     "INFO\r"),      ("i-back",   "\r"),
    ("warpset",  "W3\r"),
    ("impulse",  "M 3 3\r"),
    ("warpmove", "M 2 7 4 4\r"),
    ("lasers",   "L\r"),         ("amount",   "500\r"),   ("l-back", "\r"),
    ("torp",     "T\r"),         ("salvo",    "1\r"),
    ("aim",      "8 5\r"),       ("t-back",   "\r"),
    ("plan",     "PLAN\r"),      ("p-back",   "\r"),
    ("msgs",     "MSGS\r"),      ("m-back",   "\x1b"),
    ("shup",     "SHUP\r"),      ("shdn",     "SHDN\r"),
    ("approach", "M 4 7\r"),     ("dock",     "D\r"),     ("d-back", "\r"),
    ("chart2",   "C\r"),         ("back2",    " "),
    ("warp2",    "M 1 7 5 5\r"),
]


def symbol(mapfile, name):
    for line in open(mapfile):
        f = line.split()
        if len(f) == 5 and f[4] == name:
            return int(f[0], 16)
    sys.exit("reucheck: %s not in %s -- is this a DEBUG build?" % (name, mapfile))


def raw_set(mon, start, data):
    """A memory write that does NOT resume the machine."""
    end = start + len(data) - 1
    body = (bytes([0]) + struct.pack("<HH", start, end) + bytes([0])
            + struct.pack("<H", 0) + bytes(data))
    rtype, err, _ = mon.cmd(vice_mon.CMD_MEM_SET, body)
    if err:
        raise IOError("mem_set error %d" % err)


def compare(a, b):
    """Every step must match in characters AND colours, and in number."""
    ra, rb = json.load(open(a)), json.load(open(b))
    bad = 0
    for x, y in zip(ra, rb):
        if x["screen"] == y["screen"] and x["colour"] == y["colour"] \
                and not x.get("stuck") and not y.get("stuck"):
            continue
        bad += 1
        print("  DIFFERS: %s%s" % (x["step"], "  (stuck)" if x.get("stuck")
                                   or y.get("stuck") else ""))
        for i, (l1, l2) in enumerate(zip(x["text"], y["text"])):
            if l1 != l2:
                print("    row %2d |%s|\n           |%s|" % (i, l1, l2))
        if x["screen"] == y["screen"]:
            print("    (characters agree; colours differ)")
    if len(ra) != len(rb):
        bad += 1
        print("  %d steps against %d" % (len(ra), len(rb)))
    print("reucheck: %d steps, %s" % (min(len(ra), len(rb)),
          "every screen identical" if not bad else "%d DIFFER" % bad))
    return 1 if bad else 0


def main():
    if sys.argv[1:2] == ["--compare"] and len(sys.argv) == 4:
        return compare(sys.argv[2], sys.argv[3])
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 3:
        sys.exit(__doc__.strip().splitlines()[2])
    d64, mapfile, out = args
    reu = "--reu" in sys.argv
    # AT 1x A DISK BUILD SITS SILENT FOR MORE THAN THREE SECONDS while the
    # 1541 loads an overlay, and a still screen is all this can see: the
    # first real-time run recorded a blank title and a warp that had not
    # happened yet. Real time waits longer.
    global QUIET
    if "--realtime" in sys.argv:
        QUIET = 8.0
    extra = [tuple(s.split("=", 1)) for s in
             (a[len("--cmd="):] for a in sys.argv if a.startswith("--cmd="))]
    steps = STEPS + (extra or GAME)
    inject = symbol(mapfile, "kb_inject")
    entropy = symbol(mapfile, "kb_entropy")

    # --realtime drops warp, for timing a build rather than checking it.
    cmd = ["x64sc"] + ([] if "--realtime" in sys.argv else ["-warp"]) + [
           "-binarymonitor",
           "-binarymonitoraddress", "ip4://127.0.0.1:%d" % vice_mon.PORT]
    if reu:
        cmd += ["-reu", "-reusize", "128"]
    else:
        cmd += ["+reu"]
    vice = subprocess.Popen(cmd + ["-autostart", os.path.abspath(d64) + ":trek64"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    record = []
    try:
        time.sleep(4)
        mon = vice_mon.Mon(check=False)

        def grab():
            s = mon.mem_get(0x0400, 1000)
            c = bytes(b & 0x0F for b in mon.mem_get(0xD800, 1000))
            return s, c

        def settle(t0):
            last, since = None, time.time()
            while time.time() - t0 < STEP_TIMEOUT:
                s, c = grab()
                h = hashlib.sha1(s + c).digest()
                if h != last:
                    last, since = h, time.time()
                elif time.time() - since >= QUIET:
                    return s, c
                time.sleep(0.25)
            return grab()

        class Stuck(Exception):
            pass

        def send(ch):
            # A MACHINE THAT HAS CRASHED NEVER TAKES THE KEY, and the first
            # version of this loop waited for it for ever -- found by making
            # the overlay manager fail on purpose, which is the only way to
            # learn what this script does when it is needed.
            t = time.time()
            while mon.mem_get(inject, 1)[0]:
                if time.time() - t > KEY_TIMEOUT:
                    raise Stuck()
                time.sleep(0.05)
            if ch == "@":
                raw_set(mon, entropy, [SEED_ENTROPY & 0xFF, SEED_ENTROPY >> 8])
                raw_set(mon, inject, [13])
                mon.resume()
            else:
                mon.mem_set(inject, bytes([ord(ch)]))

        for name, keys in steps:
            t0 = time.time()
            stuck = False
            try:
                for ch in keys.replace("\\r", "\r").replace("\\e", "\x1b"):
                    send(ch)
                s, c = settle(t0)
            except Stuck:
                stuck = True
                s, c = grab()
            text = [decode(s[r * 40:r * 40 + 40]) for r in range(25)]
            record.append(dict(step=name, keys=keys, screen=s.hex(),
                               colour=c.hex(), text=text, stuck=stuck))
            print("  %-14s %5.1fs%s" % (name, time.time() - t0,
                                        "  STUCK: the key was never taken" if stuck else ""))
            if stuck:
                break
    finally:
        vice.send_signal(signal.SIGKILL)
        vice.wait()
    json.dump(record, open(out, "w"), indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())

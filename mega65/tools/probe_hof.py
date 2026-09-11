#!/usr/bin/env python3
"""The hall-of-fame WRITE on the MEGA65 -- open list item 9.

This has been listed as unwitnessed since 2026-09-03, with the reason given as
circumstance: "a driven self-destruct scores -930, which qualifies for no slot,
so nothing is written and there is no file to check". It is not circumstance,
it is arithmetic, and the Atari settled it on 2026-09-10.

`hof_offer()` takes a score only if it beats the slot already there; a fresh
table is all zeros; so a qualifying score must be POSITIVE. A self-destruct
always books -200 for the ship, -430 for the crew and -300 for the incomplete
mission -- a hard floor of -930 that no single session climbs out of, because
clearing the -300 means killing every Mongol alive.

So `ship.killed` is poked past it. THIS IS A TEST OF THE WRITE, NOT OF SCORING:
a poked input makes every figure on the evaluation screen meaningless except
the ones under test. Everything downstream -- hof_offer, the serialiser,
plat_write_all, the bytes on the D81 -- runs for real.

AND ON THIS MACHINE THE DISK IS HONEST EVIDENCE. The MEGA65 writes to a real
D81 that `c1541` reads back; this port's SAVE was verified that way on
2026-09-08. Altirra's virtual-write problem does not apply here.

    python3 tools/probe_hof.py          (after `make debug disk`)
"""
import os
import re
import socket
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ELF = os.path.join(HERE, "build", "egatrek-debug.elf")
D81 = os.path.join(HERE, "build", "hof.d81")

# Packed for the 6502, same shared Ship struct as every port -- CALIBRATED
# below rather than trusted, because a host offsetof would not match.
OFF_ENERGY, OFF_SHIELDS, OFF_STARDATE, OFF_KILLED = 4, 8, 16, 37


def sym(name):
    nm = subprocess.check_output(
        [os.path.expanduser("~/llvm-mos/bin/llvm-nm"), "--print-size", ELF]).decode()
    for ln in nm.splitlines():
        f = ln.split()
        if len(f) == 4 and f[3] == name:
            return int(f[0], 16)
        if len(f) == 3 and f[2] == name:
            return int(f[0], 16)
    sys.exit("probe_hof: no %s -- is this the `make debug` build?" % name)


def main():
    subprocess.check_call(["c1541", "-format", "ega trek,01", "d81", D81],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for src, name in (("build/OVERLAYS-DEBUG.BIN", "overlays.bin"),
                      ("build/disk/STRINGS.DAT", "strings.dat"),
                      ("build/disk/MUSIC.DAT", "music.dat"),
                      ("build/disk/BRIEF.TXT", "brief.txt")):
        subprocess.check_call(["c1541", "-attach", D81, "-write",
                               os.path.join(HERE, src), name + ",s"],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("probe_hof: fresh D81 -- TREK.SCR is NOT on it yet", flush=True)

    KB, SHIP = sym("kb_inject"), sym("ship")
    rom = os.path.expanduser("~/Library/Application Support/xemu-lgb/mega65/MEGA65.ROM")
    sock = "/tmp/m65hof.sock"
    if os.path.exists(sock):
        os.unlink(sock)
    log = open("/tmp/m65hof.log", "w")
    shot = os.path.join(HERE, "build", "hof.png")
    p = subprocess.Popen([os.path.expanduser("~/xemu/bin/xmega65"), "-rom", rom,
                          "-sdimg", "@mega65.img", "-prgmode", "65", "-8", D81,
                          "-prg", os.path.join(HERE, "build/egatrek-debug.prg"),
                          "-besure", "-headless", "-screenshot", shot,
                          "-uartmon", sock], stdout=log, stderr=log)
    try:
        time.sleep(9)
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(sock)
        s.settimeout(0.4)

        def talk(c):
            s.sendall((c + "\n").encode())
            time.sleep(0.06)
            out = b""
            try:
                while True:
                    d = s.recv(65536)
                    if not d:
                        break
                    out += d
            except socket.timeout:
                pass
            return out.decode("latin1")

        def peek(addr, n):
            got = b""
            while len(got) < n:
                r = talk("m%08x" % (addr + len(got)))
                m = re.search(r":%08X:((?:[0-9A-F]{2})+)" % (addr + len(got)),
                              r.upper())
                if not m:
                    sys.exit("probe_hof: peek at $%X read nothing" % addr)
                got += bytes.fromhex(m.group(1))
            return got[:n]

        def key(k):
            talk("s%08x %02x" % (KB, k))
            for _ in range(200):
                r = talk("m%08x" % KB)
                m = re.search(r":%08X:(..)" % KB, r.upper())
                if m and int(m.group(1), 16) == 0:
                    return
                time.sleep(0.05)
            sys.exit("probe_hof: key %02x was never consumed" % k)

        def keys(spec, settle=1.5):
            for k in spec.split(","):
                if k:
                    key({"RETURN": 0x0D}.get(k, ord(k) if len(k) == 1 else 0))
            time.sleep(settle)

        keys("RETURN"); keys("N,RETURN"); keys("N,RETURN")
        keys("J,A,M,I,E,RETURN"); keys("1,RETURN"); keys("T,R,E,K,RETURN", 3)

        sh = peek(SHIP, 61)
        w = lambda i: sh[i] | (sh[i + 1] << 8)
        ok = (w(OFF_ENERGY) == 5000 and w(OFF_SHIELDS) == 2500
              and w(OFF_STARDATE) == 35000)
        print("probe_hof: energy %d shields %d stardate %d -> offsets %s"
              % (w(OFF_ENERGY), w(OFF_SHIELDS), w(OFF_STARDATE),
                 "CONFIRMED" if ok else "WRONG"), flush=True)
        if not ok:
            sys.exit("probe_hof: refusing to poke against unconfirmed offsets")

        talk("s%08x %02x" % (SHIP + OFF_KILLED, 150))
        talk("s%08x %02x" % (SHIP + OFF_KILLED + 1, 0))
        back = peek(SHIP + OFF_KILLED, 2)
        print("probe_hof: poked ship.killed=150, reads back %d"
              % (back[0] | (back[1] << 8)), flush=True)

        # THE ENDGAME IS THREE SCREENS: loss memo -> DETAILED EVALUATION ->
        # hall of fame, and THE WRITE IS ON THE THIRD. The first version
        # pressed two RETURNs, printed "reached the hall of fame", and stopped
        # on the evaluation -- so no write ever ran and the empty D81 read as
        # a broken write. The claim was a print statement, not an observation.
        keys("S,RETURN", 3); keys("T,R,E,K,RETURN", 4)   # self destruct
        keys("RETURN", 3)                                 # memo -> evaluation
        keys("RETURN", 5)                                 # evaluation -> HOF
        keys("RETURN", 4)                                 # and past it
        print("probe_hof: walked the endgame (memo, evaluation, hall of fame)",
              flush=True)
        time.sleep(3)
    finally:
        p.terminate()
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            p.kill()

    out = subprocess.run(["c1541", "-attach", D81, "-dir"],
                         capture_output=True, text=True).stdout
    hit = [l for l in out.splitlines() if "trek.scr" in l.lower()]
    print("\nprobe_hof: TREK.SCR on the D81: %s" % (hit or "NOT PRESENT"))
    if hit:
        # `trek.scr,s` -- the SEQ suffix is required; without it c1541 prints
        # no error and writes no file, and the check below then blames the
        # game for the tool.
        subprocess.run(["c1541", "-attach", D81, "-read", "trek.scr,s",
                        "/tmp/m65trek.scr"], capture_output=True)
        d = open("/tmp/m65trek.scr", "rb").read()
        i = d.find(b"JAMIE")
        print("probe_hof: %d bytes; JAMIE at %s -> %r"
              % (len(d), i, d[i:i + 36] if i >= 0 else None))
        # core/hof.h: the file "is exactly 300 bytes only because every score
        # in it is 0". A three-digit score adds two. 302 is the format
        # agreeing with itself, which is worth more than the file existing.
        print("probe_hof: 300 + %d digits of score = %d bytes -- %s"
              % (len(d) - 300, len(d),
                 "as core/hof.h documents" if len(d) == 302 else "UNEXPECTED"))


main()

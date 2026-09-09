#!/usr/bin/env python3
"""Measure POKEY: which divisor does each mode actually use, and what is a frame?

Drives src/sndprobe.c. For each configuration it pokes a register block into
page 6, lets the 6502 write it to POKEY for real, then asks Altirra what came
out -- so the answer is the emulated core's behaviour rather than a manual's.

Two things every port in this project has got wrong once are asked here rather
than assumed: whether the OS frame counter keeps running for a program that has
taken the machine over (the X16's did not), and which region the machine is in
(the MEGA65 ran its music 19% fast on the wrong numerator).
"""
import pathlib
import re
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve()
ATARI = HERE.parents[1]
SERVER = pathlib.Path.home() / "AltirraBridge-nightly-macos-arm64" / "AltirraBridgeServer"
sys.path.insert(0, str(SERVER.parent / "sdk" / "python"))
from altirra_bridge import AltirraBridge      # noqa: E402

CMD = 0x0600
XEX = None            # filled in by main
TONE = 0xA8           # distortion 5 (pure tone), volume 8

CONFIGS = [
    ("8-bit  64kHz  AUDF=63",   0x00, [63, 0, 0, 0], [TONE, 0, 0, 0]),
    ("8-bit  64kHz  AUDF=255",  0x00, [255, 0, 0, 0], [TONE, 0, 0, 0]),
    ("8-bit  64kHz  AUDF=0",    0x00, [0, 0, 0, 0], [TONE, 0, 0, 0]),
    ("8-bit  15kHz  AUDF=63",   0x01, [63, 0, 0, 0], [TONE, 0, 0, 0]),
    ("8-bit  15kHz  AUDF=255",  0x01, [255, 0, 0, 0], [TONE, 0, 0, 0]),
    ("8-bit  1.79M  AUDF=63",   0x40, [63, 0, 0, 0], [TONE, 0, 0, 0]),
    # 16-bit: channels 1+2 joined (bit4), channel 1 clocked at 1.79MHz (bit6).
    # AUDF1 is the low byte and AUDF2 the high; the output is channel 2's, so
    # the volume goes in AUDC2 and channel 1 is silenced.
    ("16-bit 1.79M  AUDF=200",  0x50, [200 & 0xFF, 200 >> 8, 0, 0], [0, TONE, 0, 0]),
    ("16-bit 1.79M  AUDF=1000", 0x50, [1000 & 0xFF, 1000 >> 8, 0, 0], [0, TONE, 0, 0]),
    ("16-bit 1.79M  AUDF=4000", 0x50, [4000 & 0xFF, 4000 >> 8, 0, 0], [0, TONE, 0, 0]),
    ("16-bit 64kHz  AUDF=1000", 0x10, [1000 & 0xFF, 1000 >> 8, 0, 0], [0, TONE, 0, 0]),
    # THE CONFIGURATION THE DRIVER WILL ACTUALLY USE: both pairs joined
    # (bits 3 and 4) and both clocked at 1.79MHz (bits 5 and 6) = $78, giving
    # two 16-bit voices out of POKEY's four channels -- music on 1+2, effects
    # on 3+4. Two voices is what this seam exists for; the original had one PC
    # speaker and could not keep a hit from chopping the tune.
    ("TWO VOICE 955/1897", 0x78,
     [955 & 0xFF, 955 >> 8, 1897 & 0xFF, 1897 >> 8], [0, TONE, 0, TONE]),
]


def start_server(log):
    fh = open(log, "w")
    proc = subprocess.Popen(
        [str(SERVER), "--bridge", "--settings=user", "--pacing=unlimited"],
        stdout=subprocess.DEVNULL, stderr=fh)
    deadline = time.time() + 20
    while time.time() < deadline:
        if proc.poll() is not None:
            sys.exit("pokey: server exited -- see %s" % log)
        m = re.search(r"token-file:\s*(\S+)", pathlib.Path(log).read_text())
        if m:
            return proc, m.group(1)
        time.sleep(0.2)
    proc.kill()
    sys.exit("pokey: no token-file line")


def apply(a, ctl, audf, audc):
    ack = a.peek(CMD + 1, 1)[0]
    a.poke(CMD + 2, ctl)
    for i, v in enumerate(audf):
        a.poke(CMD + 3 + i, v)
    for i, v in enumerate(audc):
        a.poke(CMD + 7 + i, v)
    a.poke(CMD, 1)
    a.frame(4)
    return a.peek(CMD + 1, 1)[0] != ack


def sounding(a):
    for c in a.audio_state()["channels"]:
        if c["volume"] and c["freq_hz"]:
            return c
    return None


def sweep(a):
    print("%-24s %-8s %8s %12s" % ("configuration", "clock", "period", "freq Hz"))
    for label, ctl, audf, audc in CONFIGS:
        if not apply(a, ctl, audf, audc):
            print("%-24s NOT APPLIED" % label)
            continue
        c = sounding(a)
        if not c:
            print("%-24s silent" % label)
            continue
        print("%-24s %-8s %8s %12.3f"
              % (label, c["clock"], c["period_cycles"], c["freq_hz"]))


def regions(a):
    """$D014 read $0F while the base clock Altirra computed was NTSC's 63,921,
       so one of the two readings is being misread. The only way to know which
       is to make the machine PAL and look again."""
    print()
    for mode in ("pal", "ntsc"):
        try:
            a.config("video", mode)
        except Exception as exc:
            print("config video=%s rejected: %s" % (mode, exc))
            continue
        a.boot(str(XEX))
        a.frame(200)
        d014 = a.peek(CMD + 0x13, 1)[0]
        ticks = a.peek(CMD + 0x12, 1)[0]
        apply(a, 0x00, [63, 0, 0, 0], [TONE, 0, 0, 0])
        c = sounding(a)
        # And the 16-bit base in the SAME region, because that is the constant
        # the driver divides by and it is not the 8-bit one.
        apply(a, 0x50, [955 & 0xFF, 955 >> 8, 0, 0], [0, TONE, 0, 0])
        c16 = sounding(a)
        print("video=%-5s  $D014=$%02X  RTCLOK/200 %d\n"
              "             8-bit  AUDF=63  -> %9.3f Hz   base %8.0f Hz\n"
              "             16-bit AUDF=955 -> %9.3f Hz   base %8.0f Hz  "
              "-> AUDF = %d/n - 7"
              % (mode, d014, ticks,
                 c["freq_hz"], c["freq_hz"] * 128,
                 c16["freq_hz"], c16["freq_hz"] * 2 * 962,
                 round(c16["freq_hz"] * 2 * 962 / 20.0)))


def main():
    global XEX
    XEX = ATARI / "build" / "sndprobe.xex"
    (ATARI / "build").mkdir(parents=True, exist_ok=True)
    proc, token = start_server(str(ATARI / "build" / "bridge.log"))
    try:
        with AltirraBridge.from_token_file(token) as a:
            a.boot(str(XEX))
            a.frame(200)
            ticks = a.peek(CMD + 0x12, 1)[0]
            print("RTCLOK ($0014) changed %d times in 200 frames -- %s\n"
                  % (ticks, "IT RUNS" if ticks else "DEAD, like the X16's jiffy"))
            sweep(a)
            regions(a)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


main()

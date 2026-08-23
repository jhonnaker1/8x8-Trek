#!/usr/bin/env python3
"""Pull EGA Trek's music and sound effects out of the original executable.

No recording needed, and recording would be strictly worse. The original is a
Turbo Pascal program driving the PC speaker through the runtime's Sound()
routine, so what is in the binary is not audio -- it is note data: exact
frequencies in Hz and exact durations in timer ticks. That converts straight
to SID, POKEY or Paula. A capture would give square waves to pitch-detect
back into the numbers already sitting here.

HOW IT WAS FOUND

  1. tp_labels.csv already located the runtime's SOUND at load-module offset
     0x0227D6. Scanning the whole image for far calls to it gives EIGHT sites,
     which is a small enough number to read by hand.
  2. Two of those are a 440Hz/250ms beep, four are procedural sweeps, and the
     rest live in one routine that walks a byte array -- the music player.
  3. The player is an ISR (register-saving prologue, `mov ds, <DGROUP>`,
     iret) hooked on the stock 18.2065Hz timer. Nothing in the game code
     writes PIT ports 40h or 43h, so that rate is not reprogrammed.

THE FORMAT

A track is a flat byte array of (duration, frequency/10) PAIRS, terminated by
a zero duration. Frequency 0 is a rest. Duration is in timer ticks, scaled by
the tempo argument -- both music tracks pass tempo 1, so one unit is one tick,
54.9ms.

    StartMusic(track: pointer; repeats: word; tempo: byte)

DGROUP

Track pointers are DS-relative, so they need the data segment's base. It was
brute-forced first -- the only base in the whole file at which all five short
tracks parse as valid pairs with correct terminators -- and then confirmed
independently: the ISR loads `mov ax, 0x28a2 / mov ds, ax`, and
0x28a2 * 16 + LOAD_BASE is exactly the same address. Two routes, one answer.

WHICH TRACK IS WHICH was read out of the running game rather than guessed:
dosbox-automation's memory API, the player's current-track variable at
DGROUP+0x1cb6, sampled on the title screen and again at the end of a game.

COPYRIGHT. The note data is Nels Anderson's creative work, exactly like the
message prose, so this tool WRITES ITS OUTPUT INTO build/ AND NOTHING IS
COMMITTED. reference/ is gitignored for the same reason. What the repo keeps
is how to get the data, not the data.
"""
import os
import struct
import sys

EXE       = "reference/EGATREK_unpacked.exe"
LOAD_BASE = 1248 * 16
DGROUP    = 0x2D820          # file offset; see the header for both derivations
TICK_HZ   = 1193181.0 / 65536.0     # 18.2065, the stock PC timer

# offset -> (what starts it, repeats, tempo), from the seven StartMusic sites
TRACKS = [
    (0x057E, "TITLE MUSIC",      99, 1),
    (0x071A, "END-OF-GAME MUSIC", 99, 1),
    (0x099A, "effect (2 notes)",   0, 1),
    (0x09A0, "effect (6 notes)",   1, 1),
    (0x09AE, "effect (2 notes)",   1, 1),
    (0x09B4, "effect (2 notes)",   1, 1),
    (0x09BA, "effect (3 notes)",   1, 1),
]

NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def note_name(hz):
    """Nearest equal-tempered name, with cents of error so a bad fit shows."""
    if hz <= 0:
        return "rest", 0
    import math
    n = 12 * math.log2(hz / 440.0) + 69          # MIDI number
    near = int(round(n))
    cents = int(round((n - near) * 100))
    return "%s%d" % (NAMES[near % 12], near // 12 - 1), cents


def read_track(data, off, limit=2000):
    p = DGROUP + off
    out = []
    while len(out) < limit:
        dur, freq = data[p], data[p + 1] * 10
        p += 2
        if dur == 0:
            break
        out.append((dur, freq))
    return out


def main():
    data = open(EXE, "rb").read()
    outdir = "build"
    os.makedirs(outdir, exist_ok=True)
    path = os.path.join(outdir, "egatrek-music.txt")

    with open(path, "w") as f:
        f.write("EGA Trek music and effects, extracted from %s\n" % EXE)
        f.write("tick = %.4f Hz, so one duration unit = %.1f ms\n\n"
                % (TICK_HZ, 1000.0 / TICK_HZ))
        for off, what, repeats, tempo in TRACKS:
            t = read_track(data, off)
            secs = sum(d for d, _ in t) * tempo / TICK_HZ
            f.write("=" * 68 + "\n")
            f.write("%s  --  DS:%04X, %d notes, %d bytes, %.1fs, repeats %d, tempo %d\n"
                    % (what, off, len(t), len(t) * 2 + 2, secs, repeats, tempo))
            f.write("=" * 68 + "\n")
            for i, (d, hz) in enumerate(t):
                name, cents = note_name(hz)
                f.write("%4d  %3d ticks %6.1f ms  %5d Hz  %-5s %+4d cents\n"
                        % (i, d, d * tempo * 1000.0 / TICK_HZ, hz, name, cents))
            f.write("\n")
    print("wrote %s" % path)

    for off, what, _, tempo in TRACKS:
        t = read_track(data, off)
        secs = sum(d for d, _ in t) * tempo / TICK_HZ
        print("  %-18s DS:%04X  %3d notes  %5.1fs" % (what, off, len(t), secs))


if __name__ == "__main__":
    sys.exit(main())

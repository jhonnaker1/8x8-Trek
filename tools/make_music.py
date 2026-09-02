#!/usr/bin/env python3
"""Compose the port's OWN music, and write it in the original's data format.

WHY THIS EXISTS. `tools/gen_music.py` extracts Nels Anderson's note data out of
EGATREK.EXE. That data is his creative work in the same way his prose is, so it
is gitignored and never committed -- which also means a fresh clone plays
silently, and a release disk cannot carry sound at all.

This replaces it with music written for this port. The tracks are ours, so this
file and its output CAN be committed, a clone gets sound with no reference/ at
all, and a release disk can ship with it.

FAITHFUL IN ROLE, NOT IN TUNE. The original's shapes were measured first so the
new tracks sit in the same places:

    title          205 notes  45.5s  170-700 Hz   a tune
    end of game    319 notes  43.5s  190-1480 Hz  a finale, wider
    lasers           2 notes   0.1s  ~300 Hz      a tick
    torpedo          6 notes   0.3s  80-220 Hz    a descending thump
    alert            2 notes   0.8s  500->1000    a two-tone klaxon
    death pod        2 notes   0.8s  300->600     the same, lower
    incoming fire    3 notes   0.9s  400-800      a three-note hit

None of Anderson's intervals are reproduced. What is kept is the DURATION, the
REGISTER and the JOB each track does, because those are what make the port feel
like the game rather than what make the tune his.

TAKING THE SID SERIOUSLY. The original is a PC speaker: one square wave, no
envelope, no chords. The data format is one note at a time and the player has
one voice for music, so the harmony here is ARPEGGIATED -- chords spelled out
three notes at a time at 1 to 2 ticks each, which is how the C64 has implied
chords on a single voice since 1982. At 18.2 ticks a second a three-note cycle
runs at about 6Hz, fast enough to hear as a chord rather than as notes.

FORMAT. Pairs of (duration in player ticks, frequency in Hz/10), terminated by
a (0,0) pair. Frequency 0 is a rest. One tick is about 55ms; see sidfreq.h.
A byte holds 1..255, so 10Hz to 2550Hz -- the bass end is coarse, one
byte being 10Hz, so low notes are a little out of tune and that is the
format's limit rather than a mistake.
"""
import os
import sys

TICKS_PER_SEC = 18.2

# ---------------------------------------------------------------- pitches
#
# Equal temperament from A4 = 440, stored as Hz/10 and therefore COARSE low
# down: D3 is 146.8Hz and rounds to a byte of 15, which is 2.2% sharp. That is
# the original's format and its limitation, not ours, and it is why the bass
# here mostly sits at or above G3 where the error falls under 1%.
NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def hz(name):
    if name == "-":
        return 0
    letter, octave = name[:-1], int(name[-1])
    semis = NAMES.index(letter) + 12 * (octave + 1) - 69
    return 440.0 * (2.0 ** (semis / 12.0))


def by(name):
    if name == "-":
        return 0
    v = int(round(hz(name) / 10.0))
    if v < 1 or v > 255:
        raise ValueError("%s is %.1fHz, outside the byte's 10..2550" % (name, hz(name)))
    return v


def seq(pairs):
    """[(note, ticks), ...] -> [(ticks, byte), ...], merging nothing."""
    return [(t, by(n)) for n, t in pairs]


def arp(notes, ticks, cycles):
    """A chord as a fast cycle -- the one-voice harmony trick."""
    out = []
    for _ in range(cycles):
        for n in notes:
            out.append((n, ticks))
    return out


# ------------------------------------------------------------ the tracks
#
# 4/4 at about 91 BPM: a sixteenth is 3 ticks, a quarter 12, a bar 48.
S, E, Q, H, W = 3, 6, 12, 24, 48


def title():
    """D minor. The first thing anyone hears, so it carries the mood: a lone
    ship, a long way out, against something much larger. Melody in the octave
    above middle C, harmony arpeggiated underneath it."""
    m = []
    # Two bars of rising Dm, quiet and open -- the curtain going up.
    m += arp(["D3", "A3", "D4", "F4"], S, 6)
    # A: the theme.
    m += [("A4", H), ("F4", Q), ("G4", Q)]
    m += [("A4", H), ("D5", H)]
    m += [("C5", Q), ("A#4", Q), ("A4", H)]
    m += [("G4", H)] + arp(["D4", "G4", "A#4"], S, 3) + [("-", S)]
    # A2: the answer, one step further out.
    m += [("A4", H), ("F4", Q), ("G4", Q)]
    m += [("A4", H), ("C5", H)]
    m += [("D5", Q), ("C5", Q), ("A#4", H)]
    m += [("A4", H)] + arp(["F4", "A4", "C5"], S, 3) + [("-", S)]
    # B: the lift into the relative major, and the only time it looks up.
    m += [("F5", H), ("E5", Q), ("D5", Q)]
    m += [("C5", H), ("D5", H)]
    m += [("A#4", Q), ("C5", Q), ("D5", H)]
    m += [("A4", H)] + arp(["D4", "F4", "A4"], S, 3) + [("-", S)]
    # A': return, and this time it falls all the way home.
    m += [("A4", H), ("F4", Q), ("G4", Q)]
    m += [("A4", H), ("D5", H)]
    m += [("C5", Q), ("A#4", Q), ("A4", Q), ("G4", Q)]
    m += [("F4", H), ("E4", Q), ("D4", Q)]
    m += [("D4", W)] + arp(["D3", "A3", "D4"], S, 4)
    return seq(m)


def endgame():
    """The same D minor material, but the tune is allowed to finish. Wider than
    the title at both ends, which is what the original's end track does too."""
    m = []
    m += arp(["D3", "F3", "A3", "D4"], S, 4)
    # The theme, slower and an octave up -- it has become a statement.
    m += [("D5", H), ("A#4", Q), ("C5", Q)]
    m += [("D5", H), ("F5", H)]
    m += [("E5", Q), ("D5", Q), ("C5", H)]
    m += [("A#4", W)]
    m += [("D5", H), ("A#4", Q), ("C5", Q)]
    m += [("D5", H), ("A5", H)]
    m += [("G5", Q), ("F5", Q), ("E5", H)]
    m += [("D5", W)]
    # The climb, and the highest the port ever goes.
    m += [("F5", Q), ("G5", Q), ("A5", H)]
    m += [("A#5", H), ("A5", H)]
    m += [("G5", Q), ("F5", Q), ("E5", H)]
    m += [("D5", H)] + arp(["D4", "F4", "A4"], S, 4)
    # Down and out.
    m += [("A4", H), ("F4", Q), ("D4", Q)]
    m += [("A3", W)]
    m += arp(["D3", "A3", "D4"], S, 6)
    m += [("D3", W)]
    return seq(m)


# The five effects. Same durations and registers as the originals, different
# intervals -- these are a fraction of a second each and their whole job is to
# be recognisable, so the shape matters and the notes do not.
def sfx_lasers():      return [(1, 33), (1, 28)]                    # a down-tick
def sfx_torpedo():     return [(1, 24), (1, 20), (1, 17),
                               (1, 14), (1, 11), (1, 9)]            # a falling thump
def sfx_alert():       return [(5, 88), (9, 52)]                    # two-tone klaxon
def sfx_pod():         return [(5, 62), (9, 31)]                    # the same, lower
def sfx_incoming():    return [(3, 78), (3, 44), (4, 26)]           # a three-note hit


TRACKS = [
    ("MUS_TITLE", "title screen",                         title),
    ("MUS_END",   "end of game",                          endgame),
    ("SFX_A",     "lasers fire",                          sfx_lasers),
    ("SFX_B",     "torpedo launch",                       sfx_torpedo),
    ("SFX_C",     "status goes to ALERT",                 sfx_alert),
    ("SFX_D",     "Vandal Death Pod enters the quadrant", sfx_pod),
    ("SFX_E",     "incoming Mongol fire",                 sfx_incoming),
]


def main():
    hdr = "c128/src/music_data.h"
    src = "c128/src/music_data.c"
    built = [(name, fn(), what) for name, what, fn in TRACKS]

    for name, notes, what in built:
        for dur, b in notes:
            if not (1 <= dur <= 255):
                sys.exit("%s: duration %d does not fit a byte" % (name, dur))
            if not (0 <= b <= 255):
                sys.exit("%s: frequency %d does not fit a byte" % (name, b))

    with open(hdr, "w") as f:
        f.write("/* GENERATED by tools/make_music.py -- do not edit.\n"
                "   The music is this port's own; see that file. */\n"
                "#ifndef MUSIC_DATA_H\n#define MUSIC_DATA_H\n\n")
        f.write("#define MUSIC_PRESENT 1\n\n")
        for i, (name, t, what) in enumerate(built):
            f.write("#define %-10s %d   /* %s, %d notes */\n" % (name, i, what, len(t)))
        f.write("\n#define MUS_COUNT %d\n\n" % len(built))
        f.write("/* Byte offset of each track inside MUSIC.DAT. */\n")
        f.write("extern const unsigned int mus_offset[MUS_COUNT];\n\n#endif\n")

    with open(src, "w") as f:
        f.write("/* GENERATED by tools/make_music.py -- do not edit. */\n")
        f.write('#include "music_data.h"\n\n')
        f.write("/* Offsets only. The notes live in MUSIC.DAT on the game disk and\n"
                "   stream into far memory at startup, which keeps them out of a 42K\n"
                "   address space. */\n")
        f.write("const unsigned int mus_offset[MUS_COUNT] = {\n")
        off = 0
        for name, notes, what in built:
            f.write("    %5d,   /* %s */\n" % (off, name))
            off += (len(notes) + 1) * 2
        f.write("};\n")

    blob = bytearray()
    for name, notes, what in built:
        for d, b in notes:
            blob.append(d & 0xFF)
            blob.append(b & 0xFF)
        blob += b"\x00\x00"
    os.makedirs("c128/build", exist_ok=True)
    open("c128/build/music.dat", "wb").write(bytes(blob))

    total = sum(sum(d for d, _ in n) for _, n, _ in built) / TICKS_PER_SEC
    print("make_music: %d tracks, %d bytes, %.1fs of music -> build/music.dat"
          % (len(built), len(blob), total))
    print("make_music: wrote %s and .c -- this port's own, no reference/ needed"
          % hdr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

EGA Trek for the Commander X16
==============================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


RUNNING IT

Put every file in this folder into one directory and start the emulator from
inside it, so that HostFS can see them:

    cd egatrek-x16
    x16emu -prg trekx16.prg -run

On real hardware, copy the whole folder to the SD card and LOAD "TREKX16.PRG"
from that directory.

ALL FIVE FILES ARE REQUIRED and the game reads them at startup:

    TREKX16.PRG    the program
    OVERLAYS.BIN   code that is paged into a 3,968-byte window on demand
    STRINGS.DAT    every word on screen
    MUSIC.DAT      the music
    BRIEF.TXT      the twelve-page briefing, streamed a byte at a time

A missing STRINGS.DAT plays with blank labels rather than refusing to start,
and a missing MUSIC.DAT plays silently -- but a missing or STALE OVERLAYS.BIN
will crash the game, so keep the set together.


PLAYING IT

Answer Y to the briefing at startup; it is twelve pages and explains the whole
game. Type HELP at the command line for the order list.

Commands are typed at CMD: in the COMMAND panel and are not case sensitive.
The arrow keys raise and lower shields, which is what the original's own help
screen lists first.


WHAT IS IN IT

All 25 commands, the nine-panel console, save and restore, the hall of fame,
and the mechanics the manual never mentions: boarding parties, the Vandal
death pod, plasma bolts in both directions, black holes, supernovae, the death
ray's five outcomes, tractor beams, wear and tear, reinforcements, a spy who
sabotages a system, a settlement with a clock running against it, and a
damaged ship that keeps getting worse if you do not fix it.

Messages are coloured by the department that speaks them, as the original's
are.

The MAIN VIEWER shows one page. The original cycles ten; the other nine are
deferred on every port. Room is not what stops them: what SELECTS the other
nine was never measured. Orbit selects the one you see, and that is the only
selector any of these ports knows.

SAVE writes EGATREK.SAV into the directory you started from, and the setup
screen offers to restore it. The hall of fame is TREK.SCR and is created on
first use.

Answering NO to "Play Again?" shows a farewell, waits for a key, and hands you
back to BASIC on this screen.


THIS IS NOT NELS ANDERSON'S CODE

Nothing here is disassembled, decompiled or translated from EGATREK.EXE. The
original was measured -- run under instrumentation, its constants read out of
the binary, its behaviour observed -- and reimplemented in C from those
measurements. The prose on screen, the briefing and the music are this port's
own; none of Anderson's text or note data is included.

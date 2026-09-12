EGA Trek for the Commodore 128
==============================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


RUNNING IT

You need a C128 in 80-column mode -- the console is 80x25 with a colour on
every cell, which is what the VDC gives and what the VIC-IIe does not. Press
the 40/80 DISPLAY key before you start, or set it in your emulator.

Everything is on the one disk image. Put it in drive 8 and:

    RUN "TREK128"

or in VICE:

    x128 -autostart egatrek-c128.d64

THE PROGRAM NEEDS THE REST OF THE DISK and reads it as it goes:

    TREK128        the program
    OVERLAYS       eleven images -- OVLEVAL, OVLHOF, OVLFRONT and the rest --
                   paged into a 4K window at $AF00 on demand
    STRINGS.DAT    every word on screen
    MUSIC.DAT      the music
    BRIEF.TXT      the briefing, streamed a page at a time
    TREK.SCR       the hall of fame

A missing STRINGS.DAT plays with blank labels rather than refusing to start,
and a missing MUSIC.DAT plays silently -- but a missing or STALE overlay image
will crash the game, so keep the disk together.

JIFFYDOS IS ASSUMED but not required. The port was developed and timed with
JiffyDOS at both ends; on a stock 1541 it works and the disk access is
slower.


PLAYING IT

Answer Y to the briefing at startup; it explains the whole game. Type HELP at
the command line for the order list.

Commands are typed at CMD: in the COMMAND panel and are not case sensitive.
The cursor keys raise and lower shields, which is what the original's own help
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

One colour is inexact. The VDC's palette is fixed, and its dark yellow stands
in for EGA's brown -- fifteen of the sixteen land exactly.


SAVING

SAVE asks for a filename and offers EGATREK.SAV. The setup screen offers to
restore it. The hall of fame is TREK.SCR. Both go to device 8, so the disk
must not be write protected.


QUITTING

Answering NO to "Play Again?" shows a farewell, waits for a key, and then
RESETS the machine. That is not tidiness: this program occupies the whole of
BASIC's text area, so there is nothing to return to. BASIC comes back on the
40-column screen, which is the other window if you are running an emulator.


THIS IS NOT NELS ANDERSON'S CODE

Nothing here is disassembled, decompiled or translated from EGATREK.EXE. The
original was measured -- run under instrumentation, its constants read out of
the binary, its behaviour observed -- and reimplemented in C from those
measurements. The prose on screen, the briefing and the music are this port's
own; none of Anderson's text or note data is included.

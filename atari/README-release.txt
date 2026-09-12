EGA Trek for the Atari 800XL with VBXE
======================================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


YOU NEED A VIDEO BOARD XE

This port draws its console on VBXE, not on ANTIC. Eighty columns and sixteen
colours on their own values is what the nine-panel console needs, and a stock
800XL has neither. Without VBXE the program will run and you will not see it.


RUNNING IT

Put the disk in drive 1 and turn the machine on with BASIC disabled -- hold
OPTION on an XL or XE. That is all: the disk boots itself.

    egatrek-atari.atr


BE PATIENT WITH THE FIRST SCREEN

ON A STOCK 1050 THIS TAKES ABOUT TWO MINUTES. That is measured, not
estimated: 112 seconds. Nothing is wrong. The game has to read 36,474 bytes
of overlays, strings and music into VBXE's memory before it can draw the
title screen, and a 1050 reads at the speed a 1050 reads.

If you have a fast-SIO drive -- a Happy or Speedy 1050, an XF551, an SD-card
drive -- or if you are running this in an emulator, it will take a few
seconds instead. Altirra with its usual SIO patch loads the same disk in
about 12 seconds of emulated time.


THERE IS NO DOS ON THIS DISK

Atari's DOS is Atari's, so a disk carrying it would not be ours to give away.
This one carries its own boot record and its own directory instead, and talks
to the drive through SIO directly. That also freed $0700-$1FFF, which is
where the program's data now lives -- on the tightest target in this project,
that is the difference between fitting and not.

The disk holds:

    EGATREK.XEX    the program
    OVERLAYS.BIN   code paged into a 4K window in VBXE's RAM on demand
    STRINGS.DAT    every word on screen
    MUSIC.DAT      the music
    BRIEF.TXT      the briefing, streamed rather than held in memory

plus eleven empty slots for saved games and the hall of fame.

Because the format is this port's own, ATARI DOS AND SPARTADOS CANNOT READ
THIS DISK, and neither can a PC tool that expects DOS 2. Nothing is wrong
with the image; it simply is not a DOS 2 disk.


PLAYING IT

Answer Y to the briefing at startup; it explains the whole game. Type HELP at
the command line for the order list.

Commands are typed at CMD: in the COMMAND panel and are not case sensitive.
The cursor keys raise and lower shields, which is what the original's own
help screen lists first.


WHAT IS IN IT

All 25 commands, the nine-panel console, save and restore, the hall of fame,
and the mechanics the manual never mentions: boarding parties, the Vandal
death pod, plasma bolts in both directions, black holes, supernovae, the
death ray's five outcomes, tractor beams, wear and tear, reinforcements, a
spy who sabotages a system, a settlement with a clock running against it, and
a damaged ship that keeps getting worse if you do not fix it.

Messages are coloured by the department that speaks them, as the original's
are.

The MAIN VIEWER shows one page. The original cycles ten; the other nine are
deferred on every port. Room is not what stops them: what SELECTS the other
nine was never measured. Orbit selects the one you see, and that is the only
selector any of these ports knows.

Music and effects play on POKEY, two voices in 16-bit mode: the tune on
channels 1+2 and effects on 3+4, so a hit during the title track does not
chop the tune.


SAVING

SAVE asks for a filename and offers EGATREK.SAV. Type whatever you like; the
disk has room for eleven saved games, counting the hall of fame. The hall of
fame is TREK.SCR and appears the first time a game earns a place in it.

The disk must not be write protected.

Quitting -- answering N to "Play Again?" -- restarts the machine from this
disk, because the program occupies everything between $3000 and the OS ROM
and there is nothing else to return to.


THIS IS NOT NELS ANDERSON'S CODE

Nothing here is disassembled, decompiled or translated from EGATREK.EXE. The
original was measured -- run under instrumentation, its constants read out of
the binary, its behaviour observed -- and reimplemented in C from those
measurements. The prose on screen, the briefing and the music are this port's
own; none of Anderson's text or note data is included.

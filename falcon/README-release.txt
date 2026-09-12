EGA Trek for the Atari Falcon030
================================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


RUNNING IT

A Falcon030 with a VGA MONITOR, and about 150K free.

The VGA monitor is not a preference. The game sets 640x480 in sixteen colours
-- the mode a Falcon on VGA already boots into -- and that mode does not exist
on an RGB monitor or a television. It has been run on VGA and nowhere else.

Copy the whole folder anywhere and run EGATREK.PRG from the desktop. It sets
the screen mode at startup and PUTS THE OLD ONE BACK when you quit, so you get
your desktop returned as you left it.

RUN IT FROM ITS OWN FOLDER. The four files are opened by bare name through
GEMDOS, so they are looked for in the current directory -- double-clicking
EGATREK.PRG from the folder it lives in is what you want. (The Amiga port
finds its files through PROGDIR: and does not care; this one does.)

FOUR FILES, and the game reads them at startup:

    EGATREK.PRG    the program
    STRINGS.DAT    every word on screen
    MUSIC.DAT      the music
    BRIEF.TXT      the twelve-page briefing, streamed a page at a time

A missing STRINGS.DAT plays with blank labels rather than refusing to start,
and a missing MUSIC.DAT plays silently.


PLAYING IT

Answer Y to the briefing at startup; it is twelve pages and explains the whole
game. Type HELP at the command line for the order list.

Commands are typed at CMD: in the COMMAND panel and are not case sensitive.
The setup prompts are line editors -- type the answer and press RETURN. The
arrow keys raise and lower shields, which is what the original's own help
screen lists first.

SAVE writes EGATREK.SAV into the same folder and the setup screen offers to
restore it. The hall of fame is TREK.SCR and is created on first use.


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

Sound is the YM2149 on two channels -- a square wave, because the original is
one square wave out of a PC speaker. Music on one channel and effects on the
other, so a laser does not cut the music off.

The text is the machine's own 8x16 system font, read out of ROM at startup.
The seventeen box-drawing and badge glyphs are this port's own artwork.


THIS IS NOT NELS ANDERSON'S CODE

Nothing here is disassembled, decompiled or translated from EGATREK.EXE. The
original was measured -- run under instrumentation, its constants read out of
the binary, its behaviour observed -- and reimplemented in C from those
measurements. The prose on screen, the briefing, the music and the
box-drawing glyphs are this port's own; none of Anderson's text or note data
is included.

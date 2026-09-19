EGA Trek for the Amiga
======================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


WORKBENCH ICONS

The zip holds two of them, and you want both:

    egatrek-amiga.info          beside the drawer -- makes the DRAWER visible
    egatrek-amiga/egatrek.info  inside it -- makes the GAME double-clickable

Copy the drawer AND the .info file next to it to your hard drive; AmigaDOS
pairs an icon with the thing of the same name beside it. Without them
Workbench does not show a program at all unless Show All Files is on, which is
why an earlier release looked like an empty drawer on a real machine even
though the game was sitting in it.

Running it from a Shell works exactly as before and needs neither.


RUNNING IT

OCS or ECS, Kickstart 2.0 or later, and about 200K free. Copy the whole drawer
anywhere and run it from a shell:

    egatrek

It opens its own 640x200 screen in sixteen colours and gives the machine back
when you quit: answering NO to "Play Again?" shows a farewell, waits for a key,
then closes the screen and returns you to the shell. The files it needs are found through PROGDIR:, which means the
program's own drawer -- so it does not matter what directory you run it from,
and it works from Workbench.

FOUR FILES, and the game reads them at startup:

    egatrek        the program
    STRINGS.DAT    every word on screen
    MUSIC.DAT      the music
    BRIEF.TXT      the twelve-page briefing, streamed a page at a time

A missing STRINGS.DAT plays with blank labels rather than refusing to start,
and a missing MUSIC.DAT plays silently.


PLAYING IT

Answer Y to the briefing at startup; it is twelve pages and explains the whole
game. Type HELP at the command line for the order list.

Commands are typed at CMD: in the COMMAND panel and are not case sensitive.
The arrow keys raise and lower shields, which is what the original's own help
screen lists first.

SAVE writes EGATREK.SAV into the program's drawer and the setup screen offers
to restore it. The hall of fame is TREK.SCR and is created on first use.


WHAT IS IN IT

All 25 commands, the nine-panel console, save and restore, the hall of fame,
and the mechanics the manual never mentions: boarding parties, the Vandal
death pod, plasma bolts in both directions, black holes, supernovae, the death
ray's five outcomes, tractor beams, wear and tear, reinforcements, a spy who
sabotages a system, a settlement with a clock running against it, and a
damaged ship that keeps getting worse if you do not fix it.

Messages are coloured by the department that speaks them, as the original's
are.

The MAIN VIEWER shows one page. The original has TEN and picks one AT RANDOM,
re-rolling every few seconds while it waits for you to type and alternating
each draw with a view from outside the ship; a player forces a page by typing
its number. This port draws the orbit page while orbiting and the nearest
enemy otherwise.

That is a future feature rather than an unknown -- the mechanism was read out
of the original binary in September 2026. What stops the ten pages is the
viewer panel's width: two of them are wider than it, and the panel's size is
shared by every port.

Sound is Paula on two channels -- a square wave, because the original is one
square wave out of a PC speaker. Music on one channel and effects on the
other, so a laser does not cut the music off.


THIS IS NOT NELS ANDERSON'S CODE

Nothing here is disassembled, decompiled or translated from EGATREK.EXE. The
original was measured -- run under instrumentation, its constants read out of
the binary, its behaviour observed -- and reimplemented in C from those
measurements. The prose on screen, the briefing, the music and the
box-drawing glyphs are this port's own; none of Anderson's text or note data
is included.

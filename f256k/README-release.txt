EGA Trek for the Foenix F256K
=============================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


RUNNING IT

An F256K with an SD card. Nothing else -- no SID chips, no expansion.

Copy all six files to the ROOT of the card, then at the SuperBASIC prompt:

    /- egatrek

(`/-` hands off to pexec, which loads and runs EGATREK.PGZ.)

SIX FILES, and the game wants all of them in the root directory:

    EGATREK.PGZ    the program
    OVERLAYS.BIN   the parts of the program that take turns in memory
    STRINGS.DAT    every word on screen
    MUSIC.DAT      the music
    BRIEF.TXT      the twelve-page briefing
    TREK.SCR       the hall of fame, and it ships EMPTY

OVERLAYS.BIN IS NOT OPTIONAL and must be the one that came with this
EGATREK.PGZ. The two are cut from a single build and the game checks a stamp
in the file; mixing them stops the game with a message rather than running
something that would be very hard to explain.

A missing STRINGS.DAT plays with blank labels rather than refusing to start,
and a missing MUSIC.DAT plays silently.

The game reads the card ONCE, at startup, and then only to save. Everything
it needs is in RAM after that.


PLAYING IT

Answer Y to the briefing at startup; it is twelve pages and explains the whole
game. Type HELP at the command line for the order list.

Commands are typed at CMD: in the COMMAND panel and are not case sensitive.
The setup prompts are line editors -- type the answer and press RETURN. The
cursor keys raise and lower shields, which is what the original's own help
screen lists first.

THIS KEYBOARD HAS NO ESC KEY. It is a C64 layout, so RUN/STOP is what the
game means by escape, and DEL is backspace.

SAVE writes EGATREK.SAV to the card and the setup screen offers to restore it.
The hall of fame is TREK.SCR and is updated as people finish games.


WHAT IS IN IT

All 25 commands, the nine-panel console, save and restore, the hall of fame,
and the mechanics the manual never mentions: boarding parties, the Vandal
death pod, plasma bolts in both directions, black holes, supernovae, the death
ray's five outcomes, tractor beams, wear and tear, reinforcements, a spy who
sabotages a system, a settlement with a clock running against it, and a
damaged ship that keeps getting worse if you do not fix it.

Messages are coloured by the department that speaks them, as the original's
are.

THE TEXT IS TALLER HERE THAN ON ANY OTHER 8-BIT PORT. The console is 80
columns by 25 rows, and this machine draws it in 8x16 character cells where
the others use 8x8. The original ran at 640x350 in an 8x14 cell, so this is
the closest any of these ports gets to its proportions.

The sixteen colours are EGA's own, loaded into Vicky's palette, so every
colour is exact -- including the brown that fixed-palette machines render as
olive.

The box-drawing and badge glyphs are this port's own artwork, built into the
machine's font memory at startup.

The MAIN VIEWER shows one page. The original has TEN and picks one AT RANDOM,
re-rolling every few seconds while it waits for you to type and alternating
each draw with a view from outside the ship; a player forces a page by typing
its number. This port draws the orbit page while orbiting and the nearest
enemy otherwise.

That is a decision rather than an unknown. The mechanism was read out of the
original binary, the ten pages were then costed, and they are not being
built. The viewer panel is seventeen columns on every port -- three of the
ten page titles are wider than that on their own -- and the page routines
would want about four thousand bytes of memory that ten of the thirteen
ports do not have. Keeping them on the disk instead would mean a disk read
every few seconds while you sat thinking about your next order.

Sound is the SN76489 PSG, two of its three tone channels: music on one and
effects on the other, so a laser does not cut the music off. The PSGs are
inside the machine's FPGA, so this works on every F256K -- the SID sockets are
not used and do not need to be filled.

AND THE MUSIC KEEPS PLAYING WHILE THE CARD IS READ. On the other 8-bit ports
a load stops the tune dead and the last note drones until it finishes. This
machine's storage is asynchronous, so the game advances the music while it
waits.


THIS IS NOT NELS ANDERSON'S CODE

Nothing here is disassembled, decompiled or translated from EGATREK.EXE. The
original was measured -- run under instrumentation, its constants read out of
the binary, its behaviour observed -- and reimplemented in C from those
measurements. The prose on screen, the briefing, the music and the
box-drawing glyphs are this port's own; none of Anderson's text or note data
is included.

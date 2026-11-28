EGA Trek for the NeXT
=====================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


RUNNING IT

A COLOUR NeXT running NeXTSTEP 3.3: a NeXTstation Color or Turbo Color, or
a NeXT with a NeXTdimension board. The program is a NeXTSTEP application,
EGATrek.app.

It has been run on a NeXTstation Turbo Color under NeXTSTEP 3.3, in the
Previous emulator (inside BMNeXT's simulator). It has not yet been run on
real hardware or under OPENSTEP 4.2.

A NeXT without a colour screen -- a NeXT Computer, a NeXTcube, a mono
NeXTstation -- is told so in words, and the program quits rather than
painting a console whose colours cannot be told apart.

egatrek-next.tar holds EGATrek.app. Copy it onto the NeXT and, in a
Terminal, in the folder you want the game in:

    tar xf egatrek-next.tar

Then double-click EGATrek.app in the Workspace. In Previous, the host's
shared folder appears on the NeXT as /Net/nfs.

THE GAME'S DATA IS INSIDE THE APPLICATION -- the words on screen, the music
and the briefing sit in the EGATrek.app folder beside the program -- so
copying EGATrek.app is the whole installation.

SAVE writes EGATREK.SAV into the folder that HOLDS EGATrek.app, and the
setup screen offers to restore it. The hall of fame is TREK.SCR, beside it
too, and is created on first use. That folder must be one you can write to.

It is a window, with the NeXT's own menu: Info (with the Info Panel), Hide
and Quit. QUITTING FROM THE MENU, OR CLOSING THE WINDOW, LEAVES AN UNSAVED
GAME -- SAVE first.


PLAYING IT

Answer Y to the briefing at startup; it is twelve pages and explains the whole
game. The orders are explained there -- there is no HELP command, and an order
the ship does not know gets NO SUCH ORDER.

Commands are typed at CMD: in the COMMAND panel and are not case sensitive.
The setup prompts are line editors -- type the answer and press RETURN (or
the keypad's ENTER). The up and down arrow keys raise and lower shields,
which is what the original's own help screen lists first. The key marked
"delete" erases.


WHAT IS IN IT

All 25 commands, the nine-panel console, save and restore, the hall of fame,
and the mechanics the manual never mentions: boarding parties, the Vandal
death pod, plasma bolts in both directions, black holes, supernovae, the death
ray's five outcomes, tractor beams, wear and tear, reinforcements, a spy who
sabotages a system, a settlement with a clock running against it, and a
damaged ship that keeps getting worse if you do not fix it.

Messages are coloured by the department that speaks them, as the original's
are, in EGA's own sixteen colours -- exactly, since a colour NeXT's four
bits a gun hold EGA's levels without rounding.

The MAIN VIEWER shows one page. The original has TEN and picks one AT RANDOM,
re-rolling every few seconds while it waits for you to type and alternating
each draw with a view from outside the ship; a player forces a page by typing
its number. This port draws the orbit page while orbiting and the nearest
enemy otherwise.

That is a decision rather than an unknown. The mechanism was read out of the
original binary, the ten pages were then costed, and they are not being built.
The viewer panel is seventeen columns on every port -- three of the ten page
titles are wider than that on their own -- and the page routines would want
about four thousand bytes of memory that most of these ports do not have.
Keeping them on the disk instead would mean a disk read every few seconds
while you sat thinking about your next order.

Sound is a square wave, because the original is one square wave out of a PC
speaker -- written out as 16-bit samples and played by NeXTSTEP's sound
library, at the pitch each note was meant to have. The NeXT plays one sound
at a time, so effects take the voice while they play and the music carries
on after, as the original's single speaker does. SND at the command line
turns sound off and on.

The text is the NeXT's own Courier, read at startup into 8x16 cells. The
seventeen box-drawing and badge glyphs are this port's own artwork, and so is
the icon.


THIS IS NOT NELS ANDERSON'S CODE

Nothing here is disassembled, decompiled or translated from EGATREK.EXE. The
original was measured -- run under instrumentation, its constants read out of
the binary, its behaviour observed -- and reimplemented in C from those
measurements. The prose on screen, the briefing, the music and the
box-drawing glyphs are this port's own; none of Anderson's text or note data
is included.

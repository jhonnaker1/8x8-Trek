EGA Trek for the colour Macintosh
=================================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


RUNNING IT

A colour Macintosh with:

    Color QuickDraw and 32-Bit QuickDraw -- built into System 7; on System
        6.0.x, the 32-Bit QuickDraw extension
    a screen of 640x480 or larger
    the screen set to 16 colours or more, in the Monitors control panel
    768K of memory for the program (640K at the least)

It has been run on a Quadra 800 under System 7.6.1 (in QEMU), and on a
Mac II under System 6.0.8 with 32-Bit QuickDraw (in MAME), both in 256
colours. It has not yet been run on real hardware.

A Mac that falls short is told so in words, naming what it lacks, and the
program goes back to the Finder rather than painting a screen you
could not read. A Mac Plus, SE or Classic has no colour and is not supported.

TWO FILES, ONE PROGRAM:

    egatrek.dsk    an 800K floppy image holding the application and nothing
                   else. Mount it in an emulator (or write it to a floppy)
                   beside any System 6 or 7 startup disk, and double-click
                   EGA Trek.
    egatrek.bin    the same application as MacBinary, for copying onto a
                   real Mac's hard disk. Decode it on the Mac (StuffIt
                   Expander does) rather than copying the .bin as it is.

THE GAME'S DATA IS INSIDE THE PROGRAM -- the words on screen, the music and
the briefing -- so there are no other files to keep beside it. Copy the
application anywhere.

SAVE writes EGATREK.SAV into the same folder as the application, and the
setup screen offers to restore it. The hall of fame is TREK.SCR, beside it
too, and is created on first use.

It is a window, with a menu bar: About EGA Trek in the Apple menu and Quit
in the File menu. QUIT FROM THE MENU LEAVES AN UNSAVED GAME -- SAVE first.


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
are, in EGA's own sixteen colours.

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

Sound is the Sound Manager's square-wave synthesizer on two channels -- a
square wave, because the original is one square wave out of a PC speaker.
Music on one channel and effects on the other, so a laser does not cut the
music off. System 6 grants only one channel; there the effects share it and
take it while they play, as the original's single speaker does. SND at the
command line turns sound off and on.

The text is the Mac's own Monaco, read at startup into 8x16 cells. The
seventeen box-drawing and badge glyphs are this port's own artwork, and so is
the Finder icon.


THIS IS NOT NELS ANDERSON'S CODE

Nothing here is disassembled, decompiled or translated from EGATREK.EXE. The
original was measured -- run under instrumentation, its constants read out of
the binary, its behaviour observed -- and reimplemented in C from those
measurements. The prose on screen, the briefing, the music and the
box-drawing glyphs are this port's own; none of Anderson's text or note data
is included.

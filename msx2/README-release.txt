EGA Trek for the MSX2
=====================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


RUNNING IT

An MSX2 with 128K of RAM AND AN MSX-DOS 2 KERNEL -- Nextor, which most SD
and IDE cartridges carry, or the MSX-DOS 2 cartridge. Played under openMSX on
a Philips NMS 8250 and a Sony HB-F1XD, each with Nextor; the MSX-DOS 2
cartridge and real hardware have not been tried.

A STOCK MSX2 WILL NOT RUN IT. Without an MSX-DOS 2 kernel the machine boots
this disk into Disk BASIC and the game never starts -- checked, not assumed.

egatrek-msx2.dsk is a 720K disk image. BOOT FROM IT: put it in drive A and
switch on. The game IS this disk's COMMAND2.COM, the program MSX-DOS 2 starts
after it boots, and there is no DOS prompt. Under openMSX:

    openmsx -machine Philips_NMS_8250 -ext SunriseIDE_Nextor -diska egatrek-msx2.dsk

The boot takes about twenty seconds from a floppy: the game is 53K. The title
music starts once the title has drawn and stops at the first key, as the
original's does.

IT CANNOT BE STARTED FROM A DOS PROMPT. The game needs all but a few hundred
bytes of the memory MSX-DOS 2 gives a program, and the command interpreter
keeps 1,280 bytes for itself when it runs one -- so the game checks at
startup, and from a prompt it says NOT ENOUGH MEMORY FOR EGA TREK and gives
the prompt back. The same message appears if a machine's disk interfaces
leave too little; every interface takes some.

QUITTING RESTARTS THE GAME. With no command interpreter to return to,
MSX-DOS 2 loads the disk's COMMAND2.COM again -- which is the game. The
title is back about twenty-five seconds later.

PAL and NTSC machines both work; the music keeps the same tempo on either.

THE FILES ON THE DISK

    MSXDOS2.SYS    MSX-DOS 2 -- see "MSX-DOS 2" below
    COMMAND2.COM   the game
    STRINGS.DAT    every word on screen
    MUSIC.DAT      the music
    BRIEF.TXT      the twelve-page briefing, streamed a page at a time

A missing STRINGS.DAT plays with blank labels rather than refusing to start,
and a missing MUSIC.DAT plays silently.


PLAYING IT

Answer Y to the briefing at startup; it is twelve pages and explains the whole
game. The orders are explained there -- there is no HELP command, and an order
the ship does not know gets NO SUCH ORDER.

Commands are typed at CMD: in the COMMAND panel and are not case sensitive.
The setup prompts are line editors -- type the answer and press RETURN. The
cursor keys up and down raise and lower shields, which is what the original's
own help screen lists first. BS deletes.

SAVE writes EGATREK.SAV onto the disk and the setup screen offers to restore
it. The hall of fame is TREK.SCR and is created on first use. Both need the
disk to be writable.


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

That is a decision rather than an unknown. The mechanism was read out of the
original binary, the ten pages were then costed, and they are not being
built. The viewer panel is seventeen columns on every port -- three of the
ten page titles are wider than that on their own -- and the page routines
would want about four thousand bytes of memory that most of these ports do
not have. Keeping them on the disk instead would mean a disk read every few
seconds while you sat thinking about your next order.

The screen is SCREEN 7 -- 512x212 in sixteen colours -- with the console
drawn as 80 columns of 6x8 cells and EGA's own palette programmed into the
V9938. A full console takes about two seconds to draw; the V9938's command
engine fills the blank areas and draws each character, and the machine's own
sprites are switched off because the game has none.

Sound is the PSG on two channels -- a square wave, because the original is
one square wave out of a PC speaker. Music on one channel and effects on the
other, so a laser does not cut the music off.

The 6x8 font and the box-drawing glyphs are this project's own artwork.


MSX-DOS 2

MSXDOS2.SYS on this disk is MSX-DOS 2 as distributed with Nextor, (c) The
MSX Licensing Corporation, and is included under the terms in
NEXTOR-LICENSE.txt beside this file: free of charge, and not for sale. It is
the only part of this disk that is not this port's own.


THIS IS NOT NELS ANDERSON'S CODE

Nothing here is disassembled, decompiled or translated from EGATREK.EXE. The
original was measured -- run under instrumentation, its constants read out of
the binary, its behaviour observed -- and reimplemented in C from those
measurements. The prose on screen, the briefing, the music and the
box-drawing glyphs are this port's own; none of Anderson's text or note data
is included.

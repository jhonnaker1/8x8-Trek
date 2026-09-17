EGA Trek for the Apple IIgs
===========================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


WHAT YOU NEED

An Apple IIgs and a 3.5" drive. That is the whole list -- any ROM, any memory
size, no expansion card, and no system software.


HOW TO RUN IT

Put EGATREK.PO in a 3.5" drive and switch the machine on.

That is all there is to it. THERE IS NO PRODOS ON THIS DISK and nothing to
type: the machine reads the first block, which is this port's own loader, and
that reads the game. It takes about fifteen seconds, and the title screen
appears when it is done.

On an emulator, mount it as the 3.5" disk. Under MAME:

    mame apple2gs -flop3 EGATREK.PO


WHAT THIS PORT IS

Super Hi-Res, 320x200, which is exactly 40 columns by 25 rows of 8x8 cells --
no margin and no rounding. The palette holds EGA's own sixteen colours, so
what you see is what the original's EGA card put on a monitor, brown included.

Sound is the Ensoniq 5503: two oscillators, music on one and effects on the
other, so a hit during the title tune does not chop it. The original had one
PC speaker and could not do that.

The disk is entirely this port's own -- its own boot block, its own directory,
and its own reader over the drive's firmware. Nothing on it belongs to anyone
but Nels Anderson and this project, which is why it can be given away.


THE CONSOLE IS IN TWO HALVES -- PRESS C FOR THE OTHER ONE

EGA Trek's console is eighty columns wide and this machine's graphics mode is
forty, so it is the same console seen a half at a time. You spend the game on
the tactical page; C at the CMD: prompt shows the chart page, and any key
brings you back. C costs no turn, and neither does looking.


SAVING

There is room on the disk for four saved games. The disk is not write
protected, and it needs not to be -- saves go on the same disk the game came
from.


WHAT IS DIFFERENT TO LOOK AT

The font is this project's own, drawn for these ports, because a IIgs's
character generator is not readable by the CPU -- so unlike the Amiga and ST
builds, not one glyph here comes out of the machine's ROM.

Otherwise: it is the game.

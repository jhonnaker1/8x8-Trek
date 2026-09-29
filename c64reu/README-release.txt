EGA Trek for the Commodore 64 with an REU
=========================================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


RUNNING IT

Any Commodore 64 or 64C, PAL or NTSC, WITH A RAM EXPANSION UNIT: a 1700,
1764 or 1750, or anything that emulates one. 128K is enough. Without one the
game says so and stops; the Commodore 64 disk (egatrek-c64.d64) needs none.

The port works out whether it is on a PAL or an NTSC machine by counting
raster lines, and tunes the SID to match.

Everything is on the one disk image. Put it in drive 8 and:

    LOAD "TREKREU",8,1

then

    RUN

In VICE:

    x64sc -reu -reusize 128 -autostart egatrek-c64reu.d64:trekreu

IT TAKES A WHILE TO START, AND THEN IT IS FAST. Before the title screen the
game reads all twenty of its code overlays off the disk into the REU -- the
screen counts them -- about 47 seconds with JiffyDOS. After that it never
loads code from the disk again: a screen change or an order that took three
to six seconds on the Commodore 64 disk takes about one here. A fastloader
shortens the start; it does nothing for the rest, which is already DMA.


THE CONSOLE IS IN TWO HALVES

EGA Trek's console is eighty columns wide. This machine has forty, so it is
the same console seen a half at a time.

You spend the game on the TACTICAL page:

    short range scan, status, lasers, the command line, the main viewer,
    and the message log

PRESS C AT THE CMD: PROMPT and the CHART page replaces it:

    the long range chart, the state of every system, and the ship's badge

ANY KEY BRINGS YOU BACK. C costs no turn, and neither does looking.

Nothing is lost, only moved -- every panel of the eighty-column console is on
one page or the other. Messages arrive on the tactical page, which is where
any key puts you back, so you will not miss one by looking at the chart.

At the CMD: prompt, type an order and press RETURN. The briefing explains the
orders -- there is no HELP command, and one the ship does not know gets NO
SUCH ORDER. Every prompt in this game is a line editor -- type your answer and
press RETURN, including the ones that ask Y or N.

RUN/STOP is ESC, which is what the self-destruct prompt wants when it offers
you a way out.


WHAT IS ON THE DISK

    trekreu       the game
    strings.dat   every word on screen
    music.dat     the music
    brief.txt     the mission briefing, 24 pages, read from the disk as you
                  page through it
    trek.scr      the hall of fame; it starts empty and the game writes to it
    ovl*          twenty code overlays, read into the REU at the start

SAVE writes EGATREK.SAV to the same disk, so do not write-protect it.


ABOUT THIS PORT

Nothing here is disassembled or translated from EGATREK.EXE. The original was
measured -- its rules read out of the binary and its screens photographed --
and then reimplemented. The prose, the music and the briefing are this port's
own work.

This is the Commodore 64 port with its code moved. The C64 disk keeps about
forty kilobytes of the game in memory and swaps eleven overlays in from the
drive; this one keeps twenty-two and swaps twenty from the REU, where a swap
is a copy by the REU's own DMA rather than a disk read. Because of that, one
overlay may call another here -- the ship's engine and the console drawing
live in the REU too -- which the disk version cannot allow.

It is also the game the C64 OS version is built from: egatrek-c64os.zip,
released alongside it, is this port made into a C64 OS application.

TESTED IN VICE, with its REU, and not yet on a real one. If you run it on
hardware, the project would like to hear how it went.

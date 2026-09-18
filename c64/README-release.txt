EGA Trek for the Commodore 64
=============================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


RUNNING IT

Any Commodore 64 or 64C, PAL or NTSC. The port works out which one it is on
by counting raster lines, and tunes the SID to match -- the same frequency
word is nearly half a semitone apart on the two machines.

Everything is on the one disk image. Put it in drive 8 and:

    LOAD "TREK64",8,1

then

    RUN

A fastloader helps a lot but is not needed. There are eleven code overlays on
this disk and the game swaps them in as you move between screens, so a JiffyDOS
or an SD2IEC with fastload makes the screen changes noticeably quicker.


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

At the CMD: prompt, type HELP for the full list of orders. Every prompt in
this game is a line editor -- type your answer and press RETURN, including
the ones that ask Y or N.

RUN/STOP is ESC, which is what the self-destruct prompt wants when it offers
you a way out.


WHAT IS ON THE DISK

    trek64        the game
    strings.dat   every word on screen
    music.dat     the music
    brief.txt     the mission briefing, 24 pages, read from the disk as you
                  page through it
    trek.scr      the hall of fame; it starts empty and the game writes to it
    ovl*          eleven code overlays

SAVE writes EGATREK.SAV to the same disk, so do not write-protect it.


ABOUT THIS PORT

Nothing here is disassembled or translated from EGATREK.EXE. The original was
measured -- its rules read out of the binary and its screens photographed --
and then reimplemented. The prose, the music and the briefing are this port's
own work.

The Commodore 64 is the eighth machine this port runs on, and it took the
least new code of any of them: its screen driver was written for the Commodore
128's forty-column mode and runs here unchanged, because a VIC-IIe in forty
columns and a VIC-II are the same chip. It is also roomier than you would
expect -- BASIC's ROM is switched out to give the program its memory, and the
words you are reading on screen live in the RAM underneath the KERNAL.

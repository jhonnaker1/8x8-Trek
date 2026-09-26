EGA Trek for the Commodore Plus/4
=================================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


RUNNING IT

Any Commodore Plus/4, 50Hz or 60Hz. The port works out which one it is on by
reading TED's raster counter -- 312 lines against 262 -- and sets the music's
tempo to match. A Commodore 16 will not run this: the game needs the Plus/4's
full 64K.

Everything is on the one disk image. Put it in drive 8 and:

    LOAD "TREK4",8

then

    RUN

Plain LOAD, not LOAD ...,8,1. The program carries its own BASIC line and is
started with RUN, so it wants the ordinary BASIC load that relinks BASIC's
pointers -- which is what LOAD "TREK4",8 does.

A fastloader helps. There are eleven code overlays on this disk and the game
swaps them in as you move between screens, so a JiffyDOS or an SD2IEC with
fastload makes the screen changes noticeably quicker.


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

ESC is ESC on this machine, which is what the self-destruct prompt wants when
it offers you a way out.


QUITTING RESTARTS THE MACHINE

The game runs with RAM mapped over both ROMs, so there is no BASIC underneath
to return to. Q hands the machine back by resetting it: you get a fresh boot,
not the BASIC you started from. The farewell screen says so and waits for a
key first.


WHAT IS ON THE DISK

    trek4         the game
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

The Plus/4 is the twelfth machine this port runs on, and the only one that was
set aside twice before it booted -- for a reason that is invisible in a
specification sheet. On a Commodore 64 a program can page out BASIC and still call the
KERNAL; here the switch is all or nothing, so with the game's own memory in
place there is no KERNAL to call and every system call has to be wrapped in
code that maps the ROM back for the length of it. TED's two tone generators
give the music and the sound effects a voice each, which the PC speaker the
original was written for never had.

EGA Trek for C64 OS
===================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


WHAT YOU NEED

C64 OS, on a Commodore 64 or 64C, PAL or NTSC, WITH A RAM EXPANSION UNIT.
Built and tested against C64 OS 1.08.

THE REU MUST HAVE TWO 64K BANKS FREE for the game. C64 OS keeps bank 0 for
itself and one bank for each fast app switching slot you have configured;
EGA Trek asks C64 OS for two more after those. So the REU needs room for
the slots plus three banks:

    512K REU      at most 5 fast app switching slots
    1M REU        at most 13
    2M REU        at most 29
    4M and up     any number C64 OS offers (it allows 31)

If there is not room, the game says so, names the setting to change, and
returns you to C64 OS.


INSTALLING IT

This zip holds egatrek.car, a C64 OS archive, and this file.

Copy egatrek.car onto your C64 OS volume, into //os/applications/, and
double-click it there: C64 OS unpacks it and an EGA Trek folder appears.
Or unpack it anywhere and move the EGA Trek folder into //os/applications/.

Then open EGA Trek from Applications, as any other C64 OS app.


RUNNING IT

IT TAKES A MOMENT TO START, AND THEN IT IS FAST. Before the title screen the
game reads its code overlays from its folder into the REU -- the screen counts
them to 23 -- and never loads code from disk again: every screen change after
that is a copy by the REU's own DMA.

THE GAME TAKES THE WHOLE MACHINE WHILE IT PLAYS. C64 OS's menu bar and fast
app switching are not there until you leave; when the mission is over the
game says HIT A KEY TO RETURN TO C64 OS, and does.


THE CONSOLE IS IN TWO HALVES

EGA Trek's console is eighty columns wide. This machine has forty, so it is
the same console seen a half at a time.

You spend the game on the TACTICAL page:

    short range scan, status, lasers, the command line, the main viewer,
    and the message log

PRESS C AT THE CMD: PROMPT and the CHART page replaces it:

    the long range chart, the state of every system, and the ship's badge

ANY KEY BRINGS YOU BACK. C costs no turn, and neither does looking.

At the CMD: prompt, type an order and press RETURN. The briefing explains the
orders -- there is no HELP command, and one the ship does not know gets NO
SUCH ORDER. Every prompt in this game is a line editor -- type your answer and
press RETURN, including the ones that ask Y or N.

RUN/STOP is ESC, which is what the message log, and the self-destruct prompt
when it offers you a way out, want.


WHAT IS IN THE FOLDER

    main.o        the game
    ovl*          23 code overlays, read into the REU at the start
    strings.dat   every word on screen
    music.dat     the music
    brief.txt     the mission briefing, read from the folder as you page
                  through it
    trek.scr      the hall of fame; it starts empty and the game writes to it
    menu.m        the menu C64 OS shows for the app
    about.t       what C64 OS says about it

SAVE writes EGATREK.SAV into the same folder, and the hall of fame is kept
there too, so the volume must not be write-protected.


ABOUT THIS PORT

Nothing here is disassembled or translated from EGATREK.EXE. The original was
measured -- its rules read out of the binary and its screens photographed --
and then reimplemented. The prose, the music and the briefing are this port's
own work.

This is the Commodore 64 with an REU port (egatrek-c64reu.d64) made into a
C64 OS application: the same game and the same overlays, drawn into C64 OS's
screen and font, reading its keys, files and REU banks through C64 OS, and
quitting back to it. C64 OS's font has no box-drawing characters, so the game
lends fifteen of its icon slots the console's lines while it runs and puts
C64 OS's icons back when you quit.

TESTED IN VICE, on C64 OS 1.08 from a CMD HD image with a 16MB REU and eight
fast app switching slots, and played through, start to quitting. Not yet on
real hardware. If you run it on a real machine, the project would like to hear
how it went.

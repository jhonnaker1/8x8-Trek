EGA Trek for the Atari ST and STE
=================================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


RUNNING IT

Any ST, Mega ST, STE or Mega STE with 1MB of RAM and a COLOUR monitor or a
television. The game needs about 110K, so 512K is enough in principle; 1MB is
what it was developed on.

Copy the whole folder anywhere -- a floppy, a hard disk partition, or an
emulator's GEMDOS drive -- and double-click EGATREK.PRG. The four files must
stay together: the program opens them by name from the directory it is run
from.

IT SWITCHES TO LOW RESOLUTION ITSELF and puts your desktop back when it quits.
If you are in ST HIGH (monochrome), it will say so and exit rather than paint
an unreadable screen: the console needs sixteen colours and mono has none.

On an STE you get a slightly better picture for free. The palette is written
in the STE's four-bits-a-gun encoding, whose extra bit a plain ST simply
ignores, so the same program shows the nearest of 512 colours on an ST and the
nearest of 4096 on an STE.


THE CONSOLE IS IN TWO HALVES

EGA Trek's console is eighty columns wide. This machine gives forty at sixteen
colours, so it is the same console seen a half at a time.

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


WHAT IS IN THE FOLDER

    EGATREK.PRG   the game
    STRINGS.DAT   every word on screen
    MUSIC.DAT     the music
    BRIEF.TXT     the mission briefing, 24 pages, read from disk as you page

SAVE writes EGATREK.SAV beside them, so the folder must not be read-only.


ABOUT THIS PORT

Nothing here is disassembled or translated from EGATREK.EXE. The original was
measured -- its rules read out of the binary and its screens photographed --
and then reimplemented. The prose, the music and the briefing are this port's
own work, and so are the seventeen box-drawing and badge glyphs. The letters
and digits come from your machine's own ROM font, so no font is shipped.

The ST is the ninth machine this port runs on and the cheapest of them to
reach: every seam except the video driver was already written for the Atari
Falcon, and `vc +tos` builds for both.

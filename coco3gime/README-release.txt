EGA Trek for the Tandy Color Computer 3
=======================================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


WHAT YOU NEED

A CoCo 3 with 128K and a disk drive. That is the whole list.

There is a second CoCo 3 build of this game, egatrek-coco3, and it needs a
SuperSprite FM+ card. This one does not. It uses the CoCo 3's own GIME for
80x25 text in eight colours and the machine's own 6-bit DAC for sound, so it
runs on a stock machine -- a real one, a CoCo SDC, or an emulator.

A 512K machine works too and gains nothing; the game fits in 128K.


HOW TO RUN IT

Write TREK.DSK to a floppy, or mount it on a CoCo SDC. Then:

    CLEAR 25,&H6FFF
    LOADM"TREKLDR"
    EXEC

Then wait. The loader reads 45K with no progress bar -- about a minute on a
real floppy, much quicker on an SDC -- and the title screen appears when it
is done.


WHAT IS DIFFERENT ABOUT THIS BUILD

The GIME's text mode has no box-drawing characters, no block and no reverse
video, so the console is drawn with what the font does have: panel rules are
underlines, which join across cells into continuous lines, and solid cells are
spaces in a background colour. Vertical rules are dotted, because a `|` does
not reach the top and bottom of its cell. The badge is a filled rectangle
rather than a rounded disc -- there are no half-height blocks to round it with.

Eight colours, and they are the eight the console needs: every colour that
carries a game rule survives, and only decoration folds.

Sound is one voice. Every other machine this game has been ported to has a
sound chip that holds a pitch in a register; the CoCo has a DAC and a tone
exists only while something toggles it. So music and effects share the
speaker and an effect interrupts the tune -- which is exactly what the
original's PC speaker did.

The string pool lives on the disk rather than in memory, so the game reads the
drive while it draws. That is what makes a 45K program, a 4,000-byte screen
and 7K of text fit in a 64K address space at once.


THE CONTROLS

Type commands at the CMD: prompt. `?` lists them. `C` shows the chart; any key
returns from it.

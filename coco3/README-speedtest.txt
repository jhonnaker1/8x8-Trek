DOUBLE SPEED ON A REAL CoCo 3 -- the one test that needs no SuperSprite
======================================================================

WHAT THIS ANSWERS

EGA Trek's CoCo 3 port runs the machine at 1.78 MHz, twice the standard
speed, from the title screen onward -- including every overlay load and every
SAVE. CoCo 3 disk I/O at double speed is a known hazard on real hardware, and
no emulator reproduces it faithfully. That is the last open question about
this port, and it has been untestable because the game needs a SuperSprite
FM+ video card.

BUT IT IS A DISK QUESTION. This program touches no video at all -- no
SuperSprite, no V9958, no sound chip. It runs on a bare CoCo 3 with any disk
controller, a CoCo SDC included.


RUNNING IT

Mount or insert SPEEDTST.DSK as drive 0, then:

    CLEAR 25,&H6FFF
    LOADM"SPEEDTST"
    EXEC

It reads 306 sectors TWICE at 0.89 MHz, then the same 306 sectors twice at
1.78 MHz, and returns to the BASIC prompt. On a floppy that is a couple of
minutes; on an SDC it should be quick. The drive light will work throughout.

Nothing is written to the disk. It only reads.


READING THE RESULT

    PRINT PEEK(&H7F0D)

    90 means it finished. Anything else means it stopped early, and
    PRINT PEEK(&H7F01) says where: 161 = started, 177 = in the slow pass,
    178 = in the fast pass, 179 = both passes done.

Then the two columns:

    PRINT PEEK(&H7F04), PEEK(&H7F05)      0.89 MHz: errors, mismatches
    PRINT PEEK(&H7F08), PEEK(&H7F09)      1.78 MHz: errors, mismatches

FOUR ZEROES IS A PASS and the item closes.

WHAT MATTERS IS THE DIFFERENCE BETWEEN THE TWO LINES, not either one alone. A
tired drive or a marginal disk shows up in BOTH columns and says nothing about
the clock. Errors that appear only in the second line are the hazard this was
built to look for.

If the fast column is not clean:

    PRINT PEEK(&H7F0A), PEEK(&H7F0B)      first bad track, first bad sector

and the fix is already worked out -- drop to 0.89 MHz around the sector reads
and put it back, which costs two port writes per sector.


WHAT THE TWO COUNTS MEAN

"errors" are sectors the controller itself reported as failed. "mismatches"
are worse: the sector was read twice, the controller said both were fine, and
the two copies differ. A controller that reports success and hands back a
wrong byte is exactly what double speed is suspected of, and a status byte
cannot see it.

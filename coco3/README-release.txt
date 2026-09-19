EGA Trek for the Tandy Color Computer 3
=======================================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


WHAT YOU NEED

A Color Computer 3, a SuperSprite FM+ video card, and something to read a
disk with -- a real floppy drive or a CoCo SDC.

THE CARD IS NOT OPTIONAL. The CoCo 3's own video gives eight colours in
80 columns; this game wants sixteen per pixel and a 512-wide bitmap, and the
SuperSprite's V9958 is what provides them. Without the card there is nothing
to run this on -- the program will load and run and draw a perfect picture
into a chip that is not there, and your monitor will keep showing the BASIC
screen.

AND NOBODY HAS RUN THIS ON A REAL ONE. Every claim in this file was measured
under MAME, whose SuperSprite FM+ emulation is what the whole port was
developed against. The author of the port does not have the card. THIS PORT IS
THEREFORE CORRECT AS FAR AS AN EMULATOR CAN SHOW AND UNTESTED ON THE MACHINE --
the distinction matters, and the rest of this file tries to be honest about
which half any given sentence rests on.

In a Multi-Pak, put the DISK CONTROLLER IN SLOT 4 and the card in slot 1. That
order is not a preference: with the controller anywhere else the machine will
not boot Disk BASIC at all. The card answers at $FF7x regardless of which slot
is selected, which is why the two coexist. You need the Multi-Pak: the card
and the controller both want the cartridge port.


A CoCo SDC SHOULD WORK, AND THIS IS USUALLY THE PART THAT DOES NOT

Most SD-card replacements hook DSKCON in software, and this port never calls
DSKCON -- it runs in all-RAM mode with no ROM to call and drives the WD1773's
own registers. That is the exact thing that makes software fail on those
devices. The CoCo SDC is built for this case: it emulates the floppy
controller in hardware, and it is in that mode unless something deliberately
takes it out.

Four settings, all of them the factory defaults:

    DIP "DRGN"      OFF        -- the CoCo address scheme, not the Dragon one
    DIP 4, 2, 1     all OFF    -- Flash bank 0, which holds SDC-DOS
    Jumper DRQ      NOT fitted -- it routes the drive's DRQ to the CART FIRQ
                                  line, which is a Dragon arrangement
    Jumper AUTO     NOT fitted

MOUNT TREK.DSK FROM THE SD CARD, NOT OVER DRIVEWIRE. DriveWire is implemented
in software by SDC-DOS and gets none of the hardware emulation -- the guide
says the emulation features "are only available to images located on the SD
card". Mount it as an ordinary disk image in drive 0; a raw block array will
not work either, for the same reason.

This has NOT been run on real hardware. What has been checked is the one thing
that is checkable without it: $43 written to $FF40 is how a program takes the
SDC out of FDC Emulation Mode, and this port never writes it -- it uses four
values, $29 and $A9 reading, $39 and $B9 writing, across 5,911 writes. MAME
has no SDC device to test against, so that is a removed risk rather than a
confirmation.


RUNNING IT

Put the disk in drive 0 and type:

    CLEAR 25,&H6FFF
    LOADM"TREKLDR"
    EXEC

THEN WAIT, AND KEEP WATCHING THE BASIC SCREEN. Nothing appears to happen while
the loader reads 44K in, and there is no progress bar. ON A REAL FLOPPY THAT IS
ABOUT SIXTY SECONDS; on a CoCo SDC it is far quicker, because there is no head
to step. When it finishes, THE MONITOR SWITCHES ITSELF to the card's output and
the title screen is there. If you give up at forty seconds on a real drive you
will conclude it does not work; it does.

Quitting hands the monitor back to the CoCo and restarts BASIC.


PLAYING IT

Answer Y to the briefing at startup; it is twelve pages and explains the whole
game. Type HELP at the command line for the order list.

Commands are typed at CMD: in the COMMAND panel and are not case sensitive.
The setup prompts are line editors -- type the answer and press RETURN.

THERE IS NO KEYBOARD BUFFER. The game reads the keyboard matrix directly, so
anything typed while a panel is redrawing is lost. Let the console finish
before you type.

SAVE writes EGATREK.SAV to the disk in drive 0 and the setup screen offers to
restore it. The disk ships with room for it.


WHAT IS IN IT

All 25 commands, the nine-panel console, save and restore, the hall of fame,
and the mechanics the manual never mentions: boarding parties, the Vandal
death pod, plasma bolts in both directions, black holes, supernovae, the death
ray's five outcomes, tractor beams, wear and tear, reinforcements, a spy who
sabotages a system, a settlement with a clock running against it, and a
damaged ship that keeps getting worse if you do not fix it.

Music and effects come from the card's YM2149, two voices: one carries the
tune and one carries the effects, so a hit during the title track does not
chop it.


THINGS TO KNOW

IT IS NOT BRISK. A full console repaint takes about four seconds, because
every character is drawn a pixel-pair at a time across a card the 6809 reaches
through two I/O ports. It is playable, and that is the honest word for it.

(This paragraph used to rank the port against the others -- "the slowest of
the seven", then "of the eight". Both were true when written and neither
stayed true, because a ranking is a claim about every port and the list keeps
growing. The four seconds is the measurement; the ranking was the part that
kept going stale.)
The game runs the CPU at 1.78 MHz, twice the CoCo 3's default, to help.

THE 1.78 MHz MODE HAS NOT BEEN TRIED ON REAL HARDWARE. Everything here was
developed and tested under MAME. CoCo 3 disk access at double speed is
historically a hazard, and if this port misbehaves on a real machine that is
the first thing to suspect.

WHERE THAT WOULD SHOW. The 44K load is NOT at risk -- the loader runs before
the game touches the speed latch, so it reads at the standard 0.89 MHz. The
switch to 1.78 MHz is the last line of the video setup, so everything after the
title screen is at double speed: the briefing and the music are read then, and
so is every SAVE. A machine that loads and shows a title screen but comes up
with no words in it, or that fails only on SAVE, is pointing straight at this.

THE DISK IS A STANDARD DISK BASIC DISKETTE, 35 tracks single sided, written by
this project's own tools. DIR will list it.


WHAT IS ON THE DISK

    TREKLDR.BIN    the loader you EXEC
    EGATREK.RAW    the game itself, 44K, read into place by the loader
    *.OVL          eleven overlays, paged in as the game needs them
    STRINGS.DAT    every word on screen
    MUSIC.DAT      the music
    BRIEF.TXT      the twelve-page briefing, streamed a page at a time

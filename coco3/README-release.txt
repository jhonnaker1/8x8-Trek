EGA Trek for the Tandy Color Computer 3
=======================================

A remake of EGA Trek, written by Nels Anderson and released as shareware
between 1988 and 1992. The original is his; this is a port of it to a
different machine. If you enjoy this, go and find the original -- and if you
enjoy that, register it. That was always the deal.

    https://archive.org/details/EGATrek


WHAT YOU NEED

A Color Computer 3, a disk drive, and a SuperSprite FM+ video card.

THE CARD IS NOT OPTIONAL. The CoCo 3's own video gives eight colours in
80 columns; this game wants sixteen per pixel and a 512-wide bitmap, and the
SuperSprite's V9958 is what provides them. Without the card there is nothing
to run this on.

In a Multi-Pak, put the DISK CONTROLLER IN SLOT 4 and the card in slot 1. That
order is not a preference: with the controller anywhere else the machine will
not boot Disk BASIC at all. The card answers at $FF7x regardless of which slot
is selected, which is why the two coexist.


RUNNING IT

Put the disk in drive 0 and type:

    CLEAR 25,&H6FFF
    LOADM"TREKLDR"
    EXEC

THEN WAIT ABOUT A MINUTE, AND KEEP WATCHING THE BASIC SCREEN. Nothing appears
to happen for roughly sixty seconds while the loader reads 44K off the floppy
-- a real drive is not fast and there is no progress bar. When it finishes,
THE MONITOR SWITCHES ITSELF to the card's output and the title screen is
there. If you give up at forty seconds you will conclude it does not work; it
does.

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

IT IS THE SLOWEST OF THE SEVEN PORTS. A full console repaint takes about four
seconds, because every character is drawn a pixel-pair at a time across a card
the 6809 reaches through two I/O ports. It is playable and it is not brisk.
The game runs the CPU at 1.78 MHz, twice the CoCo 3's default, to help.

THE 1.78 MHz MODE HAS NOT BEEN TRIED ON REAL HARDWARE. Everything here was
developed and tested under MAME. CoCo 3 disk access at double speed is
historically a hazard, and if this port misbehaves on a real machine that is
the first thing to suspect.

THE DISK IS A STANDARD DISK BASIC DISKETTE, 35 tracks single sided, written by
this project's own tools. DIR will list it.


WHAT IS ON THE DISK

    TREKLDR.BIN    the loader you EXEC
    EGATREK.RAW    the game itself, 44K, read into place by the loader
    *.OVL          eleven overlays, paged in as the game needs them
    STRINGS.DAT    every word on screen
    MUSIC.DAT      the music
    BRIEF.TXT      the twelve-page briefing, streamed a page at a time

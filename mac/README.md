# EGA Trek for the colour Macintosh

**Status: released in v0.24.0 (2026-11-26), and played** -- Jamie, on QEMU's
Quadra 800 under System 7.6.1 in 256 colours: *"works perfectly"*. A Mac
II-class machine on System 6 has not been tried. See NOTES.md, "SCOPE: the
colour Macintosh", for what was measured.

A Toolbox application: a window holding the 80x25 console, a menu bar
(About, Quit, an Edit menu for desk accessories), and the game's data built
into the program as resources, so copying the application is the whole
installation. Saves and the hall of fame are files beside it.

**Needs** a colour Mac with Color QuickDraw and 32-Bit QuickDraw (built into
System 7; an INIT on 6.0.x), a 640x480 screen, and 16 colours or more. A Mac
that falls short is told so in words and the program returns to the Finder.

## Building

Needs [Retro68](https://github.com/autc04/Retro68); here it lives on the
Retro68 sparse bundle (`/Volumes/Retro68`), as commodore-uno/mac found it.

    make verify                 the gate `make ports` runs: tools/verify_mac.py
    make release                build/egatrek-mac.zip, the release asset
    make                        build/EGATREK.bin (MacBinary), build/EGATREK.dsk
                                (800K floppy, the application alone), and
                                the host test of the note table
    make smoke                  build/SMOKE.bin, the first-light test
    tools/q800.sh build/EGATREK.bin OUTDIR "wait:85 key:ret wait:20 shot:a"
    tools/q800play.sh build/EGATREK.bin     to PLAY it, in Screen Sharing

`q800.sh` copies the application into the Startup Items of a clone of the
System 7.6.1 disk on the Retro68 volume, boots QEMU's Quadra 800, records
its sound, and runs screenshot and key steps against the QEMU monitor.

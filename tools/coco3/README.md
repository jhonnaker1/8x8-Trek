# CoCo 3 + SuperSprite FM+ probes

MAME with `-ext ssfm` is the instrument. These Lua scripts poke the V9958 from
the CoCo's side and read results back to the host; the port's own driver gets
written against what they establish.

    diag.lua   Does a Lua write reach the V9958? Writes VRAM through
               $FF78/$FF79 and reads it back. DE AD BE EF round-trips.
    dump.lua   Sets GRAPHIC6, draws a bracket, and DUMPS ALL OF VRAM to
               vram.bin for the host to reconstruct. This is the verification
               path for the video driver -- see below.
    rom.lua    Dumps $8000..$FEFF, the BASIC ROMs.
    sectest.c  Reads track 17 sector 3 with STANDALONE DSKCON -- no Disk
               BASIC ROM -- and leaves the sector at $3000 and DCSTA at
               $3100 for the host to check.

**USE THE DEBUGGER, NOT LUA, TO DRIVE A LOADED PROGRAM.**
`emu.add_machine_frame_notifier` in this build stops firing after the first
callback that does substantial work -- setting PC, or a run of VDP writes --
so a script that pokes a program in and then waits for it never sees the
result. It looks exactly like the program hanging. `-debug -debugscript` is
reliable and is what the blit benchmark already used:

    gtime 3000
    load prog.raw,3800          ; strip the 5-byte DECB header first
    pc=3800
    gtime 800
    printf "%04X %02X\n",pc,b@3100
    dump out.txt,3000,100,1,0   ; NOTE: lengths are HEX

Run a Lua probe with:

    mame coco3 -rompath "$HOME/Library/Application Support/Ample/roms" \
         -ext ssfm -autoboot_script <f>.lua -seconds_to_run 8 \
         -nothrottle -video none -window

## MAME WILL NOT SHOW YOU THE CARD'S SCREEN

`-listdevices` reports `:ext:ssfm:screen` at 544x466/50.16Hz, and it renders
black no matter what. **The VDP is provably alive**: VRAM round-trips, and
reading the status register twice gives `80` then `00` -- the vblank flag set,
and the read cleared it. Setting the BACKDROP to white (R#7 = 15) still
produces nothing, which rules out the bitmap and the palette. `-view "Screen 1
Pixel Aspect"` does not select it either; snapshots stay the 1285x466
composite with the card's half black.

**So verify by reading VRAM back, not by looking.** The driver's job is to put
the right bytes in VRAM; whether MAME paints them is MAME's problem.
`dump.lua` plus a host-side reconstruction renders the bitmap as a PNG, and
that is a stronger check than a screenshot anyway -- it is the same move as
confirming the Falcon's geometry by drawing a figure rather than trusting a
byte count.

## The whole three-piece stack runs

    -ext multi -ext:multi:slot1 ssfm -ext:multi:slot4 fdc -flop1 <disk>.dsk

CoCo 3 + Multi-Pak + SuperSprite FM+ (V9958 **and** YM2413) + a WD1773 and a
drive. The scope's "three pieces of hardware" is all modelled.

## STORAGE WORKS WITHOUT THE DISK BASIC ROM, and that is what makes 55K fit

cmoc ships two disk layers and the difference decides the memory map:

  * **`disk.h`** is a Disk BASIC filesystem -- `openfile`, `read`, `seek`,
    `close` -- and it is **READ-ONLY** ("to do both read and write
    operations, see the decbfile library", which is not installed here). It
    goes through the ROM's DSKCON, so it needs ROM mapped at `$C000`.
  * **`dskcon-standalone.h`** drives the WD1773 directly: `dskcon_init`,
    `dskcon_processSector`, `dskcon_nmiService`. **No ROM.** `DCOPC` 2 reads
    and 3 writes, so writing is reachable too.

**VERIFIED ON THE MACHINE**: `sectest.c` read track 17 sector 3 with `DCSTA =
0`, and the 64 bytes dumped match the host's disk image exactly and decode as
the `STRINGS DAT` directory entry that `writecocofile` put there. A whole
program doing this is **999 bytes** including the C runtime.

That settles the memory map. The port needs all-RAM mode for its 55,399 bytes
at `$1200..$EA66`, which overlaps where ROM would be -- so it cannot call Disk
BASIC, and standalone DSKCON is the answer. The Atari port reached the same
place by writing its own SIO seam after dropping DOS; **the filesystem layer
on top -- the directory walk and the FAT -- is ours to write here too**, since
the library's own filesystem needs the ROM the port cannot keep.

`tools/mkdisk.py` makes a blank 161,280-byte image and `~/cmoc/bin/writecocofile
-b image.dsk FILE` puts a file on it.

## What is established

**GRAPHIC6 (SCREEN 7), 512x212 in sixteen colours, one byte = two pixels:**

    R#0 = $0A    M5=1 M4=0 M3=1
    R#1 = $40    BL=1, display enabled
    R#9 = $80    LN=1, 212 lines
    R#8 = $08    colour 0 is a real colour, not transparent
    R#7 = $00    backdrop

256 bytes a line, 212 lines, **54,272 bytes a screen**.

**THE VRAM ADDRESS COUNTER CARRIES PAST THE 16K BOUNDARY ON ITS OWN.** All
54,272 bytes were written after a single address set and read back intact, so
the driver never has to touch R#14 mid-blit. Worth knowing: the obvious
defensive code for that would have been pure cost.

**Ports (confirmed earlier by 6809 code, not inferred):** `$FF78` data,
`$FF79` address/status, `$FF7A` palette, `$FF7B` register indirect -- the MSX
`$98/$99/$9A/$9B` layout in the CoCo's slot window.

## THE FONT IS THE OPEN QUESTION

**There is no ROM font on this machine.** `$8000..$FEFF` rendered as a bitmap
is all code -- a font announces itself as regular columnar shapes and there
are none. The GIME's text character generator is internal silicon, not a
table the CPU can read.

Every other port borrows the machine's own glyphs at runtime: the Amiga takes
topaz, the Falcon reads Line-A's 8x16, the Atari 8-bit copies the OS ROM font,
and the Commodore ports use the chargen ROM. **This target has nothing to
borrow**, and 80 columns in 512 pixels needs SIX-pixel cells, which no stock
8-wide font would fit anyway.

So the ASCII set has to be authored here -- about 96 glyphs at 6x8, on top of
the 17 box and badge glyphs every port already draws for itself. That is a
content task, not a coding one.

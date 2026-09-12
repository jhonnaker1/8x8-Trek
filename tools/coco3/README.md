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

Run one with:

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

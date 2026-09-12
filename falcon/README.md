# EGA Trek — Atari Falcon030

Sixth port, **started 2026-09-11** and not yet released. It draws the console
and plays; sound is a stub and nobody has sat down with it.

```sh
make early     # link the whole game against stubbed seams and read the size
make           # build/EGATREK.PRG plus the data files
make run       # boot it in Hatari with build/ mounted as drive C:
```

## What this machine gives the port, and what it takes away

**It boots into the mode the console wants.** A Falcon on a VGA monitor comes
up in `$001A` — `VGA|COL80|BPS4`, 640×480 in sixteen colours, 153,600 bytes.
That was measured before a line of the driver was written, and confirmed by
drawing rather than inferred from the byte count; see `tools/falcon/`.

**Three seams simply are not here.** No overlays, no far-memory banking, and
storage is GEMDOS `Fopen`/`Fread`/`Fwrite`. The budget arithmetic that
dominates the four 6502 ports does not exist: the first link, with every seam
stubbed, came to 102,438 bytes resident on a machine with 1 to 14MB.

**The planes are word-interleaved, and that is the one thing that does not
carry from the Amiga**, whose four bitplanes are four separate regions. Here
the four planes of the same sixteen pixels sit in four consecutive words, so
one cell's eight pixels are one byte in each of four words eight bytes apart —
and which half of each word depends on whether the column is even or odd.

**The cell is 8×16, not the Amiga's 8×8.** 640×480 with 8×8 cells is 80×60,
more than twice the console's rows. At 8×16 it is 80×30, the console takes 25,
and the spare five become a margin split top and bottom.

## The font is the ROM's; the box glyphs are ours

Line-A hands back three system font headers. This port reads them and takes
the one that is 8×16 **by measuring `form_height`**, not by taking index 2 —
EmuTOS 1.3.0 happens to put them in the order 6×6, 8×8, 8×16, and a different
ROM need not. Copying at runtime and never shipping a glyph is the same rule
the Atari 8-bit port follows with the OS ROM font.

The seventeen box-drawing and badge glyphs are this port's own artwork, drawn
at 8×16. **They are restated for the taller cell, not doubled from the
Amiga's 8×8** — doubling gives a four-pixel horizontal rule against a
two-pixel vertical one, and the corners stop meeting.

## Two traps worth keeping

**`scr_put` takes a C128 screen code, not a character.** The first build drew
the Q in "WILL YOU REQUIRE A BRIEFING" as the ship's saucer, because ASCII `Q`
is 81 and screen code 81 is the saucer. `scr_puts` maps ASCII to screen codes
and `glyph_rows` maps back for the ROM lookup; the missing-glyph marker is
loud on purpose, because a blank cell hides exactly this class of bug.

**The setup prompts are line editors and want RETURN.** Two answers typed
without one both land in the same field and read as an input fault. That was
the harness being wrong, not the port, and only screenshotting after every
single keypress separated them.

## Building

vbcc's Atari TOS target, which was already installed before this port started:
`$(VBCC)/config/tos` and `targets/m68k-atari`. Two things about `vc` that cost
a build each:

- **`-I` takes no space.** `vc -I src` drops the path silently.
- **`vc` shells out to `vbccm68k` by bare name**, so `$(VBCC)/bin` has to be on
  `PATH` or the failure reads like a missing toolchain.

No `-Werror`: vbcc's warning set is not gcc's, and its "statement has no
effect" on `core/`'s `(void)` casts is not a defect.

## Running

Hatari, with `build/` mounted as GEMDOS drive C:, so there is no floppy image
to make and nothing to copy — the same arrangement Amiberry gives the Amiga.

**Use EmuTOS, not Atari's TOS 4.04**: the latter double bus-errors on a Falcon
here, and Hatari says so in as many words. EmuTOS is also free software, so a
bootable image would be ours to redistribute — the lever that removed Atari DOS
from the 8-bit port.

## What is open

1. **Sound.** The only stubbed seam, and the only one the scope never
   measured. `snd_enabled()` returns 0 rather than claim a driver that is not
   there.
2. **`make verify` and a place in the root `make ports` gate.** Every other
   port has one; this is currently the only port whose breakage nothing
   catches.
3. **SAVE and restore, unexercised.** The seam is written and GEMDOS makes it
   the easiest on the project. Easiest is not witnessed.
4. **Nobody has played it.** Screenshots are not a person at the keyboard, and
   on this project that distinction has found bugs no instrument could.

# EGA Trek — Atari Falcon030

Sixth port, **started 2026-09-11 and released 2026-09-12 as v0.14.0**. It
draws the console, plays the YM2149, saves and restores, and Jamie has played
it. Every seam is built; nothing here is stubbed.

```sh
make early     # link the whole game against stubbed seams and read the size
make           # build/EGATREK.PRG plus the data files
make verify    # geometry and glyph coverage -- the root `make ports` runs it
make run       # boot it in Hatari with build/ mounted as drive C:
```

## What `make verify` checks, and why those two things

This port has none of the pools the 6502 ports run out of, so the checks they
need have nothing to bite on. It checks the two things that fail **silently**
here instead:

**Geometry.** 80×25 cells of 8×16 inside 640×480. Change the cell height, the
row count or the margin and nothing complains — the bottom rows just leave the
screen, where on a 6502 port the same mistake is a linker error.

**Glyph coverage**, and this is the one worth having. A screen code with no
`box[]` entry and no ASCII mapping draws the missing-glyph marker, which is
only loud if somebody happens to be looking at that panel. The Amiga found its
glyph set by sweeping the shared UI **by hand**, and that sweep turned up
fifteen where an eyeball count gives eleven. A hand sweep goes stale the next
time someone adds a panel; this one runs every build.

**Each check was verified by breaking the thing it protects** — and the glyph
sweep *failed that test on its first version*. It passed happily with `G_CROSS`
deleted from the driver, because the box junctions never appear as call
arguments: they live in `junctions[]` in layout.c and reach the screen through
an array subscript. The sweep now reads glyph constants out of data tables too.
A check that cannot fail reports coverage it does not have.

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

## Sound: the YM2149, and the clock was measured

The PSG is the closest thing to a SID since the C128, and the easiest sound
seam here as a result. The original made one square wave out of a PC speaker;
a PSG tone channel *is* that, with no sample buffer to allocate, no waveform
table and no duty cycle to get wrong. Channel A is music, channel B effects,
so a laser does not cut the music off. The machine's 8-bit stereo DMA audio is
the better instrument for *sampled* sound, which this game does not have.

**The 2 MHz clock was measured, not looked up.** Four tones at periods
spanning what the music actually uses — 90–930 Hz, not round numbers — came
back 2,000,160 / 2,008,460 / 2,005,520 / 2,004,640 Hz, every point inside
0.42%. Four points, because one cannot tell a wrong scale from a wrong
intercept: the X16's beep was an octave flat *and* had a free first tick, and
a single sample would have "confirmed" either story.

**Then the driver was checked against the track it plays.** Thirteen seconds
of the title screen, recorded and matched note-for-note to a contiguous run of
the title track starting at note 24 — **pitch +0.43% against a semitone of
5.95%, tempo −0.77%.** (Two of the thirteen are instrument artifacts: the
first note was truncated by when recording started, and two adjacent
same-frequency notes merged in the grouper.)

**Timing comes from the clock, not the call count.** `_hz_200` at `$4BA`,
read through `Supexec`. Ticking once per `snd_poll()` would be easy and wrong:
the key loop calls it about sixty times a second but other callers run at no
fixed rate, so the tempo would wander with whatever the game was doing. The
C128 and MEGA65 both lost time to a tempo bug.

**Two mixer bits are not the mixer.** Bits 6 and 7 of R7 are the PSG's port
direction, and on this machine port A drives floppy select, side select and
the printer strobe — so bit 7 must stay 1 or the drive stops answering.
Writing `0x00` there to "turn everything off" is the classic way to lose the
floppy.

## SAVE and restore, witnessed

`make storetest` builds `STORTEST.PRG`, which exercises the eleven promises in
`core/storage.h` on the machine and prints PASS or FAIL for each. **It found a
real bug on its first run.**

GEMDOS made this seam so easy — `Fopen`, `Fread`, `Fclose`, no channels, no
device numbers, no sector buffer — that it was written straight through and
looked obviously right. It was not. `Fread(h, max, buf)` reads *up to* `max`
bytes and reports how many, so a file **longer** than the buffer came back
`STOR_OK` with a silent truncation — the one behaviour `storage.h` names as
forbidden: *"silently reading half a save is worse than refusing."* It now
measures the file with `Fseek` to the end and refuses an oversized one. **The
easiest seam on the project was the one that skipped its own contract.**

Then the round trip itself, end to end: a game driven to the console, `SAVE`
to the default `EGATREK.SAV`, then **a fresh boot of a new process** answering
Y at "RESTORE A SAVED GAME". The short-range scan, the status panel and the
galaxy chart come back **pixel-identical**. That is the check worth making,
because the galaxy is generated per game from keyboard entropy — reproducing
the same star field and the same chart values means the 625-byte record
round-tripped, not that two games happened to look alike.

(625 bytes is 24 of header plus the 601-byte save record, which is what the
shared serialiser says it should be.)

## What playing it found, from one false claim

The message panel drew empty boxes and `MSGS` showed three entries of
"STARDATE: 0.0". **One cause, and it was a comment I invented.**
`vdc_set_address`, `vdc_data_write` and `vdc_data_read` were still stubs,
under a note reading *"Nothing outside the C128's own driver calls these."*

**`ui.c` calls all three, every time it files a message.** The scrollback is
32 slots of 64 bytes kept outside `ui.c`'s own variables, because on the C128
it lives in spare VDC video RAM that the 8502 cannot address. Every message
went into a black hole and read back as zeros.

A negative claim about our own port, written beside the code that disproves
it — the oldest failure shape in `NOTES.md`, invented while writing a stub and
never checked. **No instrument here saw it, and none would have**: the console
was otherwise perfect, `make verify` checks geometry and glyph coverage, and
every screenshot looked right. It took a person playing.

It is a plain array now, and `make storetest` round-trips the log store in
three checks — **verified by putting the stub back and watching all three
fail.**

## What is open

1. ~~**Sound.**~~ **DONE** -- see below.
2. ~~**`make verify` and a place in the root `make ports` gate.**~~ **DONE** --
   `make verify` checks console geometry and glyph coverage, and the root gate
   runs it. **This line said "open" for a day after it was built**, and was
   caught by re-deriving rather than reciting when Jamie asked what was left.
3. ~~**SAVE and restore, unexercised.**~~ **WITNESSED** -- see below.
4. ~~**Nobody has played it.**~~ **Played 2026-09-11** — *"it looks and sounds
   and plays good"* — **and it found two bugs in one sitting.**

5. ~~**Not released.**~~ **RELEASED 2026-09-12 in v0.14.0** -- `make release`
   builds `build/egatrek-falcon.zip`, a folder like the Amiga's and the X16's
   because GEMDOS opens the four data files by bare name and wants them beside
   the .PRG. `README-release.txt` ships inside it.

**All five are closed and the list is empty.** The one thing this port has
never had is a second pair of hands: it has been played once, by Jamie, on an
emulated Falcon under Hatari. Nobody has run it on real hardware, and the VGA
mode it sets is the only one it knows -- an RGB monitor or a TV is untested
and expected to be wrong.

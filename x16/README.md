# EGA Trek — Commander X16

Second port, **released in v0.10.0** and shipping in
[v0.13.2](../../../releases/latest). The modern 8-bit machine: a 65C02, a
CBM-compatible KERNAL, and VERA — a video chip with per-cell foreground and
background colour, so the nine-panel console renders as designed with no
mapping.

    cd x16 && make game     # build/trekx16.prg  -- NOT `make`, see below
    cd x16 && make verify   # load address, window, overlay images, soft stack
    cd x16 && make run      # x16emu with the data files
    cd x16 && make release  # build/egatrek-x16.zip -- a folder, not a disk image

**`make` builds the smoke test, not the game.** `all: smoke`, which is the only
port where the default target is not the thing you want. It has caught people
out, including the root README, which listed no X16 build commands at all until
2026-09-10.

## Why this port came second

It was chosen as the cheapest next step after the C128 and it stayed cheap: the
same compiler family, so everything learned about llvm-mos carried over; a code
space the same shape, so the overlay machinery transferred rather than being
redesigned; and every KERNAL call the C128 disk seam uses exists here. Sixteen
colours, so no mapping. `x16emu` gives `-dump` of RAM, banked RAM and VRAM, so
the string pool and the overlays can be checked byte-exact rather than by
looking at the screen.

## What is different from the C128, seam by seam

**Video.** VERA is a register block at `$9F20`: point the address port at a
VRAM address, choose an auto-increment, then read or write the data port. The
address is 17 bits — low 16 in `ADDR_L`/`ADDR_M`, bit 16 in `ADDR_H` bit 0,
with `ADDR_H` bits 4-7 holding the increment code.

**The charset is a MODE, not an upload — and that cost four wrong attempts.**
The X16 boots in ISO-8859-1, where `$C0` is `A-grave`, so the first attempt
drew the console frame as letters. Under charset 2 the font is the C64
screen-code layout, box-drawing set and all, and the port switches to it rather
than uploading glyphs of its own. **Measured from the font bitmaps in VRAM
rather than reasoned**: tile `$41` is `18 3C 24 66 7E 66 66 00`, an
unmistakable `A`, so the read was sound; tile `$C0` under ISO is that same `A`
with an accent stroke. `make charset` is the probe that dumps those raw tiles —
a control glyph first, because if `$41` is not a letter bitmap then the read is
wrong and nothing else means anything.

**And the split is the whole point**: STRINGS are converted, GLYPHS ARE NOT.
The box-drawing set lives at 64..127, exactly the range an ASCII conversion
rewrites — so putting that conversion in `scr_put`, as this file did for three
attempts, turns every panel border into letters. It belongs in `scr_puts`.

**Input is a KERNAL call.** `GETIN` (`$FFE4`) hands over a key and returns zero
when none is waiting. That deletes the C128's hand-transcribed table of fifty
row/column pairs, the `make verify` check that the table is ASCII rather than
PETSCII in the linked binary, and the second check that it can spell every
command word — a table which shipped for weeks holding only the letters some
command happened to need, so the self-destruct password JAMIE could not be
typed. **`GETIN` returns lowercase ASCII, not PETSCII**, which is the one thing
about it that surprises a Commodore programmer.

**Far memory is paged, and that was this port's top risk.** The C128 has a flat
64K bank reached through the KERNAL's `FETCH`/`STASH`, so a read never crosses
anything. The X16 pages **8K at a time** into a window at `$A000`, selected by
the RAM bank register at `$00` — so a read of `len` bytes at `off` can span two
banks, and the C128 implementation has no code for that case because it cannot
happen there. The string pool is **just under one 8K page** — `STRINGS.DAT` is
7,289 bytes today — so the first read to cross `$2000` would have been the
music rather than the prose.
`far_read`/`far_write` loop, clipping each pass at the window edge, and
`make memtest` is the test that crosses the boundary on purpose.

**Overlays are a memcpy, not a disk load.** Shaped like the MEGA65's: all
eleven images live in banked RAM and a swap copies one into a low-RAM window.
They ride in the far store beside the string pool and the music, so they arrive
through the two seams that are already tested rather than a third path. The
window is in LOW RAM because `$A000..$BFFF` is the banked window the images are
read *through*.

**No disk image.** The X16 has no 1541, so `make release` ships a folder of
files for the emulator's or a real machine's filesystem.

## Live figures

    verify: window 3968 bytes, linker script and Makefile agree
    verify: low RAM ends $8F30, __stack $8F80 -- 80 bytes for the soft stack
    verify: largest overlay msgs 3811 + 2 stamp of 3968, 155 spare
    verify: 11 overlay images, each byte-identical to its ELF section -- ok

`make verify` prints them on every build; this file deliberately does not
repeat them anywhere else. **The figures above were 140 and 157 when this
section was written and are 80 and 155 now** — they moved under it, which is
the argument for reading `verify` rather than this file.

### THE SOFT STACK WAS OVERFLOWING ON SAVE -- MEASURED AND FIXED 2026-09-11

    demand on SAVE    86 bytes   measured twice, TREK_STACKPROBE
    was available     80 bytes   -- six bytes went into .noinit under every save
    now available    184 bytes   window 3,968 -> 3,888

`__stack` **is** the overlay window base here, so the stack grows down into the
gap above `__heap_start` — the only headroom this port has — and an overrun
lands in `.noinit` rather than in the window. That is the quiet failure of the
two: no crash and no wrong picture, which is why v0.12.x and v0.13.x shipped it
and a play session never showed it.

**SAVE is the deep path and nothing else is close.** The rest of the game —
setup, warp, self-destruct, the loss memo, the evaluation, the hall of fame —
reaches 59. The serialiser is about 2.5K of code.

**The size is a ratio, not a round number**: 184 against 86 is 2.14×, where the
C128 reserves 256 against its measured 143, which is 1.79×. It costs the
largest overlay 80 of its 155 spare bytes, leaving 75, and `make verify` checks
both halves.

**And the resource is managed now, not merely reported.** `verify_prg.py`
carries the measured demand and a floor of 144 and fails below either. The old
floor was 64 — a number nobody had measured, which the overflowing build
passed. Proved by putting the window back and watching it fail.

The instrument is kept: `TREK_STACKPROBE` in `src/x16input.c`, with its build
recipe in the comment. **It does not fit in the hole it measures** — shrink the
window to make room, which also widens the sentinel region so the answer is not
clipped.

## Two bugs this port is where we found

**Overlay rule 4.** `trek_score()` is resident but its whole body is
`trek_score_sheet()`, which lives in the eval overlay — so calling it is a call
into a window whether or not that is obvious at the call site. It sat one
statement after `load_hof()` had already swapped the window, the call went to
the address the function has in the *eval* layout, the shorter hof image does
not reach that far, and the CPU ran into unwritten bytes. **Measured here on
2026-09-06, PC = `$983F`+1.** The C128 and MEGA65 carried the same call in the
same order at different addresses — this port is only where a game was played
to the end first. It is rule 4 in `core/overlay.h` now and `make verify` fails
a resident caller outside `main.c`.

**The missing `"p"` clobber.** Inline asm containing a `jsr` must declare the
status register, or the compiler emits a compare before the call and branches
on it after. Latent in the released C128 and X16 until 2026-09-08.

## Sound, and the octave it was flat by

VERA's PSG lives in VRAM at `$1F9C0`, four bytes per voice: frequency low,
frequency high, volume with pan in the top two bits, then waveform with pulse
width.

**Every note this port played was an octave flat from v0.10.0 to 2026-09-10.**
The tens-of-Hz to VERA conversion used `Hz * 2^25 / 25e6`; VERA's accumulator
is **seventeen** bits clocked at 25MHz/512, so the law is `2^26`. Music,
effects and the refusal beep were all a full octave below every other port
reading the same `MUSIC.DAT`.

Two more came out of the same run. The refusal beep played 200Hz for six frame
ticks where the original measures **440Hz for 250ms** — and six ticks were five
frames, because VERA's ISR LINE flag is already set when the loop starts. And
`WAVE 0x00` selects VERA's *narrowest* pulse, measured at 0.9% duty, where its
own comment claimed a 50% square; the duty law is `(width+1)/128`, established
by sweeping three widths rather than read from documentation.

**`make run-sndtest` measures the sound itself.** x16emu records a WAV of its
audio output — the only emulator on this project that can — so the beep and the
reference tones are read out of a recording rather than out of the source. The
run opens with tones whose answers are known in advance, because a measurement
with nothing to check itself against is not one:

    reference: driver says 440 Hz, VERA gives 439.9 Hz  (x1.000)
    reference: driver says 300 Hz, VERA gives 299.6 Hz  (x0.999)
    beep:      439.9 Hz   253.0 ms   duty 49.7%

## Driving it

`x16emu` has no key injection and no memory poke, so a scripted run cannot type
at the game from outside. `make autoplay` builds a variant with the keys
**compiled in** (`TREK_AUTOPLAY` in `src/x16input.c`) that plays itself to the
console, which is how this port gets screenshotted without a human. It is never
part of the game build.

`make keyprobe` is the other instrument: run it, type keys, read the hex off
the screen. It is what caught the case bug — `src/x16input.c`'s header had
*reasoned* that GETIN speaks PETSCII, and it does not.

## Heard, and confirmed

**Jamie listened to it on 2026-09-10 and said it sounds right** -- which is the
only check that could settle the two changes above. The octave fix and the
waveform fix both shipped on arithmetic: a reference tone reading 439.9Hz
against 440, and a duty cycle reading 49.7% against 50. Those say the numbers
agree with each other. They cannot say the music sounds like music, and this
port had been a full octave flat for four months without any measurement
noticing, because every measurement was of the thing that was wrong.

## What is open

**Nothing specific to this port, and nothing on THE OPEN LIST touches it any
more.** Colour per message is built (2026-09-10), ships on all five ports, and
was **looked at here on 2026-09-11** -- Jamie: *"the x16 port looks, sounds and
plays great"* -- which closed this port's share of item 23. Only the MAIN
VIEWER's other nine instrument pages stay deferred, everywhere.

The **quit** was fixed the same day: `vdc_shutdown()` is `scr_clear()` here, and
`main()` used to call it before waiting for the farewell keypress, so the
goodbye was erased one line after it was drawn and the player waited on a blank
screen. The teardown moved after the key. The message underneath it was wrong
too -- it was the C128's "BASIC IS ON THE 40-COLUMN SCREEN.", a screen this
machine does not have -- and had never been read by anyone precisely because it
was being erased. See `c128/src/main.c` on the quit order.

The item that used to sit here — "nobody has heard v0.12.1" — closed on
2026-09-10 when Jamie listened: *"sounds right."* Every sound figure this port
had until then came from a recording or a driver probe, and they had all agreed
with each other for the four months it played an octave flat. See THE OPEN LIST
in [`NOTES.md`](../NOTES.md).

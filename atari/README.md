# EGA Trek — Atari 800XL + VBXE

Fifth port. **Started 2026-09-06 with a measurement, not a driver**, because
the scope for this target ends with an instruction: *"Measure it properly
before committing: link the whole game early."*

    make early        link the game against stubbed seams and report the budget

## The answer, and it is the shape of the whole port

```
address space   $4000..$BFFF        32768 bytes
overlay window                       4608 bytes
for resident                        28160 bytes

RESIDENT IS OVER BY 3358 BYTES
```

That is with **every seam stubbed** — no video, no input, no storage, no far
memory, no sound — so the real gap is larger than 3,358. It is also with the
C128's exact overlay split already applied: eleven overlays, the same functions
in the same windows.

**Without overlays at all the game is about 61,000 bytes of 6502 code**, against
32,768 of address space. The C128 fits the same game in 37,823 bytes, so this
target is roughly 5K short before anything platform-specific is added, and
about a third of the game has to be paged where the C128 pages a quarter.

**This is the tightest target in the project** and it was worth knowing on day
one rather than after a video driver.

## Why $4000, and why the space is short

VBXE's MEMAC window maps its VRAM **into the 6502 address space** — uno's
`vbxevid.c` opens it with `MEMAC_CONTROL = 0x29`, "window at $2000, CPU enable,
8K" — so `$2000..$3FFF` belongs to video memory, not to the program. `$C000` up
is the XL's OS ROM. That leaves `$4000..$BFFF`.

Uno found this **by crashing into it**: a build that loaded at `$2000` had
`vbxe_init()` map the window over its own code and execution fell into VRAM,
`PC=$25ED illegal=1`. The emulator's Program Error dialog is a long way from
where the mistake was, which is why `atari.ld` says so at length.

The window size is configurable (`bits 0-1 = 4K << n`), so a 4K window at
`$2000` would give `$3000..$BFFF` = 36,864 — 4K more resident, at the cost of a
4K VRAM window instead of 8K. **That is the first thing to try** if the paging
work proves harder than the arithmetic suggests.

## What is already known, and what is not

**Known, measured before this port existed:**

* The core compiles for this target clean — `trek.c`, `planet.c`, `hof.c`,
  `serial.c`, no shims.
* **It is a TEXT target, not a bitmap one.** VBXE has a real
  character-plus-attribute mode: 80 columns, per-cell foreground *and*
  background from a 1024-colour palette. So this is a `vdc.c` rewrite like the
  MEGA65 and X16, not the font-and-blitter work the Amiga needed. The project
  had this in the wrong cost class for two weeks.
* **25 rows work.** Measured on AltirraSDL with a test program filling every
  row: ROW 00 through ROW 24, 80 columns. The console fits as designed.
* Far memory and the message log have an obvious home in VBXE's 512K of VRAM.

**Two traps the reference already documents**, each found the hard way in uno:

* The FX core exposes CSEL/PSEL/CR/CG/CB directly at `$D644-$D648`. The older
  VBXE manual documents those addresses as an MSEL/MB0-3 commit protocol;
  following the manual scrambles the palette so text renders in a colour
  indistinguishable from its background — which looks like blank glyphs, not
  like a palette bug.
* MEMAC window A is `$D65E/$D65F`. The v1.0-beta manual's `MA_CPU` at `$D64C`
  **does not exist** on the FX core: Altirra's register switch has no case for
  it, so writes are silently dropped and the window never opens.

**Not known yet:** how much of the 3,358 (plus the drivers) a twelfth and
thirteenth overlay can absorb, and whether `front` — which grows with the save
record and cannot be split — becomes the ceiling.

## The stubs measure the game, not themselves

`src/stubs.c` is every seam with nothing behind it, and **every stub writes
through a `volatile`**. The X16's early link measured 1,564 bytes once — the
whole game, apparently, in a tenth of the space — because its stubs returned
constants, LTO proved the results unused, and the optimiser deleted the *game*
rather than the stubs. A stub that can be reasoned about measures nothing.

## The instrument

AltirraSDL with the AltirraBridge: a socket carrying screenshots, memory reads
and writes, CPU state, breakpoints and input injection. The profile wants
XL / NTSC / 1088K / VBXE, and **BASIC disabled**, since the program runs up to
`$BFFF`.

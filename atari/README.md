# EGA Trek — Atari 800XL + VBXE

Fifth port. **Started 2026-09-06 with a measurement, not a driver**, because
the scope for this target ends with an instruction: *"Measure it properly
before committing: link the whole game early."*

    make early        link the game against stubbed seams and report the budget

## Is it viable? Yes — but only if `run_turn` can be split

Measured 2026-09-06, and this is the whole argument:

```
address space   $3000..$BFFF        36864   4K MEMAC window, not 8K
overlay window                       4608
writable data (.data/.bss/.noinit)   2322
leaves for code+rodata              29934

the game's code+rodata, seams stubbed 29149
the driver layer it still needs       4539   measured on the X16
                                    -------
                                      33688   SHORT BY 3754
```

**The 8K→4K window is the first lever and it is already spent.** With VBXE's
default 8K window the program starts at `$4000` and is 3,358 bytes short before
drivers; at 4K it starts at `$3000`, which buys 4,096 bytes and is what makes
the number above merely difficult instead of hopeless.

### Why it is short, and it is not the code

The Atari's resident profile is *the same as the C128's*, function for
function:

```
              Atari      C128
run_turn       6434      6917
main           4682      5018
trek_move_warp 1002       913
report_move    1143       719
```

So this is not a target that generates worse code. **The C128 wins on
structure**: its writable data lives in a separate `lowram` region at
`$1300..$1C00` that does not compete with code at all, while the Atari's
`.data`/`.bss`/`.noinit` come out of the same space. That difference alone is
2,322 of the 3,754.

### What could close it, in order of what they cost

1. **Split `msgs` and `planet`** so the overlay window returns to 4,096 —
   **+512**, and cheap.
2. **Split `run_turn`.** It is 6,434 bytes of LTO-merged turn engine, and parts
   of it are genuinely cold: rare events, the death pod, black holes,
   supernovae, the reports each of those writes. **This is the only remaining
   candidate large enough to matter** — after `main` and `run_turn` the biggest
   resident function is 1,143 bytes, and everything at that size is drawn or
   run every turn, so paging it means a disk load per turn.
3. Writable data into VBXE VRAM beyond the message log — but only for things
   already reached through a seam. `io_buf` is wanted as one contiguous blob
   and cannot move; that is settled and recorded for the C128.

### The recommendation

**Do not start with a video driver.** The decision this port turns on is
whether `run_turn` splits, and that is measurable *today*, on the C128, with no
Atari-specific code written — and it would give every other port back a
kilobyte or two as a side effect. If it splits, this target is a `vdc.c`
rewrite like the MEGA65 and the X16. If it does not, the honest options are a
reduced feature set here or dropping the target.

## The early-link numbers

`make early` reports the current position and re-measures as code moves. With
VBXE's default 8K window it read:

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

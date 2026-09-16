# EGA Trek on the Commodore Plus/4 — scoping

**Status: the toolchain, the rig and the memory map are established. No game
code has been linked yet.**

## llvm-mos targets this machine, and the gap was never a compiler

The survey said the Plus/4 needed *"cc65 rather than llvm-mos"*. That was
loose, and the same looseness sits in the F256K entry. **llvm-mos has no
`plus4` PLATFORM** — no ready-made crt0, linker script or `basic-header.o` —
but the compiler handles the 7501 as the 6502 core it is, and
`mos-platform/commodore/lib/commodore.ld` has **zero C64-specific references**.

What this port had to supply, and it is all of it:

  * **`plus4.ld`** — derived from `c64.ld` and `commodore.ld`. A memory region,
    the imaginary-register boilerplate, and the KERNAL entry points each
    platform normally defines. Only the symbols the link actually asks for are
    defined, so a wrong guess is an undefined symbol rather than a jump into
    nothing. So far that is `__CHROUT = 0xFFD2` — **the Plus/4 keeps the CBM
    jump table where the C64 has it**, which is a large part of why the C64's
    KERNAL model transfers.
  * **`src/basichdr.c`** — twelve bytes of BASIC so `RUN` works, in C because a
    `.s` file assembled to a section of **size zero without a diagnostic** and
    the program silently started at crt0 with no BASIC line in front of it.
    It needs `KEEP()` in the link script: nothing *references* a BASIC header,
    the machine reads it, so `--gc-sections` drops it otherwise.

`xplus4` from VICE is the rig, driven by the same `tools/vice_mon.py` binary
monitor the C64 and C128 ports use. `tools/run_p4.py` runs a PRG and reads a
report, and leaves the emulator up.

## The banking question, measured

**This is what the port turns on.** On a C64 the KERNAL is `$E000..$FFFF`, so a
program at `$0801..$BFFF` calls it without hiding a byte of itself. On a Plus/4
the ROMs are BASIC `$8000..$BFFF` **and** KERNAL `$C000..$FCFF`, and
`$FF3E`/`$FF3F` switch **both at once** — so a program filling the address
space cannot call the KERNAL without hiding half of itself.

`src/bankprobe.c`, under `xplus4`:

    A1        armed
    80 20     $A000 / $E000 with ROM in        BASIC and KERNAL ROM bytes
    5A A5     after $FF3F, written and read    RAM is there and writable
    80 20     after $FF3E                      ROM back -- the switch is real
    5A A5     after $FF3F again                THE WRITES SURVIVED being hidden
    5A        completed

So the shape is the C64's, scaled up: **the program lives in RAM under both
ROMs, and banks ROM in only around a KERNAL call** — which means the banking
code, the soft stack, and anything live across that call must sit below
`$8000`. That is `c64mem.c`'s job description with a bigger hidden region.

**AND THE REPORT'S ADDRESS IS THE LINKER'S CHOICE, NOT MINE.** The first
version put it at `$0500` by analogy with the C64's cassette buffer; on a
Plus/4 that holds `4C 1C 99`, a live system jump vector, and the probe read the
KERNAL's own bytes back. A linker-placed array cannot be wrong about what is
free.

## The video seam works

`src/ted.c` (112 lines) plus `src/ted.h` and `src/egated.h`, and the **real
`layout40.c`** draws the console frame through it under `xplus4`:

     0|##STATUS#############LONG RANGE CHART###|
     1|#                  #                   #|
    10|########################################|
    11|##BADGE################COMMAND##########|
    14|##LASERS##############                 #|

224 non-space cells, two hues live. `tools/screen_p4.py` decodes `$0C00`
without masking bit 7 — masking it is how a screenshot of a working C64 title
screen came back blank (instrument #22).

**It is the same shape as `vic.c` on purpose**, because everything above it is
shared and the C64 proved the shape. Three things differ and they are all in
`ted.h`: the screen is `$0C00`, the colour is `$0800`, and **a colour cell is a
whole BYTE — luminance in bits 6-4, hue in bits 3-0** — not a nybble index.
`__attribute__((noinline))` is on every primitive, carried over from `vic.c`
with its measured reason: inlined there, the same code made the 40-column
build 1,817 bytes bigger and overflowed its region.

**THIS PORT FOLDS NO COLOURS.** The C64 collapses two pairs because its
sixteen fixed entries do not contain everything EGA has; on TED light blue is
blue turned up, so all sixteen survive distinctly. `src/egated.h` was checked
against `tools/check_colours.py`'s `PLUS4` table — which predates this port and
is what `make ports` runs — and **all sixteen agree**.

**And a frame bench proves the video seam and nothing else.** Every defect
Jamie found in the card-less CoCo 3 lived in `ui.c` with live game state, which
no bench here reaches.

## What is next, in order

1. **Decide the map against the ROM constraint.** `$1001..$7FFF` is 28,671
   bytes and always visible; `$8000..$FCFF` is 32,000 more that the ROM hides
   during a KERNAL call. The resident image is about 40K, so it MUST span
   `$8000` — which is fine for code that is not executing then, and fine for
   the far store because **writes pass through**. What must be forced below
   `$8000`: the storage layer, the soft stack, anything live across a call,
   and the overlay window (overlay code can call storage).
2. **Sound: TED's two voices, not SID.** `sid.c` cannot be linked here the way
   the C64 links it.
3. `p4mem.c` (far memory, `$FF3E`/`$FF3F` where `c64mem.c` uses `$01`),
   `p4log.c`, `strings.override.txt`, a `verify`.
4. What comes free, and it is most of the game: `layout40.c`, `ui.c`,
   `main.c`, `strpool.c`, `core/`, and the C64's `input.c`, `storage.c` and
   `overlay.c` — the Plus/4 keeps the CBM KERNAL jump table where the C64 has
   it, which is why that transfer is plausible rather than hopeful.

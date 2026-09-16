# EGA Trek on the Commodore Plus/4 — PARKED, 2026-09-16

**IT DOES NOT BOOT. Everything below the game does: video, sound, far memory,
the string pool, the overlay split and the disk are built and MEASURED. The
program does not start.** Parked deliberately rather than abandoned — this
file is the handover, and its job is to stop the next attempt re-deriving what
is already known.

## What works, and is measured on the machine

| Piece | State |
|---|---|
| Toolchain | `plus4.ld` + `src/basichdr.c`; llvm-mos with **no `plus4` platform**, so the link script, the BASIC header and the KERNAL jump table are this port's own |
| Rig | `xplus4` on the shared `tools/vice_mon.py`; `tools/run_p4.py`, `screen_p4.py`, `setpc_p4.py` |
| Video | `src/ted.c`, 112 lines — the **real `layout40.c`** draws the console frame, 224 cells |
| Colour | `src/egated.h`, checked programmatically against `check_colours.py`'s `PLUS4` table — **all sixteen agree, and this port folds nothing** |
| Sound | `src/tedsnd.c`, **two voices**, measured 438.3 / 998.4 / 198.7 Hz against 440 / 1000 / 200, plus voice 2 |
| Far memory | `src/p4mem.c` — **cheaper than the C64's**: the read path does not bank at all |
| Disk | `make d64` — 11 overlay images, data files, `strings.dat` proven to load: 7,496 bytes, count 334 |
| Budget | resident to `$AC26`, window `$CC00`, **5,890 bytes spare**, `verify_p4` 15 checks |

## Where it stops

`p4bank.c` — the file that banks RAM in — is what stops `main()` being reached.
Bisected with a two-line `hello.c`:

    without p4bank.c   ran = $5A   main() RUNS
    with p4bank.c      ran = $00   main() does NOT run

One cause is certain and fixed: **`.init.NNN` sections are concatenated and
fallen through**, so a C function there returns out of the init chain — they
are `naked` assembly now. That did not make it run, so there is more in that
file.

## What the next attempt should NOT re-derive

  * **`$FC00-$FCFF` is always KERNAL ROM.** Not bankable. The far store's limit
    is `$FBFF`; it was `$FCFF`, and the pool was already 36 bytes into ROM.
  * **The HI ROM is `$C000-$FBFF` AND `$FF40-$FFFF`**, so banking RAM in takes
    the 6502's vectors with it.
  * **`$FCB3` is a cartridge ROM-bank trampoline, not a RAM-mode one** —
    `STA $FDD0` / `JMP $CE00`, exit `LDX $FB` / `STA $FDD0,X`, and no value of
    X means RAM. Disassembled, not read about. The encyclopedia points banked
    programs at it and it cannot serve this one.
  * **`$FF3F` removes BOTH ROMs**, unlike the C64's `$01 = $3E` which leaves the
    KERNAL mapped. So **every KERNAL call needs a banking shim** — `p4bank.c`
    overrides every `cbm_k_*` the game links. This is the cost the survey
    missed entirely; it said "video, a new sound driver, a link script".
  * **The soft stack must not sit in a loaded section.** It was `$7F00` while
    `.text` ran `$1055..$9908`. `verify_p4.py` checks this now.
  * **The zero-page question is OPEN and UNTESTED.** `plus4.ld` carries
    `__basic_zp_start = 0x0002` verbatim from `c64.ld`, so `__rc0..__rc31` sit
    at `$0002..$0021`. That is free on a C64 with BASIC out; nobody has
    established it is free on a Plus/4 being called into. **The probe written
    to test it was broken** — it reported "32 of 32 clobbered" with the KERNAL
    call removed as well, because it never completed. Write a better one.
  * **Masking interrupts costs the keyboard.** `input.c` reads through `GETIN`,
    which the KERNAL's IRQ fills. This port will need its own keyboard seam.

## Honest assessment

The survey costs this port at three seams. Two of them — video and sound — are
done and measured. The third, the link script, was the easy part. **What the
survey did not cost is the banking**, and that is the whole difficulty: a
machine whose ROM switch is all-or-nothing needs a shim around every system
call, its own interrupt handling, and its own keyboard. That is not a link
script; it is a fourth seam bigger than the other three.

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

## The whole game links, and the staging number is in

`make early`'s question, asked the way every 6502 port here asks it: link
everything RESIDENT and read the overflow.

    ram region      52,479 bytes   $1001..$DCFF
    all-resident    68,976 bytes   demand
    overflow        16,497 bytes

**So it needs overlays, like every 6502 port here.** For scale, the C64 fits
40,735 of 46,847 with an eleven-way split, so that split frees roughly 28,000
bytes — which would leave about 40,976 in this machine's 52,479. There is
room; the split is not a squeeze.

Everything above the seams links unchanged: `main.c`, `ui.c`, `layout40.c`,
`strpool.c`, `input.c`, `storage.c`, `overlay.c` and `core/` entire. **The
C64's `input.c` and `storage.c` linked with no edit at all** — the Plus/4 keeps
the CBM KERNAL jump table where the C64 has it, and that is the survey's "the
C64's storage, input, overlay and KERNAL model" turning out to be true.

What the link asked for on the way, all of it real work a platform package
would normally supply: the KERNAL jump table as linker symbols, `plat_exit`,
and `__ovl_start`.

## The KERNAL vectors are proved, and so is the banking window

`src/loadprobe.c`, against a real d64 under `xplus4`:

    far_load       ok, base 0
    far_size       7496          exactly strings.dat's size
    first 4 bytes  4E 01 00 00   the string count, 334

So `SETLFS $FFBA`, `SETNAM $FFBD` and `LOAD $FFD5` are right, the whole banked
window works — ROM in, three calls, ROM out, and a return into code that was
invisible throughout — and `far_read` reads it back. That was the largest
remaining risk in the port and it is retired.

**AND THE PROBE FOUND SOMETHING WORSE ON THE WAY.** It reported nothing at all
the first time, because **the BASIC header's SYS pointed into the middle of
`kernal_load_raw`.** The header sends `RUN` to `$100D`, which is whatever
section follows it — and the moment `.lowtext` was placed there to keep it
below `$8000`, `RUN` jumped into the KERNAL banking assembly. Every build in
this port had that, including the game.

`verify_p4.py` **passed it**, and the reason is the useful part: it checked
that the SYS number equals the load address plus the header's length. That is
arithmetic, and it stayed true while pointing at the wrong code. The entry is
now three bytes of `JMP` emitted by the link script, and the gate checks the
cell holds `$4C` and that the address after it is where `.text` really starts
— a check about the machine rather than about a sum.

## The design question that was left, and how it went

**The soft stack was the problem, and nothing else was.** Resolved by option 3
below: `kernal_load_raw` is assembly and touches no C local between the stores
to `$FF3E` and `$FF3F`, so the soft stack can live anywhere. Above `$8000` the ROM
hides the program during a KERNAL call — which is fine for code, because the
only code executing then is `.lowtext`, and fine for a return address, because
it becomes visible again the moment the ROM goes out. It is NOT fine for
llvm-mos's soft stack, which `.lowtext`'s own C locals spill to while the call
is in progress.

Three ways out, and the third is the cheap one:

1. Split `ram` either side of the stack — **does not work**: code and rodata
   are about 37K on the C128's 40-column build and neither half is that big,
   so the general code genuinely must span `$8000` contiguously.
2. Alias `c_readonly` low and `c_writeable` high — same arithmetic, same
   answer.
3. **Give the banked window no soft stack to use.** Write `kernal_load` so
   nothing between `$FF3E` and `$FF3F` touches a C local — then the soft stack
   can live anywhere and the whole question dissolves.

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

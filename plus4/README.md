# EGA Trek on the Commodore Plus/4 — PARKED, 2026-09-16

## UNPARKED IN PART, 2026-09-16: the startup blocker is FOUND and FIXED

**The cause was never this file's banking.** `p4bank.c` had been blamed as a
whole since the port was parked — *"the same hello.c ran without this file and
did not run with it"* — and a whole file is not a cause.

**Bisected on the machine**, one instruction at a time (`src/stepprobe.c`):

    sei                                  main() reached
    + $FF0A = 0   (TED mask off)         main() reached
    + the CPU's IRQ/NMI vectors          main() reached
    + $FF3F, RAM in                      main() NEVER REACHED

Then, with markers either side of that store, **the hook was seen to finish**:
`$A1` before the bank and `$A2` after it, with RAM in. So the bank switch works
and something between it and `main()` dies. The disassembly names it:

    .init.250 <shift>:  lda #$0e
                        jsr $ffd2      ; BSOUT -- second charset

**llvm-mos's commodore libc puts its own hook in the init chain, after ours,
and it calls the KERNAL.** With both ROMs gone, `jsr $FFD2` lands in
uninitialised RAM. Jamie, watching the emulator, described it better than the
marker bytes did: *"seemed like the tape drive pressed play"* — garbage
executing into `$01`, whose bit 3 is the cassette motor.

**cc65's own `libsrc/plus4/crt0.s` does the identical `lda #14 / jsr $FFD2`,
and does it BEFORE banking.** That is the documented precedent, and it is what
this port now does: the hook moved from `.init.010` to `.init.260`, after
`shift`. A minimal program then reaches `main()` with RAM banked in —
measured `A1 A2 5A`.

### And the reason it had been at `.init.010` was a claim that is false

The file said the hook had to precede `.init.200`'s bss clear, *"because bss
is at `$A1CE`, under the ROM, so zeroing it with the ROM still mapped would
write through to RAM the program cannot then read back consistently."*

`src/vecprobe.c` measured that, in the tightest corner of the map: a write made
with the ROM mapped **lands in the RAM beneath and reads back correctly** once
RAM is banked in — `$BE $EF` at `$FFFE`. So zero-bss with the ROM in is fine
and the constraint that forced the hook to the front of the chain never
existed. (The old justification cited `src/wrprobe.c`, which measured `$A000`
and `$E000` and never touched `$FFFE` — a citation to a measurement of
somewhere else.)

### SECOND FIX: the soft stack was never initialised

`__rc0/__rc1` read **`$FFCD`** against a `__stack` of `$CC00`, and
`__do_init_stack` was **absent from the binary entirely**. The commodore
platform links `zero-bss` on its own and does **not** link the stack
initialiser; this port's link line never asked for it. So the soft stack
pointer held whatever BASIC had left at `$02/$03`, and every C call built its
frame in the middle of the far store and the I/O page.

`-linit-stack` fixes it: `__rc0/__rc1` now reads `$CBCD`, fifty-one bytes below
`$CC00` and in use.

**The Apple IIgs port met the identical thing the same day**, on the `common`
platform, and its Makefile says so. Nothing in a map tells you an init library
is missing, because what is absent is an absence.

### THE THIRD FAULT DOES NOT EXIST -- it was the monitor

This file previously said `$FFFE/$FFFF` read `$114D` where `p4_stray_irq` was
at `$104D`, and called it an unexplained overwrite. **That was a conclusion
about the instrument.**

VICE's binary monitor shows a MIXTURE at `$FFF6..$FFFF` on this machine: bank 0
gives the ROM's own bytes for part of it (`8D 3E FF` at `$FFF6` is the KERNAL's
`STA $FF3E`, and `$FFFC` is the ROM's reset vector) and something else for the
rest. Reading the same sixteen bytes through each bank shows bank 2 as clean
ROM -- `$FFFE = $FCB3`, which `src/vecprobe.c` had already measured.

**Asked with the program's own eyes, the vector is right.** A probe that
reads `$FFFE/$FFFF` itself, immediately after the hook writes them, reports
`$1059` against a `p4_stray_irq` of `$1059`. The CPU sees what the hook wrote.
The tell was in the dump all along and I read past it: `$FBF0`, `$FF80`,
`$FFC0` and `$FFF0` all begin with the identical `FF FF 00 00 00 00`, which is
a 64-byte period, which is the TED register file.

**This was reported to Jamie as a real fault before it was checked.** Four
instruments misled this session; this is the one that reached him.

### WHERE IT ACTUALLY IS NOW

With both fixes in, measured on the live machine:

  * the init chain completes and `main()` runs
  * `__rc0/__rc1` is `$CBCD`, below `__stack` at `$CC00`, and in use
  * `vdc_init()` has cleared the screen -- after a `LOAD`+`RUN` it would still
    carry BASIC's own text, and it is 1000 spaces
  * **`STRINGS.DAT` is in the far store at `$DD00`, byte for byte** -- so the
    banked KERNAL `LOAD` works through `p4bank.c`'s wrappers

**The screen is blank and no panel has been drawn.** The next suspect is the
title overlay -- `ovl_load` goes through the same banked `LOAD` -- and after
that the keyboard, which this port does not have: `p4bank.c` disables
interrupts, `input.c` reads through `GETIN`, and `GETIN` is filled by the
KERNAL's IRQ. That cost is written down in `p4bank.c` and has never been paid.

### THE TITLE OVERLAY: cause found, fix NOT working

**`overlay.c` calls `cbm_k_load(0, __ovl_start)`, and `p4bank.c` does not wrap
`LOAD`.** It wraps eleven KERNAL calls -- SETLFS, SETNAM, OPEN, CLOSE, CHKIN,
CKOUT, CLRCH, CHRIN, CHROUT, READST, GETIN -- and llvm-mos's commodore libc
compiles `cbm_k_load` to a bare `jsr $FFD5`. With both ROMs banked out that is
a jump into uninitialised RAM.

**Exactly the fault `.init.250 <shift>` had** -- a KERNAL call made with the
KERNAL gone -- in a file this port does not own and therefore never read. The
string pool arrives because `p4mem.c` has its own `kernal_load_raw`, banked, in
`.lowtext`; the overlays have nothing.

cc65's `libsrc/plus4/kload.s` is the missing wrapper, in four instructions, in
a segment commented *"Must go into low memory"*.

**And adding it regresses startup — but NOT because of the wrapper.**

Bisected with `-DP4LOAD=0..3` in `p4bank.c`, each variant written to a copy of
the disk and booted:

    P4LOAD=0   no wrapper at all              strings load, main() reached
    P4LOAD=1   k_load defined, NEVER CALLED   ?SYNTAX ERROR
    P4LOAD=2   + the override, no call        no strings
    P4LOAD=3   the full wrapper               ?SYNTAX ERROR

**Merely defining an unused function breaks it.** So the control: `P4LOAD=9`
puts **thirty-four NOPs** in `.lowtext` — nothing callable, nothing referenced,
the same size `k_load` takes. **It breaks in the same way.**

**The fault is the SHIFT, not the LOAD.** Every symbol after `.lowtext` moves
by the same amount and the linker fixes every reference — `__p4_start`,
`main`, `far_load`, `ovl_load` and `__bss_start` all move together, and
`verify_p4` passes fifteen checks on the result. Something in this port
nevertheless depends on the layout it had, and **that** is the blocker. The
missing `LOAD` wrapper is a real and separate fault that cannot be tested until
it is lifted.

Jamie, watching: **"cpu jam at 9cf5"** — which is inside `memcpy` in both
builds, at different offsets into it.

The earlier attempts, kept because the first one is its own lesson:

  * a plain wrapper -- and the image then held exactly **one** `jsr $ffd5`
    where it should hold two, so LTO had folded it into `p4mem.c`'s
    `kernal_load_raw` or dropped it. The count of a distinctive instruction is
    a cheap check and it was on screen a step before the regression was.
  * `used, retain, noinline` -- two `jsr $ffd5` now, mine at `$1172` inside
    `.lowtext`, and **the same `?SYNTAX ERROR`**.

`verify_p4` passes all fifteen checks on that build: entry `JMP $11A8`,
`.lowtext` `$1011..$1187`, resident top `$B93C`, `__stack` `$CC00`, 4,804 bytes
spare, and the init chain in the right order (100 init-stack, 200 zero-bss,
250 shift, 260 bank-in, call_main). The link is sound; the regression is
something else about the wrapper's presence, and it is **not diagnosed**.

The wrapper is left in, with this note, because it is the right fix and the
port ships nowhere -- `plus4` is in neither `RELEASE_PORTS` nor
`tools/check_ports.py`.

### THE LAYOUT DEPENDENCY, FOUND: the soft stack is ROM-shadowed at startup

`.init.260` **never runs** -- a marker written as its first act reads `00`. And
Jamie, watching: **"jam at ce12"**, which is inside the KERNAL's own interrupt
handler at `$CE00`. So the program dies during `.init.100` (init-stack),
`.init.200` (zero-bss) or `.init.250` (`shift`) -- all of which run **before**
the hook, with the ROM still mapped and interrupts still enabled.

**`__stack` is `$CC00`, which is under the ROM.** A write there passes through
to RAM -- `src/vecprobe.c` measured that. **A READ RETURNS ROM.** So every C
frame the early init pushes is written to RAM and read back as KERNAL ROM.

That is the layout dependency, and it is not a hardcoded address: **whether
startup survives depends on how much soft stack the early code happens to use
and which ROM bytes happen to sit there**, and both move when anything shifts.
Thirty-four bytes of NOPs change the answer because they change the addresses,
not because they are code.

### Which means `.init.010` was RIGHT, and `shift` needed the other fix

The two constraints are now both explained and they are not in conflict --
they were mis-resolved:

  * the hook must run **before** `.init.100`, so the soft stack is in real RAM
    from the first C frame. `.init.010` did that.
  * `shift`'s `jsr $FFD2` must happen **with the ROM mapped**. Moving the hook
    after it satisfied this and broke the first.

**The fix is to bank at `.init.010` and neutralise `shift` itself** -- override
the symbol, or do the charset call inside the hook with the ROM briefly back.
cc65 has no such conflict because it banks RAM in first and keeps it in: its
KERNAL wrappers are four instructions that touch only registers, so nothing
reads the soft stack while the ROM is mapped.

**Not attempted.** `shift` lives in `libc.a` and whether a strong definition
here wins the link, or collides, is untested.

### And cc65 was asked, which is how the first fault was found

`cl65 -t plus4` builds and **runs**: a program that writes `$5A` to `$A000` and
`$A5` to `$E000`, reads both back, and still reaches the KERNAL to `printf`
them. So the model works on this machine.

**But cc65 is not a different approach — it is this one, done correctly.** Its
`libsrc/plus4/kbsout.s` is four instructions in a segment commented *"Must go
into low memory"*: `sta ENABLE_ROM`, `jsr $FFD2`, `sta ENABLE_RAM`, `rts`.
`kload.s` is the same around `$FFD5`. That is `p4bank.c`'s banked wrappers and
`plus4.ld`'s `.lowtext`, line for line.

What cc65 actually gave this port was the **reference that located the bug**:
its crt0 does the charset `jsr $FFD2` *before* banking, and ours did it after.
Switching toolchains would not have fixed anything structural — and it would
have cost the 13% llvm-mos is smaller, the overlay sections, and every
`__attribute__((section))` in the port.



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

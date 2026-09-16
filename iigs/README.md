# EGA Trek on the Apple IIgs — scoping

**Status: the machine is measured back in, and nothing else exists yet.**
There is no game here. There are four probes, a link script, a MAME rig and
one result, and the result is the one the port turns on.

## What was in doubt, and what the answer is

The IIgs was ruled out on 2026-09-05 after a real measurement: 640 mode gives
**four** freely-placeable colours, because a pixel's palette group depends on
its position within the byte, and the console's nine panels sit side by side
so per-scanline palettes cannot help. That is still true and is not what
changed. What changed is that **320 mode is 40 columns**, and 40 columns
stopped being disqualifying when the Atari ST shipped in v0.17.0.

So the question became: does 320 mode give the console what it needs, and can
llvm-mos reach it? Both halves are now measured.

| Question | Answer | How |
|---|---|---|
| Does `mosw65816` produce code a real IIgs runs? | **Yes** | `make colours` — compiled C draws on ROM 03 in MAME 0.289 |
| Can compiled C reach SHR in bank `$E1`? | **Yes** | `sta [dp],y` from inline asm; 256 of 256 bytes in the target page, 0 of 256 in the page above |
| Does the assembler encode 65816 long addressing? | **Yes, all of it** | `8F` `9F` `B7` `97` `C2` `E2` `EB` `8B` `AB` `54` all assemble and disassemble |
| How many colours does 320 mode place freely? | **Sixteen** | `tools/count_colours.py`, the same instrument that counted four in 640 mode |
| Is the pixel format what the datasheet says? | **Yes** | 160 bytes a line, two pixels a byte, palette `$0RGB`; every one of the sixteen colours came back as the exact value programmed |

The console needs fifteen. **Sixteen is enough, and it is measured rather
than read off a data sheet.**

## The two things the survey got wrong in this port's favour

`README.md` lists the cost as *"a hand-built linker config, an Ensoniq 5503
sound driver, ProDOS storage, and cross-bank writes … llvm-mos's 65816 is a
fast 6502 in 16-bit address space, **no `rep`/`sep`, no long addressing**"*.

The second half is false and was never tested. The **code generator** emits
8-bit 6502-shaped code, which is true and is fine — but the **assembler**
encodes the whole 65816 instruction set, and that is what a hand-written video
seam needs. It also emits `lda $00214e` (absolute long) for globals on its own
initiative, next to `stz $214e` (absolute, DB-relative) for the same variable
in the same loop body — which is why `phk`/`plb` is not optional.

## What runs here

    make colours    build the band probe, run it, count the colours (expects 16)
    make pagemap    write each page of SHR with its own number and read it back
    make all        build every probe

`src/gsprobe.c` sixteen bands over a sixteen-colour palette — the measurement.
`src/gsmap.c`   every SHR page written with its own page number.
`src/gsfill1.c` the long-store primitive alone, one page, one value.
`src/gsdiv.c`   `p / 7` for 112 values, because it was a suspect and is not.

## Four traps, all met on day one, all costing a run each

**1. `sec; xce` — establish the CPU mode, never inherit it.** The rig sets the
PC mid-boot, so the machine arrives in whatever mode the ROM was in, and MAME
reported `E=0`: native. llvm-mos emits 8-bit code unconditionally, and in
native mode the M and X flags decide whether `lda` is one byte wide or two.
Forcing emulation mode made no difference to the bug being chased — the mode
was **ruled out, not fixed** — but a program whose register widths depend on
what the ROM was doing when it was entered is not a program.

**2. `phk; plb` — set the data bank, and do it in `.init.NNN` with `naked` and
no `RTS`.** `.init` sections are concatenated and fallen through; an ordinary
C function there ends in `RTS` and returns out of the middle of the chain, so
`main` is never reached. That cost the Plus/4 port days and is applied here on
the first build.

**3. `__attribute__((used, retain))` on every symbol inline asm names.** The
Plus/4 note said such symbols must not be `static`. **That is not enough.** A
symbol only assembly refers to is invisible to LTO — nothing in the IR names
it — so it is internalised and dropped, and the failure is `undefined symbol:
shrp` at LINK time out of a file that compiled clean.

**4. An asm input operand the asm OVERWRITES must be declared.** The first
`bankpoke` took `"a"(lo), "x"(hi)`, and the compiler was keeping the loop
counter in A across the call. The asm destroyed A; the compiler carried on
with `inc`/`cmp #200` on whatever the asm left there; the loop ran forever
with the counter stuck at 1. **It looked exactly like code that never
started.** The reliable fix is to take nothing in registers — operands come
from fixed globals, which is also what the C128 port concluded.

## And one trap in the RIG, which cost more than all four

**A dump of SHR taken while SHR is ON is not what the program wrote.** Reading
`$E1/2000..` back through MAME's program space returns every written page
**twice** once `$C029` bit 7 is set: the same program, one line different,
reads back as the identity (0 mismatches of 125 pages) with the display off
and doubled (124 of 125) with it on.

For an afternoon that made a correct program look broken. `gsprobe`'s own log
said it wrote 112 pages, one each, `$20`..`$8F`, with the right value every
time — and the memory said nine values fourteen pages wide. **Two instruments
disagreed and the program was not the liar.** What settled it was building the
same probe twice with one line different, which is the only form of this
question that has an answer.

Related, and the reason the bands finally appeared: on this rig **`$C029` must
be set BEFORE the buffer is filled.** Drawing into a dark screen and revealing
it at the end is the right instinct on every other machine here, and it is
what the first three forms of the probe did. The working 2026-09-05
measurement turned SHR on first; so does this one.

## The boot chain, and it is ours end to end

**A disk we wrote boots this machine and runs our code, with no ProDOS on it.**

A ProDOS-order block device boots by reading **block 0** into `$0800` and
jumping to `$0801` with X holding slot×16. So block 0 is 512 bytes of whatever
we put there, and `src/boot.S` puts a loader there: it reads the payload off
the disk through the **slot's own ProDOS block driver** — the byte at `$CnFF`
is the driver's offset within `$Cn00`, and the call takes command in `$42`,
unit in `$43`, buffer in `$44/$45` and block in `$46/$47` — and jumps to
`$2000`.

That is the same argument that got the Atari VBXE port its own boot record and
SIO seam: **the disk is ours to give away.** No system file of Apple's is
redistributed, there is no MLI, and `$BF00` is not a global page.

    make disk        build/egatrek.po -- 800K, block 0 ours, payload from block 1
    make bootcheck   boot it and count the colours (the whole chain)
    make failcheck   boot a disk whose first read cannot succeed

`make verify` runs all three.

### The failed-read path is executed, not merely written

`a failed load must not RETURN` is the C128 overlay seam's rule and it applies
to a first-stage loader more than anywhere: there is nothing to return *to*.
So a failed read paints the screen flat magenta and stops — measured as
`#aa00aa` across all 128,000 content pixels, a colour the console never draws.

**And the first attempt to test it was not a test.** Asking for block 1598 of
a 1,600-block disk *read fine*; the loader jumped into a page of zeros, the
machine ended in ROM at `$FE/00F2`, and the screen stayed dark — which looked
exactly like the failure path failing. The control was what failed: on a
1,600-block image the only block the drive cannot deliver is one past the end.
`START_BLOCK=1600` makes the driver return carry set, and then the path runs.

## What has NOT been established

* **A filesystem.** The loader reads a flat run of blocks. The game needs
  named files — `STRINGS.DAT`, the music, eleven overlays — so either this
  disk grows a directory of its own (the CoCo 3 and Atari ports both wrote
  one) or the layout is fixed extents the build computes. Not decided.
* **Zero page — and the question CHANGED when ProDOS went away.** `iigs.ld`
  carries `__basic_zp_start = 0x0002`, inherited from `c64.ld` through
  `plus4.ld`, where that inheritance went unexamined and became the Plus/4's
  open question. With no MLI there is no ProDOS to collide with, but **the
  slot firmware's block driver has its own zero-page use** (`$42`..`$47` at
  minimum, and the screen holes) and the loader calls it. Measure what the
  driver touches before the game's data lands anywhere near it.
* **Keyboard, sound, storage, and the console itself.** None attempted.
* **Whether the image fits.** `$2000..$BEFF` is 40,191 bytes against the C64's
  `$0801..$CFFF`. The survey's claim that the machine needs *no overlays and
  no far memory* rests on it starting at 256K — which is true of the machine
  and says nothing about what a ProDOS 8 program can address without work.

## The rig

Ample's MAME 0.289 (`~/ample/Ample.app`) with its own romset, where
`apple2gs -verifyroms` answers **`romset apple2gs is good`** — the reason it
is the instrument of record rather than the 0.281 collection, whose
`apple2gs.zip` needs a stand-in `megaii.chr`.

`tools/run.lua` loads a flat bank-0 image, reads it back before jumping,
sets `PB`/`PC`, samples the CPU state, and checks a completion byte **before**
printing any report the probe wrote — because an output array read out of a
run that never finished is uninitialised memory presented as a finding.

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
    make early       link the real game against a stubbed seam and size it
    make sheet       draw the console proof sheet on the machine and measure it
    make sheetfail   prove the sheet gate catches the wrong box glyphs

`make verify` runs all six.

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

## Does the game fit? Measured, with the seam stubbed

The Plus/4's `make early`, asked here on day one. Link the real shared
sources — `main.c`, `layout40.c`, `ui.c`, `strpool.c` and `core/` entire —
against `src/gsstub.c`, which implements the **twenty-six symbols** that are
the whole platform seam, and read the map.

    all-resident, no overlays     overflows $2000..$BEFF by 16,098 bytes
                                  (the Plus/4 overflowed by 16,497 on the same
                                  sources -- the cross-check that the number is
                                  the GAME's, not this port's)

    with the overlay split        resident  $0800..$89A5
                                  HEADROOM  13,915 bytes
                                  overlays  11, largest 3,800 of 4,096

    + the real video driver       resident  $0800..$A102
                                  HEADROOM  7,934        video   cost 5,981
    + the real storage seam       resident  $0800..$AC0C
                                  HEADROOM  5,108        storage cost 2,826
    + far memory in bank $01      resident  $0800..$A99C
                                  HEADROOM  5,732        far mem GAVE BACK 624

**`make early` runs it at every stage; `tools/verify_gs.py` checks the
invariants a link can break silently.**

**And the headroom figure is the LOWER of two bounds, which the tool used to
get wrong.** It reported "headroom to the window" — true while the window was
the top of RAM, and false the moment the window moved into the language card,
where it is a different mapping entirely. The figure jumped by 4,096 bytes the
program could not use. It now reports the distance to `__stack` or the window,
whichever is lower, and says which.

**And the video number is the Falcon's lesson repeating almost exactly.**
`gsvid.c` compiles to 1,962 bytes on its own and costs **5,981** when it is
linked — a factor of 3.05, against the Falcon's 4,636 for a 1,559-byte driver,
a factor of 2.97. The callers grow: a stub is unfoldable only while nothing
downstream is real.

    + keyboard and overlays       HEADROOM  5,236        cost 496
    + the Ensoniq                 HEADROOM  4,398        cost 838
    + the generated music data    HEADROOM  3,622        cost 776

**3,622 bytes spare with every seam real**, against the C64's 6,112 and the
40-column C128's 304.
The C64 shipped with 6,112 spare *after* its drivers were in. What this port
has that the C64 did not is somewhere to put things: the **2,048-byte message
log** is in bank 0 today and need not be, the **language card** at `$D000` is
16K nothing has claimed, and **bank `$01`** is another 64K. None of that is
free, and none of it has been attempted.

### $0800, not $2000, and that was ProDOS's answer to a question this port
### no longer asks

The image loaded at `$2000` and stopped at `$BEFF` because that is where
ProDOS 8 puts a `SYS` file and where its global page starts. With our own boot
block neither applies, so the image goes as low as an Apple II program can and
the port keeps the **6,144 bytes the convention was costing**. It does not
close a 16,098-byte gap — that needs the overlay split every other 8-bit
target here takes — but it is free.

The overlay window is at `$B000`, the top of RAM, and that address is forced
rather than chosen: the C64 puts its window at `$C000` because that is the 4K
its `$01` never covers, and on a IIgs `$C000..$CFFF` is I/O in every
configuration.

### The loader relocates itself, and the first version broke its own rule

Block 0 is loaded at `$0800` by the firmware and the game image also starts at
`$0800`, so the loader copies its own page down to `$0300` and continues
there. One page, not all 512 bytes: `$0400..$07FF` is text page 1 and the slot
firmware keeps its **screen holes** in it, which the block driver we are about
to call reads and writes.

`src/boot.S` carried a comment saying the unit number in X *"is the only thing
that says which drive we came from"*. The relocation loop was added after that
comment was written and **destroyed X in the same edit**. The machine ended in
ROM at `$FF/BD53` with unit 0. `stx unit` now happens before the copy.

## The console draws

`src/gsvid.c` is `c128/src/vdc.h`'s 40×25 grid in Super Hi-Res. 320×200 is
40×25 cells of 8×8 **exactly** — no margin, no centring, none of the
arithmetic that cost the CoCo 3 card port a `MARGIN_Y`.

**The palette holds EGA's own sixteen, so `EGA_TO_VDC` is the identity and
brown is brown.** The C128 maps EGA onto a fixed RGBI chip with a 4-bit rotate
and loses exactly one colour to olive; this port writes EGA's values into a
programmable palette, as the MEGA65 does. The levels make it free — EGA's
two-bits-per-gun values are `$00/$55/$AA/$FF`, every one a duplicated nibble,
so the IIgs's four-bit guns take the high nibble and the colour is exact.
Measured off the sheet: `#aa5500`.

**The font is the project's own.** `coco3/src/font6x8.h` is generated by
`coco3/tools/gen_font.py` from artwork authored in this repository — which
matters here beyond licence, because a IIgs's Mega II character generator is
**not CPU-readable** and the Amiga/ST trick of lifting ASCII glyphs out of ROM
at runtime is not available. Six significant columns go into an eight-pixel
cell shifted left by one, so every glyph gets a blank column either side.

**But the box glyphs cannot be shifted, they have to connect.** A six-pixel
rule in an eight-pixel cell draws a dashed line, and at this scale that reads
as a slightly lighter line rather than as a defect — so codes 64..127 are
authored at eight wide by a stated mechanical widening of the six-wide set.
`make sheetfail` builds the naive version and proves the gate catches it:
**40 black pixels in a 160-pixel rule**, a two-pixel gap at each of twenty
cell boundaries.

`make sheet` draws every text code, the box set, a drawn panel, sixteen
colours and reverse video on a real machine, and `tools/check_sheet.py`
measures four things no eye can judge on a screenshot.

## Sound: the Ensoniq, calibrated at three points

Two oscillators — music on one, effects on the other, which is what
`c128/src/sid.h` asks for and what the original's single PC speaker could not
do. The DOC has thirty-two; two is the shape of the shared driver, not a
limit of the machine.

**The frequency was measured before a note of music was written**, because
this project shipped a port an octave flat for four months off a single-point
check and shipped the CoCo 3's first build an octave *sharp* for the same
reason:

    register    MEASURED      Hz per unit
     $0400      1757.1 Hz      1.71592
     $0800      3514.8 Hz      1.71621
     $1000      7026.9 Hz      1.71555

Ratios 2.0003 and 1.9992 against a wanted 2.0, so the **model** is right; the
three constants agree to 0.04%, so the **number** is right. One point could
not have told those apart.

Theory, *afterwards*: with two oscillators enabled the sample rate is
894886/(2+2) = 223,721 Hz, and a 256-entry table at resolution 0 gives
`reg × 1.70686` — 0.53% below what was measured. The measured column ships.
**Arithmetic that agrees with itself is not evidence.** And the constant is
tied to `$E1`: the sample rate depends on how many oscillators are enabled, so
it is only this constant while that says two.

Then four tones through the **shipping driver's own `voice_note`** — a test
that reimplements the frequency arithmetic proves the test:

    wanted    MEASURED     error
       440     438.9 Hz    -0.25%
      1000    1000.0 Hz    +0.00%
       200     200.7 Hz    +0.33%
       440     440.1 Hz    +0.02%   <- on the SECOND voice

`make sound` runs it; `make soundfail` proves the gate rejects an octave.

### The driver was right and the game was silent

`make music` boots the real game, records it, and slices the burst. It exists
because the first build played **nothing**: `kb_waitkey()` did not call
`snd_poll()`. `sid.h` says it must, in as many words — *"that is where this
port spends every second it is not drawing"* — and the thing that never called
it was thirty-nine bytes away from the driver that was already measured.

**And the fix looked like a failure too.** `hearit.py` reports a tune with no
rests in it as ONE burst at its middle pitch: *"441.0 Hz for 14.6 seconds"* —
which is exactly what a stuck note reads as. Slicing the burst shows 21
distinct pitches. `make musicfail` feeds the gate a synthesised steady tone and
proves it says no.

**No zero bytes in the wavetable.** The DOC halts an oscillator when it reads
a sample of zero — that is how one-shot sounds end — so a waveform that swings
through zero stops itself. `$40` and `$C0`.

## The game runs

**EGA Trek's title screen is on an Apple IIgs, the setup dialogue answers, the
title tune plays, and the whole chain works: boot block, block reads, the
image, string pool in bank `$01`, overlays read into the language card,
keyboard, console, Ensoniq.** Every seam is real.

    make game     build build/trek.po
    make play     boot it and drive it -- GS_KEYS="Return,n,Return" etc.

Two seams were all that remained after far memory, and between them they cost
**542 bytes**:

* `src/gskey.c` — **39 bytes.** `$C000` holds the last key with bit 7 set;
  `$C010` clears the strobe. No ROM, no interrupts, no firmware, which matters
  on a port that has taken the machine and switched the language card in. A
  IIgs returns real ASCII including lowercase, so it folds to upper here
  rather than at thirty call sites — the X16 met the same difference and it
  cost a release.
* `src/gsovl.c` — eleven 4K images read from the disk into the window at
  `$D000`. Shaped like the C128's rather than the Atari's: the Atari holds
  every image in VRAM because its twelfth overlay is on the hot path, and this
  port has no such overlay and no spare bank big enough (bank `$01` has ~16K
  left against 45,056 for eleven padded images).

### Two things this port did not have to learn again

**`ovl_fatal` is the shared one.** This file defined its own for about a
minute, and the duplicate-symbol error was the cheap half of what that was
worth. The expensive half is in the shared version's comment: it writes the
overlay number through `scr_puts` because `scr_put` takes a SCREEN code and a
digit is ASCII. The private copy used `scr_put` — right on machines where
digits map to themselves, and **this port's font is indexed by C128 screen
code, where they do not.** A private copy of a shared routine loses every
lesson the shared one has learned.

**Every slot carries a stamp** — the low sixteen bits of `ovl_load`'s address
in the link it was cut from, at offset 4094. An overlay is linked *with* the
resident half, so yesterday's `OVERLAYS.BIN` beside today's program jumps into
the middle of some other function. On the MEGA65 that reset the machine to
BASIC and could mimic any bug you cared to name; on the C128 a missing
eleventh image opened the game on an uninitialised galaxy, having first shown
the SAVE GAME dialog, because that is what lived at that address in the image
actually in the window.

### And the first boot said C128 across the top

`src/strings.override.txt`, four ids, same shape as `c64/`, `st/` and
`mega65/`. The generator owns the numbering, so this port builds the same
`strdata.h` and the same `STR_COUNT` as every other — a mismatch there makes
the game run with **every label blank** rather than complain.

## Will more RAM help? No — but the RAM already under the ROM will

Asked because the console driver left only 3,838 bytes for five seams.
`make ram` measures it, at both ends of what MAME will emulate.

**The constraint is not kilobytes, it is bank 0.** llvm-mos emits 16-bit code:
every `jsr`, every pointer and the whole soft stack are sixteen bits, and it
**never emits `jsl` or `rtl`**. Code executes only in the bank it was linked
for, however many banks the machine has. Extra banks are **storage**, never
program space.

**The language card is the exception, and it is the answer.** `$D000..$FFFF`
in bank 0 is RAM once the soft switches say so, and code there runs with
ordinary sixteen-bit calls. Measured:

    $D000 bank 2 / $E000 / $FFF0   written and read back      $A1 $A2 $A3
    $D000 bank 1                   written and read back      $B1
    back to bank 2, $D000          still $A1                  -- the two
                                                                 $D000 banks
                                                                 are SEPARATE
    ROM switched back, $E000       $4C                        -- a JMP: ROM

**16K, of which 12,288 bytes is contiguous executable space** (`$D000..$FFFF`
using one `$D000` bank), with the other 4K as bankable storage. That is three
times what is left, on a machine feature every IIgs has. Identical at
`-ramsize 1M` and `-ramsize 8M`, which is the right behaviour for something
that does not depend on expansion.

Banks `$01`, `$E0` and `$E1` are real, distinct and writable — also identical
at both sizes. That is where the message log, the string pool and an overlay
cache can go, and it is present on every machine.

### And the bank-count probe is abandoned, on purpose

Walking all 224 banks was tried three times. The first version reported **222
banks — 14 MB — and reported the same 222 at `-ramsize 1M` and at
`-ramsize 8M`.** An answer that does not move when the thing being measured
moves by a factor of eight is not an answer: a store to missing memory and the
load after it can both come off the CPU's own data latch, so a phantom bank
returns exactly what was written to it.

The second version put a different value on the bus between the write and the
read, which fixed the latch and made 1M differ from 2M — and still could not
tell 2M from 8M, and reported banks answering **scattered rather than in a
run**, which is not how memory is fitted.

**The third attempt was the one not to make.** Three instruments in a row for
a question whose answer changes nothing. And the rig cannot see the machine
that matters anyway: **MAME's smallest `apple2gs` is 1M**, so a stock 256K
IIgs is not testable here — which makes "do not rely on any bank above `$01`"
a constraint this project has no way to check its way out of.

## Files, with no ProDOS on the disk

`src/gsblk.c` is `core/storage.h`'s five `plat_*` over the slot firmware's
block driver, plus this port's own directory. The format is
`atari/src/atarisio.c`'s at 512 bytes, and for the Atari's two reasons
exactly: **PRODOS on the disk is Apple's code**, so a bootable image would not
be ours to give away, and **the MLI uses zero page during a call**, which
collides with llvm-mos's imaginary registers and is not something this project
could measure its way out of.

    block 0     the loader          block 2..  the game image
    block 1     directory, 32x16    then       file data, each file CONTIGUOUS

Contiguous, so there is no link field in every block and no free map. The
save's name is typed by the player and cannot be known when the disk is built,
so `mkdisk.py` lays down **slots** — entries with an extent assigned and no
name — and `dir_claim()` takes one on the first write to an unknown name. That
is the whole allocator: it cannot fragment, because nothing is ever freed and
every slot is the same size. The slot bit does a second job — **a write to
`STRINGS.DAT` is refused by the FORMAT** rather than by nobody having tried it.

`make files` runs fifteen checks on the machine, and **half of them are
refusals**: a file that is not there is reported missing, a write to a name
with no slot bit is refused, and the save is **read back** — a save that
cannot be re-read is the failure mode that matters and only the second half of
that pair can see it.

### The handoff, and the arm-check that proves it earns its place

The loader knows two things the game cannot re-derive — which drive the
machine booted from, and where that slot's block driver lives — and zero page
does not survive the C runtime's arrival. Four bytes at `$0280` carry them,
with a signature.

**The signature is not decoration.** `tools/run.lua` pokes an image straight
into memory and sets the PC, which is how every probe in this port was tested,
and in that path `boot.S` never runs — so those bytes are whatever the ROM
left and `jmp (gs_drv)` would go anywhere. `make filesfail` runs the identical
binary that way: the signature is detected absent, **every file call is
refused cleanly and nothing hangs**, and the gate reports thirteen failures.

## The briefing, and the gate that did not exist

**Jamie played the first build for three minutes and found what fifteen gates
had missed: the briefing did nothing.** The prompt answered Y and the game
carried on to the setup screen.

The code was right. `ui_briefing()` opens `BRIEF.TXT` and its own comment says
what happens when it is not there — *"NO FILE, NO BRIEFING. A disk without
BRIEF.TXT skips it silently and the game starts, exactly as a disk without
STRINGS.DAT plays without words."* **The disk was wrong: `mkdisk.py` was never
told to put the file on it.**

So the failure had no symptom any gate could see, and not one of the fifteen
ever pressed Y at that prompt. `make briefing` does now, and `make brieffail`
builds the same disk without `BRIEF.TXT` to prove the gate catches exactly
this. That is the recorded lesson arriving on schedule: *a bench reaches the
screens somebody listed*.

### And the bleed test had a false positive, which is worse than no test

`check_briefing.py` also asserts that no row draws on scanline 7 — the blank
one between rows — because a six-pixel font in an eight-pixel cell makes tight
text LOOK like overlapping lines on a small screenshot, and **I misread it as
a defect three times in one session.**

The first version failed the setup screen for the **cursor**: a solid cell
lights all eight scanlines and is not bleed. It would have failed every
console screen in the game, since the console is built out of solid cells.
What counts is a column lit on scanline 7 that is *not* lit on all of 0..6 —
part of a glyph, in the gap.

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

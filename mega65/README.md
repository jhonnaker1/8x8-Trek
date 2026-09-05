# EGA Trek — MEGA65 (native C65 mode)

The second port, and the first of the four **text-mode siblings** the target
order puts ahead of the bitmap machines. Status: **first light** — the console
frame draws from the shared layout, in 80 columns, on an exact EGA palette.

```sh
make          # build/smoke.prg
make run      # in Xemu
```

## Why native mode, not C64 mode

A `$0801` binary runs the MEGA65 in **C64 mode**, where 80 columns do not
exist — the C64 never had them, and setting VIC-IV's H640 bit from inside
changes nothing. Colour RAM makes it concrete: 80×25 is 2000 cells and a
C64-mode program reaches only the 1K window at `$D800`.

llvm-mos's `mega65` target links at **`$2001` with a BASIC 65 header**, which
is what actually selects C65/native mode. There H640 is real and mega65-libc
reaches the full colour RAM at `$FF80000` through the 45GS02's 32-bit
addressing, so every one of the 2000 cells gets its own colour.

## The EGA palette, exactly — including brown

The first thing this machine buys over the C128. The VDC is a fixed RGBI chip:
the C128 port maps EGA onto it with a 4-bit rotate, fifteen colours land
exactly, and **EGA's brown comes out olive** — recorded in `core/ega.h` as the
one colour that port cannot have.

The VIC-IV's palette is programmable, so `m65vid.c` writes EGA's own values
into entries 0..15 and the mapping becomes the identity.

Measured off a screenshot rather than assumed: all sixteen land on their EGA
index, brown at `(170, 85, 0)` **exact**. Eight read one LSB low (84 for 85,
254 for 255), which is Xemu's rendering — a wrong palette byte would be off by
a whole nibble step, not by one.

A happy accident makes the palette bytes revision-proof: EGA's two-bits-per-gun
levels are `0x00`, `0x55`, `0xAA`, `0xFF`, every one a duplicated nibble. They
are the same colour whether the core reads the register as eight bits or as the
low nibble of a VIC-III-compatible four.

## What is shared, and what this port has to supply

`core/` and the console layout compile unchanged — the smoke build already
links `../c128/src/layout.c` and uses `layout.h`'s glyph constants as they
stand, because the MEGA65 uses the same C64-family screen codes the C128's VDC
does.

`ui.c` and `main.c` — 4,500 lines of console and command handling — touch the
platform through a seam that is already abstracted at core level:

| seam | calls | MEGA65 |
|---|---|---|
| `scr_*` | 224 | **done** — `m65vid.c` |
| `ovl_load` | 25 | a **no-op**: 40MHz and far more RAM, so nothing needs overlaying |
| `kb_*` | 17 | to write |
| `snd_*` | 15 | the MEGA65 has real SIDs, so `sid.c` should port nearly as-is |
| `vdc_data_*` | 9 | the one genuinely C128-shaped thing: the message log lives in spare **VDC RAM**. Here it becomes plain RAM |
| `plat_*` | 5 | SD-card file I/O via mega65-libc |
| `far_*` | — | plain RAM; the bank-1 seam exists because the C128 has 42K, and this machine does not |

## Overlays are still needed here, and the first version of this file was wrong

That paragraph used to say the three big C128 subsystems — ten 4K code
overlays, bank-1 far memory, byte-at-a-time disk streaming — were "none of them
needed here". **Measured, and it is not true of the first one.**

llvm-mos's mega65 region is `$2001..$CFFF`: **45,055 bytes**. The whole game is
about **66,500 bytes** of code. Unmapping the KERNAL as well would buy 8K and
still leave it short. So the window stays, and `core/overlay.h`'s `OVL_CODE`
had to stop testing `__mos__` — the MEGA65 is built by llvm-mos too, and that
test was asking the wrong question. It is `TREK_OVERLAYS` now, and each
platform's Makefile says whether it wants windows.

**What does change is the cost, and it changes completely.** The C128 reads
each 4K image off a 1541 — hundreds of milliseconds, which is why that port
works so hard to call `ovl_load` rarely and why `check_overlay_calls` exists.
Here the ten images sit in banked RAM at `$50000` and come in by **DMAgic**:
one `lcopy`, microseconds. Same mechanism, and the cost that shaped the C128
port's structure is gone.

The other two really did evaporate. The string pool and music live at `$40000`
and the message log at `$44000`, reached with `lcopy`/`lpeek`/`lpoke` instead
of an MMU dance, and `far_read` is a DMA burst.

    resident   33,566 bytes of 40,959    ($2001..$BFFF)
    window      4,096 bytes              ($C000..$CFFF)
    images     40,960 bytes              banked at $50000, from OVERLAYS.BIN

## Running it headlessly -- SOLVED, and the answer was in Xemu's source

    make                                  # build/egatrek.prg + build/OVERLAYS.BIN
    tools/putfiles.sh build/OVERLAYS.BIN ../c128/build/strings.dat ...
    tools/run.sh build/egatrek.prg shot.png 20

**Use Xemu's OWN default card**, `-sdimg @mega65.img`, which lives in its prefs
directory. Xemu fdisk/formats that card itself the first time it creates one,
and a card it made needs no ONBOARDing -- so nothing waits for a human and
`-headless -screenshot` works.

Three hand-built images failed before that: a bare FAT32; an MBR plus FAT32 the
Hypervisor would not `CHDIR /` into; and a blank card offered to the machine's
own FDISK+FORMAT utility, which cannot work because that utility is a file ON
the card. `targets/mega65/sdcontent.c` in Xemu's source is the authority and
says why: the card needs **two** partitions -- type `0x0C` FAT32 at LBA 2048
and a type `0x41` MEGA65 system partition -- plus a fixed disk signature
`837dcba6`. Guessing at that three times cost more than reading it once.

Two smaller traps, both of which look exactly like a hang:

  * **The PRG must load at `$2001`.** Xemu detects that as BASIC and AUTO-RUNs
    it; anything else just gets a `SYS` line typed and waits for RETURN. A
    build that links straight to a `.prg` emits an ELF, which loads at `$457F`
    and is never started. `mega65.ld` deliberately has no `OUTPUT_FORMAT`, so
    every target must go through objcopy.
  * **mtools** will not touch the card until the FAT32 BPB has non-zero CHS
    geometry, which Xemu's formatter leaves at zero. Those fields are legacy
    and unused by FAT32-LBA; `putfiles.sh` patches them once.

## What actually runs, as of now

`smoke2.c` is a staged harness -- each stage draws a marker, so one screenshot
says how far it got. All of these pass:

    STAGE 1  video up, EGA palette
    STAGE 2  STRINGS.DAT loaded through the Hypervisor
    STAGE 3  drawing still works after Hypervisor file calls
    U.S.S. LEXINGTON        -- a real pooled string, from the real file
    STAGE 4  window contains code after ovl_load: A6 16 DA A2 0C 86 04 A2
    STAGE 5  MUSIC.DAT loaded
    STAGE 6  music started
    STAGE 7  2000 frames of snd_poll(), which DMAs into a stack local each one

And the real binary reaches `MAIN REACHED`, `LOOP TOP`, `MUSIC OK`, `OVL OK`.

**AND EVERY ONE OF THOSE STAGES PASSED WHILE THE GAME WAS BROKEN**, which is
the lesson worth keeping. The harness links the same `m65storage.c` and
`m65mem.c` the game does, and they work here -- because this binary is small
enough that the register allocator never parks a live value in the pseudo-
register the library clobbers. The miscompile below needs the whole program to
appear. A staged harness proves the code it links, not the program.

**RESOLVED 2026-09-03, and it was never `ui_title()`.** This paragraph used to
say the title screen "is not reaching its own first statement", reasoned from
the markers before it surviving. They survive because the program had no title
overlay to run. See "The bug that was actually there", below.

## What is verified, and what is not

**Verified:** the toolchain, the video layer and the palette — the smoke build
draws the console frame and a screenshot measures all sixteen colours onto
their EGA index. The full game **links** at the sizes above with `-Wall
-Werror`. The shared `ui.c`, `main.c`, `strpool.c` and `layout.c` compile for
this target unchanged, which is the real portability result.

**Verified 2026-09-03: the game runs.** Startup, the three file loads, the
DMAgic overlay path, input and the console all work. The release build reaches
its title screen and blocks in `kb_waitkey()` with the screen drawn; driven
headlessly from there through setup into the nine-panel console; and a `MSGS`
command pages an overlay in over a live console, so runtime overlay swapping
works too. `make drive` reproduces all of it.

The string pool is byte-exact against the file, and so are MUSIC.DAT, all ten
overlay images in banked RAM, and the window after an `ovl_load` -- checked
with `-dumpmem` rather than by looking at the screen.

Jamie played the release build on 2026-09-03 and found three faults; sound and
the briefing are fixed below, and one crash remains unexplained.

## Still open (2026-09-05)

Three faults, none of them reproduced by a build check, so they are written
here rather than left in commit messages where nobody re-reads them.

  * **`ship` is corrupted after the hall of fame.** The console comes back with
    `$6464` in energy, impulse, shields and enemies_left, and 100 in torps and
    laser_eff -- the shape of `sys[]` written at the wrong offset. The ship is
    intact *during* play (energy 5000 read from behind the self-destruct
    dialog), so it happens somewhere in the end-of-game sequence.

    **It is MEGA65-only**, established 2026-09-05 rather than assumed:
    `core/trek.c` and `core/hof.c` pass their native guard tests, and the C128
    driven through the identical sequence returns a coherent ship
    (`energy=5000, impulse=500, shields=2500, torps=9, level=3, enemies=37`).
    So it is in this port's platform layer, not in shared code.

  * **A crash and a freeze around the hall of fame**, both reported from play
    on 2026-09-03. One dump showed `$454854` -- ASCII "THE" -- sitting in a DMA
    descriptor. Neither has been reproduced since, and the overlay-id fix
    (`fe9d4f1`) plausibly addresses the crash without that being demonstrated.

  * **Saving does not work.** `plat_write_all()` returns `STOR_ERROR`, so SAVE
    and the hall of fame report a failure rather than writing. This is a
    *bounded* job, not a mystery -- see "Writing files IS possible" below --
    but it needs a low-memory trampoline and has not been built. Scope it
    around **byte-at-a-time CBDOS I/O**, which is proven working here: the
    KERNAL's banked LOAD exists in the ROM but delivers nothing under Xemu, so
    it cannot be tested. See "The C65 KERNAL HAS a banked LOAD" below.

Everything above the first two items is fixed and verified; the sound, briefing
and end-of-game overlay fixes since 2026-09-03 have been checked headlessly and
**not yet played by a human**.

## What `make verify` checks

Added 2026-09-05. This port had no verify step at all for six days after the
C128's overlay call-graph check caught a guaranteed crash -- same architecture,
same one window, same `.ovl_*` section names, nothing looking at them.

    load address      the PRG must start at $2001 or Xemu will not auto-run it
    resident space    the image against mega65.ld's OWN `ram` region
    overlay layout    one run address, distinct load addresses, each fits
    overlay calls     rules 2 and 3 of core/overlay.h -- shared with the C128
    OVERLAYS.BIN      ten 4K slots, each byte-identical to its ELF section
    build stamp       the last two bytes are this link's ovl_load

The call-graph and layout checks live in `tools/overlay_check.py` and are
shared with the C128, because the hazard is the architecture's and not the
machine's. What is deliberately NOT here: the message, panel, dialog and
briefing width checks, which read shared sources against shared constants and
cannot come out differently for this target (`make -C c128 verify` runs them);
and the key table, because there isn't one -- `$D610` hands this port ASCII.

**Two things it found on the day it was written.** The Makefile printed
"resident N bytes of 40959" while `mega65.ld`'s region was 40447, so every
free-space figure this port had ever reported was 512 bytes too generous. And
the image builder pads with `b'\0'*(4096-len(d))`, which for an over-length
overlay multiplies by a negative, writes an over-length slot, and silently
shifts every slot after it -- no exception, no warning, and the wrong bytes
DMA'd into the window from then on.

Each check was verified **by breaking it**: a corrupted load address, a shrunk
`ram` region, one flipped byte inside an image, a wrong stamp, a resized
OVERLAYS.BIN, a shrunk window, and a planted `ovl_load` inside `.ovl_msgs` --
seven deliberate faults, seven non-zero exits, and the call-graph one named
`.ovl_msgs:do_hail` exactly.

## The bug that was actually there

`plat_read(stage, 64)` was reading **whole files in a single call** and
returning their length modulo 256. Measured on the machine with Xemu's uart
monitor, breaking on the instruction that stores `far_len`: the entire startup
produced two hits, `far_len := 116` for a 7,284-byte STRINGS.DAT and `+156` for
a 412-byte MUSIC.DAT.

The generated loop kept its remaining count in `$81/$82` and tested it there,
but emitted its decrement against `__rc24`. Nothing decremented the tested
value, so the loop ran until EOF, and `done` came out eight bits wide.

**The cause is that mega65-libc's `fileio.s` is hand-written assembly that
clobbers llvm-mos's zero-page pseudo-registers without declaring it** -- `stx
__rc4 / sty __rc5` around the hyppo trap, and the trap returns through
registers the compiler was never told about. Stamping zero page after `open()`
and reading it back across one `read512()` shows `$06`, `$07` and `$16`
changing. Believing the call clobbers nothing, the compiler kept a live value
in one of them.

`src/m65hyppo.s` wraps `open`, `read512` and `close` and saves all 32
pseudo-registers across them. That is about 130 cycles against a hypervisor
trap and a 512-byte DMA, and it costs roughly a kilobyte of resident space,
because the compiler now knows it must preserve registers around the call. The
loop then compiled correctly.

**Why it took so long.** Every read SUCCEEDED -- every sector shows in the HDOS
log -- so the loop looked healthy while the destination pointer stood still.
`far_load` wrote one chunk to the pool base, `ovl_init` never wrote to banked
RAM at all, and `ovl_load` then DMA'd four kilobytes of uninitialised memory
into the window. `ui_title()` was called into that. It was always fine.

**The instruments that settled it**, neither of which was being used: `-dumpmem`
writes 384K of linear RAM on SIGTERM, and `llvm-nm` on the ELF turns that dump
into the value of every static after the fact -- no on-screen instrumentation
and no perturbing the build. `-uartmon` opens a unix socket with breakpoints,
registers and memory. Three earlier hypotheses -- a wrong window address, a
zero-page clash, banked RAM that was not RAM -- were each disproved in minutes
once a probe was written, and each had taken an hour to argue about first.

## Sound works, and the day it cost is the lesson

**`snd_poll()` was never called.** Not from the game, not from anywhere -- only
`smoke2.c` ever called it, which is how a sound driver was written, linked and
called "verified" without a note ever being played. The C128 port ticks it from
inside the key scan; this port's `kb_waitkey()` just spun. Three faults, all
fixed:

  * **No call site at all.** `snd_poll()` now runs in the key wait loop, which
    is where the C128 has it.
  * **Tempo used the NTSC constant on a PAL machine.** `snd_tick_num()`
    converts frames to the original's 18.2Hz ticks and the numerators differ by
    19% (363 against 304). The old comment said region detection was
    unnecessary because the MEGA65 clocks its SIDs at a fixed rate -- true of
    PITCH, and `snd_tick_num` is not pitch. `detect_region()` reads the raster
    and gets PAL under Xemu.
  * ~~The VIC registers read as a constant without `mega65_io_enable()`.~~
    **RETRACTED 2026-09-03 -- that was the stale card as well.** It was
    "measured" during the same window, on runs where the game never started, so
    every reading was just a startup value. Re-measured against a matching
    card: `$D011`/`$D012` read correctly with no `mega65_io_enable()` in
    `raster_line()` at all, region comes out PAL, and the call is gone. It had
    been writing `$D02F` twice and the CPU port at `$00` once per key poll --
    thousands of times a second, for nothing.

### The tempo was double, and the frame counter is why

Jamie said the title music sounded too rapid. The data settles it without an
ear: the title track is 106 notes over 942 ticks, which at the original's
18.2065Hz is **2.05 notes a second**, and it was running at 3.8.

**The MEGA65's native display is 625 physical lines, so the VIC-II compatible
raster at `$D011`/`$D012` runs 0..311 TWICE per frame.** Measured: 99.8 wraps a
second against PAL's 50. Every other machine this driver runs on wraps once,
which is why counting wraps as frames is right on the C128 and wrong here.

Halving it would have worked on this machine and been a guess about every
other. Instead **CIA1's time-of-day tenths** -- the only honest clock here,
measured at 10 a second while no ROM interrupt runs at all -- are used to count
how many wraps a second actually holds, and the accumulator step follows.

**And the calibration has to be CONTINUOUS.** A one-shot in `snd_init()`
counted 49 wraps in its second while the same code in the running game counted
99.6, both out of one run: the rate genuinely changes once the video mode
settles, and calibrating early gets the wrong half. `snd_poll()` re-derives it
every second, with a short first window so the opening bars are only briefly
wrong. Sanity check on the arithmetic: 50 wraps a second yields 364 and
sidfreq.h's hand-computed PAL constant is 363.

Measured after: `tick_num` settles at 182 (100 wraps a second) and the track
plays 174 original ticks in ten seconds -- 17.4 against 18.2065, or 96% speed,
which is inside the error of timing two separate emulator runs.

`raster_line()` is also **bounded** now. It spun on `for (;;)` until two reads
of `$D011` agreed about bit 7, which is a hang with no escape in a routine the
key loop calls thousands of times a second -- and a hang there freezes the
screen with the current SID note still gated on, which is what Jamie saw at the
hall of fame. **That freeze has NOT been reproduced**, headlessly or otherwise:
driven to that exact screen the CPU cycles normally and the music keeps
advancing. So the bound is a hypothesis about his freeze and a plain defect fix
regardless.

### What it cost, and why

Adding the call made the machine wedge within ten seconds, every time, into a
DMA with a corrupted descriptor. That was chased for hours through five
hypotheses -- a runaway soft stack, a zero-page clash, an IRQ handler, reading
`$D012` at high frequency, the DMA job living in zero page -- and a working
sound fix was backed out and written up as unfixable.

**Every one of those runs had a STALE OVERLAYS.BIN on the card.** `make sd`
had been skipped and a `build/disk` snapshot copied instead, so the images were
from an older link than the PRG. The game loaded them, jumped into the middle
of some other function, and the machine reset. Two separate wrong conclusions
were reported to Jamie before the card was checked.

Three things made it worse, and all three are fixed:

  * **A "no crash" result was read as success without looking at the picture.**
    The run that shipped as "stable" was a BASIC prompt for twenty seconds. The
    check was `dialogs=0` and an uptime, which a machine that never started
    passes perfectly.
  * **Memory dumps were read at hardcoded addresses** that had moved between
    builds, so "the music driver never ticks" was three unrelated variables.
    `tools/` reads symbols out of the ELF now, and so does `drive.py`, whose
    `kb_inject` address had gone stale the same way.
  * **Nothing tied the two files together.** Now the Makefile writes
    `ovl_load`'s address into the last two bytes of OVERLAYS.BIN and
    `ovl_init()` checks it, so a mismatch is a message on screen naming the
    problem instead of a reset to BASIC. Verified by breaking it on purpose.

`trek_dma()` in `m65mem.c` -- our own DMA descriptor in `.bss` rather than the
zero-page one llvm-mos gives mega65-libc -- was written during the hunt and is
kept. It was never the cure, and Jamie's hall-of-fame crash (a DMA source of
`$454854`, whose low bytes are the ASCII "THE") has NOT been reproduced since
and is not explained. **Treat it as open.**

## The briefing did nothing, and why

Answering Y got the setup screen instead of the briefing. `findfile` for
BRIEF.TXT was failing: hyppo has four file descriptors and **`close(fd)` never
freed one**, so the fourth open in a session failed and the briefing is the
fourth file this game opens.

Two separate faults, one on each side:

  * `trek_close` took its argument in A, and `save_rc` destroys A. Hyppo was
    asked to close descriptor `$B2` -- the soft stack pointer's low byte.
  * With that fixed, hyppo still refused: **its `openfile` does not return a
    usable descriptor here.** Every open returns `$18`, the trap number itself,
    and `closefile` rejects it with error `$89`. `plat_close()` now calls
    `closeall()`, which works. A probe opening five files in a row gets three
    and then fails; with `closeall` it gets all five.

That is sound only because this port never has two files open at once, which
is stated at the call site so the next person does not widen it.

## Writing files IS possible: the ROM has to be mapped back in

`plat_write_all()` returns STOR_ERROR because mega65-libc's fileio is
open/read512/close with no write anywhere. That is still true -- but it is not
the only route, and the other one works.

**The internal drive is device 8, driven by the C65 DOS in ROM, and ordinary
CBM KERNAL calls reach it.** Proven from BASIC first: with a D81 mounted,
`OPEN 2,8,2,"TEST,S,W"` writes a real SEQ file. Proven then from our own
machine code, which is the part that matters -- the same `cbm_k_setlfs` /
`cbm_k_setnam` / `cbm_k_open` / `cbm_k_ckout` / `cbm_k_bsout` sequence that
`c128/src/storage.c` already uses, writing a file to the D81 and closing it.

**WHY IT HANGS WITHOUT THAT, and it took three probes to find.** llvm-mos links
`unmap-basic.o` into every MEGA65 program:

    sei
    ldx #$2f / stx $00        ; CPU port DDR
    ldx #$3e / stx $01        ; bank the ROM out
    ldx #$44 / stx $d030      ; VIC-III ROM mapping

That pages the C65 BASIC/DOS ROM out so the program gets the RAM. `OPEN` on
device 8 then calls into DOS code that is no longer there and never returns --
which is why BASIC succeeds and we hang, and why it looks like a device
problem when it is a banking one. Interrupts are NOT the cause; a probe with
`cli` hangs identically.

Mapping it back is three stores, the same ones llvm-mos's own `.fini` uses on
exit:

    $01 = $3F, $D030 = $64      /* ROM in  */
    $01 = $3E, $D030 = $44      /* ROM out */

**THE CONSTRAINT, and it is the whole design problem.** While the ROM is
mapped, $8000..$BFFF is not our RAM. This port's resident image runs to about
$B236, so the code performing the I/O and its buffers must live BELOW that
window -- the probe worked because it sits at $2001 and is a few hundred bytes.
The save record is 601 bytes and the hall of fame 300, so a low-memory
trampoline plus buffer is feasible, but it is real work and not a one-line
change.

It also changes how the port ships: a D81 has to be mounted alongside the SD
card's data files.

## The C65 KERNAL HAS a banked LOAD, and Xemu cannot run it (2026-09-05)

Asked because a D81 would let this port save, and if switching reads to the
same path were also fast, the Hypervisor could go entirely. The C128 gets an
8x speedup from `SETBNK` ($FF68) loading straight into bank 1 -- see
`c128/src/farmem.c` -- so the question was whether the C65 KERNAL has the
equivalent.

**It does.** The ROM carries TWO KERNAL jump tables, and checking only the
first one gives the wrong answer:

  * `$2FF00` (lower half) is the plain C64 table. No `$FF68`; `$FFD5` LOAD is
    the C64 routine, storing through `sta ($AE),y` -- 16-bit, current map only.
  * `$3FF00` (upper half, the C65-mode KERNAL) carries the **C128-style
    extended table**, and `$FF68` -- SETBNK's exact slot on the C128 -- is a
    live `JMP $03A8`.

And that half's LOAD is built for banking. Its inner loop:

    $CD52   18        clc
            8a        txa
            65 ad     adc $AD
            85 ad     sta $AD
            90 06     bcc +6
            e6 ae     inc $AE
            d0 02     bne +2
    $CD5E   e3 af     inw $AF        ; carry into the BANK bytes
    ...
    $F57B   ea 92 ad  sta [$AD],Z    ; 45GS02 32-BIT FLAT store

A 32-bit destination pointer at `$AD..$B0` that carries across bank
boundaries. That is a banked load by construction, not by inference.

**AND IT DELIVERS NOTHING UNDER XEMU.** A probe (`sei`-free, ROM mapped in,
SETNAM/SETLFS/SETBNK/LOAD, markers between each step) run against a D81 built
with `c1541`:

    SETNAM, SETLFS and the $FF68 call all return cleanly
    LOAD returns SUCCESS: carry clear, end address $0200 for a 512-byte
      file loaded at $0000 -- and $4200 for the same file at $4000
    the payload appears NOWHERE except $11406 and $11504, 254 bytes apart,
      which are the DOS's own sector buffers

**Identical with SETBNK and without it**, so this is not about banking: plain
LOAD does not deliver either.

What is underneath it: Xemu raises an **unhandled memory access** at that
moment, and `-headless` auto-answers EXIT. That is why the first three runs
looked like a hang at `$CD52` and quit after two seconds -- `uptime=00:02`
against a 40-second wait, which is the tell. `-skipunhandledmem` lets the
probe finish, and the transfer then silently does nothing. The DMA job LOAD
leaves at `$1400` shows the destination advancing correctly (`$41FA` for a
load begun at `$4000`) with a destination **megabyte of `$02`** -- which does
not exist on a 384K machine. That is the access Xemu cannot handle.

**WHAT THIS MEANS FOR THE D81 PLAN, and it is not the capability question.**
The capability is real and cannot be validated -- not by us, and not by
anyone with only Xemu. Every check this port has runs through that emulator.
A load path that reports success while moving nothing is the exact shape of
the `read512` bug that cost three days, and shipping one we cannot test would
be worse than not having it.

So: **do not bet a D81 port on a fast banked LOAD.** Either scope it around
byte-at-a-time CBDOS I/O -- which is PROVEN working here, and is how the write
test above passes -- and accept the cost on 48,647 bytes of startup reads, or
treat "make Xemu run LOAD" as a prerequisite that comes first.

Reproducing any of it: `-dumpmem` plus `llvm-nm` on the probe's ELF reads the
markers back; `-skipunhandledmem` is the flag that turns a two-second phantom
hang into a real run; and Xemu's own default D81 lives at
`~/Library/Application Support/xemu-lgb/mega65/hdos/mega65.d81`, which
`c1541 -attach ... -write` will fill (the `-8` route behaves identically, so
it is not the variable it looks like).

## The llvm-mos ABI, measured -- and what it clears (2026-09-05)

Written down because two bugs here were this, one lead died on it, and it was
being reasoned about rather than checked.

**`__rc20`..`__rc31` are CALLEE-SAVED. `__rc2`..`__rc19` are not.** Measured:
compile a function holding values live across an opaque call and the prologue
pushes `__rc20`-`__rc23` and spills `__rc24`-`__rc31` to the soft stack, and
nothing below. **Pointer arguments pass in `__rc2/__rc3`**; byte arguments in
A, then X, then `__rc2`, `__rc3` -- which is exactly why `trek_close(fd)` broke
(a byte in A, destroyed by `save_rc`) while `trek_open(name)` never did.

`__rc0` is at `$02`, so `__rc4`=`$06`, `__rc5`=`$07`, `__rc20`=`$16`.

**Now scan every hand-written .s in mega65-libc for writes to the callee-saved
range.** There are none:

    dirent.s        __rc2, __rc3
    fileio.s        __rc4, __rc5
    memory_asm.s    __rc4 .. __rc8

All caller-saved, all legal. **So the library's assembly was never the
problem.** The original probe saw `$06`, `$07` and `$16` change across one
`read512()`; `$06`/`$07` are the library's own legal scratch, and **`$16` is
`__rc20`, which nothing in the library writes.** It came from the Hypervisor
trap itself. The wrapper in `src/m65hyppo.s` is still exactly right -- the
reason stated at the top of that file just names the wrong culprit.

**What this CLEARS.** `lpeek`/`lpoke` (`memory_asm.s`) look like the identical
bug -- hand-written asm assigning pseudo-registers -- and `m65mem.c` calls
`lpoke` for every message-log write, including during the end-of-game sequence
where `ship` is corrupted. They touch only `__rc4`..`__rc8` and they do not
trap. **They are safe, and the message log is not a suspect for the `$6464`
corruption.** Any future hyppo entry point needs the shim; anything in
`memory_asm.s` does not.

## It found a bug in the C128 port

Compiling the shared code for a second target immediately flagged
`-Wreturn-type` on `do_use()`, which had gained a `uint8_t` return the day
before and still fell off the end of its energium path. The caller tests
`do_use() == USE_WANT_BOLT`, so a garbage return could have fired the plasma
bolt dialog after mining a crystal. The C128 build had been printing that
warning too; the grep used to check builds filtered warnings out. Both ports
now build with `-Werror`.

## Verifying it

Xemu flushes `-screenshot`, `-dumpscreen` and `-dumpmem` on **SIGTERM**, which
is what makes a program with no exit path checkable — `-prgexit` never fires,
because llvm-mos binaries do not reliably return to BASIC. (`-dumpscreen` has
produced nothing for this port; the other two are what get used.)

**`-dumpmem` is the instrument that matters.** It writes 384K of linear RAM,
laid out flat, so `d[0x2001:]` is the loaded PRG, `$20000` is the ROM, `$40000`
is the string pool and `$50000` the overlay images. Run `llvm-nm` on the ELF and
every static in the program becomes readable after the fact — `far_len`,
`ovl_live`, the DMA descriptor — with no on-screen instrumentation and nothing
perturbed. Adding printouts to chase this bug changed the register allocation
and made it move.

**`-uartmon <socket>`** opens a MEGA65 serial monitor on a unix socket: `r` for
registers, `m<28-bit addr>` to read, `s<addr> <bytes>` to write, `b<addr>` to
break, `t0` to resume. A breakpoint on one store settled in seconds what a day
of reasoning had not.

**`make drive`** runs the instrumented build headlessly and screenshots the
result:

    make debug
    python3 tools/drive.py out.png RETURN N RETURN N RETURN J A M I E RETURN 3 RETURN X RETURN

Xemu cannot inject a keystroke — `$D610` is the ASCII key register and writing
it POPS the queue rather than filling it — so the debug build carries
`kb_inject` (as the C128 port does) and the driver pokes it through the monitor.
It **handshakes on the byte**, waiting for the game to zero it before sending
the next: `kb_inject` holds one key, and a fixed delay silently lost six of them
across the disk load between the briefing question and the setup screen, which
put every later answer on the wrong question.


## A debugging session spent on a phantom

Worth recording because the lesson is not about the MEGA65.

The game reached its **title screen** once, drawn from the shared `ui.c` with
the overlay DMA path working. The only fault was silence. The music base fix
went in, and the next three runs came back "purple screen" -- so the fix looked
like a regression, and two rounds of analysis went into finding what the sound
driver could possibly have broken. A linker-script collision between the
overlay window and the soft stack was found and fixed on the strength of it.

**The purple screen was the ONBOARDING SCREEN**, dimmed behind one of Xemu's
own modal dialogs. It was never the game. The runs had never got as far as our
code.

Two things went wrong, and only one of them was the emulator's fault:

  * **A screenshot would have settled it in seconds** and I had no way to take
    one, so I reasoned from a three-word description instead. "Screenshot
    before theorising" is written down in this project already.
  * **I asked for a colour, not for a picture.** "Still purple" is consistent
    with a dozen states; the actual screen was unambiguous the moment it was
    seen.

The window/stack fix stands on its own -- `__stack` at $D000 really did grow
down through a window at $C000, and `far_read` really does DMA into a stack
local -- but it was found by accident while chasing something that was not
happening.
# EGA Trek — MEGA65 (native C65 mode)

The second port, and the first of the four **text-mode siblings** the target
order puts ahead of the bitmap machines. Status: **feature complete** — the
whole game in 80 columns on an exact EGA palette, with sound, a streamed
briefing, saving and restoring, and a game playable from the title screen
through the hall of fame to a second game. It reads and writes **one D81** on
device 8 and does not touch the SD card. **Played by hand and released in
v0.12.0** on 2026-09-08.

```sh
make          # build/egatrek.prg + build/OVERLAYS.BIN
make verify   # load address, resident space, overlay layout and call rules
make d81      # build/egatrek.d81 -- the whole game, one image
make run      # in Xemu, with the D81 on drive 8
make drive    # headless: inject keys, screenshot, --peek symbols
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

`core/` and the console layout compile unchanged — this port links
`../c128/src/layout.c` and uses `layout.h`'s glyph constants as they stand,
because the MEGA65 uses the same C64-family screen codes the C128's VDC does.

`ui.c` and `main.c` — 4,500 lines of console and command handling — touch the
platform through a seam that is already abstracted at core level:

**This table used to be a plan, and three of its rows outlived the work.** It
said `ovl_load` was "a no-op: nothing needs overlaying" fifteen lines above the
section explaining why that is false, `kb_*` stayed "to write" after it was
written, and `far_*` claimed this machine needs no far seam while the port was
already keeping the string pool, the music and the message log in banked RAM.
What it records now is what is built.

| seam | calls | MEGA65 |
|---|---|---|
| `scr_*` | 224 | `m65vid.c` — VIC-IV, H640, full 2000-cell colour RAM |
| `ovl_load` | 25 | **needed after all** — one window at `$C000`, eleven images DMA'd in from banked RAM |
| `kb_*` | 17 | `m65input.c` — plus `kb_inject` in the debug build, which is how this port is driven |
| `snd_*` | 15 | `m65snd.c` — real SIDs, so the C128's driver ported nearly as-is |
| `vdc_data_*` | 9 | the one genuinely C128-shaped thing: the message log lived in spare **VDC RAM**. Here it is banked RAM at `$44000` |
| `plat_*` | 5 | the C65 DOS on device 8 — reads AND writes, one D81 |
| `far_*` | — | banked RAM via `lcopy`/`lpeek`/`lpoke`, not the C128's MMU dance |

**Row four went stale the day it stopped being true, twice.** `plat_*` said
"Hypervisor file I/O — reads only" for as long as that was the port's one open
item, and the header above still said "it cannot write files" in the same
commit that made it write them. This file's own record of stale front doors is
three paragraphs long; it takes a deliberate pass to stay honest.

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
Here the images sit in banked RAM at `$50000` and come in by **DMAgic**:
one `lcopy`, microseconds. Same mechanism, and the cost that shaped the C128
port's structure is gone.

The other two really did evaporate. The string pool and music live at `$40000`
and the message log at `$44000`, reached with `lcopy`/`lpeek`/`lpoke` instead
of an MMU dance, and `far_read` is a DMA burst.

**`make verify` prints the live figures and this file no longer quotes them.**
They had gone stale twice: once as a set, and again the day after being
re-quoted with a date attached, because a date makes a number look maintained
without making it so. The shape is one 4K window at `$C000`, eleven images
banked at `$50000`, and the resident half filling `$2001` upwards to meet it.

## Running it headlessly -- SOLVED, and the answer was in Xemu's source

    make d81                              # the program and its files, one image
    make drive DRIVE_KEYS="RETURN N ..."  # keys in, screenshot out

**`-sdimg @mega65.img` is still passed, but only because the MACHINE boots off
the card** -- the game itself has read and written a D81 on drive 8 since
2026-09-08. That flag names Xemu's own image in its prefs directory, which Xemu
partitions and formats itself, so it needs no ONBOARDing and nothing waits for
a human. That is what makes `-headless -screenshot` work at all.

Three hand-built cards failed before that was understood: a bare FAT32; an MBR
plus FAT32 the Hypervisor would not `CHDIR /` into; and a blank card offered to
the machine's own FDISK+FORMAT utility, which cannot work because that utility
is a file ON the card. `targets/mega65/sdcontent.c` in Xemu's source is the
authority: **two** partitions -- type `0x0C` FAT32 at LBA 2048 and a type
`0x41` MEGA65 system partition -- plus disk signature `837dcba6`. Guessing at
that three times cost more than reading it once. **None of it matters to the
game any more**, and it is kept because it is what makes the emulator boot.

Two smaller traps, both of which look exactly like a hang:

  * **The PRG must load at `$2001`.** Xemu detects that as BASIC and AUTO-RUNs
    it; anything else just gets a `SYS` line typed and waits for RETURN. A
    build that links straight to a `.prg` emits an ELF, which loads at `$457F`
    and is never started. `mega65.ld` deliberately has no `OUTPUT_FORMAT`, so
    every target must go through objcopy.
  * ~~**mtools** will not touch the card until the FAT32 BPB has non-zero CHS
    geometry.~~ **Gone with hyppo**, along with `tools/putfiles.sh`: files go
    on the D81 with `c1541` now and mtools is not involved.

## The staged harness, and what it could not tell us (HISTORICAL)

`smoke2.c` is a staged harness -- each stage draws a marker, so one screenshot
says how far it got. **This is a record of bring-up, not a current test.** It
still compiles, but it has NOT been re-run since the port moved from the
Hypervisor to the C65 DOS, and its overlay images are its own, so it needs a
disk built for it rather than the game's. The stages as they passed then:

    STAGE 1  video up, EGA palette
    STAGE 2  STRINGS.DAT loaded (through the Hypervisor, as it then was)
    STAGE 3  drawing still works after a file call
    U.S.S. LEXINGTON        -- a real pooled string, from the real file
    STAGE 4  window contains code after ovl_load: A6 16 DA A2 0C 86 04 A2
    STAGE 5  MUSIC.DAT loaded
    STAGE 6  music started
    STAGE 7  2000 frames of snd_poll(), which DMAs into a stack local each one

And the real binary reached `MAIN REACHED`, `LOOP TOP`, `MUSIC OK`, `OVL OK`.

**What replaced it is `make drive`** -- keys into the real game, a screenshot
out, and `--peek` to read any symbol out of the running machine. A harness that
links the code is worth less than one that drives the program, which is exactly
the lesson the paragraph below paid for.

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

The string pool is byte-exact against the file, and so are MUSIC.DAT, all
ELEVEN overlay images in banked RAM, and the window after an `ovl_load` --
checked with `-dumpmem` rather than by looking at the screen. (It said "ten"
here for two days after an eleventh was added. A count only ever goes stale
downward; re-derive it from the list, never from a header.)

**Verified 2026-09-08: saving.** SAVE writes `EGATREK.SAV` onto the D81 --
confirmed by reading the image back with `c1541`, not by trusting a return code
-- and a restore run returns a console whose pixels hash identically to the one
that was saved, skipping the setup screen as a successful `save_read` does.

**NOT verified: the hall-of-fame WRITE.** It draws correctly, but a driven
self-destruct scores -930, which qualifies for no slot, so nothing is written
and there is no file to check. It goes through the same `plat_write_all` the
save proves. That is an argument, not a measurement.

Jamie played the release build on 2026-09-03 and found three faults -- sound,
the briefing, and a crash that turned out to be overlay rule 4 -- all fixed
below. **He played it again on 2026-09-08, after the D81 rewrite: "it works
great."** That is the whole port confirmed by a human, which is the one thing
no rig here can stand in for.

## Still open (2026-09-08)

**Nothing on the build list.** Saving was the last item and it works: a game
SAVEs to the D81, a RESTORE brings back a pixel-identical console, and the port
no longer touches the SD card at all. Played by hand on 2026-09-08 and released
in v0.12.0.

**One thing is still unverified rather than unbuilt**, and this section said
"Nothing" over it until 2026-09-09: **the hall-of-fame WRITE**. Twice above,
this file says it has never been witnessed, because no driven game has ever
scored high enough to write a row. It goes through the `plat_write_all` the
save proves — an argument, not a measurement. "Still open" and "still
unverified" are different lists and this file was keeping only one of them.

The two deferred features -- the MAIN VIEWER's other nine pages and colour per
message -- were never started, and are deliberate scope rather than defects.
There is about 5K of resident space for them.

## Saving works, and the SD card is gone

Done 2026-09-08. `m65storage.c` is now the C65 DOS on device 8 -- essentially
`c128/src/storage.c` with a banking wrapper -- and everything the game reads or
writes lives on one D81. `make d81` builds it; `make run` and `make drive`
attach it with `-8`.

    make d81      # build/egatrek.d81 -- the whole game, one image
    make drive    # headless; --keep-disk preserves a save between runs

What went with the Hypervisor: `m65hyppo.s`, `after_hyppo()`, the
four-descriptor budget, the `closeall`-instead-of-`close` workaround, and the
512-byte sector buffer this file used to call the biggest thing the port kept
in bank 0. Reading a byte at a time needs no buffer.

**Verified as a round trip, not as a return code.** SAVE writes `egatrek.sav`
(3 blocks, confirmed with `c1541`); a restore run on the same disk comes back
to a console whose pixels hash identically to the one that was saved -- same
quadrant, same sector, same Mongol count, same chart -- and it skips the setup
screen, which is what a successful `save_read` does.

**NOT separately proven: the hall-of-fame WRITE.** It draws correctly, but a
driven self-destruct scores -930, which qualifies for no slot, so nothing is
written and there is no file to check. It goes through the same
`plat_write_all` the save proves, and that is an argument rather than a
measurement. Say so rather than claiming it.

## The bug was ONE undeclared clobber, and it cost a day

Worth the space, because two confident diagnoses were wrong before the right
one, and both were wrong in the same way: **reasoning about a symptom instead
of reading the generated code.**

The symptom was a contradiction. `cmd_status()` read the drive's status text,
stored the first two characters into diagnostics as `'0'` and `'0'`, and then
its own `if (a < '0' || a > '9')` rejected them as non-digits. Both readings
were true.

  * **First diagnosis: the KERNAL clobbers `__rc`.** llvm-mos's imaginary
    registers live at `$02..$21`, and this port has a September note about
    mega65-libc's `read512` destroying `__rc4`/`__rc5`. Plausible, precedented,
    and wrong. A save/restore shim around every KERNAL call changed nothing.
  * **Second diagnosis: the soft stack.** Also wrong.
  * **What it actually was:** `llvm-objdump` on `cmd_status`:

        cpx #$30          ; compare a with '0' -- SETS THE CARRY
        jsr m65k_save
        jsr $ffcc         ; CLRCHN runs here
        jsr m65k_rest
        bcc reject        ; branches on the carry from three calls ago

    The compare was evaluated before the call and branched on after it. **`a`
    was never corrupted -- the carry was.** llvm-mos names the status register
    `"p"`, and none of the inline asm declared it, so the compiler believed the
    flags survived a `jsr` into ROM. `mos-platform/neo6502/include/kernel.h`
    declares `"p"` on exactly this shape of call.

Turning the locals into `static volatile` globals had appeared to fix it, which
is what made the first diagnosis so convincing: it forced a reload after the
call and moved the compare with it. **A symptom disappearing is not a cause
being found.**

**AND THE SHIM WAS THEN DELETED, on a measurement.** Filling `$02..$21` with a
pattern, making one `CHRIN` and reading it back changes **zero bytes** -- the
hyppo hazard was llvm-mos's own assembly using those addresses as scratch, not
a property of calling into ROM. `plat_open` holds C locals live across OPEN,
CHKIN and the status read, so the working round trip covers the calls the probe
did not. That returned 441 bytes.

Three smaller faults were real and are fixed:

  * **`$00` is the port DDR that gates `$01`.** `mega65_io_enable()` pokes it
    with 65 to force full speed, leaving the DDR wrong for the next bank
    switch, so `lda $01` read something never written. `rom_in()` writes `$2F`
    to `$00` first, as `unmap-basic.o`'s own `.init` does. This one took the
    machine from dead to drawing.
  * **No `0:` drive prefix.** The C128 emits one and its 1541 wants it; this
    DOS errors on every open until it comes off.
  * **Reading past EOF never terminates.** `CHRIN` keeps returning a byte with
    the status bit set, so a `while (plat_read(...))` loop never ends. "EOF
    already seen" is now a distinct state.

## The hall of fame was rule 4, and the discriminator that proves it

Retested 2026-09-07, because a fix nobody re-runs the failing case against is a
hypothesis. `620ac09` fixed a resident `trek_score()` calling into a window
that `load_hof()` had already swapped, and said this port's two hall-of-fame
faults "should be retested". They were, on the identical key sequence, against
two builds that differ only by that commit:

    RETURN N RETURN N RETURN J A M I E RETURN 3 RETURN X RETURN   setup
    S X RETURN X RETURN                                          self destruct
    RETURN RETURN RETURN                                         memo, evaluation
    RETURN                                                       dismiss the hall of fame

| | at `620ac09^` | at HEAD |
|---|---|---|
| the hall of fame screen | **name rows missing** | drawn complete |
| the RETURN that dismisses it | **machine dies, 3/3** | survives, 3/3 |
| `ship` afterwards | unreachable | coherent |
| the second game's console | unreachable | energy 5000, shields 2500, 12 systems at 100 |

The broken screen is the part worth keeping. Before the fix the hall of fame
**arrives already wrong** -- the dotted name placeholders are simply absent --
which is what running off into unwritten window bytes and coming back looks
like when it does not happen to jam. The next keystroke then takes the machine
down. Both are downstream of one wild execution.

**Two claims in the old list were wrong, and both were negatives about this
port.** "It is MEGA65-only ... so it is in this port's platform layer, not in
shared code" was established by driving the C128 through the same sequence and
getting a coherent ship -- a real experiment, and a real result, that supported
a conclusion it could not reach. The fault was in shared code all along, in
`main.c`, and all three ports carried it; the C128 simply survived it. And
"none of them reproduced by a build check" is now false twice over: `make
verify` fails the tree before `620ac09`, and the sequence above reproduces the
death deterministically.

**What is NOT claimed.** The `$6464` pattern was never witnessed here directly:
this build dies at the hall of fame before a second console can be reached, so
the corruption Jamie saw cannot be reproduced on this tree to watch it go away.
What is shown is that the code path which produced it executed garbage before
the fix and does not after, and that a second game now starts coherent.
Likewise Jamie's DMA descriptor of `$454854` -- ASCII "THE", which is text the
hall of fame itself puts on screen -- is *consistent* with a wild source
pointer from the same execution, and was not re-witnessed.

## Quitting works now, and the banking never needed guessing at

Fixed 2026-09-07, and it was two faults stacked, both visible in one
screenshot: **a blank brown page**.

`plat_exit()` was an empty stub with an honest comment saying a `$FFFC` reset
was the likely answer but "the MEGA65's banking at that moment has not been
measured". It did not need measuring -- it needed **reading**. llvm-mos links
`unmap-basic.o` into every MEGA65 program, and disassembling it gives both
halves:

    .init.010   sei / $00=$2F / $01=$3E / $D030=$44     pages the C65 ROM OUT
    .fini.990   $01=$3F / $D030=$64 / cli               pages it back IN

With the ROM out, `$FFFC` is RAM and the reset vector is whatever happens to be
there. So `plat_exit()` now does exactly what `.fini` does and then takes the
vector, and the machine comes back to a BASIC 65 `READY.` with the ROM's own
palette. Reaching `exit` at all was the hang: llvm-mos's is `jsr _fini` then a
branch to itself.

`vdc_shutdown()` was the other half, and **the C128 port had already learned
this lesson**: its own version carries a comment saying it deliberately does
NOT clear the screen, because clearing left the display the player was watching
black and that reads as a crash. This one cleared. It erased the goodbye
`main()` had drawn one statement earlier -- immediately before `main()` waits
for the player to read it. The brown was the same mistake twice over:
`bgcolor(6)` restores a stock C65's blue, but this port has reprogrammed the
palette to EGA, where 6 is brown. It does nothing at all now; the reset
restores the machine.

The quit line is a **string override**, not a code change. `main.c` prints
S_10, which on the C128 is "BASIC IS ON THE 40-COLUMN SCREEN." -- true there
and meaningless on a machine with one screen that is about to restart. The
override is 34 characters because `main.c` prints it at a hardcoded `x=23` and
23+17 is dead centre on 80 columns; the first attempt was 24 and sat visibly
left. It also names the keypress, which the C128's wording never does.

## What the C65 DOS actually needs -- MEASURED 2026-09-07

Two probes, `make probe-bank` and `make probe-dos CFG=n`, run because the
scope of "make saving work" turned entirely on facts nobody had measured. The
answer is **much better than the estimate**: the low-memory trampoline this
port has been assuming for three days **is not needed at all**.

### Probe 1 -- what stops being our RAM

Markers at five addresses, written with the ROM out, read back under each
banking config. Nothing here can hang: it maps, reads, unmaps.

| cfg | `$01` | `$D030` | `$8000` | `$9000` | `$A000` | `$B000` | `$CF00` |
|---|---|---|---|---|---|---|---|
| none | 3E | 44 | RAM | RAM | RAM | RAM | RAM |
| A | 3F | 64 | RAM | RAM | **rom** | **rom** | **rom** |
| B | 3F | 44 | RAM | RAM | **rom** | **rom** | RAM |
| C | 3E | 64 | RAM | RAM | RAM | RAM | **rom** |

**The two shadows are INDEPENDENT and each has its own control:** `$01` bit 0
(LORAM) puts BASIC over `$A000..$BFFF`, `$D030` bit 5 (ROMC) puts the C65 ROM
over `$C000..$CFFF`. So there is a config for every combination.

**`$8000..$9FFF` is RAM under every one of them.** The 2026-09-04 note saying
"while the ROM is mapped, `$8000..$BFFF` is not our RAM" was wrong by 8K, and
that 8K is where the storage code already lives -- `plat_read_all` is at
`$913E`. Nothing has to move.

**THE FIRST RUN OF THIS PROBE GOT THE `$01` BITS BACKWARDS** and is worth
recording, because it produced a confident wrong answer rather than an error.
It tested `$3B` as "KERNAL and I/O, BASIC out"; `$3B` actually SETS LORAM and
CLEARS CHAREN -- BASIC in and I/O gone, the opposite question. Every config
read "`$A000` shadowed" and it looked like BASIC could not be paged out at all.
`$3E`, which `unmap-basic.o` already leaves, *is* KERNAL + I/O with BASIC out;
`$3F` only adds BASIC.

### Probe 2 -- which config the DOS will accept

One config per run, because a config the DOS rejects **hangs**. Config A first,
to check the rig reproduces the known-good result before any negative was
believed. The result is the **D81 directory read back with `c1541`**, not the
status byte: a KERNAL call can report success and write nothing.

| cfg | | outcome |
|---|---|---|
| A | `$3F`,`$64` | file written -- reproduces 2026-09-04 |
| B | `$3F`,`$44` | **HANGS** at OPEN, never returns |
| C | `$3E`,`$64` | **file written**, carry clear, `READST=$00` |

**ROMC is required and BASIC is not.** Config C is the one to use, and it
differs from the state the game already runs in by **exactly one bit** --
`$D030` bit 5.

What that buys, against the trampoline the old note called for:

  * `io_buf` (`$A784`, 626 bytes) stays visible. **Nothing to copy down.**
  * `hof` (`$A9F6`) stays visible.
  * The soft stack, `$C000` growing down into `$BFxx`, stays visible -- so the
    write path can be ordinary C. That one mattered most: under BASIC ROM,
    stores fall through to RAM while loads return ROM, so a push/pop pair
    straddling the mapped window corrupts silently.
  * Only `$C000..$CFFF` -- the overlay window -- is shadowed, and no code needs
    to execute there during the call.

**And the DOS does not scribble the window.** `$C000..$CBFF` was filled with a
pattern before the write and re-checked after: **zero bytes changed**. That
check is self-validating -- a fill that never landed would have shown
mismatches everywhere, so reading zero proves both halves ran. The loaded
overlay survives a save, and nothing has to be reloaded afterwards.

### Probe 3 -- and the SD card stops being worth keeping

The two-filesystem problem is real: writes go to a D81 on device 8, reads go
through the Hypervisor to the SD card's FAT32, and **a save written into the
D81 is invisible to hyppo's `findfile`**. The obvious escape is to put
*everything* on the D81 and drop the Hypervisor entirely -- one I/O stack, one
medium, and the save reads back from where it was written.

That turns on one number. `OVERLAYS.BIN` is 45,056 bytes and has to reach
banked RAM at startup, and the C65 KERNAL's banked LOAD cannot be used (it
exists, Xemu will not run it -- see below), so it would come in through
`CHRIN`, a byte at a time. **Measured, `make probe-read`:**

    45,056 bytes   xor checksum $57, matching the file exactly
    0.9 seconds    timed on CIA1's TOD, ~50 KB/s

So the objection does not survive. A byte at a time through the C65 DOS is
fast enough to load the whole overlay set in under a second, and it is
**byte-perfect** -- the checksum is in there because a read returning zeros
quickly would look like a wonderful result.

**THE FIRST RUN WAS ONE BYTE SHORT** and that is worth keeping, because it is
the bug any real implementation will have. `if (instat) break;` before counting
gave 45,055 bytes and a checksum of `$C3`. ST bit 6 (EOF) is set **together
with the last good byte**; only bit 7 means the byte is junk. What identified
it was arithmetic rather than a guess: `$C3 ^ $94` is exactly `$57`, and `$94`
is the last byte of OVERLAYS.BIN -- the high half of the build stamp.

What dropping hyppo would take with it: `m65hyppo.s` and its `__rc`-saving
shim, `after_hyppo()`, the four-descriptor budget and the `closeall`-instead-of
-`close` workaround, and **the 512-byte sector buffer**, which this file calls
the single biggest thing the port keeps in bank 0. Reading a byte at a time
needs no buffer at all -- each byte can go straight to `$50000+n` with `lpoke`.

The cost is that the player mounts a D81 instead of copying five files onto the
card, which is how MEGA65 software is normally distributed anyway, and it makes
this port ship like the C128's `.d64`.

### Probe 4 -- the command channel, and proving the scratch is load-bearing

The last three unknowns, all of them things that work on a 1541 and might not
here. `make probe-cmd`, against a fresh D81:

| step | | got | wanted |
|---|---|---|---|
| 1 | command channel on a fresh disk | `00,OK` | 00 |
| 2 | write, file absent | `00,OK` | 00 |
| 3 | write again, **no scratch** | `63,FI` | **63 FILE EXISTS** |
| 4 | scratch `S0:TREKSAVE` | `01,FI` | 01 FILES SCRATCHED |
| 5 | write again, after scratch | `00,OK` | 00 |
| 6 | open a name that is not there | `62,FI` | 62 FILE NOT FOUND |
| 7 | read the file back | `00,OK` | 00 |

Eight bytes back, `$41..$48` -- exactly what was written. The D81 afterwards
holds **one** `TREKSAVE`, not two.

**STEP 3 IS THE POINT OF THE PROBE.** Writing the same name twice with no
scratch in between is *supposed* to fail, and it did. Without that row, step 5
proves nothing: a run where every step succeeds cannot tell you which step
mattered, and "the scratch is necessary" would have been an assumption wearing
a passing test.

So the command channel behaves exactly as `c128/src/storage.c` expects, error
codes included -- which means `classify()` (62 is NOT FOUND, everything else
non-zero is a real fault) ports across unchanged, and `plat_read_all` really
can tell "no save yet" from "broken disk".

**And the window survives a read**: `$C000..$CBFF` again zero bytes changed.
That was the one with teeth, because the briefing streams from disk *while an
overlay is loaded*. It does not need reloading afterwards.

Nothing about the write path is unmeasured now. What is left is writing it, and
one pass on real hardware.

## Driving this port headlessly

`tools/drive.py out.png [--peek SYMBOL[:LEN]] KEY...` injects keys through the
uart monitor and screenshots the result. `--peek` was added for the retest
above: a screenshot answers "what does the player see" and cannot answer "what
is in `ship`", and the whole of the old `$6464` report is a claim about memory.
Symbols are looked up in the ELF, never hardcoded -- these addresses move
between builds, which has already cost this port one afternoon.

    python3 tools/drive.py build/end.png --peek ship RETURN N RETURN ...

    drive: ship $ACDE 61 bytes
      +00  02 04 04 01 88 13 f4 01 c4 09 09 64 00 00 0a 00
      +16  b8 88 00 e4 89 03 21 00 64 64 64 64 64 64 64 64

`Ship` packs to one-byte alignment on llvm-mos, so the fields are sequential:
energy at +4, impulse +6, shields +8, torps +10, laser_eff +11, level +21,
enemies_left +22, and the twelve `sys[]` percentages at +24.

## What `make verify` checks

Added 2026-09-05. This port had no verify step at all for six days after the
C128's overlay call-graph check caught a guaranteed crash -- same architecture,
same one window, same `.ovl_*` section names, nothing looking at them.

    load address      the PRG must start at $2001 or Xemu will not auto-run it
    resident space    the image against mega65.ld's OWN `ram` region
    overlay layout    one run address, distinct load addresses, each fits
    overlay calls     rules 2, 3 and 4 of core/overlay.h -- shared with the C128
    OVERLAYS.BIN      one 4K slot per overlay, each byte-identical to its ELF section
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
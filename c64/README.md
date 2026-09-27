# EGA Trek — Commodore 64

Eighth port, **built and released as v0.16.0 on 2026-09-14** — the same day it
was started. Every seam is real: video, keyboard, sound, storage (read *and*
write), overlays and far memory.

**It has been played**, which on this project is the step that finds the
faults, and this sitting found none: *"its perfect."* Only the second sitting
here to find nothing — the first was the C128, the port every other one is a
diff against.

```sh
make            # build/trek64.prg + the eleven overlay images
make debug      # the same with TREK_DEBUG_INPUT, for scripted runs
make d64        # build/egatrek-c64.d64 -- runs verify first
make d64-debug  # the instrumented disk tools/play.py drives
make verify     # the cheap gate, no emulator: what `make ports` runs
make writecheck # write 600 bytes and read them back IN ONE RUN
make run        # x64sc, autostart
make d64-reu    # the REU build -- NOT a release; see below
make reucheck   # play one game on both builds and compare every screen
python3 tools/boot.py        # boot the disk, screenshot it, LEAVE IT RUNNING
python3 tools/play.py        # drive past setup to the console, and leave it
```

`tools/boot.py` and `tools/play.py` both leave VICE running unless you pass
`--kill`. That is deliberate: a tool that gets a machine somewhere interesting
should hand it over.

## Almost none of this port is its own code

`c128/src/vic.c` was written as the C128's 40-column driver and is linked here
**unchanged**. A VIC-IIe in 40-column mode and a VIC-II are the same chip for
everything this game does: screen memory at `$0400`, colour RAM at `$D800`,
one global background, the same character ROM in the same place. `ui.c`,
`main.c`, `layout40.c`, `input.c`, `sid.c`, `storage.c`, `overlay.c` and
`strpool.c` come across the same way.

So `make -C c128 game40` is this port's regression test as much as its own,
and the four `#ifdef __C128__` blocks in the shared files are the entire
difference:

| file | what is guarded | why |
|---|---|---|
| `vic.c` | `$D030` bit 0 | the C128's 2MHz latch; a C64 has no such register |
| `vic.c` | `$0A2C` | VM1 on a C128. **On a C64 that address is inside this program's own code** — BASIC text starts at `$0801`, so the store would patch a byte of `.text` |
| `storage.c`, `overlay.c` | `jsr $FF68` | SETBNK is a C128 vector; on a C64 that address is in the middle of the KERNAL |
| `input.c` | the cursor keys | the C128 has four dedicated ones on `$D02F`; the C64 has one per axis and needs shift decoded |

Two files are actually the C64's, and both replace a piece of C128 hardware:
`src/c64mem.c` for bank 1, and `src/c64log.c` for the VDC's spare 16K.

## The memory map, and why this is the roomier machine

Measured, not argued, at release (v0.16.0, 2026-09-14). `make verify` prints
the live figure: 40,856 resident and 5,991 spare on 2026-09-26.

|  | resident | region | spare |
|---|---:|---:|---:|
| C128, 40-column | 39,591 | 39,935 | **304** |
| C64 | 40,735 | 46,847 | **6,112** |

```
$0801..$BEFF   program: code, rodata, data, bss, and the 2K message log
$BF00..$BFFF   soft-stack guard; __stack = $C000, growing down
$C000..$CFFF   the overlay window -- the 4K nothing ever covers
$D000..$DFFF   I/O
$E000..$FFF9   the far store: STRINGS.DAT and MUSIC.DAT, under the KERNAL
$FFFA..$FFFF   RAM copies of the NMI/RESET/IRQ vectors
```

Nearly 7K more room than the C128 has, for two reasons that are nothing to do
with the CPU. BASIC's 8K at `$A000` is plain RAM once LORAM is cleared
(llvm-mos's `unmap-basic.o` does it in `.init.010`), and **the overlay window
moved out of the program's address space** — on the C128 those 4K are carved
out of `ram`, here they are the untouchable page at `$C000`.

What is tight instead is the far store: 8,186 bytes holding 7,902, with 284
spare. `make verify` fails the build when they stop fitting. The escape hatch
is the spare room above — `MUSIC.DAT` can become resident data again. Do not
solve it by shortening the prose.

## Far memory is a memcpy here

On a C64, **writes always go to RAM**; only reads see the ROMs. So the
KERNAL's own LOAD, running out of the KERNAL ROM at `$F4A5`, stores the file
through `(EAL),Y` into the RAM underneath itself. One call, no banking, no
chunking, and the ROM it is executing from stays mapped throughout.

Reading it back is the only part that banks, and it has two traps that are
handled rather than hoped past.

**The map is 101, not "clear HIRAM".** Going from 110 to 100 gives RAM ONLY
and takes the I/O page with it — a string fetch on a machine with no VIC, no
SID and no CIAs. `c64mem.c` computes the value from whatever `$01` already
holds, so the datasette bits are preserved.

**`SEI` does not mask an NMI, and RESTORE is wired to one.** While the KERNAL
is out the 6502 takes its vectors from RAM at `$FFFA`, so a player leaning on
RESTORE mid-fetch would send the CPU into two bytes of prose. The vectors are
written into RAM pointing at an `RTI` and the store stops six bytes short;
`verify_c64.py` fails if `FAR_LIMIT` ever grows over them.

## The message log was the seam nobody had counted

`NOTES.md` item 57 said far memory was the one new seam the C64 needed. It is
two. `ui.c` keeps a 32-entry message log outside the program through
`vdc_set_address` / `vdc_data_write` / `vdc_data_read`, and every port
implements them — the C128's 40-column build still uses real VDC RAM, because
the 8563 is in the machine even when it is not driving the monitor. **A C64
has no such chip.**

The answer turned out to be the cheap one: a plain 2K array in `.noinit`,
because of the spare room above. It could have gone under the KERNAL beside
the pool; it does not, because there is no room there and because every read
would cost an interrupts-off bank switch.

## The REU build — not a release

`make d64-reu` builds a second C64 program whose overlays **may call each
other**. It exists for one reason: C64 OS gives an app 30,976 bytes, and this
game fits in that only if the engine and the console drawing leave resident
memory — which the disk overlays forbid, because a swap from a 1541 costs
seconds. From an REU it is a DMA. See `NOTES.md`, *"THE TRIAL LINK"* and
*"THE REU OVERLAY MANAGER"*.

It is the same C as the release, with three differences:

* **Seven more overlays** — view, panel, nav, time, turn, laser, torp — marked
  `OVL_CODE_REU` in the shared sources, where the macro is empty on every
  other port. Twenty in all, each inside the 4K window at `$C000`.
* **`src/c64reu.c` replaces the disk loader.** The first `ovl_load` loads all
  twenty images from the disk once, checks each one's stamp, and stashes it
  in the REU at 4K per image. After that `ovl_load` does nothing.
* **The link is patched after it is made.** `tools/reu_thunks.py` reads the
  relocations `-Wl,--emit-relocs` leaves in the ELF, and points every JSR or
  JMP that crosses into another overlay at a six-byte thunk. It refuses to
  patch on a function pointer into an overlay, or on an operand that does not
  hold what its relocation predicts. Calls inside one overlay stay direct.

`src/reuovl.s` is the manager. A thunk is `jsr ovl_far` followed by the
overlay id and the function's address. If that overlay is already in the
window it jumps straight there. Otherwise it pushes the id that is loaded,
DMAs the callee in, calls it, DMAs the pushed one back and returns. A, X and
Y survive both ways, and no zero page is touched.

**It always puts back what was loaded, even for a resident caller.** A
resident helper called from overlay A returns into A, so its call into B must
leave A in the window afterwards.

**Measured 2026-09-26:**
* 48 thunks, 70 calls patched, and 150 jump-table entries left alone. Switch
  tables sit in resident `.rodata` and point into their own function, which is
  the only code that reads them.
* The manager is 63 bytes of `reu_xfer`, 143 of `ovl_far`, 288 of thunks and
  153 of tables.
* The resident program is 21,823 bytes, against the release's 40,856 (`report_size.py`).

**Twice as fast once started.** Timed at 1x, the scripted game after the
title took 24.6 seconds against the disk build's 50.0: every command that
loads an overlay drops from 3-6 seconds to about one. The price is paid once,
at the title: 47 seconds to load all twenty images from a stock 1541.

It needs a 128K 1700 or larger (`make runreu` boots one in VICE). With no REU
it says so and stops.

**`make reucheck` is the test.** It plays one pinned game on the disk build and
on the REU build: the chart, two moves, a laser kill, a torpedo, docking and
the modal screens. It compares every screen in characters and colours. The
galaxy is pinned by writing `kb_entropy` and the password's RETURN while VICE
is stopped, so both builds play the same game with no change to the code. All
35 screens match. With the DMA that puts the caller back removed, the very
first console differs.

## Watch out for

**The host `.d64` lags VICE's drive.** After a SAVE the image lists
`0 "egatrek.sav" *seq` and `c1541` answers `ERR = 62, FILE NOT FOUND`. That
is not this port: running the same sequence against the C128's own 40-column
disk gives the identical splat, on a port whose save and restore have been
played. **Ask the drive** — `make writecheck` does, and reports 600 bytes
written, read back, byte-identical.

**`make -C c128 verify` still owns the shared checks.** `tools/verify_c64.py`
deliberately checks only what is true of this machine — the memory map — and
leaves the overlay call graph, the key table, the panel geometry and the
string-pool ceiling to the C128's, which reads the same sources. Two copies
would be two things to keep in step.

**Adding one pooled string changes this binary too.** `strpool.c` compiles
`STR_COUNT` into a guard and into arithmetic, so a binary whose count
disagrees with the `STRINGS.DAT` beside it refuses the pool and plays with
every label blank. See `NOTES.md` item 60.

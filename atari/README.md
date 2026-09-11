# EGA Trek — Atari 800XL + VBXE

## Be patient with the first screen

**On a stock 1050, this disk takes about two minutes to load.** That is
measured, not estimated: 112 seconds, for 36,474 bytes of overlays, strings
and music that all have to be in VBXE's memory before the title screen can be
drawn. Nothing is wrong; the drive is just a 1050.

**On an emulator, or on an Atari with a fast-SIO drive, it is a few seconds.**
Altirra with its default SIO patch loads the same disk in 12.7 seconds of
emulated time and rather less of yours. A Happy/Speedy-class 1050, an XF551 or
an SD-card drive sits somewhere between; this project has not measured one, so
no number is claimed for them.

The reason it is worth stating at all is that the slow figure was invisible
here for months. **Every boot this project had ever timed ran with Altirra's
SIO patch on**, which replaces the serial protocol with an instant transfer —
so the 12.7 seconds was the only number anybody had, and it describes no real
drive. `make run-probe-boottime` times both.

---

Fifth port. **Started 2026-09-06 with a measurement, not a driver**, because
the scope for this target ends with an instruction: *"Measure it properly
before committing: link the whole game early."*

    make early        link the game against the seams as they stand and report the budget
    make smoke        first light: the video seam and nothing else
    make run-smoke    boot it headless on AltirraSDL and screenshot it

## Where it is

**It plays, and there is no DOS on the disk.** Fifth port, self-booting on an
Atari 800XL with VBXE, driven through setup into the nine-panel console, a
turn, a SAVE and a restore.

`make run-smoke` boots an XEX on AltirraSDL's headless bridge and comes back
with a picture: eighty columns, twenty-five rows, all sixteen EGA colours on
their own indices, and the console's box-drawing set rendering as a frame with
its tees and cross joining. That is questions 1–4 of first light answered in
one screenshot.

`make run-keyecho` injects thirteen keys over the same bridge and screenshots
what the input seam returned. All thirteen came back as the values
`c128/src/input.h` names — letters uppercased, RETURN 13, ESC 27, DELETE 20.

`make run-memtest` plants a 9,000-byte pattern in VBXE VRAM and reads it back
across both 4K bank boundaries, including a 4,608-byte overlay-sized `far_bulk`
starting where it crosses two of them. Four for four, zero bad bytes, and the
screen bank survives — which is the check that two drivers share the MEMAC
bank bookkeeping correctly.

`make run-sndtest` runs the shipping sound driver in **both video standards**
and asks Altirra what frequency each POKEY voice is actually producing. Region,
both voices, the pitch at each end of the music's range, the tempo and the
loop — twelve checks, all pass, worst pitch error 0.05%.

`make run-nodostest` exercises all five `plat_*` against a disk with no DOS on
it: a whole-file read, a **streamed** read, and a write read back. Each is
checked by a rotate-and-add sum of its bytes against the host file, not by its
length — see "Reading a file is not knowing its length".

`make atr` builds the game disk and `tools/play.py` drives it: title screen,
briefing declined, a captain named JAMIE typed a letter at a time (J and I
included — those are the two the C128 could not type once), command level,
self-destruct password, and then the console. `SHUP` ran a full turn —
"ENGINEERING: SHIELDS UP", energy 5000 to 4980 — **through the paged enemy
code**. `M6,2,3,5` warped the ship from quadrant 8,8 to 6,2 sector 3,5,
stardate 3500.0 to 3523.6, chart filled in, console redrawn — **through the
paged move code**. Both new windows work on the machine, not just in the link.

**`src/stubs.c` is now empty of live code**, so `make early` measures the whole
thing. The two extra overlays are committed and gated per port — see "The two
levers that close it" — and the shipping link has **about 2,400 bytes free**,
against 693 while DOS was on the disk — see "The margin, and where it came
from". `make verify` prints the exact figure and is the only authority for it;
it now prints **two** pools, because the program's data no longer lives in the
same address space as its code.

## The budget, and it moved

`make early` is the authority and re-measures on every build. **It no longer
measures a fully stubbed game** — as each driver lands its stubs drop out of
`src/stubs.c` and the link gets the real one, so the number converges on the
truth instead of being corrected by hand at the end. The header line says
which seams are real.

Reading on 2026-09-09, with **every seam real** for the first time:

```
address space   $3000..$BFFF        36864     4K MEMAC window, not 8K
overlay window                       4608
for resident                        32256

RESIDENT IS OVER BY ABOUT 5,180 BYTES
```

**Quote the magnitude, run `make early` for the number.** That last digit is
not stable and never will be: regenerating the music alone moves it, because
`music_data.c` is compiled in, and it moved by one byte between writing this
paragraph and committing it.

It has been 3,923 (video only), 4,213 (video and input), 3,678 (far memory and
overlays added — *less*, because `ovl_load` stopped being a volatile-sink stub
inlined into each of `main()`'s loader stubs), about 3,770 with sound and about
5,180 with storage. The direction is not monotonic.

**Storage cost 1,394 bytes**, which is the second-largest seam after video and
about double what its 500-byte driver measures.

### A SEAM COSTS MORE THAN ITS DRIVER, and that is new information

Replacing the video stubs with the real driver cost **4,636 bytes** against
the driver's own **1,559**. The other 3,077 is `main()` and the eight
`ui_draw_*` routines growing:

```
.text.main            4679 -> 6038    +1359
.text.ui_draw_all      488 ->  849     +361
.text.ui_draw_chart    475 ->  683     +208
.text.ui_draw_viewer   632 ->  818     +186
.text.ui_draw_status   400 ->  562     +162
.text.ui_draw_systems  521 ->  624     +103
```

A stub that folds to one `volatile` write lets the optimiser collapse the
argument setup at every call site; a real driver does not, and `ui.c` calls
`scr_*` from hundreds of them. **So "the X16's driver layer measures 4,539
bytes" — which this README and `budget.py` both quoted as the size of the
gap — was a measurement of DRIVERS, not of seams.** The five seams still
stubbed will cost more than their sources measure, and how much more is not
known until each one lands.

`__attribute__((noinline))` on every `scr_*` entry point takes 242 bytes of
that back. It is worth doing here and nowhere else in the project.

## The three levers, all measured

```
+3,520   a twelfth overlay: trek_enemy_turn and its private damage chain
+2,282   writable data below the window, as the C128's lowram does it
  +512   splitting msgs and planet so the overlay window returns to 4,096
```

### +3,520 — the enemy overlay. ANSWERED, and it was BUILT to find out

Not reasoned about — performed on the C128 in a throwaway worktree on
2026-09-08, because this project has been wrong before about which split pays
(the fourth overlay pass named candidates that cost 863 bytes instead of
saving any).

The call graph is unusually clean:

```
trek_enemy_turn     1939   <- run_turn, and nothing else
trek_wreck_system   1132   <- trek_combat_damage
trek_take_hit        814   <- trek_enemy_turn
trek_combat_damage   393   <- trek_enemy_turn
                    ----
                    4278   private to the enemy turn

trek_laser_damage    341   <- trek_enemy_turn AND trek_fire_laser -- SHARED,
                             so it stays resident
```

Everything except `trek_laser_damage` is reachable from exactly one place:
rule 3 satisfied by construction, rule 4 by the same `PAIRED` pattern the
events overlay already uses. Measured as a twelfth overlay on the C128:
resident text 33,571 → 30,051, `.ovl_enemy` 3,832 of 4,094, `make verify`
passing including rule 4, and it **plays** — `SHUP` ran a full turn through
the newly paged code, "ENGINEERING: SHIELDS UP", energy 5000 to 4950.

**Not committed to the C128**, where it would buy nothing and cost a disk
read on the hot path. `run_turn` calls the enemy turn on essentially every
command, so this is a window swap on the hot path — which is affordable here
and ruinous on a 1541, because `ovl_load` is idempotent and because on this
target the images live in VBXE's 512K of VRAM and arrive as a memory copy.

### The DOS lever, SPENT — and it paid more than it was costed at

The Atari's `.data`/`.bss`/`.noinit` used to come out of the same region as its
code, where the C128's live in a separate `lowram` that does not compete at
all. That difference was the single largest structural disadvantage this
target had, and the fix was obvious and blocked: point `c_writeable` at
`$0700..$1FFF`, which is **where Atari DOS lives**.

It was costed at **+2,282 without DOS, +772 with** — a booted DOS 2.5 reports
`MEMLO = $1CFC`, read straight off the machine, so with DOS resident only
`$1CFC..$1FFF` is free. And DOS looked unavoidable, because **saves are named
by the player**: `ui.c` lets them type a filename, and raw sector ranges have
no names.

**Both halves of that turned out to be wrong, and in the same direction.**

*Names are not a reason to keep DOS.* A directory is sixteen entries of sixteen
bytes — `tools/nodos.py` writes it, `src/atarisio.c` reads it. What it costs is
under a sector of disk and no address space at all, because unclaimed **slots**
carry their extent from the start; nothing here allocates.

*And the region is bigger than the writable data.* `.rodata` is 2,161 bytes of
tables and literals that also never compete for anything but address space, and
it went down there too.

What was actually measured, in order:

| build | code free |
|---|---|
| CIO through `D:`, everything in `$3000` | 693 |
| SIO, everything in `$3000` | **−1,344** (did not link) |
| SIO, `.data`/`.bss`/`.noinit` at `$0A00` | 218 |
| SIO, `.rodata` there as well | **2,379** |

**That last row is the measurement, not today's figure.** The sound and exit
fixes of 2026-09-11 spent about 450 of it; `make verify` prints the live
number and is the only authority for it.

The SIO seam is about 2,000 bytes more expensive than the CIO one — it carries
a directory, a slot allocator and its own sector buffers, where `D:` had DOS
doing all three off-budget. Paying for that out of the region DOS was sitting
on still leaves this port with **three and a half times** the headroom it had,
and a disk that is ours to give away.

`$0A00`, not `$0700`, and the three pages are not rounding: the boot record
loads at `$0700` and its sector buffer is at `$0900`, and both are still live
while the program's own segments are being read in. See `atari.ld`.

## The two levers that close it

Every other port's overlay budget is governed by "an overlay swap is a disk
load, so never page anything on the hot path." **Here a swap is a copy out of
VRAM**, which is what makes both of these affordable and neither of them
affordable anywhere else.

Both were **built and measured on this target** in a throwaway worktree on
2026-09-09, not projected:

```
over by, every seam real       5,161
OVL_ENEMY                     -3,910
OVL_MOVE                      -1,578
                          ----------
                            327 SPARE     link exit status 0
```

That 327 is the **experiment's** figure and not the shipping one -- it was the
early link, in a worktree, before the drivers grew and before `io_buf` moved
out of the way. `make verify` is the authority for what the game has today.

**OVL_MOVE is the whole `M` command**, not `report_move` alone. `report_move`
is 717 bytes as written and cannot go in an overlay by itself: its callers
`move_absolute` and `do_move_manual` are resident, and rule 4 says only `main()`
or a declared pair may call into a window. Taking the command whole —
`do_move`, `do_move_prompt`, `do_move_manual`, `read_delta`, `move_absolute`,
`report_move` — gives one entry point reached from `main()` and nothing else.
`grab_digits` is shared with `do_warp` and `put_quad`/`put_sector` are used by
the rare-event reports, so all three stay resident.

The image is **1,563 bytes of a 4,608 window**, less than half full, so there
is room for more of the command layer later.

**And OVL_ENEMY is 3,910 here against 3,520 on the C128** — which is why it was
worth re-measuring rather than carrying the C128's figure across. Its image is
**4,202 bytes**, and that has a consequence below.

### Two things the old lever list had wrong

**"Split `msgs` and `planet` so the window returns to 4,096, +512."** Both of
those had already fallen under 4,096 — `msgs` is 3,887 and `planet` 3,273, and
the largest image of the eleven was `front` at 3,889. **No split was needed for
that, and had not been for some time.** The window could simply have been made
smaller.

**Except it cannot be, and that is the other correction.** `.ovl_enemy` is
4,202 bytes, so the window has to stay at 4,608 to hold it. The lever is not
merely unnecessary, it is unavailable — and it was worth 537, not 512.

### How it landed

`TREK_OVL_ENEMY` and `TREK_OVL_MOVE` are opt-in and only `atari/Makefile` sets
them, so the other four ports compile byte-identically — checked, not
assumed: `c128/build/trek128.prg` hashes the same before and after.
`tools/overlay_check.py` carries the `run_turn` → `.ovl.enemy` pairing, and
`make verify` here reads rules 2, 3 and 4 off `-fno-lto` objects.

## OVERLAYS.BIN is PACKED, and the bug that made it necessary

Every other port pads each overlay image to the window and indexes the file as
`which * OVL_SIZE`. Thirteen windows of 4,608 is 59,904 bytes, and with the
string pool and the music ahead of it in the far store that is **67,600 —
past the 65,535 the seam's 16-bit offsets can address.**

**It overflowed silently, and the way it presented is the point.** `far_used`
wrapped while `OVERLAYS.BIN` was streaming, so the last 2,064 bytes of the
images were written over the *start* of the far store, which is the string
pool. The game booted. It drew its title. It drew every panel border on the
console. **Every piece of text was missing and nothing reported anything** —
not the loader, not the string pool's own count guard, which had been read
correctly an instant before it was overwritten. What found it was watching
`far_used` climb to 58,960 and then read 2,064.

Two changes came out of it. `far_load` now refuses rather than wrapping — in
sixteen-bit arithmetic, because the obvious `(uint32_t)far_used + got` cost 61
bytes this port did not have. And `OVERLAYS.BIN` starts with its own index of
`OVL_COUNT+1` offsets and packs the images at their real sizes: **36,474 bytes
instead of 59,904**, which is 23,460 bytes of VRAM and 188 disk sectors back,
and a swap that copies only the bytes an image actually has.

### +512 — the window

`msgs` and `planet` compile larger than 4,096 on this target, which is why
`__ovl_size` is `0x1200`. Splitting either brings the window back to 4,096
and hands the difference to resident code.

## Why $3000, and why the space is short

VBXE's MEMAC window maps its VRAM **into the 6502 address space**, so
whatever it covers does not belong to the program, and `$C000` up is the XL's
OS ROM.

Uno found this **by crashing into it**: a build that loaded at `$2000` had
`vbxe_init()` map an 8K window over its own code and execution fell into
VRAM, `PC=$25ED illegal=1`. The emulator's Program Error dialog is a long way
from where the mistake was, which is why `atari.ld` says so at length.

**The window size is the first lever and it is already spent.** With uno's
default 8K window the program starts at `$4000` and gets 32,768 bytes; at 4K
it starts at `$3000` and gets 36,864. That 4,096 is what makes the budget
merely difficult.

The Atari's resident profile is otherwise *the same as the C128's*, function
for function — `run_turn` 6,434 against 6,917, `main` 4,682 against 5,018 —
so this is not a target that generates worse code. It is a target with less
room and no separate home for its variables.

## The video driver

`src/vbxevid.c`. The register map, the XDL structure and three traps come
from `commodore-uno/atari/src/vbxevid.c` (MIT, same author), which found each
of them the hard way:

* **The FX core exposes CSEL/PSEL/CR/CG/CB directly at `$D644-$D648`.** The
  older VBXE manual documents those addresses as an MSEL/MB0-3 commit
  protocol; following the manual scrambles the palette so text renders in a
  colour indistinguishable from its background — which looks like blank
  glyphs, not like a palette bug.
* **MEMAC window A is `$D65E/$D65F`.** The v1.0-beta manual's `MA_CPU` at
  `$D64C` **does not exist** on the FX core: Altirra's register switch has no
  case for it, so writes are silently dropped and the window never opens.
  Same-window read-backs then look correct, which is what makes it expensive.
* **CHBASE must be programmed by a PRIOR XDL entry**, not the one that turns
  text mode on, or every glyph comes out blank with correct backgrounds.

What is new here:

* **A 4K window at `$2000`**, for the 4,096 bytes it buys the program.
* **A VRAM layout with the screen in bank 0** — `$00000`, 4,000 bytes, with
  the XDL in the 96 it leaves over — so every `scr_*` call is a plain store
  into the window with no bank register write at all. Uno put its screen in
  bank 1 and paid a select on every write.
* **Twenty-five rows.** Uno's driver is 24. 25 × 8 = 200 scanlines plus the
  8-line CHBASE border is 208, inside the raster, and `make run-smoke` draws
  row 24 on purpose so the claim is re-made against *this* program rather
  than inherited from the measurement that first established it.
* **EGA's own sixteen in palette entries 0..15**, so `TREK_COLOUR_IS_EGA`
  makes the mapping the identity as it does on the MEGA65. Entries 128..143
  are programmed black, because the attribute encoding puts a cell's
  background at its foreground index plus 128 and an unprogrammed slot shows
  whatever was in it — uno saw that as an all-white screen.
* **A font that is BUILT, not inherited.** This is the one real difference
  from the other three 6502 targets. The C128 uses the set its KERNAL put in
  VDC RAM and the MEGA65 the C65's; the Atari ROM has letters and digits but
  no box-drawing glyphs at all, and the console is built out of box-drawing
  glyphs. So `font_build()` assembles 256 glyphs in C64/C128 **screen-code**
  order from the OS ROM's own set (through CHBAS, the documented way) plus
  seventeen authored ones — the box-drawing set, the badge disc, the systems
  bar, the ship's saucer and two arrows. **Those are this port's own artwork,
  shared with the Amiga's `amigagfx.c`, which drew them first for the same
  reason.** Reverse video is applied as a rule rather than as entries, so 160,
  226 and 228 fall out of 32, 98 and 100. A code nobody drew renders as a
  hollow box, not as nothing: the Amiga missed two of fifteen on its first
  pass, and an invisible glyph leaves a panel looking merely empty.

## The input driver, and why it carries no table

`src/atariinput.c`. The OS keyboard IRQ writes a raw key code into `CH`
(`$02FC`) — `$FF` when nothing is waiting, bit 6 for shift, bit 7 for ctrl.
That code is a **keyboard matrix position, not a character**, so every other
port here decodes such a thing with a table of its own.

**The Atari already has the table.** `KEYDEF` (`$0079/$007A`) points at a
192-byte ROM table — 64 unshifted, 64 shifted, 64 control — indexed by exactly
the byte `CH` holds. So the driver is one indexed load and a small fold from
ATASCII to the ASCII values `input.h` names: uppercase the letters, and bend
four codes by hand (EOL is `$9B` not 13, delete is `$7E`, and the cursor keys
produce `$1C`/`$1D` where `input.h` uses 1 and 2). The whole seam is 290 bytes.

**Reading that table beat injecting keys, and the difference is instructive.**
`make keytable` injects every key the game needs and prints what `CH` held —
46 of them, and it is how the fold was checked. But the bridge has no key
identifier for `+`, `*`, `:`, `@` or the cursor keys, so six came back
"unknown key name". The table answers all six directly:

```
+  $06     *  $07     :  $42 (shift ;)    @  $75 (shift 8)
up $8E (ctrl -)       down $8F (ctrl =)
```

**The Atari's cursor keys ARE ctrl-minus and ctrl-equals.** No amount of
asking the emulator for a key called `UP` was going to say so.

One thing here is reasoned rather than measured, and is marked as such at its
site: `kb_waitkey` waits for POKEY's `SKSTAT` bit 2 to come back before
returning, so the OS's auto-repeat cannot turn one press into a burst. The
bridge's `KEY` queues a press-and-release and cannot hold a key down, so the
rig cannot reach that case. First real play settles it.

## Far memory and overlays are one mechanism here

`core/farmem.h` lists the banking model of every target it was designed around
and this one is not in the list, because when it was written this machine was
going to be a 130XE with 16K banks at `$4000`. It is not. **VBXE brings 512K of
its own VRAM through the same MEMAC window the video driver already opens**, so
far memory, the message log and the screen are the same mechanism seen through
the same 4K hole. Banks 0–3 are the video driver's (screen and XDL, font, the
log's two); the far store starts at VRAM `$04000`, and the seam's 16-bit
offsets reach 64K of it — four times what any port has ever put in one.

`src/atariovl.c` is then the X16's loader with the far store underneath it: all
the images live in VRAM and a swap is a copy through the window. **On this
target that is not merely tidier, it is what makes the twelfth overlay
possible** — `run_turn` calls the enemy turn on essentially every command, so
paging it is a window swap on the hot path. A disk read there would be ruinous;
a VRAM copy is not. `ovl_load` stays idempotent, which is what makes a turn
that touched nothing else cost nothing at all.

The build-stamp check comes across from the other ports unchanged. It cost the
MEGA65 an afternoon and two wrong diagnoses before the disk was even suspected,
and it caught a real mismatch on the C128 the day it went in.

**One number that was duplicated is not any more.** The X16 keeps its window
size in both its Makefile and its linker script and relies on nobody changing
one without the other; here the Makefile reads `__ovl_size` out of `atari.ld`,
which is the only file that can actually enforce it.

### The stub that deleted the game, from the other side

The moment `far_load` became real, the early link reported **1,507 bytes** —
the X16's 1,564-byte reading arriving through a different door, and the same
disaster the top of `src/stubs.c` warns about.

The fault was one literal. `plat_read` returned `0`, so LTO could prove the
read loop never ran, so `far_load` always returned `FAR_NONE`, so `ovl_init`
always reached its `noreturn` `die()` — and everything `main()` does after its
first `ovl_load` was unreachable. **A stub is unfoldable only while nothing
downstream of it is real.** Check every stub again each time a consumer lands,
not once when it is written.

## Sound: POKEY in 16-bit, and every number measured

`src/atarisnd.c`, 1,050 bytes, +89 net once its stubs left. Pacing and pitch
are the two things this project has got wrong by reasoning — the MEGA65 ran its
music at double speed off a raster that wraps twice a frame, and the X16 paced
off a jiffy clock that returns zero for ever once a program has taken the
machine over — so `src/sndprobe.c` and `tools/pokey.py` asked the machine
instead. The probe is driven from outside: the harness pokes a register block
into page 6 and the 6502 writes it to POKEY, so the write is a hardware write
and any configuration can be tried without rebuilding.

**16-bit mode, which costs a divide, because the 8-bit modes cannot carry this
tune.** Measured against the actual note set in `MUSIC.DAT` — 90 Hz to 930 Hz,
25 distinct values:

```
8-bit, 64kHz base    bottoms out at 124.8 Hz.  Cannot reach 90 Hz.
8-bit, 15kHz base    reaches everything, but is 5.51% sharp at 930 Hz and
                     over 1.5% out on six of the 25. A semitone is 5.95%.
16-bit, 1.79MHz      worst error 0.032% across the whole set.
```

Joining channels 1+2 and 3+4 uses all of POKEY and gives exactly the two voices
this seam exists for. The divisor is `AUDF = C/n - 7`, and `C` was measured in
each region rather than derived from a CPU clock: **88,672 on PAL, 89,489 on
NTSC**, `n` being the note in tens of Hz.

**`$D014` is `$01` on PAL and `$0F` on NTSC — settled by making the machine
both.** The first reading of `$0F` sat next to a base clock that was plainly
NTSC's, so one of the two was being misread and no amount of staring at either
was going to say which.

**And the Atari has the frame clock the X16 did not.** `RTCLOK` (`$0014`), the
OS vertical-blank counter, keeps running for a program that has taken the
machine over — measured, 180 changes in 200 frames, the shortfall being the
frames spent booting. This port only clears `SDMCTL`; it never takes the
interrupt vectors.

### The bug the test caught, and it was one fault wearing three faces

`snd_poll` returned early when there was nothing to play, *before* sampling the
frame counter — so `last_frame` went stale for as long as the game was silent,
and the first poll after the music started saw hundreds of frames and burned
the whole track in one call. Three of the first run's six failures were that
one line: the track was already on its second note four frames in, and
`snd_beep`'s `done += frames_since()` was satisfied immediately, so the refusal
beep ended before it was audible **while its bounded loop correctly reported
that it had terminated**.

The catch-up is capped at four frames now. Honouring a two-second gap exactly
would keep wall-clock tempo at the price of a burst of far-memory note reads
and a flurry of notes nobody hears.

The sixth failure was the test's own arithmetic: nine ticks at 18.2 Hz is 29.6
NTSC frames against 24.8 PAL ones, so "45 frames later" lands in a different
note in each region. It checks the *sequence* of distinct pitches now — 930,
90, 930 can only happen if the track advanced and the zero pair looped it back,
and it needs no arithmetic at all.

## The disk seam: SIO, and a format of our own

`src/atarisio.c`. The Device Control Block at `$0300` and `JSR $E459` — no
handler, no filesystem, no `DOS.SYS`. The one `jsr` in the file declares the
`"p"` clobber; that omission cost this project a day on the MEGA65 and was
latent in the released C128 and X16 builds.

The format is `tools/nodos.py`'s and it is the smallest thing that can honour
`core/storage.h`, which is keyed by **name**:

```
sector 1..3     the boot record -- src/boot.s
sector 4..5     the directory: 16 entries of 16 bytes
sector 6..      file data, each file contiguous
```

Contiguous on purpose: no link byte in every sector, no free map to allocate
from. **Which leaves the save**, whose name the player types. So the disk is
built with **slots** — entries whose extent is already assigned and whose name
is still empty, flagged writable. `dir_claim()` takes one on the first write to
a name it cannot find. That is the entire allocator; it cannot fragment,
because nothing is freed and every slot is the same size.

The writable bit does a second job: a write to `STRINGS.DAT` is refused by the
**format** rather than by nobody having tried it.

### The boot record

Sector 1 of an Atari disk is a descriptor, not a program: the OS takes a sector
count, a load address and an init vector out of its first six bytes, loads that
many sectors to that address, and `JSR`s to load address + 6.

What runs there is an **XEX loader**, and that is the choice worth defending. A
flat loader — read N sectors to `$3000`, jump — is less code. But this link
emits four segments and three of them are not code: two bytes at `$02E5` that
lower `MEMTOP` before `_start` reads it (see the soft-stack note in
`atari.ld`), two at `$02E0` that are the run vector, and `.rodata` down at
`$0A00`. A flat loader would have to know about all three, and the disk build
would then diverge from the file exactly where this port has already been
bitten once. Honouring the segment format means **the same `EGATREK.XEX` boots
from the disk and loads from a DOS disk**, byte for byte.

It finds the file **by name** in the directory, not at a sector it was told
about at build time — so nothing has to be patched after assembly and the
loader cannot go stale against the disk builder. `boot.ld` asserts both bounds
on its length (three sectors; below its own sector buffer at `$0900`), and
those asserts were checked by breaking them.

A failed boot paints the background **red** and halts. A machine that hangs on
the boot's own blue is indistinguishable from one that hung somewhere else,
and this project has mistaken a silent no-op for a pass more than once.

### Reading a file is not knowing its length

`plat_read` handed back the `len` bytes asked for and then stepped to the next
sector — so a reader using any chunk but 128 silently lost the tail of every
sector. `far_load()` streams in **sixty-four**, which made `OVERLAYS.BIN`
exactly half the file, in alternating chunks.

**The probe passed while this was true.** It compared the byte count, and the
count came from the *directory*, independently of what was copied: it reported
the briefing's exact length, 10,557, and that was believed and written up as
"the streamed path passes". The bug surfaced two changes later as a build-stamp
mismatch on `OVERLAYS.BIN` — a true statement about a file that was never read.

`src/nodostest.c` sums every byte it receives now, position-sensitively, and
`tools/probe_nodos.py` compares against the host file. **And the chunk is fifty
bytes, not sixty-four**: both are smaller than a sector, but 64 divides 128, so
a reader that mishandles the middle of a sector still lands on a boundary every
other chunk. 50 never lines up. Reintroducing the bug on purpose makes exactly
one of the three checks fail, with the length still reading 10,557.

### What the CIO seam taught before it was deleted

Two faults that are worth keeping even though `src/ataristorage.c` is gone:

**A failed OPEN still holds the channel.** The missing-file check passed, and
then every later operation failed with CIO `$81` — "IOCB already open" —
because IOCB 1 was still allocated to an open that had never succeeded.

**CIO status `$03` is a success, not an error.** `$01` (more follows), `$03`
(the read ended *exactly* at end of file) and `$88` (asked for more than was
left) are all successes. The driver called `$03` an error, which is invisible
until a file's length equals the buffer it is read into — and **the save record
is a fixed size read into a fixed-size buffer, so that is the normal case, not
an unlikely one.**

## The margin, and where it came from

Run `make verify`; it is the only authority. It prints **two** pools now,
because the program's data no longer shares an address space with its code:

```
verify: code $3000..$A3B5, 2379 bytes free below the stack reserve at $AD00
verify: low data $0A00..$19BB, 4027 of 5632 used, 1605 free (where DOS was)
verify: lowram $0480..$06FF, 626 of 640 used, 14 free
```

The shipping link had **62 bytes** free at its worst and 693 with DOS on the
disk. Where the rest came from is "The DOS lever, SPENT", above.

`io_buf` is 626 bytes, the port's biggest single writable object, and it lives
at **`$0480` in the OS spare area**. It could move to `lowdata` now and free
`lowram` entirely — but `lowram` competes with nothing, so there would be no
point. `core/lowmem.h` carries the annotation and expands to nothing unless a
port asks, exactly as `OVL_CODE` does; the four released ports compile
byte-identically.

**Measured, not read off a memory map.** Every Atari memory map calls
`$0480..$06FF` free, and a map is not evidence about a running machine. It was
filled and then put through a whole-file read, two hundred streamed reads and a
write, and counted back unchanged — with DOS 2.5 still resident above it.

What goes in `.lowbss` is **not zeroed at startup**, so `io_buf`'s
write-before-read property was checked at each of its three use sites.

## SAVE works — witnessed, in one session

This file used to carry an open question here, and before that a claim that
SAVE was broken. Both are closed.

`tools/savetest.py` boots the no-DOS disk, plays into the console, saves under
a player-typed name, cold-boots **in the same emulator session**, restores, and
screenshots both. The restored console matches the one before the save —
stardate, energy, shields, quadrant, sector, damaged systems. The disk's
directory afterwards reads:

```
EGATREK SAV  sector  685     625 bytes  flags 3  (slot, claimed)
```

625 is `SAVE_HDR + TREK_SAVE_SIZE` exactly, and `flags 3` means a slot that was
unclaimed when the disk was built.

**The one session is what matters.** Altirra's default disk write mode is
*virtual* read-write: the emulated drive accepts every write and the host
`.ATR` only receives them on a clean shutdown. A restore run in a **separate
process** was reading a disk the save had never reached, so the game sat at a
filename prompt for a file that was not there — which from outside looks
exactly like a hang. Three discriminators came back negative before anyone
suspected the instrument.

**One real defect came out of that hunt and is fixed.** `plat_write_all`
reported the *transfer's* status and discarded the *close's* — and the close is
where the last sector is flushed, so a write that failed to finish could report
success.

## The stubs measure the game, not themselves

`src/stubs.c` is every remaining seam with nothing behind it, and **every stub
writes through a `volatile`**. The X16's early link measured 1,564 bytes once
— the whole game, apparently, in a tenth of the space — because its stubs
returned constants, LTO proved the results unused, and the optimiser deleted
the *game* rather than the stubs. A stub that can be reasoned about measures
nothing.

Each seam is guarded by an `ATARI_HAVE_*` define the Makefile sets, so the
real driver and its stub can never both be in the link.

## The instrument

`tools/run_atari.py` drives **AltirraBridgeServer**, the headless lean build,
over its JSON socket: boot an XEX, run a fixed number of frames, screenshot.

**VBXE comes from the user's Altirra settings, not from a switch.** The server
has `--machine`, `--memory` and `--no-basic` but nothing to add a hardware
device, so `--settings=user` is what brings in the Video Board XE that
`~/.config/altirra/settings.ini` already carries. Without it the program runs,
writes to `$D640`, nothing answers, and the screen stays as the OS left it —
which reads as a driver bug. The harness checks for the device and says so.

Two things it cost to learn:

* **Stderr goes to a file, not a pipe.** Reading the pipe only until the token
  line appears leaves the server writing into a pipe nobody drains; it fills,
  the server blocks, and the run hangs with the emulator apparently alive.
* **Screenshots use `path=`, not inline.** The inline base64 form never
  returns against this build — the command reaches the server, and the SDK
  blocks on the reply for ever.

## The snapshot harness, and the question it has to answer first

`tools/session.py`. **Every Atari test until 2026-09-10 paid for a cold boot**
-- mount, cold reset, then poll `far_used` until the game has streamed
STRINGS.DAT, MUSIC.DAT and OVERLAYS.BIN into VRAM. That is the expensive part,
it is identical every time, and `savetest.py` paid for it twice in one script
because a restore needs a fresh start. Eight experiments in an afternoon cost
sixteen boots, and the third tool call in a row that times out looks exactly
like a hang -- which is how this rig came to be described as stuck.

AltirraBridge has had `STATE_SAVE`/`STATE_LOAD` the whole time.

    with Session() as s:          # boots once, snapshots as "booted"
        s.keys("RETURN")
        s.snap("title")
        ...
        s.rewind("title")         # instant

**But the harness cannot be believed until one thing is measured.** Altirra's
default disk mode is virtual read-write -- the emulated drive accepts writes
the host `.ATR` never sees, which is exactly what made this port's SAVE look
broken when it was not. So when a rewind happens, either the disk state is
inside the snapshot (a rewind un-writes the save file, and every save test
built on a rewind is measuring nothing) or it is beside it (a rewind keeps the
file, and the test is real). **Those two answers mean opposite things and the
harness cannot tell you which without being asked.**

`make run-probe-snapshot` asks, with a discriminator rather than a
confirmation: the same restore is attempted twice, once without a rewind and
once with, and the pair says which world this is.

## What is still open

* **Nothing about the disk.** It boots, it plays, it saves, it restores, and
  the image is ours to redistribute — see "The DOS lever, SPENT" and "SAVE
  works".
* ~~The load time~~ — **judged and documented, not open.** 112 seconds on a
  stock 1050, a few seconds on an emulator or a fast-SIO drive. See "Be
  patient with the first screen" at the top of this file; it is a 1050, not a
  defect.
* ~~The link is not deterministic~~ — **documented and parked**, Jamie's call
  2026-09-11: not worth pursuing unless it causes an issue. Both binaries are
  valid and pass every check. `make reproducible` reports it and "The Atari
  link" in `NOTES.md` says how far it is pinned. **The issue to watch for is a
  binary that has to be reproduced byte-for-byte** — verifying a shipped
  artefact against a rebuild is the one job this makes impossible.
* ~~A person has not played it~~ — **played 2026-09-11, Jamie: "The game plays
  great."** It found three bugs in two sittings and all three are fixed: see
  "Played, and it was silent" below. The POKEY driver has been **heard** now,
  which is what caught the third.
* ~~It is releasable and not released~~ — **RELEASED 2026-09-11 in
  [v0.13.0](../../../releases/latest)**, the first release with all five ports
  in it. The artefact is `egatrek-atari.atr` with `egatrek-atari.txt` beside
  it; `make release` builds both.

**Nothing on THE OPEN LIST touches this port any more.** The two items still
open there want a person at a keyboard on other machines: nobody has played
the Amiga (10), and colour per message has never been seen on a screen on the
MEGA65, the X16 or the Amiga (23).

## Played, and it was silent

Two faults, one sitting, neither visible to any instrument in this directory.

### The port made no sound at all

`snd_init()` ran. POKEY was configured correctly. The game called
`snd_music()`, `snd_effect()` and `snd_beep()` from a hundred sites. **Nothing
ever called `snd_poll()`**, so no note was ever started and no effect ever
advanced — the driver holds state and a frame counter, and polling is what
turns that into sound.

`c128/src/input.c` has the call, in both of its blocking loops, and has had
since the driver landed. `src/atariinput.c` is this port's own input seam,
written from scratch because the Atari scans its keyboard through the OS —
**and the one function the two ports do not share is the one that drives the
one thing they do.**

**`make run-sndtest` passed twelve checks on this.** Both video standards, the
pitch at each end of the music's range, the tempo, the loop, worst error
0.05%. It links `src/sndtest.c` against the driver and calls `snd_poll()`
*itself*. Every number was true and none of them was about the game. A seam
measured in isolation says nothing about whether anything calls it.

`make run-probe-sound` asks the other question: boot the real disk, wait out
the title screen without pressing anything, and ask Altirra what POKEY is
actually emitting. It reports the tones and fails on silence — and on a single
tone, which is a voice gated on but never advancing, a different fault with
the same symptom from outside.

Fixing it cost 452 bytes, because until then the optimiser could see that most
of the driver was unreachable.

### And then it was three octaves sharp

Jamie, one play later: *"the pitch is way too high."*

**The Atari OS takes POKEY back on every disk read.** SIO drives the serial
port from POKEY — it joins channels 3+4 as the baud generator and clocks
channel 3 at 1.79 MHz, which is `AUDCTL = $28` — and it writes the *whole*
register, so bit 4 (join 1+2) and bit 6 (clock channel 1 fast) come back
**clear**. Those two are the music voice. Without them channel 2 stops being
the high half of a 16-bit divisor and becomes an ordinary 8-bit channel on the
64 kHz clock.

Measured at the title screen, by asking Altirra for POKEY's write-side state —
`$D200-$D207` are **write-only** and reading them returns the paddle ports,
which is how the first attempt got `AUDF1 = AUDF2 = 228` for fourteen samples
while the music changed:

```
AUDCTL $28   AUDF1 6   AUDF2 12      divisor 3078 = the 290 Hz note intended
                                     sounding  2458 Hz = 63921/(2*(12+1))
```

`snd_poll()` re-asserts `AUDCTL` now — there rather than in `voice_note()`,
because a note that starts before an overlay load and is still sounding after
it would otherwise stay wrong for its whole length. After the fix: `AUDCTL
$78`, and Altirra reports 290 Hz for the same divisor.

**Confirmed by ear, 2026-09-11** — Jamie: *"the atari's pitch fix worked. i
heard it."* Which matters because the same Altirra number was what the port had
before anyone listened, and it was reporting an 8-bit channel then.

**`make run-sndtest` passed twelve checks on this too, at 0.05% worst error.**
It links `src/sndtest.c` with no storage seam at all — the rule names five
sources and not one of them can touch a disk — so SIO never ran, `AUDCTL`
stayed `$78`, and every pitch was right. The game reads an overlay off the
disk on the hot path. **Two tests measured this driver in a world it does not
run in**: one never called `snd_poll()`, the other never touched a disk.

And `make run-probe-sound` had already printed `1332, 1998, 2458, 3196 Hz`
against a 90–930 Hz tune, called it a pass, and I noted the numbers looked
high without acting. It bounds them now, and reverting the fix makes it fail.

### "Play Again? No" left a screen of garbage

Two faults stacked.

`vdc_shutdown()` handed ANTIC its playfield back **before** `main()` waited for
the farewell keypress. On the C128 that is right — the VDC picture survives
being handed back, and the comment above it said so. On this machine the
console lives in VBXE's overlay, and the ANTIC screen the OS set up at boot is
at `$BC20`, **inside the overlay window** this program pages code through all
game. So it did not reveal the console. It revealed about four thousand bytes
of the last overlay image, rendered as text.

Under that, the game was fine: waiting in `kb_waitkey` for a key nobody could
know to press, with "MISSION ENDED, CAPTAIN. HIT A KEY AND THE ATARI RESTARTS."
drawn and then switched off one line later.

And the key did not restart it. **`jmp $e477` — COLDSV, the documented XL
cold-start vector — does not cold-start this machine.** Measured: twelve
thousand frames after the key with `far_used` flat at 0 and a black screen,
against a cold reset asked of the emulator instead reaching the title screen in
under a thousand. *Why* was not determined and is not claimed here; the OS ROM
is demonstrably mapped, since the entire disk seam runs on SIOV at `$E459`.

It is replaced rather than explained. `plat_exit()` now closes the VBXE window
— MEMAC survives every kind of reset and maps VRAM over `$2000..$3FFF`, which
is where the loader is about to write — resets the hardware stack, and jumps to
**`$0706`, this port's own boot record**, which finds `EGATREK.XEX` by name and
reloads it with nothing but SIO and code we wrote.

That `$0700` is still intact after a full game is *checked*, not argued from
the linker script: `make run-probe-exit` reads the six header bytes back at the
Play Again prompt and compares them with `build/boot.bin`.

**`tools/probe_hof.py` had been typing N at that prompt for a day**, and
rewinding to a snapshot on the very next line. Its comment says "back round" —
a guess about a screen the probe never waited to see.

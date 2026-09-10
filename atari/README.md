# EGA Trek — Atari 800XL + VBXE

Fifth port. **Started 2026-09-06 with a measurement, not a driver**, because
the scope for this target ends with an instruction: *"Measure it properly
before committing: link the whole game early."*

    make early        link the game against the seams as they stand and report the budget
    make smoke        first light: the video seam and nothing else
    make run-smoke    boot it headless on AltirraSDL and screenshot it

## Where it is

**It plays.** Fifth port, booting from a DOS 2.5 disk on an Atari 800XL with
VBXE, driven through setup into the nine-panel console and through a turn.

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

`make run-storetest` builds a DOS 2.5 disk, plants files on it, boots it with
the test as `AUTORUN.SYS`, and checks the seam in both directions — six checks
on the machine plus one on the host, all passing.

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
levers that close it" — and the shipping link has **693 bytes free** — see
"The margin, and where it came from". (This line said 708 for a day after the
close-status check took 15 of them. The number is quoted twice in this file;
`make verify` is the only authority for it.)

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

### +772 with DOS, +2,282 without — MEASURED, AND NO LONGER NEEDED

The Atari's `.data`/`.bss`/`.noinit` come out of the same region as its code,
where the C128's live in a separate `lowram` at `$1300..$1C00` that does not
compete at all. That difference is about 2,330 bytes and it is the single
largest structural disadvantage this target has.

Pointing `c_writeable` at a `lowram` region of `$0700..$1FFF` takes the
overflow down by **2,282** — close to the whole of the writable data (`.data`'s
initialiser image still loads from the code region, which is the difference).

**But `$0700..$1FFF` is where Atari DOS lives, and now there is a number for
it.** A booted DOS 2.5 reports `MEMLO = $1CFC`, read straight off the machine —
so with DOS resident the region is `$1CFC..$1FFF` and the lever is worth
**772 bytes, not 2,282.**

And the disk seam wants DOS. Not for the data files, which could come off raw
sectors through the OS's own SIO vector with no DOS at all — but because
**saves are named by the player**. `ui.c` lets them type a filename, and sector
ranges have no names, so the no-DOS route needs a filesystem of its own.

**This lever is now held in reserve rather than spent.** The two overlays below
close the budget without it, so the port can keep DOS, keep player-named saves,
and ship as an ordinary XEX on the player's own DOS disk. The fork stays
recorded because it is 2,282 bytes of headroom if this ever gets tight again.

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
them, so the four released ports compile byte-identically — checked, not
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

## The disk seam: CIO, and two statuses that are not errors

`src/ataristorage.c`, about 500 bytes of driver and 1,394 of seam. `D:` through
IOCB 1, which is what `core/storage.h` names as this port's answer in its own
header. The one `jsr` in the file declares the `"p"` clobber; that omission
cost this project a day on the MEGA65 and was latent in the released C128 and
X16 builds.

`tools/atr.py` reads and writes files inside a DOS 2 disk image, because the
bridge can mount an ATR but **not** a host directory — so there was no way to
put a file where the `D:` handler could see it without writing the filesystem.
`tools/storetest.py` then builds a disk, plants the files the test reads, adds
the test itself as `AUTORUN.SYS`, boots it, and afterwards extracts the file
the program *wrote* and checks it on the host.

**That last step is the point.** `atr.py` writing sector chains and then
reading back its own is self-consistency, not evidence. DOS reading what
`atr.py` wrote, and `atr.py` reading what DOS wrote, are two independent checks
that only pass together if the format is right. The chain encoding was checked
against `DOS.SYS`'s own sectors on the same disk rather than against a manual.

Two faults, and both are the kind that ship:

**A failed OPEN still holds the channel.** The missing-file check passed, and
then every later operation failed with CIO `$81` — "IOCB already open" —
because IOCB 1 was still allocated to an open that had never succeeded.
Nothing says so until the *next* open fails.

**CIO status `$03` is a success, not an error.** There are three successful
statuses for a read, not two: `$01` (more file follows), `$03` (the last byte
of the file was read, returned when a read ends *exactly* at the end of the
file) and `$88` (the read asked for more than was left). This driver accepted
`$01` and `$88` and called `$03` an error — which is invisible until a file's
length happens to equal the buffer it is read into. Reading 19 bytes into 128
gives `$88` and passes; reading 700 into 700 gives `$03` and fails. **The save
record is a fixed size read into a fixed-size buffer, so that is not an
unlikely case, it is the normal one.** The test writes a file and reads it back
at exactly its own length for precisely this reason.

## The margin, and where it came from

The shipping link had **62 bytes** free, which is not a margin. It has about
700 now, and the whole of the difference is one buffer. Run `make verify` for
the number -- it moved by 15 the same day, when checking the close's status on
a write turned out to cost that much.

`io_buf` is 626 bytes, the port's biggest single writable object, and it now
lives at **`$0480` in the OS spare area** — the only memory on this machine
that does not compete with code. `core/lowmem.h` carries the annotation and
expands to nothing unless a port asks, exactly as `OVL_CODE` does; the four
released ports compile byte-identically.

```
verify: resident $3000..$AB4B, 693 bytes free below the window at $AE00
verify: lowram $0480..$06FF, 626 of 640 used, 14 free
```

**Measured, not read off a memory map.** Every Atari memory map calls
`$0480..$06FF` free, and a map is not evidence about a running machine.
`src/lowprobe.c` fills the region and then does what this program actually does
— a whole-file read, two hundred streamed reads, and a write that makes DOS
allocate sectors and rewrite its own VTOC — and counts what came back changed.
Zero, with DOS 2.5 resident and MEMLO at `$1CFC`.

**And the buffer's own two uses are checked separately**, because "disk I/O
does not *clobber* the region" is a different claim from "CIO can *fill* it".
`storetest` writes a file from `$0480` and reads one back into it: 625 bytes,
zero wrong. What goes in `.lowbss` is **not zeroed at startup**, so `io_buf`'s
write-before-read property was checked at each of its three use sites.

There is more where that came from if it is ever needed: `hof` is 280 bytes and
`slot` 256, and `$1CFC..$1FFF` is another 772 with DOS 2.5 — though that one
depends on which DOS the player boots, where `$0480..$06FF` does not.

## OPEN: is SAVE broken? Probably not — and the first answer here was wrong

This section said "SAVE writes a file the game cannot read back". **That claim
is retracted, before anyone acts on it.**

What was seen: the game writes `EGATREK.SAV` at the right length, the host image
shows its directory entry marked `$03` (open-for-output) where every write
`storetest` does leaves `$42`, and restoring it never came back. Three
hypotheses were eliminated by discriminator — the buffer address, the record
ending exactly on a sector boundary, and reading into low RAM — and CIO reported
`$01` on **both** the transfer and the close, with `open_live` back to 0.

**When every discriminator comes back negative, suspect the instrument.**
Altirra's default disk write mode is *virtual* read-write: the emulated drive
accepts every write and the host `.ATR` never receives them. That explains all
of it. Killing the emulator leaves a half-flushed directory sector, which is
what `$03` was. Ejecting first makes the file **vanish entirely** — which is
what it did. And the restore ran in a *separate process* against a disk the save
had never reached, so the game sat at a filename prompt for a file that was not
there, which from outside looks exactly like a hang.

**Still unconfirmed either way.** The deciding test is save-then-restore in ONE
session, and it has not completed — because every attempt pays two cold boots
and the game streams 44,170 bytes into VRAM before it draws anything. The fix
for that is `state_save`/`state_load`: boot once, snapshot the console, and
reload in a second.

**One real defect did come out of it and is fixed.** `plat_write_all` reported
the *transfer's* status and discarded the *close's* — and on a write the close
is where DOS flushes the last sector and rewrites the directory entry, so a
write that failed to finish could report success. The close's status is checked
now, and kept in `plat_dbg_close` for when it is not.

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

## What is still open

* **SAVE, above** — unverified rather than broken, and the test that settles it
  needs the harness to stop paying for a cold boot every time.
* **Playing it properly.** A turn is not a game: nothing has fought, docked,
  landed on a planet or reached the hall of fame.
* **The load time.** `OVERLAYS.BIN` streams through CIO at boot and the packing
  cut it by 40%, but it has not been timed against a real 1050.
* A release bundle. What ships is the XEX and the data files for the player's
  own DOS disk — `tools/atr.py` builds the test disk but a DOS is not ours to
  redistribute.
* **The DOS fork above**, which decides whether 2,282 bytes are available.
* Whether `front` — which grows with the save record and cannot be split —
  becomes the ceiling once the arithmetic is closed.

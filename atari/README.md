# EGA Trek — Atari 800XL + VBXE

Fifth port. **Started 2026-09-06 with a measurement, not a driver**, because
the scope for this target ends with an instruction: *"Measure it properly
before committing: link the whole game early."*

    make early        link the game against the seams as they stand and report the budget
    make smoke        first light: the video seam and nothing else
    make run-smoke    boot it headless on AltirraSDL and screenshot it

## Where it is

**Video and input are built and running.**

`make run-smoke` boots an XEX on AltirraSDL's headless bridge and comes back
with a picture: eighty columns, twenty-five rows, all sixteen EGA colours on
their own indices, and the console's box-drawing set rendering as a frame with
its tees and cross joining. That is questions 1–4 of first light answered in
one screenshot.

`make run-keyecho` injects thirteen keys over the same bridge and screenshots
what the input seam returned. All thirteen came back as the values
`c128/src/input.h` names — letters uppercased, RETURN 13, ESC 27, DELETE 20.

Still `src/stubs.c`: sound, storage, far memory and the overlay loader.

## The budget, and it moved

`make early` is the authority and re-measures on every build. **It no longer
measures a fully stubbed game** — as each driver lands its stubs drop out of
`src/stubs.c` and the link gets the real one, so the number converges on the
truth instead of being corrected by hand at the end. The header line says
which seams are real.

Reading on 2026-09-09, with video real and five seams stubbed:

```
address space   $3000..$BFFF        36864     4K MEMAC window, not 8K
overlay window                       4608
for resident                        32256

RESIDENT IS OVER BY 3923 BYTES
```

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

### +2,282 — writable data below the window. MEASURED, AND IT IS NOT FREE

The Atari's `.data`/`.bss`/`.noinit` come out of the same region as its code,
where the C128's live in a separate `lowram` at `$1300..$1C00` that does not
compete at all. That difference is about 2,330 bytes and it is the single
largest structural disadvantage this target has.

Pointing `c_writeable` at a `lowram` region of `$0700..$1FFF` takes the
overflow from 3,923 to **1,641** — worth 2,282, close to the whole of the
writable data (`.data`'s initialiser image still loads from the code region,
which is the difference).

**But `$0700..$1FFF` is where Atari DOS lives.** DOS 2.5 puts MEMLO at about
`$1F00`, so with DOS resident there is nothing down there — and the storage
seam wants DOS, because `D:NAME.EXT` through CIO is how a file gets read on
this machine. Taking this lever means not having DOS, which means either
sector-level SIO of our own or **appending the data files to the XEX as extra
segments that load through the MEMAC window straight into VRAM.** That second
one is attractive and unproven; it also does not answer SAVE, which has to
write. **This is a real fork and it is not decided.**

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

* The five remaining seams, and what each costs beyond its driver.
* **The DOS fork above**, which decides whether 2,282 bytes are available.
* Whether `front` — which grows with the save record and cannot be split —
  becomes the ceiling once the arithmetic is closed.

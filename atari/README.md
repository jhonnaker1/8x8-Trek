# EGA Trek — Atari 800XL + VBXE

Fifth port. **Started 2026-09-06 with a measurement, not a driver**, because
the scope for this target ends with an instruction: *"Measure it properly
before committing: link the whole game early."*

    make early        link the game against stubbed seams and report the budget

## Is it viable? Yes — but only if `run_turn` can be split

**`make early` re-measures this on every build and is the authority; the block
below is the reading on 2026-09-08.** It has already drifted once -- it said
SHORT BY 3754 when first written on 2026-09-06 and shared code has moved since,
so treat the figure as a position, not a constant.

```
address space   $3000..$BFFF        36864   4K MEMAC window, not 8K
overlay window                       4608
writable data (.data/.bss/.noinit)   2329
leaves for code+rodata              29927

the game's code+rodata, seams stubbed ~29240
the driver layer it still needs       4539   measured on the X16
                                    -------
                                             SHORT BY ABOUT 3850
```

**That last digit is not stable and never will be.** It read 3754 on
2026-09-06, 3857 on the morning of 2026-09-08 and 3851 that afternoon --
regenerating the music alone moves it, because `music_data.c` is compiled in.
Quote the magnitude, run `make early` for the number.

**The 8K→4K window is the first lever and it is already spent.** With VBXE's
default 8K window the program starts at `$4000` and is 3,358 bytes short before
drivers; at 4K it starts at `$3000`, which buys 4,096 bytes and is what makes
the number above merely difficult instead of hopeless.

### Why it is short, and it is not the code

The Atari's resident profile is *the same as the C128's*, function for
function:

```
              Atari      C128
run_turn       6434      6917
main           4682      5018
trek_move_warp 1002       913
report_move    1143       719
```

So this is not a target that generates worse code. **The C128 wins on
structure**: its writable data lives in a separate `lowram` region at
`$1300..$1C00` that does not compete with code at all, while the Atari's
`.data`/`.bss`/`.noinit` come out of the same space. That difference alone is
about 2,330 of it.

### What could close it, in order of what they cost

1. **Split `msgs` and `planet`** so the overlay window returns to 4,096 —
   **+512**, and cheap.
2. **Split `run_turn`.** It is 6,434 bytes of LTO-merged turn engine, and parts
   of it are genuinely cold: rare events, the death pod, black holes,
   supernovae, the reports each of those writes. **This is the only remaining
   candidate large enough to matter** — after `main` and `run_turn` the biggest
   resident function is 1,143 bytes, and everything at that size is drawn or
   run every turn, so paging it means a disk load per turn.

   **What "split `run_turn`" actually means, established 2026-09-07:** the
   function is only **68 source lines**. Its size is LTO inlining
   `trek_run_events` (2,299 bytes) and `trek_enemy_turn` (1,939) into it, so
   the work is paging one of *those*, not carving up `run_turn`.

   **And `trek_run_events` is already paged, conditionally** — it is guarded by
   `trek_events_due()`, the same predicate `run_turn` loads `OVL_EVENTS` on.
   That pairing is the only reason it is safe, and it is documented in
   `tools/overlay_check.py`'s `PAIRED` table because rule 4's check could not
   see it.

### ANSWERED 2026-09-08: it splits, and it was BUILT to find out

Not reasoned about — performed on the C128 in a throwaway worktree, because
this project has been wrong before about which split pays (the fourth overlay
pass named candidates that cost 863 bytes instead of saving any).

**The candidate is `trek_enemy_turn` and its damage chain, and the call graph
is unusually clean:**

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

Everything except `trek_laser_damage` is reachable from exactly one place. One
entry point, one caller: rule 3 satisfied by construction, and rule 4 by the
same `PAIRED` pattern the events overlay already uses.

**MEASURED, as a twelfth overlay on the C128:**

```
resident text   33,571 -> 30,051      3,520 BYTES FREED
.ovl_enemy       3,832 of 4,094       fits, 262 spare
make verify      all rules pass, including rule 4 with the pairing declared
```

And it **plays**: driven through setup into the console, `SHUP` ran a full turn
through the newly paged code — "ENGINEERING: SHIELDS UP", energy 5000 to 4950 —
with no crash. The rule-4 check caught the missing pairing first, which is
exactly what it is for.

**3,520 against a shortfall of about 3,850**, plus the +512 from item 1, closes
it with room to spare.

### What it costs, and why the Atari can afford what the C128 cannot

`run_turn` calls the enemy turn on essentially every command, so this is a
window swap on the hot path — the thing this port's notes warn about. Two
things soften it:

* **`ovl_load` is idempotent.** On a turn where nothing else touched the
  window, `OVL_ENEMY` is already there and the load costs nothing. The real
  cost is on commands that swap the window first — the seven `load_cmds()`
  sites and the panels — where it doubles one load into two.
* **On this target the images need not come off disk.** VBXE has 512K of VRAM
  and MEMAC window A to reach it, so overlay images can live there and arrive
  as a memory copy, exactly as the MEGA65's arrive from banked RAM by DMAgic
  in microseconds. That is what makes a per-turn swap affordable here and
  ruinous on a 1541.

**So this is not committed to the C128**, where it would buy nothing and cost a
disk read on the hot path. It is recorded as measured and available.
3. Writable data into VBXE VRAM beyond the message log — but only for things
   already reached through a seam. `io_buf` is wanted as one contiguous blob
   and cannot move; that is settled and recorded for the C128.

### The recommendation

**Do not start with a video driver.** The decision this port turns on is
whether `run_turn` splits, and that is measurable *today*, on the C128, with no
Atari-specific code written — and it would give every other port back a
kilobyte or two as a side effect. If it splits, this target is a `vdc.c`
rewrite like the MEGA65 and the X16. If it does not, the honest options are a
reduced feature set here or dropping the target.

## The early-link numbers

`make early` reports the current position and re-measures as code moves. With
VBXE's default 8K window it read:

```
address space   $4000..$BFFF        32768 bytes
overlay window                       4608 bytes
for resident                        28160 bytes

RESIDENT IS OVER BY 3358 BYTES
```

That is with **every seam stubbed** — no video, no input, no storage, no far
memory, no sound — so the real gap is larger than 3,358. It is also with the
C128's exact overlay split already applied: eleven overlays, the same functions
in the same windows.

**Without overlays at all the game is about 61,000 bytes of 6502 code**, against
32,768 of address space. The C128 fits the same game in 37,823 bytes, so this
target is roughly 5K short before anything platform-specific is added, and
about a third of the game has to be paged where the C128 pages a quarter.

**This is the tightest target in the project** and it was worth knowing on day
one rather than after a video driver.

## Why $4000, and why the space is short

VBXE's MEMAC window maps its VRAM **into the 6502 address space** — uno's
`vbxevid.c` opens it with `MEMAC_CONTROL = 0x29`, "window at $2000, CPU enable,
8K" — so `$2000..$3FFF` belongs to video memory, not to the program. `$C000` up
is the XL's OS ROM. That leaves `$4000..$BFFF`.

Uno found this **by crashing into it**: a build that loaded at `$2000` had
`vbxe_init()` map the window over its own code and execution fell into VRAM,
`PC=$25ED illegal=1`. The emulator's Program Error dialog is a long way from
where the mistake was, which is why `atari.ld` says so at length.

The window size is configurable (`bits 0-1 = 4K << n`), so a 4K window at
`$2000` would give `$3000..$BFFF` = 36,864 — 4K more resident, at the cost of a
4K VRAM window instead of 8K. **That is the first thing to try** if the paging
work proves harder than the arithmetic suggests.

## What is already known, and what is not

**Known, measured before this port existed:**

* The core compiles for this target clean — `trek.c`, `planet.c`, `hof.c`,
  `serial.c`, no shims.
* **It is a TEXT target, not a bitmap one.** VBXE has a real
  character-plus-attribute mode: 80 columns, per-cell foreground *and*
  background from a 1024-colour palette. So this is a `vdc.c` rewrite like the
  MEGA65 and X16, not the font-and-blitter work the Amiga needed. The project
  had this in the wrong cost class for two weeks.
* **25 rows work.** Measured on AltirraSDL with a test program filling every
  row: ROW 00 through ROW 24, 80 columns. The console fits as designed.
* Far memory and the message log have an obvious home in VBXE's 512K of VRAM.

**Two traps the reference already documents**, each found the hard way in uno:

* The FX core exposes CSEL/PSEL/CR/CG/CB directly at `$D644-$D648`. The older
  VBXE manual documents those addresses as an MSEL/MB0-3 commit protocol;
  following the manual scrambles the palette so text renders in a colour
  indistinguishable from its background — which looks like blank glyphs, not
  like a palette bug.
* MEMAC window A is `$D65E/$D65F`. The v1.0-beta manual's `MA_CPU` at `$D64C`
  **does not exist** on the FX core: Altirra's register switch has no case for
  it, so writes are silently dropped and the window never opens.

**Not known yet:** how much of the shortfall a twelfth and thirteenth overlay
can absorb, and whether `front` — which grows with the save record and cannot
be split — becomes the ceiling. (This paragraph quoted "3,358" until
2026-09-08, which was the *8K-window* figure from before that lever was spent.
`make early` is the authority; do not copy its output into prose.)

## The stubs measure the game, not themselves

`src/stubs.c` is every seam with nothing behind it, and **every stub writes
through a `volatile`**. The X16's early link measured 1,564 bytes once — the
whole game, apparently, in a tenth of the space — because its stubs returned
constants, LTO proved the results unused, and the optimiser deleted the *game*
rather than the stubs. A stub that can be reasoned about measures nothing.

## The instrument

AltirraSDL with the AltirraBridge: a socket carrying screenshots, memory reads
and writes, CPU state, breakpoints and input injection. The profile wants
XL / NTSC / 1088K / VBXE, and **BASIC disabled**, since the program runs up to
`$BFFF`.

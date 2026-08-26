# EGA Trek remake — project notes

Status as of 2026-08-17. The C128-VDC port generates a galaxy, moves the
ship through it, and fires lasers. Enemies do not shoot back yet, and there
is no damage, docking, supply or torpedo handling.

Constants are no longer uniformly provisional: laser damage, enemy hit
points, the three energy pools, travel costs, torpedo count and the scoring
rubric are measured against the original, several of them read directly out
of its memory. See MEASURED.md, which records the wrong turns as well as the
answers.

## What this is

A remake of **EGA Trek** (Nels Anderson, 1988–1992, shareware), following the same
architecture as the [`commodore-uno`](../commodore-uno) repo: one byte-identical
shared core plus a per-platform video/sound/input layer.

## Decisions made

### Platform order: C128-VDC → Amiga (DOS-EGA is not a build target)

> **Superseded 2026-08-23 as to what comes after the C128.** The order is now
> C128 -> MEGA65 -> (X16, F256, CoCo 3) -> Amiga + VBXE together; see item 8
> under *Open questions* for the reasoning. Everything below about the C128
> being first, and about DOS-EGA not being a build target, still stands.

**The original `EGATREK.EXE` is a reference oracle, not something we're remaking.**
The DOS release already exists (mode 10h, 640×350×16) — no Watcom build of our
own core is planned for DOS. Instead, `EGATREK_unpacked.exe` runs live in
DOSBox-X whenever a rule, constant, or piece of layout needs checking:
breakpoint it in the DOSBox-X debugger to confirm real constants (laser
falloff, no-damage threshold, boarding gates, scoring weights), or just run it
and grab a screenshot of a panel/graphic to check against.
`commodore-uno/dos/src/egavid.c` (mode 10h behind `gfx.h`) stays noted below in
case a DOS build is ever wanted, but it's not on the current path.

**C128-VDC is the first port target.** EGA Trek's console is an 80×25 character
grid at an 8×14 cell — that is EGA text geometry, and the panels were laid out
on it. The VDC's 80×25 maps **1:1**; only per-cell pixel detail is lost (8×8 vs
8×14), not layout.

The VDC and EGA carry the same sixteen colours, but **not at the same indices**
— EGA/CGA is `I R G B` (bit3 intensity), the VDC is `R G B I` (bit0 intensity),
so the conversion is a 4-bit rotate left. Fifteen of sixteen are then exact;
EGA 6 is brown only because the CGA palette special-cases it, and the VDC
renders olive instead. That affects the galaxy chart's base highlight, which
the manual calls orange (l.289). Implemented in `c128/src/egavdc.h` and proven
by `c128/test/test_egavdc.c` (`make test`).

Feeding EGA indices straight to the VDC would not be a subtle bug — every
colour still renders, just as the wrong hue. `commodore-uno/c128/src/vdc.h`
documents shipping exactly that once ("only shades of blue and green, no red
or yellow"). `commodore-uno/c128/src/vdc.c` already has the driver, charset
redefinition and attribute handling.

VDC caveat: attributes carry per-cell *foreground* only; background is global.
EGA Trek has coloured panel fills. Fix is the reverse-video bit (bit 6) — ink
renders in the global background colour, paper in the attribute colour. Costs the
ability to put arbitrarily-coloured text on a coloured panel.

**Amiga is the second port target.** 68000, real bitmap, best shot at
reproducing the actual EGA artwork (ship viewer, analogue gauges). Display
design worked out below.

**Atari VBXE is now also first-tier — position in the order not yet decided.**
Its HR overlay mode is 640×200 in 16 colours, the same spec as the Amiga target
(see the VBXE section below). It was previously filed as "workable, needs a
paged UI" on the mistaken belief that VBXE bitmap maxed at 336 pixels wide;
that was true only of the SR mode Uno implemented.

### Amiga display design

Baseline is OCS/ECS (68000, Kickstart 2.0+), matching `commodore-uno/amiga`.

**Where the Amiga matches EGA exactly.** Hires is 640 wide with a 4-bitplane
cap — 640 pixels, 16 colours, identical to mode 10h on both counts. The
palette is the happy surprise: EGA mode 10h drives **2 bits per gun**, so every
level is 0, ⅓, ⅔ or 1 — `$0`, `$5`, `$A`, `$F` in the Amiga's 4-bit-per-gun
colour registers. Every EGA colour is **exactly** representable, not
approximated. That matters because colour carries real information here
(manual l.272: battleships light blue, command ships red, scouts purple,
supply green; Mongol quadrants highlighted red, bases orange).

**Where it doesn't: vertical resolution.** 350 lines is the problem. Options:

| Mode | Font | Result |
|---|---|---|
| NTSC 640×200 | 8×8 | 80×25 in exactly 200 lines — perfect fit, coarsest text |
| PAL 640×256 | 8×10 | 80×25 in 250 lines — fills the screen, but needs a custom font (see below) |
| PAL 640×512 interlaced | 8×14 | a literal 640×350 screen — *exact* EGA geometry, but flickers |

Same trade as the VDC argument above: layout maps 1:1 in character terms, only
per-cell pixel detail is lost. Reject the interlaced option as the default —
this UI is wall-to-wall high-contrast panel borders, the worst case for
interlace flicker on a 1084. Keep it behind a flag if a purist build is ever
wanted.

**What the Amiga adds over the EGA original:**

1. **No grid constraint at all** — the big one, and where this port diverges
   from the C128 leg. The VDC port is locked to an 80×25 cell grid; the Amiga
   is a bitmap, so panel borders, the ship viewer, the Dept. of Space roundel
   and the bar gauges can sit on arbitrary pixel coordinates. Lay the nine
   panels out in true pixels and fit text inside each, rather than tracing onto
   cells.
2. **Copper** — the console is naturally three horizontal bands of panels.
   Reload the palette per band and each band gets its own 16 colours. This
   *exceeds* the original, which is stuck with one 16 for the whole screen.
3. **Blitter** for panel fills and viewer art (`RectFill`/`BltBitMap` —
   `commodore-uno/amiga/src/gfx.c` already uses exactly this pattern, and
   graphics.library handles the planar framebuffer, unlike the ST port's
   hand-rolled bitplane masking).
4. **Sprites** — 8 free hardware sprites for the cursor and blinking alert
   indicators.
5. **Paula** — 4-channel sampled audio against the original's PC speaker.
   Biggest single upgrade, and nothing to reproduce since the original had
   almost none.
6. Smooth gauge animation, and a real transition on the alternating main viewer
   instead of a hard cut.

### Amiga tooling: amiga_mcp (noted 2026-08-19, not yet used)

github.com/geekychris/amiga_mcp -- "Amiga DevBench". Worth having on record
before the Amiga leg starts, because it answers two questions NOTES.md has
been carrying unanswered.

**Toolchain: already solved, and NOT via their Docker.**
`~/amiga-toolchain/bin/m68k-amigaos-gcc` is installed (bebbo's GCC 6.5.0b),
`~/vbcc` too, and `commodore-uno/amiga/Makefile` already builds with it. So
the Docker requirement -- the heaviest dependency amiga_mcp has -- does not
apply to us. Use the local toolchain and take only the dev loop.

**Dev loop.** FS-UAE (primary, with an optional patched build carrying an HTTP
debugger), Amiberry, or real hardware over TCP. Screenshots with
planar-to-chunky conversion -- not a trivial step on a bitplanar machine --
plus memory read/write, registers, source-level breakpoints and stepping,
Copper list decoding, and file transfer. That is well beyond what the C128 leg
has.

**How it differs from the VICE MCP server**, which is worth saying because the
lesson from that one was "check what is underneath, it may be a thin wrapper
over something the emulator already does". This is not that. It ships an
on-Amiga daemon (`amiga-bridge`) routing messages over AmigaOS MsgPort IPC,
and a patched FS-UAE. There is real content here; reimplementing it is not the
obvious move the way it was for VICE.

**Input injection should reach us, and there is evidence rather than hope.**
This mattered because of what happened on the C128 on 2026-08-19: our
`input.c` scans the CIA1 matrix behind the KERNAL's back, VICE's keyboard feed
fills the KERNAL buffer, and scripted input therefore could not be made to
work at all.

The Amiga leg will not have that problem if it follows the Uno precedent, and
`commodore-uno/amiga` settles what that precedent is: input goes through
`con_getkey()` in `amigacon.c`, which reads **console.device** via Intuition.
Not raw hardware. Better still, its Makefile feeds the same `src/input.c` to
BOTH the console build and the bitmap-graphics build, so going to a custom
screen did not push it off the OS input path.

Follow that and the Amiga gets the automated dev loop the C128 does not.

**Practical state, all confirmed on this machine 2026-08-19:** Amiberry is
installed at `/Applications/Amiberry.app`, FS-UAE is at `~/FS-UAE` and
`~/FS-UAE-Silicon`, and real Kickstart ROMs (47.115) are already in
`~/FS-UAE-Silicon/Kickstarts` -- so the licensing question is settled too. The
only caveat left is that amiga_mcp installs by `curl | bash`; clone and read
it first.

### Amiga PAL/NTSC: one binary

**One binary, not two.** The difference is a layout table, not code, and two
binaries would be strange for an Amiga release. Detection is one field read:

```c
#include <graphics/gfxbase.h>
/* GfxBase->DisplayFlags: NTSC=1, GENLOC=2, PAL=4, TODA_SAFE=8 */
int is_pal = (GfxBase->DisplayFlags & PAL) != 0;
```

`DisplayFlags` reflects the **booted** mode, not the Agnus type — so a
PAL-capable ECS/AGA machine booted NTSC correctly reports NTSC.
`SysBase->VBlankFrequency` (50 or 60) is a decent cross-check.

**The font constraint that decides the layout.** `commodore-uno/amiga/src/gfx.c:47-55`
forces topaz 8 explicitly, and the comment there is the reason: the UI lays
text on 8-pixel cells, and a screen's *default* font is **not** guaranteed to
advance 8px — on their A1200/KS3.1 it advanced wider and numbers placed at a
computed `+N*8` offset overlapped the label before them. topaz 8 is
definitionally 8×8 and always in ROM.

There is no ROM 8×10 font. An 8×10 means drawing ~96 glyphs plus generator
tooling (precedented — the ST port's `tools/genfont.py` exists because its ROM
font is only reachable through Line-A/VDI — but real work), against 8×8 being
free, guaranteed present, and zero bytes in the binary. **So: topaz 8×8
everywhere.** One font, one text-layout table, identical text metrics on both
standards.

That makes **640×200 the frame** (exact NTSC fit), with PAL's extra 56 lines as
slack, taken in two stages:

- **v1 — don't detect at all.** Open a 200-line screen unconditionally. PAL
  displays it in the top of a 256-line raster with a border below; that reads
  as normal on an Amiga, and plenty of NTSC-authored software shipped exactly
  so. Zero branching.
- **v2 — detect, and hand PAL's 56 lines to the main viewer panel.** That's the
  elastic one: it's pixel art, and per the manual (l.294–306) it alternates the
  external ship view with graphical function readouts, so it's the panel that
  most rewards the room. The 8-row scan/chart grids and the 12-system status
  list are fixed-height at 8×8 and should not move.

v1 → v2 is a table edit, not a rewrite.

**Timing gotcha:** PAL is 50Hz, NTSC 60Hz, so anything paced by counting
vblanks runs 20% faster on NTSC. The *logic* is safe — stardate advance is
driven by player actions, not wall clock — but gauge sweeps and the viewer's
alternation need frame-rate-independent pacing, or accept the difference as
most era ports did.

**Decision:** 640×200 hires, non-interlaced, 16 colours programmed to the exact
EGA values, topaz 8×8 throughout, one binary for PAL and NTSC. Close enough
that side-by-side against DOSBox-X it reads as the same console — chunkier
text, identical colours, better artwork in the viewer and gauge panels,
dramatically better sound.

(AGA would allow 8 bitplanes at 640 wide → 256 colours, but the original only
ever had 16, so it buys nothing here. Not worth splitting the target.)

### Atari VBXE: HR mode makes it a first-tier target

**Verified against Altirra's `vbxe.cpp`**, per the Uno README's own lesson that
for this hardware the emulator source *is* the specification. Source mirror:
`github.com/irismessage/altirra`, file `src/Altirra/source/vbxe.cpp`.

The `commodore-uno` VBXE work implemented two paths — an 80-column
char+attribute text overlay (`vbxevid.c`) and an **SR** bitmap overlay
(`vbxebmp.c`, 320×192 8bpp). Neither is what this game wants: text mode has no
pixel art, and SR is half of EGA's width. But VBXE has a **third** overlay mode
that Uno never used.

**HR (high resolution) exists and is 640×N in 16 colours.** Three pieces of
evidence:

1. `kOvModeTable[3][4]`, indexed `[(xdl1 & 3) - 1][(xdl2 >> 4) & 3]`. With GMON
   (`xdl1 & 3 == 2`) the column selects `0=SR, 1=HR, 2=LR, 3=Disabled`.
2. `RenderOverlayHR()` unpacks one byte into two pixels
   (`b0 >> 4`, `b0 & 15`) — **4bpp, 16 colours**, four pixels per colour clock.
3. `kBounds[3][2]` (half-colour-clocks): Narrow 64–192 = 128cc, Normal 48–208 =
   160cc, Wide 44–212 = 168cc. At 4 px/cc → **512 / 640 / 672**. These are
   exactly the widths `vbxevid.h` records for *text*, because text is also
   4 px/cc — the "pixel modes go 256/320/336" note in that header describes
   **SR** (2 px/cc), which is the only bitmap mode Uno implemented.

So **HR Normal = 640 wide, 16 colours** — mode 10h's exact width and colour
depth. EGA is planar and HR is packed nibbles, but the display spec is the same.

**Vertical** is XDL-defined via RPTL with no 192 cap in the emulator source; the
bound is the Atari raster (~240 with overscan). 25 rows × 8 = 200 lines fits.

**Delta from Uno's working SR driver** — smaller than it looks:

- `vbmp_init()` sets `entry[1] = 0x08` → `(0x08>>4)&3 == 0` → SR.
  **`0x18` selects HR.**
- Pixel addressing becomes nibble-packed (2 px/byte) instead of byte-per-pixel.
- `OVSTEP` **stays 320** — 640 px × ½ byte = 320 bytes/row, same as SR's 320×1.
- 640×200×4bpp = **64,000 bytes**, still under `$10000`, so Uno's fast 16-bit
  addressing survives — but the XDL must move off `$0000` (it's ~22 bytes).
- Palette drops to 16 entries from the selected overlay palette, still 24-bit
  each — so exact EGA colours, *more* precisely than the Amiga's 4-bit guns.

**Verdict:** same 640×200×16 spec as the Amiga target, linear framebuffer, no
attribute constraints, and a blitter. It beats the C128 VDC outright because it
gets 640 width *and* pixel art. The real constraint is not the display but the
**1.79MHz 6502 pushing pixels through an 8K banked MEMAC window** — the Uno
README records a `long` divide per byte turning a screen clear into ~600 frames.
Fine for a turn-based console with static panels and localised updates; would
not be for an action game.

Carried-over traps already solved in Uno's driver: `MEMAC_CONTROL` (`$D65E`) +
`MEMAC_BANK_SEL` (`$D65F`), *not* the documented-but-nonexistent `MA_CPU`
(`$D64C`); and the ROM font being indexed in Atari internal screen-code order.

### One core everywhere — the 8-bit ports are NOT Super Star Trek

Super Star Trek is **calibration material, not a target**. Reasons:

- The EGA-Trek-over-SST increment is nearly free in RAM: cloaking is a bit per
  enemy, black holes/supernovas are cell types in a map that already costs a byte
  per cell, boarding is a couple of flags, 12 systems vs 8 devices is 4 bytes.
- What is expensive on 8-bits — the 80-column console, message ring buffer, and
  math — does not get cheaper under SST, which has the identical 8×8-of-8×8
  galaxy and the same distance calculations.
- Constants get verified **once** on DOS and propagate through the shared core.
  An SST-based 8-bit branch would drift by construction.
- Uno's core is only 325 of 1,945 lines (17%); the platform layer is 83%. Uno
  builds land at 13–19KB. There is headroom.

On 40-column machines the *presentation* will likely end up SST-shaped (scrolling
text reports, paged panels) since the nine-panel console will not fit. **Vary
presentation freely; never vary logic.**

VIC-20 and TI-99 may force a reduced build (the Uno TI-99 port already outgrew the
8K cart window). Handle with **build-time feature switches on the one core**
(compile out planets, death ray, Vandals) — not a different game.

### Core portability rules

Even though DOS does not force it, the core obeys 8-bit rules from line one:

- No `float`, `double`, `malloc`, `long`. (Uno's core has none of these, which is
  why `game.c` is byte-identical across c64/c128/dos/amiga.)
- **Explicit-width types** (`uint16_t`), never bare `int` — `int` is 16-bit under
  cc65 and Watcom real mode but 32-bit on the Amiga's 68000. Energy caps at 2500,
  so these values are wider than a byte.
- 8.8 fixed point. **No runtime `sqrt` needed**: `dx`,`dy` are both 0..7, so
  precompute a 64-entry distance table indexed `(dy<<3)|dx` (max √98 ≈ 9.90).
- An arctangent table is still needed for EGA Trek's bearing readout (0° = right).
- Budget for 68000 struct alignment padding on the Amiga leg — the Uno README
  documents having to pad and reorder `Card` for exactly this.

Estimated game state: ~1.5KB (64-quadrant chart packed 1–2 bytes each, 8×8 sector
map, ~12 enemies at ~6 bytes, ship systems, and a message ring buffer that
dominates). Comfortable even on a stock C64.

## Toolchain (all present on this machine)

| Need | Location |
|---|---|
| Open Watcom (native arm64) | `~/dos-toolchain/armo64/` — `wcc`, `wcl`, `wcc386`, `owcc` (not currently needed — no DOS build planned) |
| DOSBox-X | `/Applications/dosbox-x.app` — runs the original `EGATREK.EXE`/`EGATREK_unpacked.exe` as reference oracle |
| cc65 / acme / xa | on `PATH` |
| VICE | `x64sc`, `petcat` on `PATH` |
| exomizer | on `PATH` |

## Reference material

Everything in [`reference/`](reference):

- `manual.txt` — the full 39KB `EGATREK.DOC`, control codes stripped. Authoritative
  for damage thresholds, energy economy, repair multipliers, command syntax.
- `EGATREK.REF` — command quick-reference card.
- `EGATREK.EXE` — original, LZEXE v0.91 packed.
- `EGATREK_unpacked.exe` — unpacked with `unlzexe.c` (94,855 → 193,760 bytes).
  The game is **Turbo Pascal** (Borland 1987/88 runtime).
- `strings.txt` — extracted strings.
- `console_screenshot.jpg` — the in-game nine-panel console. 320×175, i.e. a
  half-scale capture of the real 640×350 — so every coordinate in it doubles to
  a true mode 10h pixel position. Usable directly as the layout source.
- `combat-model.md` — combat math from the 1978 ancestor.
- `sst2k/` — clone of ESR's super-star-trek (gitlab.com/esr/super-star-trek),
  **BSD licensed** and so compatible with this repo's MIT. `historic/c-version/`
  is ESR's translation of the UT FORTRAN version, which is the closest readable
  thing to what Anderson actually ported. THIS is the ancestor, not the BASIC
  listing below — see MEASURED.md. Use it to generate hypotheses, then confirm
  them against the disassembly or the running game; Anderson changed formulas,
  and the laser falloff proves it.
- `superstartrek.bas` — the 1978 listing itself (`SUPER STARTREK - MAY 16,1978`).
  Its own header carries the credit chain: original by **Mike Mayfield**;
  modified version published in DEC's *101 BASIC Games* by **Dave Ahl**;
  reworked and debugged by **Bob Leedom** (Westinghouse, April & December
  1974); converted to Microsoft 8K BASIC by **John Gorders**, 16 March 1978.
  Attributed in the README — this project joins that chain rather than
  starting one, and Anderson's own manual says he did the same thing (found
  it on a DEC System 10 in 1974, then ported it to the ZX81, Apple ][, Prime
  50-series and MS-DOS himself).
- `unlzexe.c` — from github.com/mywave82/unlzexe.

Sources: Internet Archive item `EGATrek` (holds every release 1988–3.0), and
`coding-horror/basic-computer-games` for the BASIC listing.

## Mechanics found in the binary that the manual never mentions

Extracted from `strings.txt`. Nobody working from the manual alone would know to
implement these:

- **Boarding parties** seize the lasers, EnTorp control, and engineering (blocking
  shield raising)
- **Plasma bolts** — enemy weapon with its own countermeasure ("raising plasma
  bolt shield"), can fail to detonate, can kill several Mongols at once
- **Vandal cloaking device**, triggered when fired upon
- **Long-range tractor beam** dragging the ship to another quadrant
- **Scanner jamming**, distinct from scanner damage
- **Black holes** — throw the ship to a random quadrant; torpedoes sucked in or
  deflected
- **Supernovas** — blow the ship to another quadrant; a torpedo into a star can
  trigger a nova that kills a Mongol
- **Defective energium crystals** that damage main energy instead of refilling it
- **Self-destruct password**
- Landing party casualties, settler evacuation, Mongol supply stations on planets
- Friendly fire on bases

Also recovered: exact menu text for the energy-divert dialog, both scanner
legends, and the briefing pages with the Class IX spec table.

**Note:** the message prose is copyrightable. Use the extracted catalogue as a
checklist of *which situations need a message*, not as text to copy.

## Done

- `git init` + public repo: **github.com/jhonnaker1/8x8-Trek**, MIT.
  `reference/` is gitignored — the shareware terms require any distributed copy
  to be complete and unmodified, and ours is neither.
- C128-VDC skeleton stood up: `core/ega.h`, `c128/src/{vdc,egavdc,layout,main}`,
  builds to 2,353 bytes with cl65.
- EGA→VDC colour mapping derived, implemented and proven (`make test`).
- **Milestone 1 seen running in VICE (2026-08-15).** All ten panels draw with
  titles inset in the top border, and the sixteen-colour ramp renders. Both
  things flagged as likely-wrong turned out right: the hand-derived PETSCII
  box-drawing screen codes in `layout.h` are correct, and the colour mapping
  holds on emulated hardware.
- **Fixed: panel titles rendered as blank space.** cc65 translates string
  literals to PETSCII for Commodore targets, so an uppercase `A` in the source
  reaches the driver as PETSCII `$C1` (193), not ASCII `$41` — confirmed in the
  linked binary, where `"6=BROWN"` assembles to `36 3d c2 d2 cf d7 ce`.
  `ascii_to_screencode` covered 32-63, 64-95 and 97-122, so every letter fell
  through to its `return 32` and became a space, while digits and punctuation —
  which PETSCII leaves at their ASCII values — came through fine. The screen
  read `0-15` where `EGA PALETTE 0-15` was written. One added range in
  `vdc.c` fixes it.

  **This is the silent-failure class again, and the existing test could not
  have caught it.** `test_egavdc.c` works because `egavdc.h` is a header the
  native compiler can include; `ascii_to_screencode` sits in `vdc.c` next to
  the `$D600` pokes, out of reach of `cc`. Worth lifting into its own header so
  a native test can feed it PETSCII `$C1`-`$DA` and assert screen codes 1-26.

- **Milestone 2: the shared core (2026-08-15, `a61ef60`).** Galaxy generation
  and movement, with the console showing live state. `core/trek.{h,c}` compiles
  unmodified under both `cc` and `cl65` — the architectural claim of this repo,
  now exercised rather than asserted. `c128/src/ui.c` is presentation only; the
  core formats no text. Binary 9,738 bytes.

  Design points worth keeping: the galaxy is four flat byte arrays, not an array
  of structs, so the 68000 alignment tax `commodore-uno` documents for its `Card`
  cannot apply on the Amiga leg. The PRNG is owned rather than taken from libc,
  so one seed reproduces one galaxy on every platform — which is what will make a
  cross-platform comparison meaningful. Distance is the precomputed 64-entry 8.8
  table NOTES specified; no runtime `sqrt` exists anywhere.

- **Caught by the core test on its first run: the energy economy was inverted.**
  A one-quadrant hop at warp 5 cost 50 units but took 0.2 stardates, and the
  converter yields 400/stardate — so travelling *earned* 80. The clamp at
  `ENERGY_MAX` disguised it as "nothing happened". That contradicts the manual
  outright (l.261-264). Both travel scales now lose against the converter, and
  the test asserts that direction rather than merely that energy changed.

- **Fixed: `q` never quit — and neither did any other letter command.**
  `input.c`'s key table was written with C character literals, which cc65
  translated to PETSCII (`CD D7 D1` in the linked binary where `4D 57 51` was
  meant), while `main.c` compared against numeric ASCII. Digits worked, because
  cc65 leaves those alone — which is exactly what hid it, since coordinates
  echoed correctly and the input path looked healthy. Key values now live once
  in `input.h` and are included by both sides, so the two spellings cannot drift
  again.

- **KERNAL keyboard input replaced with a direct CIA1 matrix scan.** `cgetc()`
  blocks forever in this port. `commodore-uno`'s C128 port reached the same
  conclusion from a different direction, recording that `kbhit()`/`cgetc()` "go
  completely dead" there. Matrix positions were transcribed from VICE's own
  keymap (`share/vice/C128/gtk3_sym.vkm`), not a published table, per that
  port's warning that a "standard" reference was already wrong for several keys.
  The keymap's *row* is the bit strobed low on `PRA`, its *column* the bit read
  from `PRB`; `commodore-uno` stores that pair under struct fields named the
  other way round, which is easy to misread. The write/read pair needs `SEI`/
  `CLI` because the KERNAL's 60Hz IRQ does its own strobe.

## THE OPEN LIST (rewritten 2026-08-24)

Read this first; the numbered items below are the historical record of how each
one got here.

**The plan for clearing the measurement items is at the end of this file** --
"THE MEASUREMENT SESSION PLAN", five runs. **ALL FIVE RAN 2026-08-24.** What
survives them is listed below and is mostly small. Measure everything, then implement, then
size, then place in bank 1, then fix the bugs. In that order, and for the
reason given there.

**Before adding anything to this list, search for it.** Three settled questions
were written up as open in two days. `reference/manual.txt` is ISO-8859 and
**invisible to grep** until converted; `reference/strings.txt` is plain ASCII
and answers mechanics; MEASURED.md has its own back catalogue.

### Commands -- twenty of twenty-five

| | what is left |
|---|---|
| ~~`Shift-F1` boss mode~~ | **NOT DOING IT -- Jamie's call, 2026-08-24.** See below |
| `RAY` | fully specified, and bigger than it looks: four outcomes, the fourth destroys the ship and prints a **Top Secret loss report** this port has no equivalent of |
| `O)rbit` / `LAND` / `USE` | needs a core planet model. **Fully captured in run 1** -- every screen, both gates and the payoff. Nothing left to measure; this is now implementation work. See MEASURED.md, "The planet chain, captured end to end" |

### Corrections to commands already marked done

- ~~**`M)ove` manual mode.**~~ **DONE 2026-08-24.** `M` with no coordinates
  opens the `NAVIGATION` dialog; typing `M` there switches to `DELTAX:` /
  `DELTAY:`; a computer below 100% goes straight there and says COMPUTER
  DAMAGED. `trek_move_delta()` is in core so the arithmetic is natively
  tested against the manual's own worked example, and a delta that stays
  inside the quadrant uses the impulse engines rather than being billed as a
  warp jump.
- ~~**`M)ove` and `W)arp` shorthands.**~~ Already worked -- `grab_digits`
  ignores separators, so `6235`, `m6235` and `w5.2` all parsed before anyone
  checked.
- ~~**Repair composed docked with focused.**~~ **FIXED 2026-08-24.** The
  manual's four repair rates are a TABLE, not a product -- 2.5x docked times
  3x focused is 7.5, and the manual says 5x. `advance_time()` multiplied them
  and repaired a focused system at a StarBase about 40% too fast. Now four
  rate constants with all four asserted in `test_trek.c`. See MEASURED.md,
  "The repair table is a table, not a product".
- ~~**`SELFDESTRUCT_FACTOR`**~~ **DELETED 2026-08-26.** Self-destruct destroys
  no enemies at all -- run 5, four present, nearest at range 1.41, zero killed.
  The constant and the ancestor's `kaboom()` reach test are gone;
  `trek_self_destruct()` still returns a count, and it is always nought.

### Run 1 is DONE (2026-08-24) -- see MEASURED.md

Four items in, and a great deal more out.

- **The repair rates were wrong in two ways at once**: 47 should be 50, and a
  focus STARVES every other system rather than merely outpacing it. Fixed.
- **The stardate discrepancy was never real** -- the log prints
  `stardate:message-number`, and HAIL costs no time at all.
- **The turn-cost table is done, and it is one sentence**: time advances only
  on movement and docking. Combat is free.
- **The planet chain is captured end to end** -- ORBIT, LAND, USE, the gate,
  the confirmation and the payoff. Energium takes energy ABOVE its maximum.
- **Movement is a straight line and objects block it**, for quadrant changes as
  well as in-quadrant hops. This port teleports.
- Free on the way past: HAIL in range, a distress signal and a base-under-
  attack event WITH their deadline stardates -- and proof the deadline is
  enforced, since the base in 4-2 was destroyed after its own ran out.

The only run-1 item left is **Supply and Research docking quantities**: every
base encountered was a StarBase. That wants the base-type array located in
memory rather than another survey.

### Measurement -- what runs 4 and 5 still owe

Everything about *what a thing does* has been on disk; these are numbers.

**Runs 4 and 5 ran 2026-08-24.** What they closed:

- ~~The Top Secret loss report~~ **CAPTURED** -- a Dept. of Space memo with
  From/To/Date/Re, one sentence naming the loss, then stardays, kills per
  stardate and score. It is the frame the other loss endings reuse.
- ~~The Detailed Evaluation~~ **CAPTURED with every weight on screen**, and it
  found a penalty this port had deleted -- see the ship-loss entry below.
- ~~`SELFDESTRUCT_FACTOR`~~ **It destroys NOTHING.** Four enemies present, the
  nearest at range 1.41, zero killed. Another ancestor rule Anderson dropped.
- ~~`HP_SCOUT`~~ was never open. It was measured on 2026-08-21 and a stale note
  claiming otherwise got carried into the plan and this list. All four classes
  now confirmed at 100%: Scout 255, Battleship 355, Commander 695, Supply 150.
- `RAY`'s dialog, its success sequence and its no-enemies refusal are captured.

Still open from those runs:

- **`RAY`'s outcome ODDS** -- still ONE sample after three attempts. The rig
  is understood (SAVE/restore is a working checkpoint and puts the ship back
  exactly), but the emulator goes unstable under the long automated runs that
  sampling needs -- blank dialogs while the API still answers. Next attempt:
  restart dosbox PER SAMPLE, not per block. See MEASURED.md, "The apparatus
  goes unstable".
- ~~One sample of dying in COMBAT~~ **DONE 2026-08-24 and the constant is
  restored.** The sheet prints `Penalty for loss of ship .... -200` and totals
  -930 with nothing killed. The 2026-08-20 reading that gave -730 and got the
  constant deleted had missed a line. `SCORE_SHIP_LOST` is back, with
  `ship_lost_pts` on the sheet and the C128 renderer printing whichever row the
  ending calls for.
- **`Rescues @ 200 each`** -- a scoring line found on the surviving-ship sheet
  2026-08-24 that this port has never had, and the scoring end of the
  distress-signal mechanic. The weight is known; what is not is what counts as
  a rescue.
- **Boarding-party frequency and duration.** The mechanic is in the string
  table; the numbers need elapsed play.
- **The unbuilt screens**: five loss endings, Union wreckage, reinforcement
  warnings. Specification only, never prose.

**Opened by runs 1 to 3, and all cheap next time the rig is up:**

- **The shield absorption law.** SHAPE SETTLED 2026-08-24 with one enemy at
  last: at a given charge the absorbed amount is REPEATABLE, and the absorbed
  fraction RISES with charge -- 0.33 at 1000, 0.47 at 1500, 0.65 at 2000. Three
  of the four levels fit `fraction = charge / 3100`; 2500 does not. The
  constant is still open. It is emphatically not `amount - shields`.
- **`SYSTEM_DAMAGE_THRESHOLD`'s actual edge.** Systems died on roughly three
  turns in five once a few hundred units reached energy, and never while the
  shields absorbed everything. No clean edge yet.
- **Supply and Research docking quantities.** A StarBase restocks energy and
  shields to full in one 0.1-stardate turn; the other two types were never
  found. Wants the base-type array located in memory, not another survey.
- ~~The STATE OF REPAIR estimate formula~~ **ABANDONED as cosmetic
  2026-08-24.** It is not a function of points remaining at all -- two adjacent
  tenth-boundaries sit one point apart while a third sits three points away, so
  no single rate produces it. A display estimate with no gameplay effect, and
  it has cost parts of two sessions.

**Seen repeatedly across runs 1 to 5 and never measured** -- added 2026-08-24,
because all three are mechanics this port has no part of:

- **The long-range tractor beam.** It seized the ship at least four times in
  five runs and MEASURED.md calls it "a hazard to measurement", but what
  triggers it, how often, and whether it is a commander ability are all
  unknown.
- **Reinforcements arriving.** Quadrant 7-2 went from two enemies to four
  between visits and the Mongol counter went back UP. That is the mechanic
  behind the "reinforcement warnings" string, and it is distinct from the
  message.
- **The Vandal Death Pod's numbers.** One 87-unit hit observed, and it is known
  to hit enemies too. Frequency and damage law unmeasured.

**Not for the emulator:** item 5's four FITTED constants want `dis16.py`.

**Search the manual before adding anything here.** This list briefly carried
the repair rates and `REPAIR_FOCUS_FACTOR`, which the manual states outright at
l.464-469, and the reserve life support duration, at l.402. Reading it cost
nothing and found a live bug.

### Infrastructure

- **The briefing.** The last thing the disk seam was built for; needs
  `plat_open`/`plat_read`, the one part of the seam never exercised.
- ~~**The CP437 charset.**~~ **NOT DOING IT on the C128 -- Jamie's call
  2026-08-24.** PETSCII text and lettered ships are an acceptable position for
  this platform. See "The charset is dropped on the C128" below. Other ports
  decide for themselves; this is presentation, and presentation is per-platform
  by design.

### FIXED: no sound since the music moved to bank 1

**Reported by Jamie 2026-08-24. FIXED 2026-08-25, and it was never the sound
code, or the bank-1 move.** `.data` was never initialised on the real machine,
so sid.c's `static uint8_t enabled = 1;` came up 0 and `snd_poll()` returned on
its first line -- with correct note data sitting in bank 1 the whole time.

The bug is latent from the llvm-mos migration on 2026-08-23, when
`c128/trek128.ld` was written, not from the bank-1 move a day later. Why it
surfaced when it did is NOT established: uninitialised RAM is uninitialised,
and $1301 may simply have held a nonzero byte before. The coincidence is what
sent this looking at the far store for two days, and the far store was
innocent.

**The cause is our own link script.** llvm-mos copies `.data` from its load
address to its run address only if `copy-data.c.obj` is LINKED IN, and the
Commodore targets deliberately leave it out: `commodore.ld` aliases
`c_readonly` and `c_writeable` to the same region, so a Commodore `.data` has
VMA == LMA and there is nothing to copy. `c128/lib/libcrt0.a` bundles
init-stack, zero-bss, zero-zp-bss and copy-**zp**-data, and no copy-data.

`trek128.ld` overrides `REGION_ALIAS("c_writeable", lowram)` -- the whole point
of that file, and the thing that bought the SAVE seam its space. That splits
VMA from LMA, and nothing was left to bridge them. The fix is one line:
`LDLIBS = -lcopy-data` in `c128/Makefile`, 26 bytes.

**The map looked perfectly healthy.** `.data` had a VMA ($1300), an LMA
($be74) and a size (3). What it did not have was anything in `.init.200`
between `__do_zero_bss` and `__do_copy_zp_data`, and that absence is invisible
unless you go looking for it.

**How it was actually caught, and the order matters.** The bank-1 dump was
supposed to convict the far store and instead exonerated it: 1,092 bytes at
$48DB in `ram01` byte-for-byte identical to `build/music.dat`, and the string
pool at $4000 reading correctly. With the data cleared, the driver state was
the only place left, and it read `mus_on=1 mus_ok=1 mus_base=08DB` with
`mus == mus_head` and `note_left = 0` -- a driver that had been told everything
and ticked exactly zero times. `snd_poll()` has one early return. Poking
`$1301 = 1` in the running machine started the title tune mid-frame.

**Only `enabled` actually mattered.** The other two bytes of `.data` are
`alert_quad` (main.c) and `base_under_attack` (core/trek.c), and both are
re-assigned by `trek_new_game()` or the per-game reset before anything reads
them. `enabled` is written nowhere but its initialiser and `snd_toggle()`.

**`make verify` now checks the link, on every build.** If `.data` is non-empty
and its VMA differs from its LMA, `__do_copy_data` must be in the map or the
build fails. This is a CLASS, not a variable: every `static x = <nonzero>;`
anywhere in the port depended on it, and neither test suite can see it --
both run natively, where `.data` is the host's problem. `c128/Makefile` also
gained itself as a link dependency, so a change to `LDLIBS` forces a relink.

**Verified end to end.** `make sound-check` passes on both regions: worst pitch
error +0.44%, tempo +1.0% over ten notes. On the running machine `.data` reads
`FF 01 40` -- the load image exactly -- and `mus` advances on its own.

**The harness was broken too, and that had to be fixed first.**
`tools/sound_check.py` autostarted the bare `trek128.prg`. Since the bank-1
move the music is a FILE ON THE DISK -- with no disk attached `far_load` fails,
`mus_ok` stays 0 and `snd_music()` refuses, so the tool reported "never found
the first note" for a build whose driver might have been fine. It now
autostarts the D64 and `make sound-check` builds it first. **Moving data
invalidates every harness that loads the program**; grep for what autostarts
what, every time.

**A latent bug spotted in passing, not hit.** `snd_poll()` guards with
`(!mus && !sfx)`, and both are far OFFSETS, not pointers -- a track whose base
offset is 0 would read as "nothing playing". It is safe only because
`mus_base` is currently 2,267. `mus_on`/`sfx_on` already exist and say this
properly.

**The lesson to carry:** sound fails silently in the most literal way, which is
exactly why `make sound-check` exists. Verifying the loader's state proves the
data arrived, not that anything plays -- and when every piece of state is
right and the thing still does not work, the next suspect is whether the code
you are reading is the code that ran.

### The string pool is finished, and the image links again (2026-08-26)

**It links with 169 bytes free.** The movement correction had put it 1,118
bytes over; finishing the string pool took 1,292 bytes out.

Where they came from, in the order they were found:

        364  the generator learned three more drawing functions
        564  static initialiser tables, by hand -- see below
        322  a duplicate system-name table, deleted outright
        ~40  MINLEN lowered from 10 to 6
         ~2  SELFDESTRUCT_FACTOR and the rule it belonged to

`.rodata.str1.1` went from **1,564 bytes across 146 literals to 291 across
55**, and the pool now carries 3,535 bytes of text in bank 1.

**What the generator could never reach, and why.** `tools/gen_strings.py` only
rewrites a literal that is a direct argument to a known drawing function,
because a literal in a static initialiser has no call to become. So every
TABLE kept its text in the binary long after the prose around it had moved:

    sys_name[12]   143 bytes   ui.c    -> sys_name_id[]
    rank_name[5]    60 bytes   ui.c    -> rank_name_id[]
    ship_art[5]     60 bytes   ui.c    -> ship_art_id[]
    panels[].title  88 bytes   layout.c -> Panel.title is an id now

Those are hand conversions and they are the pattern for any future table:
hold the id, call `S()` at the point of use. `PANEL_NO_TITLE` (0xFF) marks the
badge panel, which carries no title -- an empty pooled string would still cost
an id and a far read to draw nothing.

**A duplicate table went with them, and it was the best single win.**
`main.c` had its own twelve-case `sys_name()` switch beside `ui.c`'s
`ui_sys_name()`, used at exactly one call site, and it said `CONVERTER` where
the original says `EnergyConverter`. Deleting it took 322 bytes and made the
wording righter, not just shorter. **Look for the second copy before
optimising the first.**

**MINLEN was a guess and it was wrong.** The generator skipped anything under
ten characters, on the reasoning that short strings cost more in call overhead
than they save. Measured, they do not: a pooled string costs two bytes of
index and turns a load-address into a load-immediate-plus-call. The department
prefixes alone -- `HELM: `, `COMMS: `, `SCIENCE: ` -- were 60 bytes sitting
above the old floor. It is 6 now. Below that the remaining literals are single
characters and separators and it genuinely stops paying.

**What is deliberately still in the binary.** `STRINGS.DAT`, `MUSIC.DAT`,
`TREK.SCR` and `EGATREK.SAV` -- 43 bytes of filenames. `STRINGS.DAT` is read
BEFORE the pool exists and the others are opened by code that must not depend
on it. None of them is an argument to a drawing function, which is what keeps
them out; there is a comment in the generator saying never to add `far_load`,
`plat_open` or `hof_*` to DRAW_FNS.

Also left: the twelve three-letter `sys_abbrev` codes. Pooling them would save
about 24 bytes and cost twelve far reads on every SYSTEMS STATUS redraw, which
is the wrong trade -- a pooled string is a 63-byte far read, roughly 0.6ms.

**Verified on the machine, not just in the map.** Title screen with its pooled
ship art, the full nine-panel console with every pooled panel title, STATE OF
REPAIR listing all twelve pooled system names, and a blocked move reporting
`NAVIGATION: BLOCKED BY OBJECT AT 8-5` with the ship standing at 7-5. Impulse
fell 500 to 494 for the one sector it flew, and the stardate did not move --
which is the sub-tenth carry working, not a bug.

**What this does NOT do is solve the ceiling.** It bought 1,292 bytes once,
and the movement correction alone cost 1,424. The remaining feature list is
estimated at 5,000-6,000 bytes against 169 free. See below.

### THE CEILING IS REAL, AND OVERLAYS ARE THE ANSWER -- measured 2026-08-26

Code is **94% of the image**: 40,621 bytes of `.text` against 2,353 of
`.rodata`. Bank 1 is barely touched -- 4,627 bytes used of 32K -- but it holds
DATA, and data is no longer what is overflowing. `FETCH`/`STASH` read bytes
into a RAM buffer; nothing executes from bank 1.

**The compiler has nothing left to give.** Measured: `-Wl,-mllvm,
--inline-threshold=-50` saves 194 bytes, `--lto-O3` saves nothing, and
`-fno-lto` COSTS 6,801. LTO is already carrying a great deal.

**Overlays would free 8,561 bytes, and that is measured rather than
estimated.** Forcing the phase functions out of `main` with `noinline` -- they
were all inlined into it, which is why `main` was 14,846 -- gives:

        ui_hall_of_fame   2,111      ui_save_game + serialiser  1,834
        ui_evaluation     1,696      ui_info_panel              1,245
        trek_score_sheet  1,270      ui_title                     647
        ui_repair_report    575      ui_setup                     749
        ui_messages_view    545
                                            total   10,672

Every one is a phase or a modal screen. None coexists with the main game loop,
and none coexists with any other. One window the size of the largest -- 2,111
bytes -- with the rest copied in from bank 1 frees 8,561.

That is more than the whole remaining feature list. The cost is a separate
link per overlay at a fixed address, discipline about which direction calls
may go, and roughly 30-60ms to copy a window. Design it as a SEAM like
`core/farmem.h`: C128, X16, F256 and CoCo 3 all want one, and MEGA65 and the
Amiga implement it as a direct call.

### Disk speed, and what the rig has been all along (measured 2026-08-26)

**This machine runs JiffyDOS at both ends** -- `JiffyDOS_C128_6.01` as the
C128 kernal and `JiffyDOS_1541-II_6.00` in drive 8 -- and it always has. Every
disk timing this project has ever taken was JiffyDOS-accelerated. Jamie's
call: **that is fine and it is the assumed environment**, JiffyDOS being
common on real machines now. Worth knowing rather than discovering later. It
is also why VICE prints `C128MEM: Warning - Unknown kernal image`: JiffyDOS
patches the kernal and the checksum no longer matches a stock ROM.

**d64 versus d81, measured.** Timed in emulated jiffies off the KERNAL clock
at `$A0`, watching `far_len` step 0 -> 3535 (STRINGS.DAT) -> 4627
(MUSIC.DAT). NTSC, so 60 jiffies to the second.

    drive                        1,092-byte file   reset -> both loaded
    1541-II + JiffyDOS  (d64)         125 jiffies        348  (5.8s)
    1581    + JiffyDOS  (d81)         118 jiffies        314  (5.2s)

**The d81 is about 10% faster overall and 6% on a small file.** Real, but not
the order of magnitude the overlay question needs, and d64 is the more
universal format. The port needs no code change either way -- same device,
same filenames -- so this is a build decision, not an architectural one.

A JiffyDOS 1581 ROM makes NO difference to the d81 figures: two runs with and
without the flag were identical to the jiffy. (They were the same
configuration: `DosName1581` in vicerc already pointed at the JiffyDOS image.)

**AND A TRAP, PAID FOR ONCE.** An early reading said the d81 was SLOWER, 30
jiffies against 22. That was the instrument: the poll started six seconds
after launch, by which time the first transition had already happened, so the
"22" was the interval between two OBSERVATIONS and not between two events.
Tracing from reset reverses the sign. Start the trace before the thing you are
timing, and check the rig by re-running at a different poll rate -- 3ms and
60ms agree here, which is what says the polling is not perturbing it.

**THE FORMAT IS NOT THE BOTTLENECK, THE READ PATH IS.** 1,092 bytes in ~2.0
seconds is about 550 bytes a second, nowhere near what JiffyDOS delivers.
`plat_read` in `c128/src/storage.c` reads ONE BYTE AT A TIME through
`cbm_k_chrin()` with a `cbm_k_readst()` after every one -- two KERNAL calls
per byte. That is the slowest path the KERNAL offers.

The KERNAL's LOAD ($FFD5) reads a whole file in one call and is exactly what
JiffyDOS accelerates hardest. It loads into the bank-0 address space, which is
useless for the far store but is EXACTLY where an overlay wants to land. So:

  * measure LOAD against the CHRIN loop before designing overlay loading;
  * if LOAD is the several-times faster it should be, an overlay swap is one
    LOAD straight into the window -- no bank 1, no far copy, no startup
    preload, and the biggest open risk in the overlay scope below disappears;
  * it would make startup faster too, which is worth having on its own.

### LOAD against the CHRIN loop: 8.2x, measured 2026-08-26

Same machine, same drive, same 4,096-byte payload, read into the same buffer
with the buffer cleared between the two so a stale success could not look like
a fast one. `c128/test/loadbench.c`, built by `make -C c128 loadbench`.

    CHRIN loop, a byte at a time   107 jiffies   1.78s    2,296 bytes/sec
    KERNAL LOAD, one call           13 jiffies   0.22s   18,904 bytes/sec

Both read all 4,096 bytes, and the buffer was compared against the payload at
four offsets afterwards -- LOAD delivers the right bytes, not just the right
count.

**And 8.2x is a LOWER BOUND on the real gain.** The bench's CHRIN path is
kinder than the port's: it does one `cbm_k_chkin` for the whole file, where
`far_load` calls `plat_read` in 64-byte chunks and pays a `chkin`/`clrch` pair
per chunk. The port's actual path is slower than the 2,296 B/s measured here.

**What this changes.**

  * **Overlays should be LOADed straight into the window.** LOAD puts data in
    the bank-0 address space, which is useless for the far store and exactly
    right for an overlay. A swap becomes one LOAD of about 3K -- roughly a
    sixth of a second -- with no bank 1, no far copy and no startup preload.
    The biggest open risk in the overlay scope below disappears with it.
  * **Secondary address 0 is the one to use**: "put it where I say", so the
    file's two header bytes are read and discarded and the overlay lands at
    the window whatever address it was built at.
  * **STRINGS.DAT and MUSIC.DAT should probably move too.** They are SEQ files
    read through CHRIN because the far store needs them in bank 1, and LOAD
    cannot reach bank 1... except that it can be LOADed into a bank-0 buffer
    and STASHed across, which is still far quicker than 4,600 bytes of CHRIN.
    Worth measuring before assuming; it is startup time, which the player
    feels on every launch.
  * The disk seam keeps its byte-at-a-time path regardless -- SAVE and the
    hall of fame need real file writes and small reads, and LOAD cannot write.

**This is the answer to "would a d81 be faster".** It would, by about 10%.
The read path is worth 8.2x. Format was the wrong question.

### The far store loads with LOAD now, and startup is 2.2 seconds shorter

Done 2026-08-26, on the back of the 8.2x measurement above.

**SETBNK takes the bank the DATA lands in.** That is the fact that made this
simple as well as fast: `lda #1 / ldx #0 / jsr $ff68` and the KERNAL's LOAD
writes straight into RAM bank 1. No bank-0 buffer, no STASH, no 64-byte
chunking. Verified byte for byte against the file through the monitor before
any of it was written.

    STRINGS.DAT  3,535 bytes    ~134 jiffies  ->  20    (6.7x)
    reset -> both files loaded   348 jiffies  -> 214    (2.2s at 60Hz)

MUSIC.DAT's interval only came down from 126 to 95, and the reason is worth
recording: that interval is not the load. `snd_init()` runs between the two
far_loads and its region detector spins up to 30,000 times reading the raster,
which is most of what is left. The same cost was in the baseline, so the
comparison is honest, but do not read 95 as "the music takes 95 jiffies".

**What went with it.** far_move lost its STASH half, the direction flag and
the $02B9 STAVEC store; the 64-byte chunk buffer went too. The image came down
140 bytes -- 169 free to 309 -- so this paid for itself twice.

**The disk build writes STRINGS.DAT and MUSIC.DAT as PRG, not SEQ.** LOAD with
secondary address 0 means "put it where I say", and it reads and discards the
file's two-byte load address to do it. The header value is ignored; $4000 is
written so a human LOADing the file by hand sees something sensible.

**The device number now lives in two places**, farmem.c and storage.c. That is
deliberate rather than sloppy: storage.c serves `core/storage.h`, which must
never learn about banks, and far memory owns bank 1, so loading it is far
memory's business. `core/` still knows about neither.

**Checked, not assumed:** the title screen renders with its full prose, bank 1
matches both files byte for byte, `make sound-check` passes on both regions --
and a disk built WITHOUT MUSIC.DAT still comes up with words on it, far_len at
3,535 and mus_ok at 0, playing silently exactly as the fallback intends. That
last one is the case that broke the port before.

### SCOPE: the overlay seam (written 2026-08-26, NOT BUILT)

Read this before starting it. The mechanism is proven, the arithmetic is
measured, and there is one open risk that should be measured FIRST because it
changes the design.

#### What it is

A fourth seam, the same shape as `core/storage.h`, `core/farmem.h` and
`core/strpool.h`: contract beside the core, mechanism per platform, `core/`
never calls it. Code that runs in PHASES is stored in bank 1 and copied into a
fixed window in bank 0 when it is needed.

    core/overlay.h      the contract
    c128/src/overlay.c  copies from bank 1 through FETCH
    MEGA65, Amiga       an empty function -- they have the room

The portable call is two lines and reads the same everywhere:

    ovl_load(OVL_END);      /* nothing at all on a flat target */
    ui_evaluation();

#### The mechanism, and it is PROVEN not assumed

Two experiments, both run against this toolchain on 2026-08-26:

**lld will place sections at the same address, with `--no-check-sections`.**
Without it the link fails with "virtual address range overlaps"; with it, it
links silently. Each overlay gets its own `>window AT>ovlimg` where `ovlimg`
is a staging region that never exists on the machine -- that gives every
overlay a DISTINCT load address, so their file ranges do not collide while
their run addresses all coincide at the window.

**The images come out with `llvm-objcopy --dump-section`.** Linking the same
objects to an ELF and dumping `.ovl.a` and `.ovl.b` produced two files of
exactly the section sizes in the map.

The window goes at the TOP of `ram`, so the region shrinks by the window size
and the resident links below it. `TRIM(ram)` in the existing OUTPUT_FORMAT
then excludes the overlays from the PRG automatically -- they are in a
different region.

#### The build

One link to ELF, everything else derived from it, so there is no possibility
of two links disagreeing about layout:

    1. compile + link -> trek128.elf   (--no-check-sections)
    2. llvm-objcopy --dump-section=.ovl.N=build/OVLN.DAT   per overlay
    3. a Python step reads the ELF's PT_LOAD segments in the `ram` range and
       writes the PRG: SHORT($1C01) followed by the resident bytes
    4. c1541 writes OVLN.DAT to the disk beside STRINGS.DAT

Step 3 is about thirty lines and this repo already builds data with Python.

#### The rules, and they are not optional

  * **Every overlay function is `__attribute__((noinline, section(".ovl.N")))`.**
    Without noinline, LTO inlines it into a resident caller and the overlay is
    silently empty -- which is exactly what it already does: every one of these
    functions was inlined into `main`, which is why `main` measured 14,846.
  * **An overlay calls RESIDENT code only.** Never another overlay: the window
    would be overwritten under it.
  * **One entry point per overlay, reached through a resident stub** that does
    the load and the call together. Nothing else may call in. That is what
    makes "forgot to load it" impossible rather than merely unlikely.
  * An overlay's statics do not survive a swap.

#### The grouping, and the arithmetic

Sizes are MEASURED, by forcing each function out of `main` with `noinline`.
The call graph was checked: none of these calls another except
`ui_evaluation -> trek_score_sheet`, and `ui_setup`'s restore path and
`ui_save_game` both reach the serialiser -- which is what puts the front end
and the save machinery in one overlay rather than two.

    OVL_FRONT   ui_title 647 + ui_setup 749 + serialiser 1,592
                + ui_save_game 242                              3,230
    OVL_EVAL    ui_evaluation 1,696 + trek_score_sheet 1,270    2,966
    OVL_HOF     ui_hall_of_fame                                 2,111
    OVL_INFO    ui_info_panel                                   1,245
    OVL_REPAIR  ui_repair_report                                  575
    OVL_MSGS    ui_messages_view                                  545
                                                       total   10,672

    window = the largest = 3,230
    freed  = 10,672 - 3,230 = 7,442, less ~250 for the loader and stubs
           = about 7,200 bytes

Against 169 free today and a remaining feature list estimated at 5,000-6,000.
It fits, with room, and there is a second tranche available later if it is
ever needed: the modal command handlers inside `main`.

Sizing the window at exactly the largest overlay is deliberate. Growing past
it becomes a LINK ERROR, which is the same tripwire trek128.ld already uses
for the stack guard.

#### The one open risk, and it should be measured FIRST

**Startup load time.** 10,672 bytes more off an emulated 1541 at roughly a
kilobyte a second is about ten seconds added to every launch, on top of the
41K PRG. That is not obviously acceptable and it changes the design:

  * If loading is fast enough, preload every overlay into bank 1 at startup
    (there is room -- 15,299 bytes of 32K with the overlays added) and each
    swap is then a bank-1 copy, about 45ms for 3,230 bytes through FETCH.
  * If it is not, the phase overlays that run once (FRONT, EVAL, HOF) can be
    read straight off the disk at the moment they are needed, and only the
    mid-game ones (INFO, REPAIR, MSGS -- 2,365 bytes together) preloaded.

**Measure the rate before choosing.** Time `far_load("STRINGS.DAT")` for its
3,535 bytes with the jiffy clock and multiply. Guessing here would design the
whole thing around a number nobody has read.

#### Smaller things that will come up

  * `far_read()` takes a `uint8_t` length. The overlay loader needs a chunked
    loop; `core/farmem.h` need not change.
  * `ovl_load` must be idempotent -- a no-op when the requested overlay is
    already resident -- or every INFO panel costs a copy.
  * A copy through FETCH is roughly 30 cycles a byte. A loop in common RAM
    with the MMU mapping bank 1 in would be about three times faster. Not
    needed at 45ms; worth knowing it exists.
  * Debugging gets harder: a symbol in the map has the window address, and
    which code is actually there depends on what was loaded last.

#### Suggested first slice

**OVL_EVAL alone**, and not because it is the biggest. It is already written,
self-contained, runs once at the end of a game, and cannot corrupt a game in
progress if the mechanism is wrong. Build the seam, the loader, the build
step and one overlay; play a game to the end; confirm the evaluation screen
still renders and the image shrank by about 2,700 bytes. Then the remaining
five are repetition.

### Wanted on their own merits

- **The function keys** -- F1 Help, F2 Lasers, F3 Fire Torpedo, F4 Move Ship,
  F5 Max Energy, F6 Fix Systems, F7 Xfer Energy, F8 Repair Status, F9 Set
  Speed, F10 Dock. They were going to arrive with boss mode; that is dropped
  and these are not.

### Named in the strings, never built

Union wreckage in a quadrant, Mongol reinforcement warnings, reserve life
support, and **five of the eight loss endings** -- including a surrender.

Two came off this list in run 1 and are now specified rather than merely named:
**distress signals and base-under-attack events** (a named planet or base, a
quadrant, and a DEADLINE STARDATE that is enforced -- the StarBase in 4-2 was
destroyed when its own ran out), and **the `USE` inventory** (a numbered list
with quantities, and raw energium takes energy ABOVE its maximum).

### Behaviour the runs found that this port gets WRONG

Not missing features -- implemented things that do not match the original.

- ~~**Movement teleports.**~~ **FIXED 2026-08-25**, with the whole model
  measured rather than just the blocking: the path, the round-half-away-from-
  zero rule, stopping in the last clear cell, billing on the TRUNCATED
  endpoint, `distance/24` for impulse and `11 * quadrants / warp^2` for warp,
  and the departure path through the quadrant being left. See MEASURED.md,
  "The movement model, entire". Four samples and two discriminators are
  replayed in `core/test/test_trek.c`.
- ~~**Turn costs.**~~ **Movement's half is done**, and the corrected warp time
  law also fixed the warp ENERGY fit without touching an energy constant: the
  three readings were all computed against an elapsed time taken from the
  one-decimal Date display, and recomputing them properly moves the model from
  +12%/-1%/-2% to +0.9%/+0.8%/0.0%. Still to audit: whether anything else in
  this port bills a turn that the original gives away. Combat is free.
- **Shields subtract; they should absorb a share.** `through = amount -
  shields` is not the original. Raised full shields take a hit WHOLE with
  energy untouched; shields down and the hit comes out of main energy instead;
  a nearly flat shield takes a steady small fraction. Run 3.
- **A system hit is far too gentle.** We take 20-59 points off one random
  system. The original leaves it at 0% eight times in eleven, and can take TWO
  systems in one turn. Run 3.

---

## Open questions / next steps

1. ~~**Close the encoding-mismatch class with `make verify`.**~~ DONE --
   `c128/Makefile` has a `verify` target and `tools/verify_prg.py` checks the
   key table is ASCII in the linked PRG. Three separate bugs
   this session were the same defect wearing different hats: blank panel titles,
   a PETSCII key table, and the dispatch constants that made `q` a no-op.
   **Neither test suite can see any of them**, because native `cc` does not
   perform the character-set translation `cl65` does — the tests pass either
   way, and only the linked binary shows the truth. Each time, the thing that
   actually found it was grepping the PRG for an expected byte pattern, done by
   hand.

   Make that a build step: a `make verify` target that checks the linked
   `trek128.prg` contains the key table as ASCII (`4D 57 51`) and not PETSCII
   (`CD D7 D1`). Cheap, and it turns "caught by playing" into "caught by
   building". Lifting `ascii_to_screencode` out of `vdc.c` into its own header
   so `cc` can reach it is worth doing too, but it is the smaller half — a
   native test of that function still would not have caught the key table.

2. ~~**Returning to BASIC wedges the C128.**~~ RESOLVED 2026-08-22 -- it does
   not, and there is no evidence it ever did. `main()` returns 0, the park loop
   is gone, and Q now ends the program properly. See "The exit bug that was
   never there" below. Original text follows.

   **Returning to BASIC wedges the C128.** Quit currently parks with the console
   readable and RUN/STOP+RESTORE as the way out, which works but is a
   workaround, not a fix. Undiagnosed: this program runs at 2MHz, drives the VDC
   directly, and scans CIA1 behind the KERNAL's back, so there are several
   candidates and no way to observe the machine from a session on this host.
   `c128/src/main.c` carries a bisect recipe — return immediately from `main()`
   with no `vdc_init()`, confirm a clean exit, then add back the 2MHz switch,
   the VDC writes, and the CIA scanning in turn.

3. ~~**Seed the RNG from something varying.**~~ DONE 2026-08-21 -- the setup screen counts keyboard poll passes while it waits for the player's answers and mixes that with the chosen level. Two runs verified in VICE gave different galaxies (quad 8-3 with 34 enemies, quad 8-7 with 31). Original text follows.

   **Seed the RNG from something varying.** `GAME_SEED` is fixed, deliberately —
   it makes a side-by-side against DOSBox-X repeatable — but it means every run
   is the same galaxy. Wire it to timing once there is a title screen.
4. ~~**Capture the original at full 640×350**~~ DONE 2026-08-19 via
   dosbox-automation; the measured table is in `layout.c` and MEASURED.md.
   Original text follows.

   Correct the provisional panel table in `c128/src/layout.c`. The only layout source so far
   is a 320×175 half-scale screenshot where a cell is 4×7 px — too coarse for
   exact column boundaries. Everything reads that one table, so it's a
   single-site edit. The same capture settles which glyphs the original uses.
5. **READ THE MANUAL FIRST -- it was unreadable to grep until 2026-08-24.**
   `reference/manual.txt` is ISO-8859 with high-bit box-drawing characters and
   this project's `grep` silently returns nothing on it, so every search of it
   ever run came back empty and was read as "the manual does not say". Convert
   it before searching:

       python3 -c "import re;print(re.sub(r'[^\x20-\x7e]',' ',
         open('reference/manual.txt',encoding='latin1').read()))"

   It closes eight items that were being carried as unmeasured, including the
   warp damage rule, `REPAIR_FOCUS_FACTOR` (3, not the DERIVED 2), the energium
   gate, and what switches NAVIGATION between its two modes. It also documents
   a damage effect for **every one of the twelve systems**, of which the port
   implements exactly one. Full audit in MEASURED.md.

5b. **The PROVISIONAL sweep -- REPOINTED 2026-08-23.** This item used to read:
   breakpoint `EGATREK_unpacked.exe` in DOSBox-X's debugger to confirm laser
   falloff, the no-damage threshold, boarding gates and scoring weights,
   because *"everything in `core/trek.h` marked PROVISIONAL is waiting on
   this"* -- the enemy-count formula, both travel energy scales, impulse
   timing, starting energy and torpedoes, and the 30-stardate mission length.

   **That list is stale, and it dissolved on its own.** `core/trek.h` now
   carries **two** PROVISIONAL markers and one of them is the legend defining
   the word, so there is exactly one real PROVISIONAL left (below). Every other
   name on the list is settled: `IMPULSE_ENERGY_UNIT` is MEASURED,
   `ENERGY_PER_DAY` comes from the manual, the scoring weights are measured,
   the enemy count is FITTED from five readings.

   **And not one of them was settled by a breakpoint.** They fell to driving
   the original under `dosbox-automation` and reading screens and memory. The
   DOSBox-X debugger has still never been used on this project.

   ### PROVISIONAL is an observation problem; FITTED is a code-reading problem

   That is the reassignment this item needed. The legend already says FITTED
   means "chosen to match readings, but not uniquely determined", which is the
   definition of a quantity where **more samples will never converge**. Playing
   the game harder cannot settle those. Reading the binary can. So the static
   analysis effort belongs on the FITTED tier, not the PROVISIONAL one:

   - the enemy-count-per-level formula (trek.h l.76)
   - `TORP_BASE` and `TORP_SPREAD` (l.694-701)
   - the long-range falloff slope, 16% per unit past the first (l.707)
   - the score kill-rate term (l.762)

   The route is **not** the DOSBox-X debugger. It is `tools/dis16.py` plus
   `tools/tp_runtime_map.py`, which already resolved the `SOUND`/`DELAY` calls
   and the CS-relative string constants. Extending an instrument that works
   beats standing up one that never has.

   ### The one surviving PROVISIONAL, and the shortcut to it

       /* PROVISIONAL: a hit that gets past the shields can wreck a system. */
       #define SYSTEM_DAMAGE_THRESHOLD  100

   Neither the chance nor the severity has been measured. This looks like it
   needs the damage array's address, which is **not** in the mapped ship record
   at 181910..182012 -- but it does not. SYSTEMS STATUS draws its bars at
   already-measured pixel positions (y267, 275, 283 ... 339, each 7px tall and
   50px wide, x261..310), so **bar length reads the percentage straight off a
   PNG**. Take penetrating hits, capture before and after, diff the bar
   lengths. No new memory mapping, and it is a `probe_original.py` session of
   the same shape as the ones already run.

   ~~**Loose thread: it recorded TEN bars against twelve systems.**~~ **NOT
   OPEN, and it never was.** MEASURED.md has carried "SYSTEMS STATUS lists
   twelve systems, not ten" since an earlier session: the console panel shows
   ten and the `REPAIR` dialog lists twelve -- the console's ten plus
   Transporter and Shuttlecraft, which the manual documents as damageable and
   which only matter for `LAND`.

   **This is the third time in two days I have written up a settled question as
   open** -- the others being what toggles NAVIGATION's two modes, and boss
   mode. All three were already recorded, in MEASURED.md or NOTES.md or the
   manual. **Search MEASURED.md and NOTES.md before adding anything to this
   list**, and remember the manual is unreadable to grep until it is converted
   (see item 5 above).

6. **Custom charset -- REFRAMED 2026-08-23.** This item used to say the port
   uses the KERNAL's stock character set and that *"EGA Trek's panels want
   CP437-style glyphs"*.

   **The frames are not characters, but the text is.** EGA Trek runs in BIOS
   mode 16, 640x350 EGA **graphics** -- which is why `GET /video/text` is
   useless against it and why the panel table had to be measured off pixels.
   The frames were read as maximal single-colour *runs*: they are pixel lines,
   not box-drawing characters, and there is no character grid to copy them
   from. The **text** is a different story -- 8x8 bitmap glyphs on an 8-pixel
   grid, and CP437 turned out to be half of where they come from. See (a).

   (An earlier revision of this item said flatly that there was no CP437 in the
   original at all. That was too strong, and dumping the fonts is what showed
   it: the frames argument is right, and it does not carry to the text.)

   What is really underneath this item splits in two.

   ### (a) The letterforms -- DUMPED 2026-08-23, and it is two fonts

   PETSCII uppercase is a distinctive Commodore face and it does not look like
   EGA Trek. That gap is now closed as a *measurement*: `tools/dump_font.py`
   extracts the font the player actually sees, and the answer is a split one.

       digits, punctuation, space   ->  the BIOS ROM CP437 8x8 font
       letters A-Z and a-z          ->  a 464-byte table inside EGATREK.EXE

   So the two previous versions of this item were each half right. CP437 *is*
   in there -- 8x8, pulled from the video BIOS at run time -- but the 58
   alphabetic slots are overridden by a table linked into the executable, one
   pixel wider and rounder than the ROM's. The full audit, the probe glyphs
   that locate each base, and the trap that makes a naive dump ship x86
   instructions as bitmaps are in MEASURED.md, "The font is two fonts".

   **The port needs both halves.** Taking only the ROM font gives correct
   digits, punctuation and box-drawing with visibly wrong letters -- and the
   letters are most of what a player reads.

   Two things deliberately left unverified rather than assumed: whether a real
   EGA card's 8x8 ROM font is byte-identical to the S3 BIOS DOSBox emulates
   here (the game reads these at run time, so on real hardware this half was
   whatever the player's card carried), and whose face the letter table is --
   Turbo Pascal's Graph unit is the obvious suspect and has not been checked.

   Output lands in `build/` and is **never committed**, same rule as
   `tools/gen_music.py`: the ROM half is DOSBox's table and the letter half is
   lifted out of Anderson's binary, and both regenerate from sources anyone
   using this repo has to fetch for themselves anyway.

   ### (b) Game symbols -- custom art either way

   The ship, Mongols, bases, stars, planets, the torpedo icons. These are pixel
   art in the original and they are custom art for us whatever font we use.
   Nothing to do with CP437, and this is the half where a custom charset
   actually earns its cost.

   **Borders are close to a wash.** PETSCII single-line box-drawing renders the
   frames well already, and unlike when this item was written those glyphs are
   no longer guesses -- `c128/test/test_panels.c` asserts their bitmaps against
   `chargen-390059-01.bin`.

   ### It is NOT disk-gated, unlike the briefing

   Worth stating because the opposite is the natural assumption. A full
   256-glyph set is 4K of data in a binary already 81% code against a 40K
   budget, and that *would* put this behind the disk seam. But the console
   needs roughly 96 glyphs -- uppercase, digits, punctuation, symbols -- at 8
   source bytes each, which is about 768 bytes, expanded to the VDC's 16-byte
   slots only during the upload. That fits in the binary. This item can be done
   whenever it is wanted.

   `commodore-uno/c128/src/vdc.c` has the upload path including the slot
   padding, and `tools/gen_charset.py` there is the generator precedent.
7. ~~**Game core, next pieces.**~~ Largely DONE: systems and repair, lasers,
   torpedoes, enemy fire and movement, docking, scoring, and a scheduled
   event queue all exist. Original text follows. Galaxy state, the distance table and the message
   log now exist. Still missing, roughly in dependency order: the 12 ship systems
   with percentage repair, combat (lasers and torpedoes), enemy AI and return
   fire, docking and resupply, then win/lose and scoring. `combat-model.md` has
   the 1978 formulas as the starting model — 1/distance falloff with a (2,3)
   random multiplier, the 15% no-damage threshold, ray-marched torpedoes with no
   accuracy roll, and `1000*(kills/stardates)^2` for score.
8. **Port order -- RESOLVED 2026-08-23.** The question this item used to ask
   (where does Atari VBXE sit relative to the Amiga, given they share a
   640x200x16 target?) is not answerable in that form any more. It was a
   two-way question from when those were the only two candidates, and its one
   distinguishing argument -- that VBXE was the only automatable leg -- was
   withdrawn on 2026-08-19 when `tools/vice_mon.py` proved otherwise. There are
   six Tier-1 targets now. Ranking six is a question we cannot answer yet and
   would gain nothing from answering.

   **The six are not comparable, so split them by what they cost.**

   *Text-mode siblings* -- X16, F256, MEGA65, and CoCo 3 down in Tier 2. Each
   puts the console on a hardware text grid with per-cell colour, so the
   platform layer is a rewrite of `c128/src/vdc.c` against the same four
   primitives. MEGA65's `m65native.c` already exposes `scr_put(x, y, ch,
   color)`, `scr_puts`, `scr_clear` and `wait_vsync` -- the same names with the
   same signatures. Whichever goes first, the rest follow nearly mechanically,
   and their order among themselves is close to arbitrary.

   *Bitmap targets* -- Amiga and VBXE. Both need a layer the other four do not:
   a font, a glyph blitter, and a dirty-cell scheme so we are not repainting
   2000 cells a frame. That is the same design work twice. Separating them by
   three text ports means designing it, forgetting it, and rediscovering it.
   They get planned together and built consecutively.

   So the original question does have an answer, just not the one it was
   reaching for: **VBXE sits next to the Amiga**, at the end, because they
   share a problem -- not because either is the hardest target.

   ### The portability argument for doing the Amiga early is weaker than it looks

   Worth writing down, because it is the instinct that would otherwise make the
   68000 leg port #2. The reasoning goes: a big-endian 32-bit target with a
   different compiler is what really stress-tests the core's portability
   contract. But most of that is already under continuous test:

   - `make port-check` compiles `core/` for the 68000 with `-Werror` on every
     `make all`, so the compile axis never goes unwatched.
   - The native test suite runs the core with a **32-bit `int`** already. The
     "int is wider than cc65's" case is covered. The genuinely dangerous
     direction is the narrow one -- a 16-bit intermediate overflowing -- and
     that is the target we run in an emulator every day.

   What is left that only a real 68000 run would find is **byte order**, and it
   has no consequence anywhere in this codebase today. It acquires one the
   moment `trek_state_save()` exists. That makes it a serialiser design
   decision, not a discovery to be made on a 68000 -- pin the order explicitly
   and it is byte-identical on every target and testable natively. See the byte
   order pin in the disk I/O seam below.

   ### The gate matters more than the order

   **Nothing starts until disk I/O lands on the C128.** Storage is the fourth
   and last seam -- video, keyboard, sound, storage. Begin port #2 before it
   exists and you implement three seams, discover the fourth, and go back
   through every port. That is the same argument already recorded for settling
   the disk seam once instead of discovering it three times; it applies one
   level up, to ports rather than features.

   ### The order

       C128 -> MEGA65 -> (X16, F256, then CoCo 3) -> Amiga + VBXE together

   MEGA65 as port #2 is a modest claim and the limits should be stated. It does
   **not** unblock the C128 briefing -- that still has to fit on the C128, which
   is what the disk seam is for. What it buys: the cheapest possible proof that
   the platform layer is genuinely a layer, the only target where the cc65
   charmap bug class does not exist at all, a toolchain and emulator both
   installed and both proven the same week, and binaries 2.3x tighter, which
   gives the briefing somewhere to be seen at full size before the C128 has to
   compress it.

   The Amiga going last is a deliberate reversal of the ordering at the top of
   this file. It stays valuable -- a fourth CPU family, a real bitmap, the port
   that would make this a genuinely cross-platform project rather than a
   6502 family tree -- but its *diagnostic* value is largely being collected
   for free already.

## Platform suitability (from the Uno lineup, revised 2026-08-22)

Ranked for *this* game, whose demands are the opposite of Uno's: high
information density, and colour carrying meaning rather than decoration -- red
for Mongols, green for a healthy system, yellow for stars, cyan for labels.

**Two hard rules.** At least **80 columns**, because the nine-panel console was
laid out on an 80x25 grid and paging it is solving a different game. And enough
**colour at that width** -- a machine that only reaches 80 columns by dropping
to two or four colours fails, because the colour is information.

That second rule is what ruled the stock Atari out of this project until VBXE
came into the picture, and it rules out more than it first appears.

### Tier 1 -- 80 columns and 16 colours, proven in the Uno lineup

| target | how | CPU |
|---|---|---|
| **C128 VDC** | 80x25 text, 16 colours per cell | 8502 |
| **Amiga** (OCS/ECS, KS2.0+) | 640x200 bitmap, 16 colours | 68000 |
| **Atari 800XL + VBXE**, HR mode | 640xN bitmap, 16 colours | 6502 |
| **Commander X16** | VERA text 80x60, per-cell fg+bg from 256 | 65C02 |
| **Foenix F256** | Vicky text 80x60, per-cell colour via CLUTs | 65C02 |
| **MEGA65**, native C65 mode | 80x25, VIC-IV H640, per-cell colour on all 2000 cells | 45GS02 |

The **stock Atari is out** and only VBXE brings it in -- ANTIC's text modes stop
at 40 columns. Exactly the same split Uno hit, for the same reason.

### Tier 2 -- capable, but at 80 columns Uno never went there

(MEGA65 was here until 2026-08-23, when Jamie built it -- see below.)

- **CoCo 3 -- VERIFIED 2026-08-22, and it qualifies.** Booted a real CoCo 3
  ROM in XRoar and typed a BASIC test: `WIDTH 80`, then `ATTR f,b` across all
  eight foreground and eight background values, with an 80-character ruler to
  confirm the width. The ruler fills the line and the capture holds **eight
  distinct hues**, which is the attribute byte's three bits each of foreground
  and background, indexing palette slots that are themselves reprogrammable
  from 64. Eight is exactly what the console needs -- white values, cyan
  labels, green healthy systems, yellow stars, red Mongols, magenta, grey,
  black -- so it fits, tightly, and the palette being programmable means we can
  pick the closest eight to EGA's set. A fourth CPU family (6809) with a
  toolchain already proven here (CMOC, XRoar).

### Tier 3 -- 80 columns exist, but the colour collapses

- **The whole Atari 16/32-bit line -- OUT, Jamie's call 2026-08-22.**

  ST, Mega ST, STE and Mega STE share the same three modes: 320x200 in 16
  colours (only 40 columns), 640x200 in **4 colours**, and 640x400 monochrome.
  So 80 columns costs all but four colours, or all of them. The STE's deeper
  palette widens the choice, not the count, and the Mega STE is an STE with a
  faster CPU and cache -- same Shifter. The escape would be per-scanline
  palette switching, but the console's panels sit side by side, so a single
  scanline crosses three of them and it would need mid-line changes. Demo work.

  **TT030 and Falcon030 do qualify on paper** -- TT Medium is 640x480 in 16
  colours and the Falcon's VIDEL reaches 640x480 in 256 -- and Hatari emulates
  both (`--machine tt|falcon`, `--tos-res ttmed`). Never verified here: the only
  EmuTOS image on this machine is the ARAnyM build, which bus-errors on TT
  hardware.

  It does not matter, because **the machines that could show the console are the
  ones nobody has, and the ones people own cannot show it.** Targeting TT and
  Falcon alone would be a port with almost no installed base. Do not re-open
  this.
- **MSX2.** TEXT2 (`SCREEN 0: WIDTH 80`) gives 80x24 but its blink attribute
  buys only a second colour pair -- four colours, same trap as the ST.

  **The SCREEN 7 idea, and an honest account of trying to check it.** SCREEN 7
  (GRAPHIC6) is 512x212 with 16 colours *per pixel* and no attribute clash, so
  a 6-pixel-wide font would put 85 columns in that 512 -- 80 with room over.
  The arithmetic is certain and the mode is textbook; what is NOT established
  is anything I measured myself.

  Tried it in openMSX on a stock Philips NMS 8250 (V9938, 128K VRAM) and did
  not get an answer. Two things went wrong and both are worth recording so the
  next attempt does not repeat them. **Typing into MSX BASIC is timing-fragile**
  -- the machine reaches its prompt somewhere between 30 and 60 emulated
  seconds depending on the run, and a `type` issued early is silently swallowed,
  which produced several runs that looked like SCREEN 7 failing when nothing had
  been typed at all. And **openMSX's raw screenshot size does not indicate the
  video mode**: the boot logo renders 640x480 and BASIC 320x240, which I read as
  512-wide versus 256-wide and it is not that. Setting R#0 and R#1 to GRAPHIC6
  through the debug interface took (they read back 0x0A/0x60) and the render
  stayed 320x240, so the screenshot size proves nothing either way.

  **What would actually settle it is a benchmark, not a datasheet check.** The
  mode's existence was never the real risk -- the risk is whether glyph
  blitting a full 80x25 console is affordable on a 3.58MHz Z80, even with the
  V9938's hardware blitter. That needs a cartridge built with SDCC, which the
  Uno msx2 port already has a working toolchain for, not another afternoon of
  poking BASIC. Until then MSX2 stays here rather than in Tier 1.

### Out

- **PET 8032** -- 80 columns and monochrome. Colour carries information in this
  game, so a mono port would be a different game.
- **Apple IIe** -- the 80-column card is text-only monochrome, and double
  hi-res colour is artifacted down to about 140 real colour pixels.
- **Everything at 40 columns or fewer**: C64, C64 OS, Plus/4, CBM-II 510,
  VIC-20, ZX Spectrum, TI-99/4A. An earlier version of this section listed the
  40-column colour machines as "workable, needs a paged UI". **That is
  superseded** -- the 80-column rule is the rule.
- **DOS** -- the original's own platform, reference oracle only, not a build
  target. Its EGA mode 10h at 640x350x16 is where the console came from.

### Text or bitmap, and why both are already proven

Uno shipped both approaches, and the split maps cleanly onto this lineup.
**Text-mode targets** (C128, X16, F256, MEGA65, CoCo 3) can put the console on
the screen directly, one panel character per cell, which is how the original
did it. **Bitmap targets** (Amiga, VBXE, and MSX2 if the narrow-font idea
holds) have to draw their own glyphs and pay for a font, and get finer panel
borders and free pixel artwork in exchange.

Neither changes `core/`. That is the whole point of the split.

## Driving the original under automation (2026-08-16)

Observational measurement — read a number off the screen, infer a rule — got
us the laser model, but it has two hard limits: every reading costs a human
keystroke and a pasted screenshot, and anything statistical (torpedo accuracy,
the random component of enemy fire) needs more samples than that can supply.

`dosbox-automation` (github.com/dosbox-automation/dosbox-automation) is a
DOSBox fork with an HTTP REST API: screenshots, scripted input, Lua, and
read/write access to guest memory and CPU registers. It ships Linux and
Windows binaries only, but it builds on macOS from source.

### Building it on this machine

Two obstacles, both environmental rather than the project's fault:

1. The `release-macos-arm64` preset hard-codes `"generator": "Xcode"`, and the
   Xcode generator is broken here — it fails to find a compiler even for a
   two-line CMake hello-world, so it is a CMake 4.4.2 / Xcode 26.6 problem.
   Configure by hand with Unix Makefiles and the preset's cache variables:

       cmake -S . -B build/rel-arm64 -G "Unix Makefiles" \
         -DIS_PRESET_USED=TRUE \
         -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
         -DVCPKG_TARGET_TRIPLET=arm64-osx \
         -DCMAKE_OSX_ARCHITECTURES=arm64 \
         -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
         -DCMAKE_BUILD_TYPE=Release

   All 15 vcpkg dependencies build clean; so does the project.

2. Resources are located relative to the WORKING DIRECTORY (`support.cpp`
   looks for `resources/`, and on macOS also `<exe>/../Resources`). There is
   no `install()` rule in CMakeLists.txt, so running the binary out of
   `build/rel-arm64/` finds no GLSL shaders and
   `set_fallback_shader_or_exit` aborts the process outright.

   This was first written up here as a packaging bug in the project. It is
   not -- verified by running from the source root with the copied shaders
   removed, where it starts cleanly and auto-selects `crt-hyllian:vga-1440p`.
   Either run with the working directory at the source root, or copy
   `resources/shaders` and `resources/shader-presets` into
   `~/Library/Preferences/dosbox-automation/`, which is what we did.

Two smaller traps: `DOSBOX_API_TOKEN` must be exactly 64 hex characters or it
is silently regenerated, and the MOUNT policy anchors on the directory of the
`-conf` file. Keeping the conf in `reference/` (already gitignored) is what
makes `EGATREK.EXE` reachable. Launch:

    DOSBOX_API_TOKEN=$(openssl rand -hex 32) \
      <build>/dosbox --noprimaryconf --conf reference/trek-auto.conf

### What the instrument can and cannot do

- `GET /video/frame?format=png` returns the screen, so readings no longer need
  a human in the loop.
- `GET /video/text` is USELESS for the game. EGA Trek runs in BIOS mode 16
  (640x350 EGA graphics), so `is_text_mode` is false and the damage figures
  are bitmap glyphs, not characters. Frames plus reading them is the route.
- `POST /input/type` types characters but does NOT submit; Enter has to go
  through `POST /input/sequence` as an explicit `KBD_enter` press/release.
- `POST /memory/search` takes an integer and a width, which is the wrong shape
  for this game -- see below.

### The state is Turbo Pascal reals, so scan, do not search

Searching for 2500 with shields visibly at 2500 returns ZERO matches, and
tenths (25000) and the warp factor as an integer both fail the same way. The
game is Turbo Pascal and holds its state as `real`: a biased exponent byte
then a 39-bit mantissa with an implicit leading 1. The integer search cannot
express that, so pull the region down with `GET /memory/{off}/{len}` and scan
it locally. `core/../scratchpad/scan.py` in the session did exactly this.

`GET /dos/internals` gives EGATREK's MCB -- segment 483, 338304 bytes -- which
bounds every scan to linear 7728..346032.

### The ship record

Scanning for known values lands the whole thing in one contiguous block, in
(current, maximum) pairs six bytes apart:

    181910, 181922, 181928   stardate 3500 (three copies)
    181934                   impulse 500
    181940 / 181946          energy 5000 / 5000
    181952 / 181958          shields 2500 / 2500
    181964 / 181970          warp 5.0 / 5.0
    181976 / 181982          9999 / 9999   (unidentified)
    182000                   3505.000      (unidentified)
    182012                   3506.699      (unidentified)

### There is probably no mission deadline

`MISSION_TENTHS` (30 stardates) has always been our invention -- the manual
never mentions a time limit and the console shows no such figure. Scanning the
entire 338KB for any real between 3500 and 3620 returns exactly three values:
3502.520, 3505.000 and 3506.699. There is no 3530, and nothing resembling a
deadline anywhere in the program's memory.

The two near-term values sit right after the ship record and look like event
timers rather than a mission end. Not yet conclusive -- a deadline could be
computed at end-of-game rather than stored -- but the balance of evidence is
that the mission simply runs until the Mongols are destroyed or the ship is
lost, and the scoring sheet's "Penalty for incomplete mission" fires on
quitting or dying with enemies left, not on running out of time.

## Mapping the Turbo Pascal runtime (2026-08-16)

Before reverse engineering the original, it is worth knowing which code is
Nels Anderson's and which is Borland's. Ghidra has no Turbo Pascal signatures.
dcc (github.com/nemerle/dcc) does -- `sigs/dcct5p.sig` -- but dcc itself
cannot get through this binary.

### What dcc actually does on EGATREK

It builds on macOS with Qt5, Boost, and `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`
(CMake 4.x rejects the bundled cotire module). Then, in order:

1. **It fails to recognise the compiler.** `pattBorl5Init` hard-codes the
   operand of a `mov [xxxx], es` as `0030`, but that is the address of a
   data-segment variable and varies per program; ours is `1CA8`. Every other
   byte of the Turbo Pascal 5 pattern matches. Wildcarding that operand, as
   the v4 pattern already does, yields "Borland Pascal v5.0 detected".
2. **Signature matching then works** -- it names INITIALISE, DoubleToFloat,
   FloatDivide, FloatMult, STACKCHK from the signature file.
3. **It walks into the game's string pool** at 0x75EE and reports the prose as
   "80386 instruction 64" (the letter `d`). Its unknown-opcode path calls
   fatalError and then sets a PROC_BADINST flag it can never reach, so the
   intent to abandon the procedure was there but unfinished.
4. Making that non-fatal gets roughly five times further and then
   **segfaults**. No output file is produced at any point.

Verdict: a 1994 research prototype against a 174KB multi-segment program with
4970 relocations. Not a route to decompiled C.

### tools/tp_runtime_map.py

The signatures are good even though dcc is not, so this reads the .sig format
directly and scans. Format is in the script's header; the only trap is that
the "ht" length field is numKeys * (SymLen + PatLen + 2) while the entries are
really SymLen + PatLen, so deriving the stride from that field misaligns every
record after the first. Two signatures need filtering: one is 23 zero bytes
and matches every run of zeros in the image.

    python3 tools/tp_runtime_map.py <dcc>/sigs/dcct5p.sig \
        reference/EGATREK_unpacked.exe

### The result

**78 distinct runtime routines**, named -- ASSIGNCRT, CLRSCR, GOTOXY, READKEY,
TEXTCOLOR, TEXTBACKGROUND, DELAY, SOUND, EXEC, GETENV, GETINTVEC,
SWAPVECTORS, BlockMove, Read, ReadEOL, CRLF, EXIT, INITIALISE and the
numbered float and string helpers.

They occupy ONE CONTIGUOUS BAND, 0x2239A..0x2899F of the load module. Nothing
matched below 0x2239A. So the boundary is clean:

    0x000000 .. 0x2239A   game code and data   140192 bytes  (81%)
    0x2239A .. 0x02A6E0   Turbo Pascal runtime  33600 bytes  (19%)

Note this CORRECTS the assumption that sent us down this path. "Most of the
174KB is Borland runtime" was wrong -- the runtime is a fifth of it, and the
game is the overwhelming bulk. The map is still worth having, because it draws
a hard line under 26KB of code that never needs reading, and it confirms
INITIALISE at 0x26920, which is exactly where we located the init routine by
hand while diagnosing dcc.

## Screen codes are checkable, so stop guessing them (2026-08-19)

`layout.h`'s box-drawing glyphs were derived by hand and shipped marked
UNVERIFIED. That was avoidable: VICE carries the C128 character ROM on disk
(`/usr/local/share/vice/C128/chargen-390059-01.bin`, 8 bytes per glyph,
screen code order), so any screen code can be rendered and read before it is
written into the source.

This came up filling in SYSTEMS STATUS. The bars were first drawn with
`G_BLOCK` (160), which fills all eight pixel rows of its cell, so six bars on
six consecutive rows fused into one solid green slab -- the panel stopped
showing six systems and started showing a rectangle. The original avoids this
by drawing 7-pixel bars on an 8-pixel pitch. Dumping the ROM found screen code
228, the reverse of the bottom-line glyph: solid across seven rows, clear on
the eighth. Same 7-of-8 pitch, one lookup, no guessing.

`c128/test/test_systems.c` now reads that ROM where it can find one and
asserts the glyph's actual bitmap, so an invented screen code fails on the
build machine instead of on screen.

## The event queue, and why it is integer to the bone (2026-08-19)

Ported from the ancestor's `events.c`. The design that transfers is a fixed
slot per event type rather than a real queue -- SST says so itself, "This
isn't a real event queue a la BSD Trek yet" -- and for a core that must run in
16 bits on a 6502 that limitation is a feature: no allocation, no list, one
array of dates and a linear scan of four entries.

What the queue buys is the thing EGA Trek's COMMUNICATIONS panel is full of:
deadlines. "The StarBase in 6-6 reports that it is under attack. They can last
until 3517.8" is a scheduled destruction with its date shown to the player.

### expran without floating point

`expran(mean) = -mean * ln(u)`. Three constraints decide the implementation
and they all point the same way:

- The core forbids `float`, `double` and `long`.
- `int` is 16 bits under cc65 and 32 on the 68000, so anything that leans on
  integer promotion computes a **different schedule on the Amiga than on the
  C128, silently, from the same seed** -- and the seeded PRNG exists precisely
  so that a seed reproduces a game everywhere.
- The 6502 has no divide instruction.

So: a 32-entry table of `-ln(u) * 32` sampled at the midpoint of each
thirty-second, indexed by the **top five bits** of the PRNG (a shift, never a
division), with the multiply split into high and low halves so no intermediate
leaves 16 bits. Table mean is 1.00 to two places.

It also saturates, which splitting alone does not give you. The deviate
reaches 4.16x its mean, so a mean above ~15750 tenths cannot be represented at
all. A wrapped result there is not a harmless wrong number -- it turns a
distant event into one firing within a few turns. No caller passes a mean that
large today; the clamp is there so none ever can.

### A test that nearly went in backwards

The first version of the overflow test asserted that a mean of 60000 never
produces a draw under 15000. That is wrong, and it failed honestly: for a mean
of 60000 a draw of 1875 is the correct short tail, `-ln(u) = 1/32`, which comes
up one time in thirty-two. A small result is not evidence of a wrap.

The property that actually separates them is monotonicity in the mean.
Reseeding before each draw fixes the table entry, so the only thing varying is
the mean, and a larger mean must never give a smaller draw. That catches
wrapping and passes the honest short tail.

## Front end and sound — not started (added 2026-08-19)

Everything so far is the console. The original wraps it in a front end the
port has none of, and makes noise the port does not. Captured from the running
original so these are a specification rather than a reminder; re-capture with
`tools/drive_original.py shot:name` at each step.

### 9. Title screen -- BUILT 2026-08-21

`Revision 3.0` small, top left. "EGA Trek" in a large outlined serif, "The
Mongol Invasion" beneath it. A line-drawn starship across the middle left. Two
panels along the bottom, both bordered in the console's style:

- **left** — the U.S.S. Lexington crest, *identical to the badge panel on the
  console*: ship name, RCB-92, the blue starred disc, "Dept. of Space". Ours
  already draws this, so the title screen reuses `ui_draw_badge()` rather than
  duplicating it.
- **right** — the shareware notice: registration fee, Nels Anderson's address
  and BBS number, "Copyright © 1992 by Nels Anderson".

The attribution panel is not optional decoration. This project's README credits
Anderson; the title screen is where a player sees it, and it should say the
same thing the original's does.

Any key advances.

### 10. Setup screen -- BUILT 2026-08-21

One screen, heading "U.S.S. Lexington / RCB-92" in a script face, with prompts
appearing in sequence down the left and earlier answers staying visible:

    Welcome aboard Captain!

    Will you require a briefing <Y/N>?          -> item 11
    Restore a saved game <Y/N>?                 -> OUT OF SCOPE, see below
    Please enter your name: KIRK
    For verification, enter your command level (1-5): 3
    Captain, please enter self-destruct password: ****

Notes worth having before building it:

- The **command level** already drives `trek_new_game(level, seed)` and the
  enemy-count band, so this prompt is wiring, not new mechanics.
- The **self-destruct password** is the SELF command's confirmation. There is
  no self-destruct in the core yet, so the prompt can be collected and stored
  before the command exists.
- The **name** is for the hall of fame in `TREK.SCR`, which is also not built.
- This is the natural home for **seeding the RNG from something varying**,
  which is open item 3 and currently blocked on there being a title screen at
  all: time the player's keystrokes through these prompts.

**Save and restore are deliberately out of scope for now.** The "Restore a
saved game" prompt still needs to appear and take N, because the sequence
reads wrong without it, but nothing behind it.

### 11. Briefing pages -- CAPTURED 2026-08-21

All fifteen pages are captured to `reference/shots/b*.png`. Contents worth
knowing before building them: the crew is **387 enlisted and 43 officers**
(which is the 430 the score sheet charges for losing the ship, confirmed
independently); the ship is a "heavy Research/Battle Cruiser, Class IX"; and
the chart's three digits are stated outright as "the number of Mongols, **base
type**, and number of stars" -- a type, not a count.

CORRECTED 2026-08-22: this item used to end "which our chart currently gets
wrong". It does not; `ui_draw_chart()` prints `gal_base[q]`, which is the type
enum. What is still unverified is the NUMBERING -- ours is 1 StarBase,
2 Research, 3 Supply, and nothing has ever checked that against the digit the
original prints. Every capture so far has a 0 in that position.

The pages are reached with Y at the first setup prompt, advanced with Enter and
abandoned with Q, and the prompt line says so: `(Hit "Enter" for next page or
"Q" to quit briefing)`.

Shown only if the player answers Y. Paged, with "(Hit "Enter" for next page or
"Q" to quit briefing)" and Q returning to the setup sequence at the restore
prompt. At least eleven pages; the section headings observed, in order:

    (intro -- "Good morning, Captain...")
    INTELLIGENCE REPORT
    RESEARCH/BATTLE CRUISER
    NAVIGATION SECTION
    ENGINEERING SECTION
    WEAPONS SECTION
    DEFENSE
    SCANNERS
    SCANNER EXAMPLES
    COMMUNICATIONS
    ... (capture ran out at ten; there are more)

Two things this costs. The text is **long** -- eleven-odd pages of three or
four paragraphs each -- which on a 6502 is a real chunk of the binary and
probably wants compressing or paging off disk. **Sized and designed 2026-08-22:
see "Briefing pages: where the text can live, and why not an REU" below.** The
short version is 7,574 bytes of prose against 1,688 bytes free, so it cannot go
in the binary at all, and disk I/O is the prerequisite. And it is **Anderson's prose**,
which is copyrightable: the same rule already recorded for the message
catalogue applies, so these are a specification of *what each page covers*,
not text to copy.

### 12. Sound

The original uses the PC speaker. The C128 has a SID, the Amiga four channels
of sampled audio, and the Atari POKEY, so this is the one area where every
port can beat the original outright rather than approximate it.

~~Nothing is captured yet~~ -- **the note data is extracted, 2026-08-22**. It
never needed an audio path: the original is Turbo Pascal driving the PC
speaker, so the binary holds exact frequencies and durations rather than sound.
Two music tracks (title, 45.5s; end of game, 43.5s), five short effect tracks
and two procedural sweeps, all dumped by `tools/extract_music.py`. Format,
derivation and the 10Hz pitch quantisation are in MEASURED.md.

Nothing extracted is committed -- the notes are Anderson's work like the prose.
`[0x1cc8]` is the original's sound on/off byte, which is what `SND` toggles.

What remains for this item is the port side: a SID driver, and deciding which
core events make which noise. Original text follows.

Nothing is captured yet -- the emulator driver has no audio path, so working
out what the original plays and when is its own task. Likely: firing, hits,
red alert, docking, destruction, and the death-pod arrival.

Design constraint from the start: sound belongs in the platform layer, not the
core. `core/` must not gain an audio call. The event list the core already
returns (`EV_HIT`, `EV_ENEMY_MOVED`, `EV_BASE_LOST` and the rest) is the right
seam -- each platform decides what noise an event makes.

## 13. The command set: twenty of twenty-five (added 2026-08-19, count updated 2026-08-23)

`reference/EGATREK.REF`, the quick reference card, is the authoritative list
and it is short enough to reproduce whole.

| command | what it does | state |
|---|---|---|
| `D)ock` | dock with a StarBase | **done** |
| `L)asers` | fire lasers | **done** |
| `M)ove` | move to quad/sector | **done** |
| `Q)uit` | quit | **done** -- asks `QUIT <Y/N>?` on the COMMAND line first, as the original does |
| `T)orps` | fire torpedoes | **done** |
| `W)arp` | set warp speed | **done** |
| `E)nergy` | energy transfer | **done** (4de6758) |
| `SHUP` / `SHDN` | shields up / down (arrow keys) | **done as words** (4de6758); **the ARROW KEYS are still unbound** -- MEASURED 2026-08-24, both `EGATREK.REF` ("Shields Up (use up arrow)") and the in-game F1 help give the arrows as the primary binding. The matrix has them since `MSGS`; this is a two-line change and the last outstanding correction to an implemented command |
| `MAX` | divert maximum energy to shields | **done** (4de6758) |
| `R)epair` | state of repair report | **done** (4799972) -- its own screen, not the dialog: twelve systems plus headers do not fit thirteen lines |
| `MSGS` | review old messages | **done** 2026-08-23 -- 32-entry log in VDC RAM at $1000, scrolling viewer, opens at the bottom, ESC only. Built to the capture; see MEASURED.md |
| `C)hart` | chart of known galaxy | **done** 2026-08-23 -- redraws the chart panel and costs no turn, which is exactly the no-op the original performs on a console already showing it |
| `F)ix` | control which system engineering repairs | **done** 2026-08-23 -- `ENGINEERING` asks for one system by number, `L` lists all twelve in two columns, `0` aborts. `REPAIR_FOCUS_FACTOR` is DERIVED and carries the experiment that would settle it |
| `INFO` | info on enemy in current quadrant | **done** -- class, sector, range, bearing and strength as a percentage, SPACE to step |
| `HAIL` | hail a StarBase | **done** 2026-08-23 as measured -- costs a turn and posts an empty COMMUNICATIONS entry. What it says with a base IN RANGE is still uncaptured, so nothing is invented |
| `A#` | acknowledge message # | **done** 2026-08-23 -- `A1` dismisses the first panel box, bare `A` clears all, out of range is a silent no-op, costs no turn, and the message stays in the log. No numbers are drawn on the boxes, because the original draws none |
| `S)elf` | self destruct | **done** -- EGA Trek's own sequence and its ESC abort, RECONCILED 2026-08-22; the blast model is still the ancestor's kaboom() |
| `RAY` | death ray | **fully specified** 2026-08-24 -- refusal with no enemies, the WEAPONS CONTROL warning, `Preparing`/`Firing!`, and four outcomes of which the fourth DESTROYS THE SHIP and prints a Top Secret loss report this port does not have. See MEASURED.md |
| `O)rbit`, `LAND`, `USE` | planets, landing, crystals | MEASURED 2026-08-23: `O` needs adjacency like docking and names the planet and its Type; `LAND` offers Shuttle Craft or Transporter, which is why both are repair entries. The rescue path works end to end and scores +200 |
| `SAVE` | save game | **done** 2026-08-24 -- `SAVE GAME` box with `EGATREK.SAV` as the default, closes on one keypress as the original's does, costs no turn. Restore is on the setup screen and skips the rest of it. Verified end to end: save, destroy the ship, play again, restore, identical galaxy |
| `SND` | toggle sound | **done** -- toggles the same flag the original keeps at DGROUP+0x1cc8 |
| `Shift-F1` | boss mode | **NOT PORTED -- Jamie's call 2026-08-24.** Fully specified: it shells out to `COMMAND.COM` via `GetEnv('COMSPEC')` and `EXIT` returns. There is no C128 equivalent and the approximation is not worth having; see "Boss mode is not being ported" |

Plus the function keys, which are shortcuts rather than new commands:
F1 Help, F2 Lasers, F3 Fire Torpedo, F4 Move Ship, F5 Max Energy, F6 Fix
Systems, F7 Xfer Energy, F8 Repair Status, F9 Set Speed, F10 Dock.

~~**Three of these are wiring, not features.**~~ CLOSED in `4de6758`,
"Shields and energy transfer: the port can defend itself now". `E)nergy`,
`SHUP`/`SHDN` and `MAX` are all dispatched from `c128/src/main.c` and the
table above marks them done. Original text follows, because the *reasoning*
still applies to whatever is in this state next.

**Three of these are wiring, not features.** `trek_divert()` is written and
tested with no way to call it, and `ship.shields_up` is modelled and read by
the enemy turn with no way to change it — so the port currently cannot raise
its own shields. That is a bigger hole in how the game plays than anything on
the front-end list, and it is the cheapest thing here to close.

**The pattern worth keeping:** a core function that is written, tested and
unreachable reads as finished on every instrument this project has. The native
suite passes, `port-check` passes, and the game still cannot do the thing.
Nothing catches that but reading the dispatch, so a mechanic is not done until
a key reaches it.

### What EGA Trek dropped from the ancestor, and why it matters

Comparing the card against SST's `commands[]` is more interesting than the
missing list, because it shows what the console IS.

Four SST commands do nothing but print state — `SRSCAN`, `STATUS`, `LRSCAN`,
`REQUEST` — and EGA Trek has none of them. They became panels. That is the
whole design difference between the two games in one observation, and it is
why the nine-panel console is not decoration: it replaces a quarter of the
ancestor's verb set.

Others were merged rather than dropped: `DAMAGES` became `R)epair`,
`DESTRUCT` became `S)elf`, `DEATHRAY` became `RAY`, `IMPULSE` folded into
`M)ove` as a sector-only move, `SHIELDS` split into `SHUP`/`SHDN`/`MAX`, and
`SENSORS`/`TRANSPORT`/`MINE`/`CRYSTALS`/`SHUTTLE`/`PLANETS` collapsed into
`O)rbit`/`LAND`/`USE`.

A few have no EGA Trek equivalent at all and are worth noting so nobody adds
them by reflex from the source: `REST` (SST's way of passing time to repair --
EGA Trek uses `F)ix` instead), `ABANDON`, `PROBE`, `COMPUTER`, `EMEXIT`,
`SEED`, `SCORE` as a command. `MAYDAY` may be EGA Trek's `HAIL`; SST scores
"calls for help from starbase" at -45 each and EGA Trek's sheet has no such
line, so if it is the same thing it is not scored.

## 14. Mechanics with no implementation at all (added 2026-08-19)

Distinct from item 13, which is about commands. These are the systems behind
them, and each is a chunk of core work:

- **Planets** — `O)rbit`, `LAND`, `USE`. Energium crystals, landing parties,
  the transporter and shuttlecraft (which are two of the twelve repair
  entries already modelled and currently mean nothing). The ancestor's
  `planets.c` is the reference.
- **The death ray** — `RAY`. "Destroy every enemy ship in the whole
  quadrant...if it works. If it doesn't work, there's no telling what may
  happen" (manual l.573-577). The ancestor's `deathray()` has the failure
  table.
- **Self destruct** — `S)elf`, gated on the password collected at setup.
- **Rescues** — worth **+200 each**, ~~currently unreachable~~ **CAPTURED
  2026-08-23 and confirmed at exactly +200**: a COMMUNICATIONS deadline
  message, warp there in time, move adjacent, `O)rbit`, `LAND`, choose
  transporter or shuttle. The evacuation deadline messages already seen in the original
  ("Planet Gallista-8, quad 8-4, requests evacuation. They can only hold out
  until 3516.5") are the trigger, and the event queue can already carry them.
- **Stars destroyed** — **-5 each** in the rubric. Needs a torpedo that can
  hit a star rather than only an enemy.
- **Boarding parties** — in the extracted string catalogue, absent from the
  manual. See MEASURED.md open item 11.
- **The hall of fame** — ~~`TREK.SCR`, top two scores per command level.~~
  The SCREEN is built (2026-08-21) and shows the current game's entry in its
  rank row; the file is not written, because that needs disk I/O and is
  deliberately absent along with save and restore. `TREK.SCR`'s format is
  known: ten records of a 25-character name padded with '.', CRLF, the score in
  decimal, CRLF — line-oriented text, not fixed records, since the score line
  varies in length. **This is the FIRST thing to build once disk I/O exists**,
  because it is the smallest real file and puts almost nothing at stake — see
  "Disk I/O: the seam, decided before anyone starts".

## 15. What is left to take from the ancestor, and what is not (added 2026-08-19)

An audit so this does not get re-derived. `reference/sst2k` has been mined for
combat, movement, repair, events, docking and scoring. Of what remains open:

**Still available, portable nearly whole**

- `planets.c` -- orbit, beam, mine, usecrystals, shuttle, survey, sensor. That
  is `O)rbit`, `LAND` and `USE` in one subsystem, and it gives the transporter
  and shuttlecraft repair entries something to mean.
- `deathray()` -- success probability and a failure-method table, which is
  what the manual's "there's no telling what may happen" needs.
- `kaboom()` -- self destruct. `whammo = 25 * energy`; every enemy where
  `kpower * distance <= whammo` dies.
- Torpedo damage, and enemy displacement on a non-fatal hit -- see MEASURED.md.
- `nova()` / `supernova()` -- a torpedo hitting a star, which is the -5
  scoring term.
- `doshield()` and `damagereport()` -- energy accounting and report content
  for SHUP/SHDN/MAX and `R)epair`.

**Available as shape only**

- ~~Enemy hit points as rolled bands rather than constants.~~ SETTLED
  2026-08-21: they are fixed per class. Do not port SST's bands. The
  readings that looked like variation were ships a death pod had already
  been through.
- Enemy counts per skill, for MEASURED open item 1.
- `mayday()` as a rough model for `HAIL`, though the semantics differ.

**Not there at all -- do not go looking again**

- **Sound.** Checked, hoping otherwise. SST's entire audio is three effects --
  500 Hz on a kill, a 50 Hz warble on teleport -- and they are ESR's
  curses-era additions, not the FORTRAN original's. EGA Trek's PC speaker
  repertoire is Anderson's own and has to be captured from the running game,
  which the emulator driver has no audio path for yet.
- **Rescues** (+200 in the rubric) and **boarding parties**. No equivalent.
  Anderson's own.
- `MSGS` and `A#` -- SST is a scrolling terminal and keeps no message log.
- `F)ix` -- SST repairs every damaged system evenly with no priority choice.
- `INFO`, the title screen, the briefing prose, the hall of fame. SST has
  `plaque()`, a printed certificate, not a high score table.
- The custom charset, the port order, the C128 return-to-BASIC bug. Platform
  work; the ancestor is a terminal program. (The last two are settled now --
  the bug was never real, and the order was decided on 2026-08-23 -- but they
  are listed here because the ancestor contributed nothing to either.)
- The DERIVED numbers still waiting on the game. The source cannot confirm its
  own guesses -- that is the whole point of the predict-then-verify loop.

The pattern: the source is largely mined for **mechanics**. What remains
divides into Anderson's own inventions and platform work, and it helps with
neither.

## 16. ~~Isolate why the KERNAL keyboard is dead in this port~~ RUN 2026-08-19

`c128/src/input.c` scans the CIA1 matrix directly because `cgetc()` blocks
forever here -- observed, and real. But the *cause* was never isolated. The
comment reaches for `commodore-uno`'s C128 port, which hit the same wall and
settled on the same answer.

**Uno's cause cannot be ours.** Its `test_kbd.c` and `test_kbd2.c` are a
controlled pair: identical programs, the only difference being a switch to VIC
bank 2 (`CIA2_PRA` bank select plus `VIC_MEMCTL`). That is what killed
`kbhit()`/`cgetc()` there. This port never touches the VIC at all -- it drives
the VDC, and `vdc_init()` does three things: write VDC registers, set 2MHz,
clear the screen. It does not disable interrupts and it does not switch banks.

So we adopted a fix by analogy and never found our own reason.

### Why it is worth an hour

If the KERNAL keyboard can be made to work in this configuration, **VICE's own
KEYBOARD_FEED works**, and the whole `kb_inject` apparatus goes away -- the
`#ifdef`, the separate debug binary, the `#pragma optimize`, and the failure
nobody has explained (see the header of `tools/vice_mon.py`). The C128 leg
would get the same automated loop Atari and Amiga are going to have, without
a debug affordance in the source at all.

And if the answer turns out to be "2MHz mode breaks it", that is worth knowing
on its own account. It would be a constraint on the whole port, not just on
testing.

### The experiment

Uno's two files are the right template: a minimal program, one variable
changed. Build three tiny PRGs, each a `for(;;) if (kbhit()) show(cgetc());`
loop drawing to the VDC or the 40-column screen, and find the step that kills
it:

1. Bare `cgetc()` loop, nothing else. Establishes the baseline works.
2. Add `C128_CLKRATE |= 0x01` (2MHz). Prime suspect -- though note the
   KERNAL's IRQ is CIA1-timer-driven and the CIA runs at 1MHz regardless, so
   the obvious theory is not obviously right.
3. Add the `vdc_init()` register writes.

Then whatever remains: cc65's C128 startup, which nobody has looked at.

**This is now cheap and needs no human.** `tools/vice_mon.py` screenshots the
VDC and reads memory, so it is three builds and three screenshots. That is the
difference from when the wall was first hit -- back then it needed someone to
watch the emulator and report back.


### Result of item 16: the hypothesis was wrong, and so was the instrument

**The bisect found nothing.** Three test programs -- a bare `kbhit()`/`cgetc()`
loop, the same plus 2MHz, the same plus 2MHz and `vdc_init()`'s VDC register
writes -- and the KERNAL keyboard works in **all three**. Neither suspect is
guilty. Whatever kills `cgetc()` in the real port is somewhere else, and is
still unexplained.

The tests store the key in a global and are read out through the binary
monitor rather than printing anything, which is what makes them work at 2MHz
where the VIC picture is blanked. They are in the session scratchpad, not the
repo; the pattern is three files sharing a `RUN_LOOP` macro and a second
module holding the globals -- separate module because cc65 lists a symbol in
the link map only if another module imports it.

**But the experiment found something much bigger by accident.** The baseline
test reported the keyboard dead while a leftover `$41` in `last_key` proved a
key HAD arrived. Chasing that contradiction found the real fault:

**VICE's binary monitor stops the machine on its first command and leaves it
stopped.** Reads keep succeeding and return a frozen snapshot; the CPU never
advances; anything injected is never processed. `CMD_EXIT` resumes it.

That single fact invalidates a run of conclusions reached earlier the same
day, all of them recorded here and in commit messages as if established:

- "VICE's KEYBOARD_FEED does not reach this port" -- it does, once resumed.
- "the machine is hung, PC pinned at one address" -- it was stopped, by me.
- "kb_inject is never consumed" -- it is, in **0.02 seconds**.

`tools/vice_mon.py` now resumes after every operation, and the C128 leg has a
full automated loop: type a command, screenshot the result, read memory.
Verified end to end by typing `W30` and seeing "ENGINEERING: WARP SPEED SET"
appear in the message stack.

**The lesson is the one this project keeps relearning.** A silent instrument
that returns plausible values is worse than one that fails, and every wrong
conclusion above was stated with more confidence than the evidence carried.
The jiffy clock at $A0 is the check: if it does not advance between reads, the
machine is stopped and nothing else read that session means anything.

### What this leaves open

`cgetc()` blocking in the real port is still unexplained, but it now matters
much less -- `kb_inject` works, so the automated loop does not depend on
fixing it. If it is revisited, the remaining candidates are the port's own
size and memory layout, or cc65's C128 startup, neither of which the bisect
touched.

## Front end and mechanics, observed directly (2026-08-20)

Captured while measuring enemy motion. Several of these close questions that
items 9-14 were carrying as guesses.

### The setup screen, item 10, in full

Five prompts, in this order, all on one page under the "U.S.S. Lexington /
RCB-92" heading, with "Welcome aboard Captain!" above them:

    Will you require a briefing (Y/N)?
    Restore a saved game (Y/N)?
    Please enter your name:
    For verification, enter your command level (1-5):
    Captain, please enter self-destruct password:

The name prompt is one we did not have on the list at all. "For
verification" is the original's phrasing for the level, and the password is
free text.

### The title screen animates, item 9

The Lexington fires a red laser at a Mongol ship on the left of the frame,
so the title is not a static image. "Revision 3.0" sits in a bar top-left,
the shareware notice in a green panel bottom-right, and the Dept. of Space
badge bottom-left -- the same badge the console carries.

### Hall of fame, item 14

Five slots, one per command level, named by rank rather than numbered:
Lt. Commander, Commander, Captain, Commodore, Admiral. Each row is name,
rank, score. So it is one high score per difficulty, not a top-five table.

### Black holes exist, and nothing in item 14 mentions them

A warp 5 move to an adjacent quadrant ended the first game outright: "U.S.S.
Lexington pulled into black hole & destroyed this stardate, with loss of all
aboard." The same route at warp 1 had stopped politely -- "Blocked by object
at 1-6" -- so **low warp stops at obstacles and high warp does not**. That is
a mechanic we model no part of: no black holes in the galaxy, no speed-
dependent collision behaviour.

### Damaged computer changes how M works

With the navigation computer damaged the M command stops accepting
coordinates and prompts `DeltaX:` / `DeltaY:` with "Computer Damaged; Manual
Only" -- the relative-movement mode the manual documents at l.515-529, which
we had read as an option the player may prefer. It is also a *consequence of
damage*, and the only way to move while the computer is out.

Related: "ENGINEERING: Move aborted; impulse engines are too damaged to use"
is a hard refusal, not a slower move.

### Torpedoes are drawn as icons

The STATUS panel shows a 3x3 grid of red torpedo pictograms, not a number --
nine of them on a fresh game, confirming TORPS_START. Our C128 panel prints a
figure where the original draws the rack.

### The MAIN VIEWER cycles

Three different things were seen in it: the enemy silhouette with class name
("MONGOL BATTLESHIP", "ARRAY MONITOR 504"), a twelve-row systems list at
100%, and a power-distribution readout (PMAX/PAVL/PPCT, "POWER DISTRIB 509").
It is a multi-page display, which is more than the single enemy panel we
built.

### The laser heat gauge, and a lesson about instruments

The LASERS panel's Temp bar, photographed immediately after a single volley
of exactly 400 units, filled 26 pixels of a 121-pixel track -- about 21% of a
scale labelled 0 / 1000 / 1500. Efficiency stayed at exactly 100%, proved not
by the gauge but by the shot itself: the damage matched `energy * (1 - d/12)`
to the unit, which it only does at full efficiency.

So a 400-unit volley does not degrade the lasers, which is consistent with
1500 being the overheat threshold and nothing happening below it.

The gauge scale itself did NOT come out of this, and the reason is worth
recording: the tick labels are character cells, so their positions quantise
to 8 pixels and three different alignment assumptions give scale maxima from
1500 to 2000. Measuring a number by photographing the bar that displays it is
the wrong instrument when the number is sitting in memory. Next session,
find laser heat in the ship record and read it.

### Enemies migrate between quadrants (2026-08-21)

Watched directly. After clearing quadrant 7-6 of three battleships the counter
read "Mongols: 34", and the quadrant held one Commander. Several turns later a
**second enemy was firing at us from 3-5**, holding 320 hit points, and the
Mongol counter still read 34 -- so it moved in from another quadrant rather
than being spawned. Killing it took the counter to 33.

We model nothing of this. `gal_enemies[]` is fixed at galaxy generation and
only ever decreases. The ancestor moves commanders between quadrants on a
scheduled event (`FSCMOVE`), which is the obvious shape to steal, and it is
probably the same mechanism behind item 14's "the commander has moved"
scanner report.

It also means a quadrant the chart calls empty need not stay empty, which
matters for how the chart is presented.

### Two smaller observations from the same session

**Supply ships may not count as enemies on the chart.** The chart showed
quadrant 8-3 as `002` -- no enemies, no base, two stars -- while the enemy
table for that quadrant held a supply ship with 120 hit points. Either supply
ships are excluded from the count or the chart had not refreshed; worth one
deliberate check, because our chart counts every hostile.

**Something other than energy can end the game.** A shields-down block ended
with the ship destroyed and the program back at its setup screen, on a turn
when energy had been pinned to 5000 immediately beforehand and the incoming
hits were about 545. Energy cannot have reached zero. Our core only ever
destroys the ship when a hit exceeds remaining energy, so whatever happened
has no equivalent in the model. Life support reaching zero is the obvious
candidate and was NOT ruled out -- the systems were pinned at the start of the
turn, not during it. Unexplained, and worth isolating before anyone trusts
the port's death condition.

### Enemy movement is triggered by firing, not by the turn (2026-08-21)

Watched directly, and it settles the shape of open item 14 even if not its
constants. Across the torpedo runs the enemies moved **after every volley** --
consistently, ship after ship, to the point where aiming at a sector became
useless because the occupant had left by the next shot.

Set against the previous session: twenty-six consecutive turns in which our
only action was a one-sector impulse move produced **no motion at all** from
two enemies, with shields up and down and the clock forced across a scheduled
event.

So the trigger is the player attacking, not a turn elapsing. That is a
different mechanism from the ancestor's, where `moveklings()` runs from the
turn resolution regardless of what the player did, and it means our
`enemies_move()` is called in the wrong place as well as computing the wrong
thing.

The cheap confirming experiment is now obvious: alternate fire-turns and
move-turns from a fixed sector against one enemy and watch position only.

### The planet list exists, and planets have classes

The MAIN VIEWER cycles to a "PLANET LIST" page:

    5-4N Gallisto-5      6-4N Cygnus-6      6-4M Gallisto-6
    7-1N Andromeda-7     7-6O Sigma-7

Quadrant, a class letter, and a name. The letters seen are N, M and O, which
is the Star Trek planet-class convention -- M being the habitable one. So the
planets in item 14 are not just a mechanic to port from `planets.c`; they are
already named and classified per galaxy, and the console has a page for them.

### The hall of fame file is plain text, and needs no emulator (2026-08-21)

`reference/TREK.SCR` is 300 bytes of ASCII, ten fixed records of:

    <25 characters of name> CRLF <score> CRLF

Ten records is **two per command level**, which matches the Hall of Fame
screen: five rank rows, each showing two entries with the second in a smaller
face. An empty file is dots for the names and `0` for the scores, which is
also exactly what the screen renders, so the display is a direct dump of the
file with no formatting logic in between.

That closes the hall-of-fame half of item 14 without a measurement session --
it was sitting in `reference/` the whole time.

### Only commanders move (2026-08-21)

Jamie watched a full session and the pattern is clean: **only Commanders ever
change sector**. Battleships, scouts and supply ships sit still however hard
they are provoked.

That is exactly the ancestor's own gate, which had been read out of
`moveklings()` and then not acted on: it runs `movebaddy()` for the commander
and the super commander unconditionally, and for ordinary ships only at expert
skill. Our port moved every enemy.

Combined with the trigger found the same day, the rule the core now implements
is: **commanders only, and only on turns when the player fires**. The console
narrates each one -- "The commander has moved. He is now at 1-3" -- which is
why the message is commander-specific.

### A third way the native tests can pass while the C128 build is wrong

The title screen's two plates and the repair report all lost their bottom
border on the VDC while `c128/make test` passed every assertion, including
ones that read the bottom-left corner cell back and found `G_BL` exactly where
it should be.

The drawing helper used a nested loop with a conditional per cell:

    if (y == 0)          c = corner or hline;
    else if (y == h - 1) c = corner or hline;
    else if (x == 0 ...) c = vline;

Native `cc` renders that correctly. The `cl65 -O` build dropped the `h - 1`
row. Rewriting it with `scr_hline`/`scr_vline` -- the shape `draw_panel()` in
layout.c has always used, and which has always been right on hardware -- fixed
it immediately.

The cause was not isolated further; `-O` versus codegen generally was not
separated, and it is recorded here as a shape to prefer rather than a compiler
bug proven. What matters is the pattern, which this port has now hit three
times: **a test that runs under `cc` is not evidence about the program `cl65`
produces.** The other two were the PETSCII key table (which is why
`make verify` exists) and the `enemy_step` out-parameter that crashed cc65
outright.

Jamie spotted this one by asking whether the same borders were missing from
the main play screen. They were not -- `draw_panel()` was always fine -- but
the question is what sent me to compare the two code paths instead of trusting
a passing test.

### S was bound to the wrong command (2026-08-21)

The port had `S` toggling shields. The reference card gives `S` to **S)elf
destruct** and spells shields out as `SHUP` / `SHDN` / `MAX`. Adding self
destruct forced the collision into the open, so the dispatch now matches whole
words before single letters -- `SHUP` begins with S, and getting that order
wrong would put the ship's destruction one keystroke from raising its shields.

The word matcher needed `& 0x7F` on the literal it compares against. cl65
translates string literals to PETSCII, where a letter sits 0x80 above ASCII,
while the keyboard scanner returns ASCII. The first version compared them
directly and `INFO` silently fell through to the unknown-command help. This is
the same mismatch that once put the key table in PETSCII and made `q` a no-op,
which is why the existing dispatch had always used numeric KB_ constants.

## The exit bug that was never there (2026-08-22)

Open item 2 -- the oldest thing on the list, dating from the first commit that
had a core in it -- said returning to BASIC wedged the C128, and named three
suspects: the 2MHz switch, the direct VDC register writes, and scanning CIA1
behind the KERNAL's back. It carried a bisect recipe and sat untouched for a
week because, in its own words, there was "no way to observe the machine from a
session on this host".

That clause is the whole story. It was already false when it was written --
VICE's binary monitor was there all along, and `tools/vice_mon.py` has been
driving it since 2026-08-19. The bug was never diagnosed. It was assumed, and
the assumption shipped a workaround to every player: *RUN/STOP + RESTORE FOR
BASIC*.

### Running the recipe

`c128/test/exit_bisect.c` is the bisect the note asked for, one suspect per
stage, in a program small enough that nothing else can be blamed:

| stage | adds | returns to BASIC |
|---|---|---|
| 0 | nothing -- just `return 0` | yes |
| 1 | + 2MHz on and back off | yes |
| 2 | + `vdc_init()` / `vdc_shutdown()` | yes |
| 3 | + a CIA1 matrix scan, SEI/CLI and all | yes |

All three suspects are innocent. So `tools/exit_real.py` built the whole port,
played it through the title, the setup screen, a quit and both end-of-game
screens with `kb_inject`, and asked again. BASIC came back and answered
`PRINT 6*7` with `42`.

### The instrument, and the trap it exists to avoid

**A wedged C128 still shows READY.** BASIC printed it before the program was
RUN and nothing erased it. Liveness is no better: the KERNAL's 60Hz IRQ keeps
bumping the jiffy clock long after BASIC is dead, so `vice_mon.py live` says
yes to a machine that will never take another command. The only question worth
asking is one only a live BASIC can answer, which is why both scripts type a
sum and look for the answer in 40-column screen RAM. `42` cannot come from the
echo of what was typed.

Feed it as UPPERCASE ASCII. VICE passes those bytes through as PETSCII, and
PETSCII `$50` is the `P` BASIC's parser wants; lowercase `$70` lands in the
shifted range, which *displays* as an uppercase P on the boot charset and then
earns a `?SYNTAX ERROR`. That error was the first proof the machine was alive,
so the wrong case was useful once and then had to go.

### The false positive, reproduced by accident

The first run of `exit_real.py` reported WEDGED. It was wrong. The scripted
keys had desynced -- `kb_inject` is a single byte with no handshake, and
vice_mon's own notes say reading it back lies -- so the port never reached the
quit at all and was still sitting in `kb_waitkey()` with the console up.

**A program blocked on input is indistinguishable from a wedged machine.** Both
ignore the keyboard; both leave READY on the other screen. That is almost
certainly what was recorded as this bug in the first place, and it is the same
class of error as the "input injection does not reach this port" and "the
machine is hung" conclusions of 2026-08-19, which turned out to be artefacts of
a monitor that had stopped the CPU. Three times now the instrument has been the
thing that was broken.

Fixing it took one line. Believing the note took a week.

## Play again, and the two keys that were never there (2026-08-22)

The end of a game is now a loop rather than a door: title, setup, game,
evaluation, hall of fame, `PLAY AGAIN?`, and YES puts you back on the title
screen with setup asked again. That is what the original does, measured -- see
MEASURED.md. NO exits to BASIC, which is only possible at all because the exit
bug turned out not to exist earlier the same day.

### Y and N did not exist in this port

`input.c`'s CIA1 matrix table had M W Q L T D E S X R, the digits, period,
comma, space, RETURN and DELETE. **No Y and no N** -- and `ask_yes()` has been
on the setup screen since it was built, asking two questions the player could
only ever answer by pressing RETURN, which it reads as no.

So `WILL YOU REQUIRE A BRIEFING (Y/N)?` has been unanswerable-yes for the whole
life of the port, and the briefing pages captured under item 11 were
unreachable by any keystroke. Nothing failed. The prompt appeared, RETURN
worked, and the answer was always the same one.

Matrix positions come from VICE's own `C128/gtk3_sym.vkm` as always -- `Y 3 1`,
`N 4 7` -- never from a published table. `make verify` now reports 27 keys and
12 letters, and still proves the whole table is ASCII in the linked binary.

### What restarting had to reset

`trek_new_game()` handles the core. The one piece of UI state that outlives a
game is `msg_count`, a file static in ui.c, so without `ui_clear_messages()`
the second game opens with the first one's damage reports still in the boxes.
Verified by playing two games through in VICE: the second console comes up with
nothing but AWAITING ORDERS CAPTAIN.

`kb_entropy` deliberately keeps counting across games, so a second game at the
same command level is not a replay of the first.

### RETURN is not an answer here

The dialog takes an explicit Y or N and ignores everything else, including
RETURN. That is a decision, not an omission: `ask_yes()` on the setup screen
reads a bare RETURN as *no*, and a RETURN that means yes on one screen and no
on another is how a session ends by accident. The original gives no guidance --
neither of its two buttons is drawn with a focus ring.

### Q asks now -- done 2026-08-22

The original answers `Q` with `Quit <Y/N>?` in the COMMAND panel; ours quit on
the keystroke, which made an accidental Q the most expensive typo in the game.
`ui_confirm()` puts the question on the command line where the original puts
it, takes an explicit Y or N -- RETURN included in what it ignores, same rule
as the play-again prompt -- and clears the line afterwards, because a stale
`QUIT <Y/N>?` sitting under the next command would be worse than no prompt.

## Sound: the SID driver, and a bug that was three semitones from its cause (2026-08-22)

Item 12's port half. `c128/src/sid.c` plays the extracted tracks on the C128's
SID, and `make sound-check` proves it does by recording VICE and measuring the
pitches.

Two voices, deliberately: voice 1 carries music, voice 2 effects, so a hit
during a track does not chop the tune. The original had one PC speaker and
could not do that. It is the cheapest possible way to take up what this file
has always said about sound -- that it is the one area where every port can
beat the original outright rather than approximate it.

### No interrupt of our own

The original's player is an ISR. This one is polled from `kb_waitkey()`, which
is where the port spends every second it is not drawing -- the whole 45-second
title track plays inside that loop. `snd_poll()` times itself by watching the
VIC raster go backwards, so calling it far more often than once a frame costs
a compare, and it needs no interrupt.

That was a risk decision, not laziness. Hooking the IRQ would mean taking it
from a KERNAL this port already goes behind the back of in `kb_waitkey()`, and
NOTES item 2 is a standing reminder of what undiagnosed machine-level trouble
costs here.

### Three constants, all measured, one assumption refuted

* **The C128's 2MHz mode does NOT change SID pitch.** The port runs the whole
  game at 2MHz, so it mattered. The same note measured 424.8Hz in both modes --
  ratio 0.997.
* **PAL and NTSC clock the SID differently and it is audible**: the same
  frequency word gives 424.8Hz on PAL and 440.4Hz on NTSC, half a semitone
  apart, against 423.9 and 440.1 predicted.
* Frame rates differ by 20% on top of that, so tempo needs the region too.

So the port detects the region and scales at runtime. `sidfreq.h` holds the
arithmetic and `test_sid.c` checks every byte value against the formula in
double precision -- which earned its place immediately by catching two wrong
multipliers, both arithmetic slips of mine, before a note was ever played.

### The region detector, and why the obvious version is wrong

Reconstructing a raster line means reading $D012 for the low byte and $D011 for
bit 8. **Two reads, with the raster moving between them.** When it crosses 255
to 256 in that gap, a stale low byte gets 256 added and the routine sees line
510 on a machine that has 263. Everything above 300 read as PAL, so it answered
PAL on every machine.

The only symptom was that every note played **3.8% sharp** -- which is exactly
the ratio of the two SID clocks, and nothing else in the game was different.

So the fixed version never reconstructs the line at all. Lines 256..311 exist
only on PAL and 256..262 only on NTSC, so the LOW BYTE while bit 8 is set
reaches 55 on one and 6 on the other. Measured at both clock speeds on both
machines: PAL 55 and 55, NTSC 0 and 6. Nothing lands near the threshold. The
sample is taken with $D011 read either side of $D012 and discarded if bit 8
moved across it.

### VICE saves its configuration, and that misled the diagnosis

The recording that showed the 3.8% error was made on an NTSC machine while I
was comparing it against a PAL measurement taken an hour earlier. Nothing had
changed on the command line -- an earlier `-ntsc` had persisted into `vicerc`.
**Always pass the region flag explicitly**; `sound_check.py` does, and says so.

Two instrument mistakes in the same session are worth naming together, because
they are the same mistake: reading a live raster counter through the binary
monitor gives 12, because the monitor freezes the machine and every sample
comes from the same instant. Park the result in memory and read THAT.

### Copyright, and what a clone gets

`tools/gen_music.py` writes `c128/src/music_data.{h,c}` and both are
gitignored. The note data is Anderson's creative work exactly like the message
prose. On a clone without `reference/` the generator writes empty tracks
instead of failing, so the port still builds and simply plays nothing -- a
build that breaks without gitignored material is a build nobody else can run.

### Not done

~~The five short effect tracks are extracted and callable but unwired.~~ DONE
2026-08-22 -- all five identified and wired. Each was named by resolving the
strings its calling routine prints, so none of it is matched by ear: lasers,
torpedo launch, the ALERT the status panel raises on arriving somewhere with
Mongols, the Vandal Death Pod's arrival, and incoming Mongol fire. See
MEASURED.md.

The effects voice was recorded and measured too, because voice 2 and the sfx
branch of `music_tick()` had never run in any test and a wrong register offset
there would have been silent: 495Hz then 1000Hz, twice, against 500 and 1000
expected, with the durations in the right 4:10 ratio.

A turn plays ONE sound, not one per event -- the effects voice is monophonic,
and the death pod outranks ordinary fire because in the original it has its own
effect and its own line in the damage report. The alert fires after the enemy
turn rather than after a move, because a tractor beam drags the ship into a new
quadrant on someone else's turn.

The end-of-game track plays through the identical path as the title track but
has not been recorded and measured.

### Where the title track stops -- corrected 2026-08-22

It plays on the title screen ONLY and stops the moment the briefing question
appears. This port originally let it run through the setup screen, which was
recorded here as a guess at the time; Jamie corrected it and the original's own
player flag confirms it -- see MEASURED.md.

## The refusal beep, and two bugs behind one wrong number (2026-08-22)

Asked for "the rest of the original's noise" -- the procedural sweeps and the
440Hz beeps that do not go through the music player. Identifying them the same
way as the effects, by resolving the strings their routines print, split the
job in half.

### The sweeps are the death ray, so they are not ported

The procedure at 0x007375 prints "the death ray is experimental in nature",
"wish to continue <Y/N>?", "Preparing death ray..." and "Firing!" -- and then
runs the sweeps. Two of them: 37Hz to 1000Hz playing f, 2f and 3f for 2ms each,
about 5.8 seconds; and 1200Hz to 3000Hz at 1ms a step, about 1.8 seconds.

`RAY` is an unimplemented command. Porting its sound first would be dead code
for a mechanic that does not exist, so the measurement above is the spec for
when `RAY` lands and nothing was written.

### The beep is the refusal beep, and it is ported

Sound(440); Delay(250); NoSound, from seven call sites -- two in the laser
dialog and five in the torpedo dialog, every one of them a refusal: no
torpedoes, tubes damaged, not enough energy. It is the sound of the ship
declining an order, and it is wired to the same refusals here, plus an unknown
command, which is marked DERIVED because the original beeps at a field its
parser rejects rather than at an unrecognised command.

440Hz is 44 in the tenths the track data already uses, so it needs no
arithmetic of its own.

### $D012 is a low byte, and using it raw is wrong in two different ways

Both showed up here, and the second had been silently wrong all along.

**As a line number** it can be 256 too high, because bit 8 is a separate read
in $D011. That was the region-detection bug: every note 3.8% sharp.

**As a frame marker it goes backwards TWICE per frame** -- once at the
255-to-256 crossing and again at the real wrap. Whether a caller sees one or
both depends on how fast it polls. The beep, timed from a tight loop, came out
half length. The music, polled from the key loop, came out right.

So `raster_line()` now does the validated read -- $D011 either side of $D012,
retry if bit 8 moved -- and everything else is built on it. It can never return
a wrong number, and one decrease means one frame at any sampling rate.

### The music tempo was right by luck

MEASURED, and the reason the beep had to be chased: **`snd_poll()` was being
called 82 times a second**. A full pass of the key matrix scan takes about
12ms, because `key_down()` does a runtime variable shift per key and cc65
compiles that to a subroutine loop. Against frames of 16.7ms (NTSC) and 20ms
(PAL) that is 1.4 samples per frame, where catching every frame needs two.

NTSC happened to land right. **PAL music ran 45% slow** and had done since the
driver was written -- and `make sound-check` could not see it, because every
note was the right note, just held too long.

The poll now happens inside the matrix scan rather than once around it, which
is about 2000 samples a second. PAL went from +43% to +0.7%.

`sound_check.py` now asserts TEMPO as well as pitch, and measures each note's
pitch from the median of its run rather than its first window -- a window
straddling a note boundary autocorrelates against two pitches at once, and one
such reading swung a note from +0.4% to -2.0% and tripped the threshold.

## Briefing pages: where the text can live, and why not an REU (2026-08-22)

Design work only, no code. Item 11 has the captures; this is what to do with
them when the time comes.

### The arithmetic that decides it

| | bytes |
|---|---|
| The original's briefing prose | **7,574** |
| Byte-pair compressed, 96 codes plus a 192-byte table | **~4,350** |
| Free space in `trek128.prg` | **1,688** ($B800 less BSS ending $B168) |

`MAIN` is $1C0D..$B800 -- that is the config's $A3F3 minus the 2K stack. The
text is 4.5x the room available and 2.6x even compressed. **It cannot go in the
C128 binary**, and trimming code will not change that.

A word dictionary of the 128 commonest words scores about the same as
byte-pair, 4,182 bytes plus a 774-byte dictionary, and is easier to decode.
Either is fine; neither is close to fitting.

### What is actually eating the budget

| segment | bytes | share |
|---|---|---|
| CODE | 32,497 | **81.4%** |
| RODATA | 4,761 | 11.9% |
| BSS -- every byte of live game state | **808** | 2.0% |

**97% of what is used is code and read-only data.** This is worth stating
plainly because it inverts the intuition: the port is not short of room for
data, it is short of room for instructions.

### Copyright decides the shape, and it helps

The prose is Anderson's, so it cannot be committed -- the same rule as the
message catalogue and the extracted music. Two options: generate it at build
time from the user's own shareware copy, which leaves a public build with no
briefing at all; or write our own pages covering the same material, which item
11 already calls for ("a specification of what each page covers, not text to
copy").

Write our own. The useful consequence is that **the byte budget becomes a
design target rather than a constraint to fight**.

### Three places it can live on a C128, all free

**The VDC's spare RAM.** The port already drives 16K of it and uses about 12K
-- screen at $0000, attributes at $0800, the KERNAL's charset at $2000..$3FFF.
That leaves **$1000..$1FFF, 4KB**, reachable with `vdc_set_address` and
`vdc_data_read`, which already exist. Write the briefing to fit 4KB compressed
and it lands in memory we are already paying for, on every C128, with no bank
switching. UNVERIFIED: that the charset really is at $2000 here. The port never
touches register 28, so it is wherever the KERNAL put it -- read it back before
relying on it.

**Bank 1.** The C128 has 128K and cc65's target uses about 40K of bank 0, so
**bank 1's 64K is idle**. `INDFET`/`INDSTA` at $FF74/$FF77 read and write across
banks at roughly 30 cycles a byte -- a 2KB page in about 30ms at 2MHz, nothing
for a page turn. This is the general answer for elbow room on this machine.

CAUTION, and it touches recent work: **BASIC keeps its variables in bank 1**,
and this port now returns to BASIC cleanly. Using bank 1 would clobber them.
Probably harmless at a fresh prompt, but it is exactly the shape of thing that
would resurrect open item 2, so it wants testing rather than assuming.

**Disk.** A file, read a page at a time. Slow on a 1541, which is why loading
once into VDC RAM or bank 1 beats paging from disk every time.

### The prerequisite is disk I/O, and it is shared

However the text is stored, it has to arrive, and this port has never done disk
I/O. That same piece of infrastructure serves `SAVE` and writing `TREK.SCR`.
It is the real first step, not the briefing. **Its seam is decided -- see "Disk
I/O: the seam, decided before anyone starts" below.** In short: serialisation
lives in `core/` as byte arrays and is tested natively; storage is four
functions per platform; and the briefing is the LAST of the three to build, not
the first.

### Across the ports: this is the project's first ASSET

Briefing text is neither core logic nor platform presentation, and the repo has
no concept for that yet. Introduce one deliberately:

- **`assets/briefing.txt`** -- one source of truth, page-delimited plain text,
  wrapped at RUNTIME rather than baked, so the same asset serves 80 columns and
  anything narrower later.
- **`tools/gen_briefing.py`** -- compiles it per target into a compressed blob
  plus a page index. The decoder is about 100 bytes of platform code.
- **C128**: loaded once from disk into the VDC's spare 4KB, or bank 1 if it
  outgrows that.
- **Amiga**: 512K makes it a non-problem -- link it in, uncompressed if you
  like. Same asset and same tool, because the point is that the text does not
  fork.
- **Atari VBXE**: 64K of base RAM puts it in the same squeeze as the C128, and
  **VBXE's 512K of video RAM through the MEMAC window is the direct analogue of
  the VDC trick**. The two constrained targets get the same shape of answer.

`core/` never sees any of it.

### Would an REU help? No -- asked and answered

An REU is a DMA store and **the 8502 cannot execute from it**. It moves data
between main RAM and expansion RAM, and data is 2% of our problem. It does
nothing about the 81% that is code.

For the data problems it could solve, the VDC's spare RAM and bank 1 are both
free and present on every C128, where an REU is hardware most players do not
have. It also does not remove the disk-I/O prerequisite: the briefing still has
to get into the REU somehow.

**The one case where it genuinely wins is code overlays** -- DMA a block of code
into main RAM and jump to it, which is the classic technique and would let the
port grow well past 40K. Against that: the program has to be restructured into
overlays with a resident manager, a non-REU fallback has to exist or the build
forks, and the manager itself costs code -- spending the scarcest resource to
buy more of it.

Revisit only if the port genuinely outgrows 40K of code, which planets, save
and the death ray together might eventually manage. VICE emulates it (`-reu`,
`-reusize`), so it would be testable the same day.

## Disk I/O: the seam, decided before anyone starts (2026-08-22)

Three things wait on this and none of them should be built first: the briefing
pages, `SAVE`, and writing `TREK.SCR`. Deciding the shape once is cheaper than
discovering it three times.

### Yes, it is different on every port

| target | API | names |
|---|---|---|
| C128, X16 | KERNAL through cc65's `cbm.h` | device 8, PETSCII, `0:NAME,S,R` |
| Amiga | AmigaDOS `Open`/`Read`/`Close`, or plain stdio | real paths, `PROGDIR:trek.scr` |
| Atari + VBXE | CIO and IOCBs | `D:NAME.EXT` |
| F256 | FoenixMCP calls to the SD card | MCP paths |
| MEGA65 | C65 DOS / SD | |
| CoCo 3 | Disk BASIC, or OS-9 | |

The error models differ too -- "file not found" and "no drive attached" are not
the same condition everywhere, and only some of these can tell them apart.

### The split that matters: serialisation is core, storage is platform

**Serialisation belongs in `core/`.** It knows the structs, it is identical on
every target, and it touches no I/O:

    uint16_t trek_state_save(uint8_t *buf, uint16_t max);   /* bytes written */
    uint8_t  trek_state_load(const uint8_t *buf, uint16_t len);

The payoff is that it is **testable on the build machine with no disk and no
emulator** -- play a game, round-trip it through a buffer, assert every field
comes back identical. That is the same discipline as the EGA-to-VDC mapping and
`sidfreq.h`, and it is why those two are the parts of this port that have never
silently broken.

**Storage belongs in the platform layer**, and it is about four functions:

    uint8_t  plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got);
    uint8_t  plat_write_all(const char *name, const void *buf, uint16_t len);
    uint8_t  plat_open(const char *name);          /* streaming, for the briefing */
    uint16_t plat_read(void *buf, uint16_t len);

Names are short opaque tokens the platform maps to its own syntax. `core/` must
never see a path, a device number or a logical file number. The C128
implementation is perhaps sixty lines: `cbm_open`, `cbm_read`, `cbm_write`,
`cbm_close`, all in `cbm.h`.

### Decide this before a byte is written: NO memcpy OF STRUCTS

The core targets the 6502 (little-endian) and the 68000 (big-endian), and
struct padding differs between compilers as well. A `memcpy` of `ship` would
produce files that disagree between ports **and would pass every test on the
machine that wrote them**. Serialise explicitly, byte at a time, in a defined
order.

This is the same class of rule as the 16-bit arithmetic constraints already in
trek.h, and it is already half-enforced: `make port-check` compiles the core for
the 68000, so a native round-trip test plus that target catches most of it.

### The byte order pin: little-endian, always, everywhere

"In a defined order" above is not a definition, so define it. **Every multi-byte
value is serialised low byte first**, regardless of the host:

    buf[n]   = (uint8_t)(v & 0xFF);
    buf[n+1] = (uint8_t)(v >> 8);

and read back the same way, never by casting the buffer to a `uint16_t *`. That
is the 6502's native order, so the 8-bit ports pay nothing; the 68000 pays two
shifts per field and gets files that interchange.

**Why this is settled here and not discovered on the Amiga.** Byte order has no
consequence anywhere in this codebase today -- nothing crosses a machine
boundary. It acquires one the instant `trek_state_save()` exists, and the
failure mode is the nasty kind: a save file written on the C128 and loaded on
the Amiga would come back with every field byte-swapped, while **every test on
both machines passes**, because each round-trips its own bytes happily. It is
the same trap as the struct `memcpy` above, one level finer.

Pinning it now also removes the last real portability question a 68000 port
would have answered -- see item 8, where that is part of why the Amiga is no
longer port #2. A native test that writes a state buffer and asserts its exact
bytes against a fixed expected array closes it for good, on the build machine,
with no emulator involved.

### The C128 needs a disk image before it needs disk code

The port autostarts a bare PRG today. File I/O needs a real disk, and **`c1541`
ships with VICE and is already installed here**, so `make d64` can build one
carrying `trek128.prg` alongside the data files. Small Makefile change, but note
it changes what `make run` means, and `tools/exit_real.py` and
`tools/sound_check.py` both autostart the PRG directly.

### Start with the hall of fame, not the briefing

`TREK.SCR` is the smallest real file and its format is MEASURED, not guessed --
`reference/TREK.SCR` is 300 bytes of plain text:

    25 characters of name, padded with '.'   CRLF
    the score in decimal                     CRLF

ten times over. The name field is fixed at 25; the score line is variable
length, so the file is line-oriented text rather than fixed records -- the
sample is exactly 300 bytes only because every score in it is `0`.

It exercises open, read, write and close end to end with almost nothing at
stake, which is the right way to find out that a 1541 wants `0:` prefixes and
that reading a file that does not exist is not an error until you check the
status channel. Then `SAVE`, which is the same plumbing carrying
`trek_state_save()`. Then the briefing, which is the one that also needs the
asset pipeline and a decision about VDC RAM versus bank 1.

## MEGA65 is Tier 1, and it is the friendliest target on the list (2026-08-23)

Jamie added a **native-mode 80-column build** to the Uno port (commit `7334f3c`
there), which turns the entry above from an inference into a proven target.

**Why the old Uno port could not do it, which is the useful part.** That build
is a cc65 `c64`-target binary loading at `$0801`, so the machine is in C64 mode
-- where 80 columns do not exist, and setting VIC-IV's H640 bit from inside
changes nothing. Colour RAM makes it concrete: 80x25 is **2000 cells** and a
C64-mode program can only reach the 1K window at `$D800`. The native build
links at **`$2001` with a BASIC 65 header**, which is what actually selects
C65/MEGA65 mode; there H640 works and mega65-libc reaches the real colour RAM
at `$FF80000` through the 45GS02's 32-bit addressing, so every cell gets its
own colour.

### Three things that make it unusually cheap for THIS game

**The video seam already matches.** Its `m65native.c` exposes
`scr_put(x, y, ch, color)`, `scr_puts`, `scr_clear` and `wait_vsync` -- the
same primitives, with the same signatures, as this port's `c128/src/vdc.c`.
The platform layer would be a rewrite of one file, not a redesign.

**The PETSCII literal trap disappears.** cc65 applies a charmap to string
literals, which has bitten this project four separate times -- blank panel
titles, a PETSCII key table, the dispatch constants, and `word_is()` again on
2026-08-22. **llvm-mos has no charmap concept at all**: literals are plain
ASCII. An entire class of silent bug does not exist on that toolchain.

**Binaries are 2.3x tighter.** 11,316 bytes against cc65's 26,042 for the same
Uno game. That matters more than it sounds, because the C128's briefing problem
is entirely a code-ceiling problem -- 81% of a 40K budget is CODE. On MEGA65
that pressure largely goes away.

### Tooling, all present on this machine

- **llvm-mos** at `~/llvm-mos`, with a `mega65` platform in `mos-platform/`.
  Prebuilt for macOS, unlike the TMS9900, vbcc and Open Watcom toolchains.
  cc65 has no MEGA65 target, so this is the route.
- **Xemu** (`/Applications/Xemu.app`) for `xmega65`, plus a MEGA65 ROM.

Unlike Uno, this port would need only the native build -- there is no reason to
ship a 40-column MEGA65 version of a game whose console is 80 columns wide.

## The C128 binary hit its ceiling, and BSS moved out (2026-08-23)

Building `MSGS` was the first change this port could not fit. Worth recording
in full, because the number everyone was working from was wrong.

### The free-space figure in the briefing note is stale

The briefing arithmetic above says **1,688 bytes free**. That was measured
before the SID driver and six commands landed. When `MSGS` began, the real
figure was **395 bytes** -- `$B800` less a BSS ending at `$B675`. The viewer,
the log and the arrow keys came to about 800 bytes of code, so the link failed
by 330.

Anyone reasoning from the 1,688 figure -- for the charset, for the briefing --
is reasoning from a number that has not been true for weeks. **Read the map,
not this file.**

### Two places the space came from, and one that was refused

**The log went into VDC RAM.** Thirty-two message entries are 1,792 bytes and
there was never a version of that which fits in 8502 RAM. The VDC has 16K of
its own, of which this port uses the screen at `$0000`, the attributes at
`$0800` and the KERNAL's character set at `$2000` -- leaving `$1000..$1FFF`
unused. A message log is close to the ideal tenant: written once per message,
read only while `MSGS` is open, and never touched in the turn loop.

**BSS moved to `$1300`.** `c128/trek128.cfg` is cc65's stock `c128.cfg` with
one line changed. MAIN is `$1C0D..$B800` and BSS was sitting in the middle of
the only space CODE has; the C128's free block between the KERNAL work areas
and the start of BASIC text at `$1C00` is 2,304 bytes doing nothing. Moving
650 bytes of BSS there bought 650 bytes of MAIN.

**VERIFIED, not assumed.** `$1300..$1BFF` was filled with a rolling pattern
through VICE's binary monitor while the port ran, then read back three times
over thirty seconds: **zero bytes changed**, with the machine confirmed live
between each read. The KERNAL's 60Hz IRQ is the only other thing executing.

**The limit of that test, stated because it will matter:** the port was sitting
on the title screen and has done no disk I/O, because it has none yet. When the
disk seam lands, re-run the same experiment across an open-read-close cycle
before trusting this. KERNAL file handling is the one subsystem that allocates
buffers, and it is the one the test did not exercise.

**Shrinking the stack was refused.** Dropping `__STACKSIZE__` from 2K to 1.5K
also links, and it was tried first. It was rejected because a stack overflow is
silent corruption and nothing here has measured the high-water mark -- trading
a known-safe 650 bytes for an unmeasured 512 is the wrong way round.

### Where it stands

With `MSGS`, `A#`, the arrow keys and the log all in, MAIN has **210 bytes**
free and BSS can still grow by about 1,650 into low RAM. The pressure is
entirely on CODE, which is what the 81%-code breakdown above already said.

### Where the next 5K comes from: bank 1, not a 64K VDC (2026-08-23)

Asked directly whether C128 bank 1 RAM or a 64K VDC would help. Measured the
segments before answering, because the answer turns on the split:

| segment | bytes | what is in it |
|---|---|---|
| CODE | 34,471 | ui.o 13,469 · trek.o 10,056 · main.o 6,504 · rest ~4,400 |
| RODATA | 5,081 | strings ~3,570 · music data 1,106 · tables ~400 |

Together **39,552 of MAIN's ~40,435**. The ceiling is CODE *and* RODATA, and
that is what decides between the two options: only one of them touches either.

#### 64K VDC -- no, and it costs something

It buys 48K more of the one thing this port is NOT short of. A 16K VDC has
about 4K free after screen, attributes and character set, and the message log
took 2K of it. **It does nothing for CODE or RODATA.**

It also excludes stock flat C128s, which is a hardware requirement bought for
storage we do not need. The only case that could justify it is the briefing,
and that is prose we are writing ourselves to budget, with disk streaming
already chosen as the mechanism.

#### Bank 1 -- yes, and worth more than it sounds

**Every C128 ever sold has 128K**, so requiring bank 1 costs no compatibility
at all. That is the asymmetry against the VDC.

**Move RODATA there and MAIN gains 5,081 bytes** -- CODE headroom goes from 210
to about 5,300, which is roughly the charset upload plus `RAY` plus the planet
model with room to spare.

Access is not exotic. The KERNAL's `FETCH` at `$FF74`, with `STASH` and
`CMPARE` at `$FF77` and `$FF7A`, live in shared ROM and read across banks
without any hand-rolled MMU switching. The change is bounded: a `scr_puts_far`
that fetches into a ~40-byte buffer and draws, plus the message path. Only one
string ever needs to be resident.

**Hand-rolling MMU switches around executing code is where bank 1 gets
dangerous.** `FETCH` is not that, and the distinction is the whole reason this
is a contained change rather than an architecture.

**Code overlays in bank 1** -- copying a subsystem in when needed -- are the
only option here that lifts the CODE ceiling rather than deferring it. Also the
most invasive, and cc65's overlay support against the `c128` target is not a
well-trodden path. Held in reserve.

#### What outranks both

**Bank 1 is C128-only work. The disk seam is work every port needs.** They
solve the same problem for this machine -- get bulk data out of the address
space -- but the seam already gates `SAVE`, `TREK.SCR`, the briefing and port
#2, and MEGA65, X16, F256 and Amiga all need storage regardless. MEGA65 has no
pressure at all, so none of the banking transfers to it.

And a fourth option that neither question named: **the port does not have to be
one binary.** Setup, briefing and game as separately loaded programs is how
8-bit games handled exactly this, and it is disk work too.

#### The order

Disk seam first. Then, if the C128 is still tight -- and it will be, because
`RAY` and the planet model are both still to come -- **RODATA to bank 1 via
`FETCH`**. That does the portable work first and keeps the C128-specific
complexity as a targeted fix rather than a design.

Skip the 64K VDC unless something later actually needs the room.

## The disk seam, built (2026-08-23)

The shape was decided on 2026-08-22 and is unchanged. What follows is what
building it actually found.

### The core half is done and needs no machine

- **`core/serial.c`** -- `trek_state_save` / `trek_state_load`, 507 bytes per
  save, little-endian pinned, no I/O and no filename. Covers the galaxy, the
  ship, the sector, enemy hit points, **the event queue and the RNG**. Those
  last two are easy to forget and both are load-bearing: without the RNG a
  restored game replays the same rolls from a fixed seed, and without the
  schedule every deadline the COMMUNICATIONS panel promised silently stops
  existing.
- **`core/hof.c`** -- TREK.SCR parse and format, plus `hof_offer`.
- **`core/storage.h`** -- the four-function platform contract.
- Both are in `make test` and both compile for the 68000 under `port-check`.

They are separate translation units from `trek.c` on purpose: ld65 links whole
modules, so anything added to trek.c is in every binary whether it is called or
not. A port with no `SAVE` simply does not list `serial.c`.

### TREK.SCR is place-major, and that is not the natural guess

Ten records, five ranks. The hall of fame shows **two entries per rank**, a
bright first place and a dim second -- so ten records are five ranks by two
places. Which record is which was settled by writing a file with `SLOT0`
through `SLOT9` in order and reading the names back off the original's screen:

    Lt. Commander  SLOT0 / SLOT5      Commodore  SLOT3 / SLOT8
    Commander      SLOT1 / SLOT6      Admiral    SLOT4 / SLOT9
    Captain        SLOT2 / SLOT7

**Records 0..4 are first place for ranks 1..5; records 5..9 are second place.**
Rank-major -- 0 and 1 both belonging to rank 1 -- is what the file looks like
and it is wrong. `hof_index()` is the only place that knows.

Also measured in the same session: the file is written **only when a score
qualifies**, not on exit. A scuttled ship scored -930 and TREK.SCR's mtime did
not move, so **scores can be negative** and a formatter without a minus sign
writes a file it cannot read back.

### A macro that native cc accepted and cc65 refused

The field list was first written once as a macro taking `P8`/`P16` as
parameters and expanded two ways. Native `cc` is happy. **cc65's preprocessor
does not expand a function-like macro that arrived as a macro argument** --
`Undefined symbol: SAVE8`. The field list is now written out twice, with the
round-trip test keeping the two honest.

Same shape as every other trap here: the build machine is happy and the target
toolchain is not.

### The C128 half is written, measured, and does not fit

`c128/src/storage.c` is the KERNAL implementation -- `cbm_open`/`read`/`write`/
`close`, `0:` prefixes, `@0:` for replace, and the status channel, because
**opening a file that does not exist SUCCEEDS on a CBM drive** and the failure
only appears when you ask channel 15. A port that skips that reads a missing
hall of fame as a valid empty one.

It is **815 bytes of CODE against 210 free**, and with `core/hof.c` alongside
it the link overflows by **2,920 bytes**. It is committed compiled and
measured but NOT in `SRC`, because the number is the finding.

cc65 has no slack left to give: `-Os` is four bytes worse than `-O`, and `-Oi`
overflows on its own.

### `make d64` exists

`c1541` builds `build/trek128.d64` carrying `trek128` and an **empty**
`trek.scr` -- empty rather than copied from `reference/`, because the
original's file is Anderson's and a fresh install should have a blank table
anyway. `make rund` boots it.

`make run` still autostarts the bare PRG and always did, so a build that reads
a file sees no drive there. `tools/exit_real.py` and `tools/sound_check.py`
autostart the PRG directly and are unaffected only because neither touches a
file.

## llvm-mos on the C128: it works, and it is worth about 6K (2026-08-23)

Jamie's suggestion, tried the same day. Short version: **the whole port
compiles, links, runs and renders correctly under llvm-mos**, and it buys
roughly 6,000 bytes -- which is the difference between "cannot add disk I/O"
and "can add disk I/O with room to spare".

### First, a correction

An earlier measurement in this session put llvm-mos at **3.2x smaller** than
cc65, comparing a small program built from `trek.c`, `serial.c` and `hof.c`
(14,997 bytes against 4,625). **That figure was wrong as a statement about
code density.** It was mostly cc65's fixed runtime overhead, which amortises
away on a real binary. On the whole port:

| | CODE | RODATA | total |
|---|---|---|---|
| cc65 `-O` | 34,471 | 5,081 | 39,552 |
| llvm-mos `-Os` | 33,228 | 5,073 | 38,301 |
| llvm-mos `-Oz` | **30,003** | 5,099 | **35,098** |

**13% smaller on code, 11% on code plus data.** Real, useful, not
transformative on its own.

`-Oz` matters: it is 3,200 bytes better than `-Os` on this codebase, which is
most of the win. `-flto` adds nothing on top of either.

### Where the other third of the headroom comes from

Not code density -- the memory model.

    cc65      MAIN = $A3F3 - $800 stack, carved out of the program's space
    llvm-mos  ram  = ORIGIN $1C01, LENGTH $A3FF, __stack = $C000 growing down

That is **2,060 bytes** llvm-mos does not reserve up front. Add the 4,454 from
code size and the headroom is about 6,500, which matches what was measured:
linking the port **plus `core/hof.c` and `core/serial.c`** ends at $A89A with
roughly **6,000 bytes free**, against cc65's 210 free *without* those two
files.

llvm-mos also put BSS inside the main region without complaint, so the
`$1300` low-RAM relocation in `trek128.cfg` is a cc65 workaround that a
migration would simply drop.

### What had to change: one file

Every other file compiled **unchanged**, with zero errors: `vdc.c`, `ui.c`,
`main.c`, `input.c`, `sid.c`, `layout.c`, `music_data.c`, and all of `core/`.
That includes `<6502.h>`'s `SEI()`/`CLI()`, which llvm-mos provides.

The exception is `c128/src/storage.c`. **llvm-mos's `cbm.h` has the entire
high-level file API inside an `#if 0`** -- `cbm_open`, `cbm_close`, `cbm_read`,
`cbm_write`, `cbm_load`, `cbm_save` are all disabled, leaving only the raw
KERNAL primitives (`cbm_k_setlfs`, `cbm_k_setnam`, `cbm_k_open`,
`cbm_k_chkin`, `cbm_k_chrin`, `cbm_k_readst`, `cbm_k_close`). Storage would be
rewritten against those: perhaps forty more lines, and arguably better, since
this port already wants control of the status channel.

### Verified running, not just building

In VICE, from the llvm-mos binary: the title screen, the setup prompts, the
full nine-panel console, and the `MSGS` overlay including its log read back
out of VDC RAM. Identical to the cc65 build on screen.

The cc65 charmap bug class -- four separate bugs in this project -- does not
exist on this toolchain, and `make verify` would become redundant rather than
load-bearing.

### What this does to the bank-1 plan

Largely retires it. Bank 1 was ~5K through a C128-only mechanism with a real
trap in it; llvm-mos is ~6K, deletes a bug class, drops the low-RAM linker
workaround, and is **the same toolchain the MEGA65 port needs anyway**, so the
work transfers instead of being spent once. Bank 1 stays available if the port
later needs more than 6K, and code overlays remain the only thing that lifts
the ceiling rather than raising it.

## The migration to llvm-mos, and the disk write that still does not commit (2026-08-23)

The port now builds with llvm-mos. `c128/trek128.cfg` is gone -- the low-RAM
BSS relocation was a cc65 workaround and llvm-mos does not need it.

### What the move actually took

**One file.** Everything else compiled unchanged. `c128/src/storage.c` had to
be rewritten against the raw KERNAL calls because llvm-mos's `<cbm.h>` keeps
the whole high-level file API inside an `#if 0`.

**One library bug found on the way.** `cbm_k_chkout` does not link: it
references `__CHKOUT` where the C128 platform's `kernal.S` defines `__CKOUT`.
`cbm_k_ckout` is the same vector and works. Worth remembering, because it
means the wrappers in this library are not all sound.

**`-Oz`, and the Makefile says so.** 3,200 bytes better than `-Os` here.

Headroom now, with the disk seam, the hall of fame, `hof.c` and `serial.c` all
linked in: **about 3,000 bytes**, against cc65's 210 with none of them.

### Reading works, end to end

Verified by writing a `TREK.SCR` full of `RDTEST0..RDTEST9` onto a disk image
and watching them come back on the hall of fame screen in the right
place-major slots -- first place bright, second dim. That also confirms the
record layout on real hardware rather than just in a unit test.

### ~~Writing does NOT~~ CORRECTED 2026-08-23: writing works too

**The previous entry here was wrong, and so was the commit message that went
with it.** It said the C128 write never committed its directory entry. The
write was fine; the *observation* was not.

The symptom was real enough: after a write, the directory entry in the host
`.d64` read back as type `0x01` -- SEQ with the closed bit clear, zero blocks.
That is exactly what a 1541 entry looks like between open and close, so it read
as "the close never happened".

**It had not happened in the host image.** VICE's drive keeps its own view of
the disk and writes the directory sector back on its own schedule, so the file
on the host lags the file on the emulated disk. Jamie called this before I did
-- twice, first about closing VICE too quickly and then about it not writing
the image until a reset, a quit or an eject.

Three measurements settled it, all of them asking the DRIVE rather than the
host file:

- **Write then read back in the same run.** 340 bytes out, 340 back,
  byte-identical, clean status.
- **Two games in one session.** Game one records ALPHA; answer PLAY AGAIN;
  game two's hall of fame shows ALPHA read back off the disk alongside its own
  BETA. That is the real feature working end to end.
- **VALIDATE.** `V0:` rebuilds the BAM and **deletes any file whose entry is
  still flagged open**. After a validate the file was still there and still
  read back 340 bytes -- which a genuinely unclosed file could not do. The host
  image then showed `0x81`, closed, because the validate had forced the drive
  to rewrite the directory.

The `@`-replace finding above is now suspect for the same reason; scratch-first
is kept because it is the safer conventional pattern, not because `@` was
proven broken.

### The rule this leaves behind

**Ask the drive, not the host image.** Reading a `.d64` that VICE is currently
managing answers a different question from the one being asked, and it will
answer it confidently and wrongly. The reliable checks are a read-back inside
the same run, a second run in the same session, or `V0:` followed by a read.

### Two hours of the debugging were a bad test rig, not a bug

Worth recording because it wasted more time than the real fault. The first
symptom was a blank hall of fame, and the cause was that `TREK.SCR` had never
been written to the test disk -- the `c1541` command ran from the wrong
directory and its error went unread. That sent the whole first diagnosis after
the serial clock rate.

And twice more, `c1541 -read` returned stale contents because VICE still held
the image open. **Check the raw d64 bytes, and let VICE exit before trusting
anything c1541 says about a disk the emulator was just using.**

## SAVE, and four bugs it took to get there (2026-08-24)

Twenty of twenty-five. Verified end to end in VICE: save a game, self destruct,
answer PLAY AGAIN, restore -- and the same galaxy, quadrant, sector, stardate
and Mongol count come back.

### The shape

`core/serial.c` serialises the GAME. It does not know the player's name or
self-destruct password and should not, so `ui.c` wraps its own header round the
core's blob:

    [13] name        [9] password      [2] level      [507] trek_state_save()

The core half already carries magic and a version and refuses what it does not
recognise, so the header needs neither. The original's own file is plain text
beginning `EGATrek 3.0` and then the player's name -- the same two ideas,
version and name, arrived at independently. Ours is binary and deliberately
not interchangeable.

### Four bugs, three of them mine and one inherited

**1. The BASIC header went to the wrong region.** The new linker script
declares `lowram` before `ram`, and a section with no explicit region goes to
the FIRST one declared -- so `.basic_header` landed at `$1300` while the PRG
still claimed to load at `$1C01`. It loaded and sat at `READY`, because `RUN`
found no BASIC line. `>ram` is now explicit, with a comment saying why.

**2. The scratch status was never read.** `plat_write_all` scratches the old
file first, and a CBM drive holds ONE pending status per channel. Leaving
`01, FILES SCRATCHED` queued meant the status check after the write read that
instead of the write's own result -- so the file landed correctly and the
command reported `STOR_ERROR`. `scratch()` now consumes its own reply.

**3. My own truncation guard rejected every save.** `plat_read_all` treats
"filled the buffer exactly" as possible truncation, because it cannot tell a
file that fit from one that did not. That is right, and it means callers must
offer more room than the file needs. A save is exactly `SAVE_BYTES` long, so a
`SAVE_BYTES` buffer failed every restore with NO SAVED GAME FOUND while the
file sat on the disk, correctly written and correctly closed.

**4. Every dialog asked for RETURN twice.** Not new, and not confined to SAVE:
`do_self()` and `do_fix()` printed `HIT RETURN TO CONTINUE` with
`ui_dialog_line` and waited, and then `ui_dialog_close()` printed its own and
waited again. Jamie spotted the double keystroke -- one green, one cyan -- and
that it affected self destruct too. Four sites fixed; `ui_dialog_close()` was
always doing both jobs.

### One deliberate departure, and a new helper for it

MEASURED: the original's save box **closes the moment it has a file name** --
no confirmation, no second keypress. Every dialog in this port ends by asking
for RETURN, which is right when the box reports something that must be read and
wrong here. `ui_dialog_dismiss()` closes without prompting; SAVE uses it on
success. Failure still stops and waits, because a disk error that flashed past
unread would be the worst outcome of the command.

### Memory: writable data moved below the program

SAVE's serialiser costs about 2.5K of code, and the release build came down to
EIGHT bytes of headroom while the debug build would not link at all. llvm-mos's
stock script puts everything above `$1C01` and ignores the RAM below, so
`c128/trek128.ld` moves `.data`, `.bss` and `.noinit` to `$1300..$1BFF` -- the
same block the cc65 build used and verified. Headroom above the program went
from 8 bytes to **1,657**.

### Still to do, noticed while capturing

The original binds shields to the **up and down arrow keys**; this port has
only `SHUP`/`SHDN` as words. The arrows are in the matrix now.

## Damage finally does something (2026-08-24)

Before this the port modelled all twelve repair percentages, drew them,
repaired them, and consulted exactly ONE of them. A ship at 10% on everything
played identically to a ship at 100%. The manual specifies an effect for every
system; these are now in `core/trek.c` as queries, so every port gets the same
answer and the manual's wording sits beside the number it produced.

| system | rule, and where it bites |
|---|---|
| Warp Engines | max warp = 1 + 0.09 x pct, clamped to the emergency 8. `trek_set_warp` refuses above it |
| Impulse | below 50% they stop. `trek_move_impulse` returns the new `MOVE_NO_IMPULSE` -- a hard refusal, matching the original's "Move aborted; impulse engines are too damaged to use" |
| Lasers | pct is the fraction of energy that becomes damage. `trek_laser_eff()` multiplies heat by battle damage, so half-repaired lasers do half the damage at any temperature |
| EnTorp Tubes | 100% = 3, 67-99% = 2, 34-66% = 1, below = none. The salvo cap follows it and the refusals are the original's words |
| Short Range Scanners | below 90% the scan shows stars and our own ship only; below 50% the panel says INOPERATIVE |
| Long Range Scanners | below 100% the chart's enemy digit becomes a DASH, not a zero -- a zero is a lie the player acts on; below 50% the panel says INOPERATIVE |
| Computer / Transporter / Shuttlecraft | `trek_autonav_ok()`, `trek_transporter_ok()`, `trek_shuttle_ok()`, all needing a full 100%. Nothing calls the last two yet; they wait on `LAND` |

`REPAIR_FOCUS_FACTOR` is **3**, not the DERIVED 2 -- the manual gives the whole
table (1x, 2.5x docked, 3x focused, 5x both), and the docked figures agree with
`REPAIR_PER_STARDATE` 20 and `_DOCKED` 47 already measured off the original.

**Shields are on the arrow keys**, which is the original's primary binding --
`EGATREK.REF` says "Shields Up (use up arrow)" and the in-game F1 help lists
only the arrows. `SHUP`/`SHDN` stay as the spoken names, as the card gives both.

Twenty-six new assertions in `core/test/test_trek.c` pin every threshold, and
each one is the manual's number rather than a value read back out of the code.
Verified on the machine as well: poking the scanners to 70% and 80% through
VICE's monitor produced a chart reading `-07 -03 -04` and a scan showing stars
and the Lexington alone.

**Not changed, deliberately.** The original's SYSTEMS STATUS panel lists ten and
omits Transporter and Shuttlecraft, which appear only in its `REPAIR` dialog.
This port's panel is already a departure -- two columns of abbreviations, to fit
a 40x14 region -- and dropping two systems would lose information the player can
otherwise only get by opening a dialog. Twelve stays.

## Manual movement, and the linker guard it forced (2026-08-24)

`trek_autonav_ok()` was written the same day and deliberately left with no
caller, because wiring it without a fallback would have stranded a player the
new damage rules had just damaged. This is the fallback.

- `M` with coordinates still works, including `m6235` and `m35`.
- `M` alone opens `NAVIGATION` asking `QUAD, SECTOR:`.
- Typing `M` there switches to `DELTAX:` then `DELTAY:`, as the manual says.
- **A computer below 100% goes straight to manual entry** and says COMPUTER
  DAMAGED. Inline coordinates are ignored in that state too -- they are
  automatic navigation, and the shorthand must not be a way round a
  restriction the long form obeys.

`trek_move_delta()` lives in **core**, not the UI. The first version did the
arithmetic in `main.c`, where nothing can test it; moving it means the manual's
own worked example is a native assertion -- 1,8,1,8 with DeltaX 1.0 and DeltaY
-2.2 lands on 2,6,1,6 -- along with the four edges and the rule that a delta
staying inside the quadrant is an **impulse** hop rather than a warp jump.

Verified on the machine: the computer poked to 80% through VICE's monitor took
`M` straight to `DELTAX:`, and `0.1` moved the ship one sector down within its
quadrant.

### The linker now enforces the stack

Adding this took MAIN down to about a hundred bytes, and llvm-mos's stock
script runs the region right up to `$C000` where `__stack` grows DOWN -- so a
build can succeed and then corrupt itself with the linker none the wiser.
`trek128.ld` now stops the region 64 bytes short, which turns that into a link
error.

Sixty-four rather than something generous because llvm-mos gives a
non-reentrant program a STATIC stack -- this one has 181 bytes of
`.Lstatic_stack` in `.noinit` -- and the soft stack is for recursion and
alloca, neither of which this code uses. It is a tripwire, not an allocation.
**It caught something immediately**: the release build fitted and the debug
build, which is ~76 bytes larger, did not. Factoring the duplicated coordinate
dispatch out of `do_move` and `do_move_prompt` paid for both.

## Boss mode is not being ported (Jamie's call, 2026-08-24)

Dropped deliberately, with the mechanic fully understood rather than for want
of knowing what it does.

**What the original does.** `Shift-F1` shells out to DOS. You get a real
`COMMAND.COM` prompt -- the graphics screen is replaced by whatever text was on
screen before the game launched, so it looks as though you were never playing.
`DIR` works. `EXIT` returns to the game with stardate, position and damage
intact. Confirmed three ways: the prompt is live and echoes typing, `COMSPEC`
sits in the string table exactly as `GetEnv('COMSPEC')` compiles to with no
fake-prompt text anywhere in the binary, and `EXIT` came straight back. The
manual then says so outright, and warns against running anything that changes
the screen mode.

**Why it is not being ported.** A shell-out has no C128 equivalent. The nearest
thing is dropping to BASIC, which throws the game away, so the honest
approximation was going to be: blank the VDC, wait for a key, restore. That is
a different feature wearing the same name -- and the thing boss mode is FOR is
an office PC with a manager walking past, which is not what a C128 sitting at
home is doing.

Against that, it is not free. It needs function keys in the CIA1 matrix, which
this port has never had, and MAIN is currently ~100 bytes clear of the linker
guard.

**What it takes with it.** The function keys would have brought F1-F10 as
command shortcuts, which the reference card documents (F1 Help, F2 Lasers, F3
Fire Torpedo, F4 Move Ship, F5 Max Energy, F6 Fix Systems, F7 Xfer Energy, F8
Repair Status, F9 Set Speed, F10 Dock). Those are still worth having on their
own merits and are NOT cancelled by this -- they are just no longer carried in
on boss mode's back. Nothing else depends on it.

Also worth keeping recorded: **boss mode is absent from the in-game F1 help**
and appears only on the reference card, so the help screen is not the authority
on what commands exist.

## The charset is dropped on the C128 (Jamie's call, 2026-08-24)

The port keeps the KERNAL's stock character set: PETSCII text, and ships drawn
as letters -- `E` for the Lexington, `K` battleship, `C` command, `S` scout,
`P` supply, `#` base, `O` planet -- with the measured EGA colours carrying the
identification, which is what the manual says they are for.

**Why, beyond the cost.** The font data cannot be committed. The CP437 half is
DOSBox's table and the letter half is lifted out of Anderson's binary, so
`tools/dump_font.py` writes into gitignored `build/` like `gen_music.py` does.
A public clone with no `reference/` would get no charset at all and fall back to
PETSCII anyway -- so the work would only ever have benefited people who already
have the original. Drawing ~96 glyphs of our own would fix that and is real
work for a purely cosmetic gain.

`tools/dump_font.py` stays. The measurement it made is worth keeping whether or
not anything consumes it, and a later port may want it.

### This does not bind the other ports

`core/` is what must never vary -- the rules, the constants, the save format.
Presentation is per-platform by design and always has been, so the Amiga and
VBXE legs will draw their own glyphs because they have no character generator
to reuse, and MEGA65 has room to do whatever it likes.

**The risk worth naming: the C128 is the FIRST port, so its ceiling is the only
one anyone has tested, and it is easy to let it define the content for
everything.** It should not. MEGA65 has 384K and the Amiga is flat. Where a
later target can do better, it should.

### For the record, the C128 is not uniquely constrained

`init-mmu.o` sets MMU config `$0E`, which already banks BASIC out: RAM from
`$0000` to `$BFFF`, KERNAL at `$C000-$FFFF`. The MMU treats `$C000-$FFFF` as
one unit, so banking it to RAM would take the KERNAL with it -- no disk I/O and
no IRQ. **`$C000` is a hard ceiling**, not a default left unraised, and the
~42K we have is close to the practical maximum for a program that uses the
KERNAL. Bank 1's 64K remains available for DATA through `FETCH`/`STASH`.

Against the rest of the lineup that is mid-table, not last: the X16 has about
38K of contiguous low RAM, slightly LESS than this; a 130XE with VBXE has
roughly 40-48K below the OS ROM plus four 16K banks; CoCo 3 and F256 page 8K at
a time out of far more; MEGA65 and the Amiga have no pressure worth the name.

## What the later ports should do BETTER (2026-08-24)

**FIRST, THE PRIORITY THIS SITS UNDER (Jamie, 2026-08-24): get all the game
logic and features working on the C128 before any other port starts.** This
list is about PRESENTATION -- pixel art, panel geometry, wrapping, fill. It is
NOT a licence to defer a feature to a roomier machine. Nothing in the rules,
the commands or the events gets postponed on the grounds that the Amiga will
have more room.

The two things dropped so far are consistent with that: boss mode is a DOS
shell with no C128 equivalent, and the charset is glyph shapes. Neither is game
logic, and neither changes what the game does.

**The consequence is that the memory work is now mandatory rather than
optional.** If features cannot be dropped to fit, then space has to be made:
the music data out of MAIN, and after that the string literals to bank 1. Those
stop being clever optimisations and become prerequisites for `RAY`, the planet
subsystem and the five missing loss endings.

---

The rest of this list needs to be concrete or it will quietly become "match the
C128 everywhere". **The C128's compromises are C128 compromises, not the
project's.** Every one below was forced by an 80x25 character grid or by ~42K of
address space, and every one is something a roomier target can simply do
properly.

`core/` still never varies -- rules, constants, save format. This is all
presentation, which has been per-platform by design since the first note.

### Forced by the CHARACTER GRID (bitmap targets should ignore these)

The Amiga and Atari VBXE draw into a 640x200 bitmap and have no character
generator to reuse, so they pay nothing to match the original exactly.

- **Panel geometry.** The C128's top band is ELEVEN rows where the original
  uses ten, because EGA Trek draws text at whatever pitch it likes inside a
  frame -- the short range scan fits a header and eight rows into eight
  character rows' worth of height. MAIN VIEWER pays for it: six interior rows
  in the original, five here. A bitmap port can use the measured pixel table
  in MEASURED.md directly.
- **The MSGS overlay.** Its content lines sit on a **10-pixel pitch**, so
  eleven of them fit in about ten character rows. The C128 needs fifteen rows
  for what the original does in ten.
- **Message wrapping.** The original wraps each message to three lines; the
  C128 holds ONE line of 36 characters, which is what fourteen rows and a
  38-cell interior will take.
- **SYSTEMS STATUS.** The original lists ten systems with full names; the C128
  shows twelve as two columns of three-letter abbreviations to fit 40x14.
- **Filled dialogs.** The original's boxes are filled dark grey. The VDC has no
  per-cell background short of reverse video, so this port's boxes show the
  screen through them.
- **The MAIN VIEWER.** The original alternates pixel art -- an external ship
  view, graphical function readouts -- with the C128 showing text. This is the
  single biggest visual gap and the one a bitmap port should attack first.
- **Ship glyphs.** Lettered ships (`K` `C` `S` `P`) are a C128 answer. See
  "The charset is dropped".

### Forced by ADDRESS SPACE (roomier targets should ignore these)

MEGA65 has 384K and the Amiga is flat; neither has this problem.

- **The briefing.** 7,574 bytes of prose that cannot be linked into a 42K
  binary at any compression, which is why the disk seam exists. A roomier
  target can hold it resident and stop worrying.
- **The message log's depth**, and the fact that it lives in VDC RAM at all.
- **Anything else that ends up streamed from disk** to buy code space.

### Genuinely platform-specific opportunities

- **Boss mode is dropped on the C128 because the machine has no shell. The
  AMIGA HAS ONE.** AmigaDOS can spawn a shell and return, so the Amiga leg can
  implement boss mode properly rather than approximating it. It is the one
  dropped feature that is not dropped everywhere.
- **Sound.** The C128 has the SID, which already flatters the PC speaker
  original. The Amiga has four channels of sampled audio and could do
  considerably more than either.
- **Colour.** The C128 VDC and EGA share sixteen colours, so that leg is exact.
  The CoCo 3 has eight at 80 columns and must choose; its palette is
  programmable, so pick the closest eight to EGA's set.

### The standing risk

**The C128 is the first port, so its ceiling is the only one anyone has
tested.** It is easy to treat what fits there as what the game is. It is not.
Where a later target can do better, it should -- and where it does, note it
here so the difference is deliberate rather than accidental.

## Bank 1: the far-memory seam and the string pool (2026-08-24)

The port had 170 bytes of MAIN left and five features to build. This moves the
bulk read-only data out of the address space and into RAM bank 1.

### The seam

`core/farmem.h` is the contract, `c128/src/farmem.c` the C128 backing, and the
shape is deliberately the same as the disk seam: contract beside the core,
mechanism per platform, `core/` never calls it. **Four of the six remaining
targets bank memory anyway** -- X16, Atari 130XE, CoCo 3 and F256 -- and the
two that do not (MEGA65, Amiga) implement it as a plain array. This is not a
C128 trick; it is the majority case, learned where it is cheapest.

The store is loaded FROM DISK, because initialised data cannot come from the
binary when the whole point is to get it out of the binary. That means
`far_load` finally exercises `plat_open`/`plat_read` -- the one part of the
disk seam nothing had used.

**It has more than one tenant**, so `far_load` appends and returns the base
offset it used. The first version loaded at offset 0 every time and the music
silently overwrote the prose.

### What moved

    strings   2,267 bytes of text  ->  194 bytes of index
    music     1,092 bytes of notes ->   14 bytes of offsets

`.rodata` went from 5,396 to 2,328. Free MAIN went from **170 to 561**.

That is less than the arithmetic suggests because the seam and the
newly-live streaming path cost about 2,700 bytes of code between them. The
real payoff is not the 391 bytes: **every future string now costs two bytes of
index instead of its length**, and everything left to build -- `RAY`'s outcomes
and loss report, five more loss endings, the planet dialogs, the boarding and
event messages -- is string-heavy.

### The pool

`tools/gen_strings.py` rewrites literals into `S(id)`, but ONLY where they are
a direct argument to a known drawing function. A literal in a static
initialiser cannot become a function call, and a script that rewrote those
would produce code that either fails to compile or, worse, compiles and
returns the wrong buffer.

`c128/src/strings.txt` is the source of truth and **is committed** -- unlike
the music, this prose is the port's own wording, so a clone with no
`reference/` still gets a game with words in it. Ids are permanent. The first
version rebuilt the list from the sources every run and wiped the pool the
second time, because by then the sources held `S(n)` and there was nothing
left to extract.

`S()` rotates through four buffers, because `ui_message(S(a), S(b))` wants two
pooled strings alive in one expression.

### Three bugs, all the same bug

Channel lifetime, in `storage.c`, three times:

1. `plat_open` called `cmd_status()` without opening channel 15, so chkin
   failed, the status read 255, and every open reported an error.
2. Fixed that, then closed channel 15 after reading the status -- which closes
   every file on the drive, so the data channel died as it was opened.
3. Fixed that, and `plat_read` still closed the data channel itself at EOF and
   cleared the flag, so `plat_close()` did nothing and channel 15 leaked. The
   strings loaded and the music did not.

None of these looked like an I/O fault. The symptom each time was a **title
screen with no words on it**. The rule is now one line: plat_open opens both
channels, plat_close closes both, nothing else touches either.

### Two llvm-mos traps worth keeping

- **An `__asm__` block with no `"memory"` clobber may be reordered** past the
  stores that set up its pointers. The first bank-1 probe read back the loop
  index instead of the data.
- **`lda (ptr),y` needs a ZERO PAGE pointer.** A `.bss` pointer gives
  "relocation R_MOS_ADDR8 out of range". `$FB/$FC` and `$FD/$FE` are free on
  the C128 and sit outside llvm-mos's imaginary registers at `$0A..$8F`.

### Where bank 1 is, and why $4000

`$D506` bits 1..0 set the common-RAM size -- 1K, 4K, 8K or 16K, shared between
both banks from the bottom. The default is 1K, so an earlier draft using
`$2000` worked on a default machine and would have silently aliased to bank 0
if anything selected 16K. `$4000` is above every setting.

Bank 1 is BASIC's variable storage, which is safe here because the game is
started with RUN (which clears variables) and BASIC is not running while it
plays. What it rules out is a port that returns to BASIC and expects its far
data to survive.

## THE MEASUREMENT SESSION PLAN (written 2026-08-24, NOT YET RUN)

Jamie's sequencing, and it is the right one: **take every remaining
measurement off the original first, then implement them all, then size the
result, then decide what goes in bank 1, then fix the bugs.** Measuring last
is what produces a port that fits and then does not.

Do not start this until the whole plan is read. The sessions below are
ordered so that each one leaves the game in a state the next one wants.

### Setup, every time

    cd ~/claude-code/dosbox-automation      # resources resolve from HERE
    DOSBOX_API_TOKEN=$(openssl rand -hex 32) \
      ./build/rel-arm64/dosbox --noprimaryconf \
      --conf ~/claude-code/egatrek/reference/trek-auto.conf

Verified working again 2026-08-24: comes up in mode 10h with the API live on
the first poll.

**Re-locate every structure at the start of every session.** Addresses move
between runs and they do NOT all move together -- the ship record shifted +16
bytes once while the twelve system percentages did not shift at all.
`tools/probe_original.py` has `find_ship_record()` for the anchor and
`enemies()` for the table; `A_*` in that file are one launch's numbers kept as
a worked example, not constants.

**Check for game-over before trusting any read.** A destroyed ship restarts
the program and the old addresses keep returning plausible, stale numbers.

### Run 1 -- the free ones, no combat, one game

Everything here is a screenshot or a subtraction and none of it needs the ship
to be in danger. Do it first because it is cheap and it clears four items.

1. **The two focused repair rows.** Damage one system, `F)ix` to concentrate
   on it, open `REPAIR`. The STATE OF REPAIR dialog prints Docked and Undocked
   estimated times side by side; it is what settled 20 and 47 a stardate, and
   it has never been read with a focus set. Read it undocked and again docked
   at a StarBase. Feeds `REPAIR_PER_STARDATE_FOCUS` (60, currently the
   manual's relative) and `_FOCUS_DOCKED` (100, likewise).
2. **The turn-cost table.** The stardate is a Turbo Pascal real in the ship
   record. Read it, issue a command, read it again -- for all twenty-five.
   One session produces the whole table, and nothing in this port has ever
   had one.
3. **The panel-versus-log stardate discrepancy.** There are THREE stardate
   copies in the ship record (offsets 181910/181922/181928 in the launch that
   was mapped). Watch which one the panel prints and which the message log
   prints. That is the whole question.
4. **Docking quantities per base type.** Dock at a StarBase, a Supply base and
   a Research station in turn, diffing energy, shields and torpedoes across
   each. The manual gives the *what* (l.356-361): StarBase everything, Supply
   life support plus energy and torpedoes, Research life support only. Only
   the quantities are missing.

### Run 2 -- laser effectiveness, and a target that has never been looked for

The heat band has beaten two sessions. **The value was searched for and is not
stored as a Turbo Pascal real or a 16-bit integer of the fired amount anywhere
in the 338KB** -- that is a settled negative, do not re-run it.

The new target is different: the manual describes **two** gauges (l.326-331),
temperature *and* effectiveness, and says effectiveness falls from heat and
from battle damage. **Effectiveness is a percentage on screen and is the
quantity that actually feeds damage.** It has never been searched for. Fire
volleys of known size in one quadrant, watch for a real or a word that tracks
the effectiveness gauge, and if it turns up the heat band follows from it.

Fall back to the gauge with a settle loop only if that fails. Heat **rises
gradually after a volley and resets on changing quadrant**, so a screenshot
taken at an arbitrary moment catches it mid-climb -- that is what made the
same 400-unit volley read 26 pixels once and 6 another time.

### Run 3 -- damage, with the systems array pinned

`SYSTEM_DAMAGE_THRESHOLD` has two samples that disagree: two hits took a
system straight to 0% with casualties, one hit did nothing. Pin energy,
shields and the twelve percentages between turns (`restore()` and
`/memory/freeze`), take fire repeatedly, and read the systems array directly
rather than the console.

Read the **damage-report text**, not the pools, when attributing a hit: a
Vandal Death Pod hits our ship and the enemy in the same turn, so a fall in
the pools is not necessarily enemy fire. This is the documented case where
the screen is the better instrument.

Same run, same rig: **`HP_SCOUT`**, still 100 and still unmeasured. Read the
enemy table at spawn and look, per class.

### Run 4 -- the expensive ones, several games

- **`RAY`'s four outcomes and their odds.** The manual (l.572-577) confirms
  the shape: destroys every enemy in the quadrant, "if it works", and if it
  does not there is no telling what happens. One sample so far. `write()`
  makes a sample cost a restore rather than a game, which is the only reason
  this is affordable. Capture the **Top Secret loss report** on the fatal
  outcome -- as a specification of what it covers, never its words.
- **Boarding-party frequency and duration.** The manual is silent; the string
  table has the mechanic. This one genuinely needs elapsed play.
- **`SELFDESTRUCT_FACTOR`**, the last ancestor-derived number inside an
  implemented command. Self-destruct with enemies at known distances and read
  their hit points falling out of the enemy table.
- **`HAIL` with a base actually in range.** Both captured outcomes are the
  no-base case. Write the ship next to a base rather than sailing to one.

### Run 5 -- screens to specify, not to copy

The planet chain is fully specified in the manual -- `ORBIT` scans for
energium crystals, `LAND` offers transporter or shuttle at 0.2 stardays,
`USE` consumes what was found -- but no screen of it has been captured.
Same for the **five unbuilt loss endings**, including the surrender, and the
events named in the strings and never seen: Union wreckage, distress signals,
Mongol reinforcement warnings. The feature list at l.158-164 adds that
**successful rescues increase score**, which is the scoring end of the
distress-signal mechanic.

Capture what each screen COVERS. Anderson's prose is not ours to copy, and
that rule has held for the briefing and the message table already.

### NOT for the emulator

- **Item 5's four FITTED constants** want `tools/dis16.py` on the routine.
  A constant that never varies should not be observed at all.
- Anything the manual states. That cost two items off this list on the day it
  was written, and one of them was hiding a live bug -- see MEASURED.md,
  "The repair table is a table, not a product". `reference/manual.txt` is
  ISO-8859 and invisible to grep; decode it as latin1 first.

### After the measurements, in order

1. **Implement everything measured**, in `core/` where it is arithmetic and
   behind the existing seams where it is not. `RAY`, the planet model, the
   function keys, the loss endings, reserve life support, the briefing.
2. **Then size it.** The C128 image is 41,410 bytes against a region of
   0xA3BF, so roughly 510 bytes free. Nothing on that list fits in 510 bytes;
   the question is how far over it goes, and that is not answerable until it
   is written.
3. **Then place it.** `core/farmem.h` is the seam and bank 1 is its C128
   backing, with prose and music already resident. Candidates in the order
   they were considered: the briefing text, the loss endings, the `USE`
   inventory, the planet tables. Decide with a measured number rather than a
   guess, which is the whole reason step 2 comes first.
4. **Then the bugs.** The sound regression is top of that list and it is
   mine. `make sound-check` has not been run since the bank-1 move.

The one ordering risk worth naming: step 4 last means the port stays silent
through all of step 1. That is Jamie's call and it is a reasonable one -- the
measurements need the DOS original and the sound bug needs VICE, and mixing
the two instruments in one session is how the last three confident wrong
numbers happened.

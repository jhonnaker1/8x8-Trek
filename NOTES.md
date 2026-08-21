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

2. **Returning to BASIC wedges the C128.** Quit currently parks with the console
   readable and RUN/STOP+RESTORE as the way out, which works but is a
   workaround, not a fix. Undiagnosed: this program runs at 2MHz, drives the VDC
   directly, and scans CIA1 behind the KERNAL's back, so there are several
   candidates and no way to observe the machine from a session on this host.
   `c128/src/main.c` carries a bisect recipe — return immediately from `main()`
   with no `vdc_init()`, confirm a clean exit, then add back the 2MHz switch,
   the VDC writes, and the CIA scanning in turn.

3. **Seed the RNG from something varying.** `GAME_SEED` is fixed, deliberately —
   it makes a side-by-side against DOSBox-X repeatable — but it means every run
   is the same galaxy. Wire it to timing once there is a title screen.
4. ~~**Capture the original at full 640×350**~~ DONE 2026-08-19 via
   dosbox-automation; the measured table is in `layout.c` and MEASURED.md.
   Original text follows.

   Correct the provisional panel table in `c128/src/layout.c`. The only layout source so far
   is a 320×175 half-scale screenshot where a cell is 4×7 px — too coarse for
   exact column boundaries. Everything reads that one table, so it's a
   single-site edit. The same capture settles which glyphs the original uses.
5. Breakpoint `EGATREK_unpacked.exe` in DOSBox-X's debugger to confirm the real
   constants — laser falloff, the no-damage threshold, boarding gates, scoring
   weights. `combat-model.md` has the 1978 values as the hypothesis to test.
   **Everything in `core/trek.h` marked PROVISIONAL is waiting on this**: the
   enemy-count-per-level formula, both travel energy scales, impulse timing,
   starting energy and torpedoes, and the 30-stardate mission length. Only the
   shield ceiling (2500) and the converter rate (400/stardate) come from the
   manual.
6. Custom charset. The port currently uses the KERNAL's stock character set;
   EGA Trek's panels want CP437-style glyphs. `commodore-uno/c128/src/vdc.c` has
   the upload path, including the VDC's 16-byte-per-glyph slot padding, and
   `tools/gen_charset.py` the generator precedent.
7. ~~**Game core, next pieces.**~~ Largely DONE: systems and repair, lasers,
   torpedoes, enemy fire and movement, docking, scoring, and a scheduled
   event queue all exist. Original text follows. Galaxy state, the distance table and the message
   log now exist. Still missing, roughly in dependency order: the 12 ship systems
   with percentage repair, combat (lasers and torpedoes), enemy AI and return
   fire, docking and resupply, then win/lose and scoring. `combat-model.md` has
   the 1978 formulas as the starting model — 1/distance falloff with a (2,3)
   random multiplier, the 15% no-damage threshold, ray-marched torpedoes with no
   accuracy roll, and `1000*(kills/stardates)^2` for score.
8. Not yet decided: where Atari VBXE (HR) sits in the port order now that it's
   first-tier. It shares the Amiga's 640×200×16 target, so the two could share
   a bitmap-layer design even though the CPUs and framebuffer access differ
   wildly (flat chip RAM vs an 8K banked MEMAC window).

   **An argument for VBXE that hasn't been weighed: it is the only leg with an
   automatable verification loop.** AltirraSDL plus AltirraBridge
   (`~/AltirraBridge-nightly-macos-arm64`) can drive an Atari headlessly —
   screenshots, frame-stepping, memory and CPU reads, breakpoints, injected
   input. VICE offers nothing equivalent here: its remote monitor halts
   emulation on connect and `screencapture` has no display access, so the C128
   leg can only be checked by eye. Item 1 above is unresolved for exactly that
   reason. A port whose output can be asserted on in a test is a materially
   different proposition from one that cannot.

## Platform suitability (from the Uno lineup)

Ranked for *this* game, whose demands are the opposite of Uno's — high information
density, colour as annotation rather than content. **Column count is the deciding
factor.**

- **Best:** `amiga` (640×200 bitmap, 16 colours), `atari` **VBXE in HR mode**
  (640×200 bitmap, 16 colours — same spec as the Amiga, see below), `c128` VDC
  (80×25 text), `x16` (80×60), `f256` (80×60). (`dos` — the original platform —
  is reference-only, not a build target; see Platform order above.)
- **Workable, needs a paged UI:** the 40-column colour machines — `c64`,
  `plus4`, `cbm510`, `mega65`, `coco`
- **Poor fit:** `pet` (80 columns but monochrome — colour carries real information
  here), `vic20` (RAM), `zxspectrum` and `ti99` (32×24, attribute clash /
  colour-per-glyph), `apple` (40×24 mono)

The C64 is a perfectly good target but the *hard* one for this game — 40 columns
against a nine-panel console means solving paging first and the game second.

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

### 9. Title screen

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

### 10. Setup screen

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

### 11. Briefing pages

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
probably wants compressing or paging off disk. And it is **Anderson's prose**,
which is copyrightable: the same rule already recorded for the message
catalogue applies, so these are a specification of *what each page covers*,
not text to copy.

### 12. Sound

The original uses the PC speaker. The C128 has a SID, the Amiga four channels
of sampled audio, and the Atari POKEY, so this is the one area where every
port can beat the original outright rather than approximate it.

Nothing is captured yet -- the emulator driver has no audio path, so working
out what the original plays and when is its own task. Likely: firing, hits,
red alert, docking, destruction, and the death-pod arrival.

Design constraint from the start: sound belongs in the platform layer, not the
core. `core/` must not gain an audio call. The event list the core already
returns (`EV_HIT`, `EV_ENEMY_MOVED`, `EV_BASE_LOST` and the rest) is the right
seam -- each platform decides what noise an event makes.

## 13. The command set: six of twenty-five (added 2026-08-19)

`reference/EGATREK.REF`, the quick reference card, is the authoritative list
and it is short enough to reproduce whole. The port implements six of it.

| command | what it does | state |
|---|---|---|
| `D)ock` | dock with a StarBase | **done** |
| `L)asers` | fire lasers | **done** |
| `M)ove` | move to quad/sector | **done** |
| `Q)uit` | quit | **done** |
| `T)orps` | fire torpedoes | **done** |
| `W)arp` | set warp speed | **done** |
| `E)nergy` | energy transfer | core `trek_divert()` exists and is tested — **wiring only** |
| `SHUP` / `SHDN` | shields up / down (arrow keys) | core models `shields_up` and the enemy turn reads it — **wiring only** |
| `MAX` | divert maximum energy to shields | `trek_divert()` again — **wiring only** |
| `R)epair` | state of repair report | needs the modal dialog we already have |
| `MSGS` | review old messages | needs a longer log than the four boxes hold |
| `C)hart` | chart of known galaxy | the console shows it permanently; may be redundant here |
| `F)ix` | control which system engineering repairs | needs a repair-priority model the core lacks |
| `INFO` | info on enemy in current quadrant | needs the MAIN VIEWER, which now exists |
| `HAIL` | hail a StarBase | no mechanic behind it |
| `A#` | acknowledge message # | no mechanic behind it |
| `S)elf` | self destruct | uses the setup screen's password — see item 10 |
| `RAY` | death ray | unimplemented mechanic |
| `O)rbit`, `LAND`, `USE` | planets, landing, crystals | unimplemented mechanics |
| `SAVE` | save game | deferred, see item 10 |
| `SND` | toggle sound | needs item 12 |
| `Shift-F1` | boss mode | screen blanker |

Plus the function keys, which are shortcuts rather than new commands:
F1 Help, F2 Lasers, F3 Fire Torpedo, F4 Move Ship, F5 Max Energy, F6 Fix
Systems, F7 Xfer Energy, F8 Repair Status, F9 Set Speed, F10 Dock.

**Three of these are wiring, not features.** `trek_divert()` is written and
tested with no way to call it, and `ship.shields_up` is modelled and read by
the enemy turn with no way to change it — so the port currently cannot raise
its own shields. That is a bigger hole in how the game plays than anything on
the front-end list, and it is the cheapest thing here to close.

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
- **Rescues** — worth **+200 each** in the scoring rubric and currently
  unreachable. The evacuation deadline messages already seen in the original
  ("Planet Gallista-8, quad 8-4, requests evacuation. They can only hold out
  until 3516.5") are the trigger, and the event queue can already carry them.
- **Stars destroyed** — **-5 each** in the rubric. Needs a torpedo that can
  hit a star rather than only an enemy.
- **Boarding parties** — in the extracted string catalogue, absent from the
  manual. See MEASURED.md open item 11.
- **The hall of fame** — `TREK.SCR`, top two scores per command level. The
  setup screen already collects the player's name for it.

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

- Enemy hit points as rolled bands rather than constants (MEASURED.md).
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
- The custom charset, the VBXE port order, the C128 return-to-BASIC bug.
  Platform work; the ancestor is a terminal program.
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

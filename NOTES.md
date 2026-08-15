# EGA Trek remake — project notes

Status as of 2026-08-14. Research and design decisions only; **no code written yet.**

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
- `superstartrek.bas` — the 1978 listing itself.
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

## Open questions / next steps

1. Stand up the C128-VDC skeleton: fixed-point math layer, 80×25 panel layout
   traced off `console_screenshot.jpg`, driven through `commodore-uno/c128/src/vdc.c`.
2. Breakpoint `EGATREK_unpacked.exe` in DOSBox-X's debugger to confirm the real
   constants — laser falloff, the no-damage threshold, boarding gates, scoring
   weights. `combat-model.md` has the 1978 values as the hypothesis to test.
   Also the go-to for screenshots when graphics/layout need checking.
3. Not yet decided: whether to `git init` this directory.
4. Not yet decided: where Atari VBXE (HR) sits in the port order now that it's
   first-tier. It shares the Amiga's 640×200×16 target, so the two could share
   a bitmap-layer design even though the CPUs and framebuffer access differ
   wildly (flat chip RAM vs an 8K banked MEMAC window).

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

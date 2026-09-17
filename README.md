# 8x8 Trek

A remake of **EGA Trek** (Nels Anderson, 1988–1992, shareware) for vintage
hardware — one byte-identical shared core plus a per-platform video, sound and
input layer, following the architecture of
[`commodore-uno`](https://github.com/jhonnaker1/commodore-uno).

The name is the galaxy: 8×8 quadrants of 8×8 sectors.

> **Status: eleven ports released.** The **Commodore 128**, **Commander X16**,
> **Amiga**, **MEGA65**, the **Atari 800XL + VBXE**, the **Atari Falcon030**,
> the **Tandy CoCo 3 + SuperSprite FM+**, the plain **Commodore 64**, the
> **Atari ST/STE**, the **stock CoCo 3** and the **Apple IIgs** are feature
> complete and released as [v0.19.1](../../releases/latest), 2026-09-16.
> The **Apple IIgs** is the new one, and it was **ruled out twice before it was
> tried** — once on colour and once on width. Super Hi-Res 320×200 is exactly
> 40×25 cells of 8×8, its palette holds **EGA's own sixteen** so brown is brown
> rather than the olive a C128 renders, and the Ensoniq 5503 gives music and
> effects a voice each. **There is no ProDOS on its disk**: block 0 is the
> port's own loader, reading through the drive's firmware, so the disk is ours
> to give away — the same argument the Atari port's boot record rests on. See
> [`iigs/README.md`](iigs/README.md).
> Every port colours each message line by the department that speaks it, as the
> original does.
> **All eleven have been played by a person**, and that is where the faults
> have come from: the CoCo 3's sitting found a console repaint taking **12
> seconds** that no benchmark here had ever timed, because the benchmark
> measured the blit and the blit was a third of the cost. The C64's sitting
> found nothing — *"its perfect"*. And the IIgs's found, in three minutes, the
> one thing **seventeen automated gates could not see**: `BRIEF.TXT` was never
> put on the disk, and the briefing skips silently when it is missing.
> **How to run each release asset, machine by machine, is in
> [`RUNNING.md`](RUNNING.md)** — emulator command lines included.
>
> The **Commodore 64** is the new one, and it took **the least new code of any
> port here** — the opposite of the CoCo 3 it followed. It is the first that is *mostly
> another port*: `vic.c` was written as the C128's 40-column driver and links
> into it **unchanged**, and eight more shared files come across behind four
> `#ifdef` blocks totalling nine lines. Two files are genuinely the C64's,
> about 215 lines. Against every expectation in its own scope note it is also
> **the roomiest 8-bit port here** — 6,112 bytes spare where the 40-column
> C128 has 304, because BASIC's 8K is plain RAM and the overlay window moved
> into the 4K at `$C000` that nothing ever covers.
> See [`c64/README.md`](c64/README.md).
>
> The **Atari ST and STE** joined on 2026-09-14 and it is **one file** — every
> seam but the video driver already existed in the Falcon port. It is the
> first port here built against a rule this project had written down and then
> had to retire: the whole Atari 16-bit line went out in August for being
> *"only 40 columns"*. See [`st/README.md`](st/README.md).
> Research and decisions are recorded in [`NOTES.md`](NOTES.md).

## The original, and why I'm doing this

EGA Trek was written by **Nels Anderson** and released as shareware between 1988
and 1992. It took the 1978 *Super Star Trek* everyone had typed out of a BASIC
listing and gave it a real command console — nine panels of scanners, gauges,
damage reports and incoming messages, all live at once, in 16 colours at
640×350. Nothing else on a PC looked like that. You weren't reading a game
report any more; you were sitting in the chair.

I loved this game. I spent hours in that console, and the shape of it — where
the galaxy chart sat, the way messages stacked up faster than you could
acknowledge them, the sinking feeling when a system dropped off the status
list mid-fight — has stuck with me for decades.

Anderson asked for suggestions in the manual, and said outright that many of
the game's features came from the people who played it and wrote in. That's
the spirit I want to keep going. This project isn't trying to replace EGA Trek
— the original still runs, and you should go play it. It's trying to carry the
game onto the machines it never reached, as faithfully as each one allows.

**All credit for the design, and for the game itself, belongs to Nels
Anderson.** EGA Trek remains his work under its own shareware terms; this
remake is a separate implementation, and nothing here relicenses or replaces
it. If you enjoy the original, register it — that was always the deal.

## The lineage

EGA Trek didn't start with Anderson either, and he was the first to say so. In
his own manual:

> A space combat game similar to EGATrek was one of the first computer games
> ever written. […] There have probably been more versions of this game written
> than any other […] I originally discovered the game around 1974 running on a
> DEC System 10 mini and was soon hooked. I've since written several other
> versions of the game for computers including the Timex-Sinclair ZX81,
> Apple ][, Prime 50-series minis and MS-DOS machines, all using BASIC of one
> sort or another.

So the man who wrote EGA Trek was doing precisely what this project is doing:
he found a game he loved, and carried it onto every machine he could reach.
This remake joins a chain rather than starting one.

That chain, as recorded in the header of the 1978 listing itself:

- **Mike Mayfield** — the original *Star Trek*, written for mini and mainframe
  machines in the early 1970s
- **Dave Ahl** — the modified version published in DEC's *101 BASIC Games*
- **Bob Leedom** — the substantial rework and debugging that made it *Super
  Star Trek*, April and December 1974, at Westinghouse Defense & Electronics
- **John Gorders** — converted to Microsoft 8K BASIC, 16 March 1978, which is
  the `SUPER STARTREK - MAY 16,1978` listing this project calibrates against
- **Nels Anderson** — EGA Trek, 1988–1992, which gave it the nine-panel console

The descent to EGA Trek is not merely stylistic. *Super Star Trek* packs each
quadrant into its galaxy chart as `K3*100 + B3*10 + S3` — Klingons, starbases,
stars. Anderson's manual describes his long-range scanner as *"the three digit
number for each quadrant represents the number of Mongols, type of friendly
star base, and number of stars (respectively)."* The same encoding, fourteen
years later. Both use an 8×8 galaxy of 8×8 sectors — which is where this
project's name comes from.

*Super Star Trek* is used here as **calibration material, not a target**: its
combat math gives the shape of the curves (damage falloff, the no-damage
threshold, the scoring formula) while the actual constants get confirmed
against EGA Trek itself. The working notes are in `reference/combat-model.md`;
the listing comes from
[coding-horror/basic-computer-games](https://github.com/coding-horror/basic-computer-games/blob/main/84_Super_Star_Trek/superstartrek.bas).

## Why not DOS

The original DOS release already exists and is very good. Rebuilding it would
add nothing, so **DOS-EGA is not a build target** — instead the original
`EGATREK.EXE` runs in [DOSBox-X](https://dosbox-x.com/) as a *reference oracle*:
breakpoint it to confirm real constants (laser falloff, the no-damage threshold,
boarding gates, scoring weights), or run it and screenshot a panel when layout
or artwork needs checking.

That matters because the game's manual documents only part of it. Boarding
parties, plasma bolts, Vandal cloaking, long-range tractor beams, scanner
jamming, black holes, supernovas and defective energium crystals are all in the
binary and absent from the docs.

## Where the C128 port stands (2026-09-02, played and closed 2026-09-11)

*The port's own documentation is [`c128/README.md`](c128/README.md) — the
memory map, bank 1, the overlay rules, the disk, and the three traps it found
first. This section is the project-level summary.*

The C128 port is **feature complete against the original's mechanics**. Every
constant it uses was read out of the binary or measured against it running:

```
BINARY 203   CONFIRMED 15   MEASURED 12   FITTED 0   DERIVED 0   PROVISIONAL 0
```

`make tiers` audits that and fails the build on an unmarked constant. Nothing
in the port is a guess about a number, and the read list, the emulator-run
list and the build list are all empty.

What that covers: all 25 commands, the nine-panel console, the twelve-page
briefing, save and restore, the hall of fame, and the mechanics the manual
never mentions -- boarding parties, the Vandal death pod, plasma bolts both
ways, black holes, supernovae, the death ray's five outcomes, tractor beams,
wear and tear, reinforcements, a spy who sabotages a system, a settlement with
a clock running against it, and a damaged computer eating your star chart.

**Colour per message is built** (2026-09-10) and ships on all eight released ports. EGA
Trek has no department palette at all -- every message site in the original
picks its own colour -- so this is a department map plus per-event exceptions,
attributed by reading every message site in the binary back to the `SetColor`
that governs it. It was deferred for two weeks on the cost of a byte per pooled
string and a far read at every drawing site; measuring the real split is what
made it affordable.

Three things are deliberately absent, each decided rather than left undone.

One is **deferred to roomier targets** rather than cut: the **MAIN VIEWER's
other nine instrument pages** carry live data and two of them are wider than
this port's seventeen-column panel. What cycles them was read on 2026-09-02 and
is no longer the blocker -- a fresh `Random(10)` roughly every six seconds, or
the page number typed as a command. It is deferred on every port, the Atari
included, by Jamie's call.

The **CP437 charset** and **boss mode** were ruled out for this platform
outright. The reasoning for all three is in `NOTES.md`.

**It has been played, and it is done.** Jamie, 2026-09-11: *"looks, sounds, and
plays great."* That closed the oldest item on the open list.

Play was the most productive thing in the project while it was open. Four
sessions at the keyboard found four bugs no build check could see: `FIX` was
missing half its command; nine letters of the alphabet could not be typed, and
the self-destruct password `JAMIE` has a J and an I in it; a yes/no question
was drawn in the wrong panel; and the last page of the briefing never waited
for a key. Each lived exactly where an automated check does not go, and each
fix shipped the check that closes its class.

**The fifth session found nothing**, which had not happened before — and
whether the game adds up to something survivable, readable and fair was the
part no build check could ever have answered.

## Targets

**Eight ports, and the rule that used to head this section is gone.** It said
80 columns; the C64 shipped at 40 and the console is the same console seen a
half at a time. What a machine actually has to carry is **enough colour** —
and, as of 2026-09-14, that no longer separates the candidates either. See
*What is left* below.

**Released, in the order they shipped.** Seven of the nine put the console on
an 80×25 grid of per-cell colour, which is the shape it was designed on; two
page it across 40-column halves.

| Platform | Display | CPU | Status |
|---|---|---|---|
| **Commodore 128** (VDC) | 80×25 text, 16 colours per cell | 8502 | **Released** — [v0.15.0](../../releases/latest); the first port, and the one the others are a diff against. See [`c128/README.md`](c128/README.md) |
| **Commander X16** | VERA text 80×60, per-cell fg+bg from 256 | 65C02 | **Released** — [v0.15.0](../../releases/latest); see [`x16/README.md`](x16/README.md) |
| **Amiga** (OCS/ECS, KS2.0+) | 640×256 bitmap, 16 colours | 68000 | **Released** — [v0.15.0](../../releases/latest); see [`amiga/README.md`](amiga/README.md) |
| **MEGA65**, native C65 mode | 80×25, VIC-IV H640, colour on all 2000 cells | 45GS02 | **Released** — [v0.15.0](../../releases/latest); one D81, see [`mega65/README.md`](mega65/README.md) |
| **Atari 800XL + [VBXE](https://vbxe.atari.org/)** | 80×25 text, per-cell fg+bg from 1024 colours | 6502 | **Released** — [v0.15.0](../../releases/latest). A **self-booting disk with no Atari DOS on it**: its own boot record and directory, SIO underneath. **About two minutes to load on a stock 1050**, seconds on an emulator or a fast-SIO drive; see [`atari/README.md`](atari/README.md) |
| **Atari Falcon030** | 640×480 on VGA, 640×400 on RGB and TV, 16 colours | 68030 | **Released** — [v0.15.0](../../releases/latest). The port that **deletes the most**: no overlays, no banking, no filesystem of our own, an 8×16 font out of ROM and GEMDOS for storage. **Picks its mode from `VgetMonitor()`**; ST monochrome is refused. See [`falcon/README.md`](falcon/README.md) |
| **CoCo 3 + SuperSprite FM+** | V9958 GRAPHIC6, 512×212, 16 colours per pixel | 6809 | **Released** — [v0.15.0](../../releases/latest). The only port with **a filesystem of its own** (Disk BASIC, written from scratch), a **first-stage loader** for a 44K image BASIC cannot place, far memory in the card's VRAM, and eleven overlays. **It was the slowest port here until the stock CoCo 3 arrived** — that one's string pool lives on the diskette. See [`coco3/README.md`](coco3/README.md) |
| **Commodore 64** | VIC-II 40×25 text, 16 colours per cell | 6510 | **Released** — [v0.16.0](../../releases/latest). The first port that is mostly *another port*: the C128's 40-column driver and eight shared files, behind four `#ifdef`s. String pool in the RAM under the KERNAL — **writes always reach RAM on this machine**, so one KERNAL LOAD fills it and reading back is a `memcpy`. **6,112 bytes spare**, more than any other 6502 port here. **The console is in two halves** — `C` shows the chart page, any key returns. See [`c64/README.md`](c64/README.md) |
| **Atari ST / STE** | 320×200 bitmap, 16 colours | 68000 | **Released** — [v0.17.0](../../releases/latest). **One file.** Every seam but the video driver comes from the Falcon port — `vc +tos` *is* the ST target, the sound is the same YM2149 through the same XBIOS call, storage is GEMDOS. The planes are word-interleaved as on the Falcon; the 8×8 glyphs are the Amiga's. **This line was ruled out in August for being "only 40 columns"** — a rule that no longer exists. See [`st/README.md`](st/README.md) |


### How much colour the console actually needs

**Fifteen are used, eight are load-bearing**, and the two numbers answer
different questions. The console uses every EGA colour except black, which is
the background — it tells `RED` from `LTRED` and `BLUE` from `LTBLUE` and means
different things by them. But only **eight** carry a game rule: the four Mongol
ship types, the chart's Mongol and base markers, the department colours and the
message colour. Collapse one of those eight and the player can no longer tell a
battleship from a command ship. Collapse one of the other seven and the screen
is less pretty.

`tools/check_colours.py` **derives that set every run** — from `core/ega.h` and
from `dept_color()` in `ui.c`, so adding a colour grows the requirement by
itself and nothing has to be remembered. Hand it a machine's palette and it
answers before anyone writes a line of a driver. It reports the **cost** of a
pass as well as the pass: the C64 folds two decorative colours, a machine with
eight slots folds seven.

An earlier version of this section said the console needed only eight and gave
no such distinction. **That sentence was load-bearing for exactly one decision**
and it kept a machine on the list that could not run the game as designed.

### What is left, and which axis rules the rest out

Run on 2026-09-14 across [commodore-uno](https://github.com/jhonnaker1/commodore-uno)'s
lineup, now that 40 columns is a shipped layout. **All eight remaining
candidates pass the colour check**, so the discriminator is no longer the
display — it is how much code already exists.

**Out on width or memory:** PET (monochrome as well), **VIC-20** (22 columns,
and 35K at most), **ZX Spectrum** — and its recorded reason was the weaker of
its two. The memory figure (48K against the ~53K this game needs) is the one
that gets cited, and the CoCo 3 GIME port has since taken ~7.9K off that by
putting the string pool on the disk. **The blocker that does not move is
WIDTH: the Spectrum is 32 columns and the narrowest layout that exists is
`layout40.c`.** A Spectrum port needs a `layout32.c` — a new console layout,
which is the expensive part, not a driver. Worth recording because a 128K
Spectrum would dissolve the stated reason and leave the real one standing.

**Out on colour, the same rule that excluded the PET:** **Apple IIe** — and it
is over-determined rather than marginal, which is worth saying now that two
rule-outs on this page have turned out to be expired. **Three independent
blockers, none of which any 2026-09-16 finding touches:** its 40-column text is
monochrome and its 80-column card is text-only monochrome, so the eight
information-bearing colours have nowhere to go; it is **24 rows** against a
console that needs 25 and uses row 24; and double hi-res colour is artifacted
down to **~140 real colour pixels**, about 35 cells, which is below 40 before
anything is drawn in them. Eight colours instead of sixteen does not help a
display with none, an ASCII-only font does not help, and moving far memory to
disk does not help; **stock Atari 800XL** — ANTIC's text modes have no per-cell
colour, which is what VBXE was for and is the cleanest illustration of why
width and colour must not be bundled; **TI-99/4A** — the TMS9918A colours
*groups of eight character codes*, so eight colours across this game's glyph set
overruns the 256 codes it has.

**Not machines:** MS-DOS is the reference oracle and never a build target;
C64 OS is the C64 again.

#### Swept 2026-09-16: what the list still gets wrong

Two rule-outs turned out to have expired without anyone noticing — the ST's
width rule and the IIgs's — so the rest were re-derived against the rules as
they stand rather than as they were written. **No conclusion here changes; the
REASONS do, and a reason is what a future pass will re-check.**

**THE CONSOLE NEEDS 25 ROWS AND THIS SURVEY NEVER CARRIED IT.** `layout40.h`
is `SCR40_ROWS 25` and row 24 holds the briefing footer, so a 24-row machine
needs a layout that does not exist — the expensive part, as the Spectrum's 32
columns are. **NOTES.md item 57 has had this all along**, and in the right
shape: the sub-80 world is *three families* — **40×25** (C64, C128 VIC-IIe,
Plus/4, CoCo 3 GIME), **40×24** (Atari 800XL, Apple IIe), **32×24** (TI-99/4A,
MSX1, ZX Spectrum) — and *"a hand-written `layout40.c` serves only the first."*
**This table discriminates on width and colour and never picked that up**,
which is the more awkward defect of the two: not an unknown, a divergence.
Its own Apple IIe entry reads *"40×24 text is monochrome"* with the 24 doing no
work. Check rows on every live candidate before believing anything else.

**The PET is filed under the wrong axis.** It sits under *out on width or
memory* with *"monochrome as well"* — but a PET is 40×25, and an 8032 is 80×25.
Both are shipped layouts. **Width does not rule it out at all**; monochrome
does, on its own. Out either way, for one reason rather than two.

**The VIC-20's memory half has eroded like the Spectrum's.** "35K at most"
against a requirement that the disk-backed pool has cut by ~7.9K. **22 columns
is the blocker that does not move**, and it is further from 40 than the
Spectrum's 32.

**Two colour reasons need re-deriving and are flagged rather than fixed**,
because neither was measured here and this project does not promote a guess to
a finding. The **TI-99/4A** entry reasons from the TMS9918A colouring groups of
character codes — but that machine is **32 columns**, which disqualifies it on
the same axis as the Spectrum whatever the colour answer turns out to be, so
the cited reason is not the load-bearing one. The **stock Atari 800XL** entry
says ANTIC's text modes have *"no per-cell colour"*, which is absolute where
the truth is likelier to be *"fewer than the eight the console needs"* —
ANTIC's multicolour character modes do carry some. **Same verdict, weaker
grounds than stated.**

**Done.** The **Atari ST/STE** was top of this list and shipped in
[v0.17.0](../../releases/latest) the same day. **The estimate held exactly**:
one file, the video driver, four bitplanes interleaved by word. `vc +tos` was
already the ST target, the sound was already the same YM2149 through the same
XBIOS call, storage was already GEMDOS. 320×200 ÷ 8×8 is 40×25.

### Two constraints the card-less CoCo 3 loosened (2026-09-16)

Neither is about colour, which stopped discriminating in September and which
the GIME port merely *proved* by shipping eight colours and being played.

**FAR MEMORY CAN BE THE DISK.** `core/farmem.h` always allowed it — "the
contract does not care where the bytes live" — but no port had done it.
`coco3gime/src/gimemem.c` reads the ~7.9K string pool a sector at a time
through a two-way cache, which is **7.9K off the RAM a candidate needs**. It
costs a drive that works while the console draws, and a one-entry cache turns
that into two disk reads per label, which is not a theoretical hazard: it made
the port unplayable until it was measured.

**AN ASCII-ONLY FONT IS ENOUGH.** The console was drawn with C128 box-drawing
glyphs, a solid block and reverse video, and nothing recorded whether those
were required or merely convenient. They are convenient. The GIME has none of
them: panel rules are **underlines**, which join across cells where a `-`
leaves a gap at every boundary; solid cells are **spaces in a background
colour**; verticals are `|` and read as dotted. **A machine with a fixed ASCII
charset and per-cell colour can draw this console**, which was never certain
before and quietly widens every remaining candidate.

**AND A FIFTH AXIS, LEARNED THE EXPENSIVE WAY: what does the candidate's ROM
switch take away?** The C64's `$01` pages out BASIC and leaves the KERNAL; the
Plus/4's `$FF3F` removes both, which needs a shim around every system call, its
own interrupt handling and its own keyboard. That is invisible in colours,
columns, rows and existing-code counts, and on the Plus/4 it decided the port.

**ROWS, CHECKED 2026-09-16 — the axis this table was missing.** The console is
25 rows and uses row 24, so a 24-row machine needs a layout that does not
exist.

| Candidate | Geometry it would use | Rows | Where that comes from |
|---|---|--:|---|
| **Plus/4** | TED text, 40×25 | **25** | NOTES item 57's 40×25 family |
| **MSX2** | **V9938 SCREEN 7 bitmap, 512×212, 6×8 cells → 80×25** | **25** | **the CoCo 3 card port already does exactly this** on the sibling V9958: `coco3vid.c` — *"512 and 8-pixel rows give 26; the console takes 80×25"*, `MARGIN_Y 6` centring 200 lines in 212 |
| **Foenix F256K** | Vicky text | **UNKNOWN** | **nothing in this repository records it, and there is no emulator or ROM here to ask.** Check before costing it |
| **CBM-II P500** | VIC-II text, 40×25 | **25** | a real VIC-II |
| **Apple IIgs** | SHR 320×200, 8×8 cells | **25** | 200 ÷ 8 |

**AND THIS CORRECTS THE MSX2 ENTRY RATHER THAN CONFIRMING IT.** The worry was
that an MSX2 is 80×**24** in text mode, which would have made the cheapest-
looking candidate expensive. **An MSX2 port would not use text mode at all** —
it would use the bitmap, exactly as the CoCo 3 card port uses GRAPHIC6 on the
V9938's sibling chip, and that yields 80×25 with six-pixel cells. The V9958
driver is the thing that transfers; the text mode was never the plan.
**MSX2 comes out of this stronger, not weaker.**

**Still live, cheapest first** — three unattempted, one parked, and the two
struck through are shipped:

| Candidate | What it would cost | What already exists |
|---|---|---|
| ~~**CoCo 3, no SuperSprite**~~ | **Released** — [v0.18.1](../../releases/latest), and **run on a real CoCo 3 from a CoCo SDC**, the first port here to reach hardware that is actually owned. The estimate said video, sound and far memory; all three were written. See [`coco3gime/README-release.txt`](coco3gime/README-release.txt) | |
| **Commodore Plus/4** | **PARKED — and this row was wrong.** Video and sound are written and measured; the link script was the easy part. **`$FF3F` removes BOTH ROMs**, where the C64's `$01` leaves the KERNAL mapped — so every KERNAL call needs a banking shim, the 6502's vectors vanish with the ROM, and masking interrupts costs `GETIN`. A fourth seam, bigger than the other three. See [`plus4/README.md`](plus4/README.md) | `layout40.c`, `ui.c`, `strpool.c`, `core/`, and the C64's `storage.c`/`overlay.c` — which did link unchanged |
| **MSX2** | a fifth CPU family and its toolchain | the CoCo 3's V9958 driver — the V9938 is its sibling, and it gives **80×25** through SCREEN 7's bitmap with six-pixel cells, which is the card port's exact arrangement. `layout.c`, not `layout40.c` |
| **Foenix F256K** | MMU far memory, and cc65 rather than llvm-mos | SID at `$D400`, so `sid.c` ports verbatim |
| **CBM-II P500** | **every screen write banked** — the opposite of the C64's "it is the same driver" | a real VIC-II and a real SID |
| ~~**Apple IIgs**~~ | **Released** — [v0.19.0](../../releases/latest), and played the day it was finished. The estimate said a hand-built linker config, an Ensoniq driver, ProDOS storage and cross-bank writes. Four of those were right; **ProDOS was not done at all** — the disk carries its own boot block, its own directory and its own reader over the drive firmware, so nothing on it is Apple's. See [`iigs/README.md`](iigs/README.md) | |

**The IIgs's exclusion expired on 2026-09-05 and this table did not notice for
eleven days.** It went on the width rule — *"320 mode is 16 colours and 40
columns, which fails the other hard rule"* — and that rule died when the ST
shipped 40 columns in v0.17.0. Its separate 640-mode failure (sixteen colours
on a scanline, only four placeable at any x, measured in MAME) is real and is
about a mode nothing now needs. **Both of the other recorded blockers have also
gone, checked 2026-09-16 rather than recalled:** `mame apple2gs -verifyroms`
answered *"romset is not present"* in September and answers **`romset apple2gs
is good`** now, so the rig exists; and `mosw65816` is a supported CPU in the
llvm-mos already installed here.

**AND THE NEXT SENTENCE HERE WAS WRONG, MEASURED 2026-09-16.** It said the
toolchain generates *"6502 code with the 65816's extra opcodes and no
`rep`/`sep`, no long addressing"*. The first half holds — the CODE GENERATOR
emits 8-bit 6502-shaped code. The second half was never tested and is false:
the **assembler** encodes the entire 65816 instruction set (`8F` `9F` `B7`
`97` `C2` `E2` `EB` `8B` `AB` `54` all assemble), which is exactly what a
hand-written video seam needs, and the code generator emits absolute-long
accesses to globals on its own initiative. **A claim about a toolchain's
output is a measurement, and this one was a recollection.**

### What a sub-80 port actually entails

The C64 measured it, so this is arithmetic rather than an estimate.

**What comes free: 10,527 lines.** `ui.c`, `main.c`, `layout40.c`, `strpool.c`
and `core/` entire — plus, on anything VIC-shaped, `vic.c`, `input.c`, `sid.c`,
`storage.c` and `overlay.c` as well. **What the C64 wrote: 882 lines, of which
222 are C** — `c64mem.c` (159) and `c64log.c` (63). The other 660 are a linker
script, a Makefile and two verify tools.

**The seams are 34 functions across six headers**, and a port fills every one
of them somehow — by writing it, or by linking somebody else's:

| Seam | Fns | What it is |
|---|--:|---|
| video | 13 | `vdc_init`, `scr_put` / `puts` / `clear` / `fill_rect` / `hline` / `vline`, `wait_vsync`, `vdc_shutdown`, `plat_exit` |
| message log | 3 | `vdc_set_address`, `vdc_data_read` / `write` — 2K of scratch outside the program |
| sound | 9 | `snd_init` / `beep` / `music` / `effect` / `poll` / `off` / `toggle` … |
| storage | 5 | `plat_read_all` / `write_all`, and `plat_open` / `read` / `close` for the streamed briefing |
| far memory | 3 | `far_load` / `size` / `read` — the ~7.9K string pool |
| overlays | 2 | `ovl_load`, plus a linker script and image-cutting in the Makefile |

Then, per port: an EGA→machine colour map, a `strings.override.txt` for the two
or three strings that name the machine, a `verify` (**a port without one makes
`make ports` true and meaningless**), two READMEs and a `RUNNING.md` entry.

Against that, what each candidate would actually cost:

**Atari ST/STE — one seam.** Video only. Everything else is the Falcon port
unchanged. The new file is planar: four bitplanes **interleaved by word**
against the Falcon's chunky 8bpp, which is the one thing that did not carry
from the Amiga either. 320×200 ÷ 8×8 is exactly 40×25.

**CoCo 3 with no card — three seams.** Video, sound and far memory, because the
SuperSprite carries all three. The disk driver, the first-stage loader, the
overlays and the toolchain carry over. The 512K MMU is the obvious new home for
the string pool.

**Plus/4 — three and a half, and it is the one that looks cheaper than it is.**
The video driver is `vic.c` with two addresses changed: screen `$0C00`, colour
`$0800`, 40×25, a `(luminance<<4)|hue` colour byte. RAM under ROM at
`$FF3E`/`$FF3F` gives the far store the same way the C64's KERNAL RAM does.
**But TED is not SID** — a genuinely new sound driver, two voices where the
game expects three — **and the keyboard is not CIA1**, so `input.c` does not
port either. Storage should, since the Plus/4's KERNAL jump table is
C64-compatible; that half is reasoned rather than measured.

**MSX2 — four seams and a fifth CPU family.** The V9938 is the V9958's sibling,
so the CoCo 3's bitmap-with-a-software-font driver is the model rather than a
rewrite. Z80 and SDCC, and a 64K window over paged RAM means overlays and far
memory both need doing.

**F256K — three seams.** SID at `$D400`, so `sid.c` ports verbatim, and Vicky's
per-cell colour text is close to `vic.c`. The costs are the MMU for far memory
and cc65 instead of llvm-mos — and whether the game fits under cc65 at all is
genuinely unknown rather than merely tight.

**CBM-II P500 — one seam, done badly.** A real VIC-II and a real SID, in a bank
plain pointers cannot reach, so **every screen write goes through a banked
accessor**. The cheapest-looking candidate with the most expensive hot path.

**Two of these were ruled out on width alone, and that reason no longer
exists.** The whole Atari 16-bit line went on 2026-08-22 because 320×200×16 is
*"only 40 columns"*, and the **Apple IIgs** because *"320 mode is 16 colours and
40 columns, which fails the other hard rule"*. Neither was ever judged on
colour; both carry all sixteen distinctly. The IIgs's separate 640-mode failure
— sixteen colours on a scanline but only four placeable at any x, measured in
MAME — is real and is about a mode nothing now needs.

**And the card-less CoCo 3's exclusion was answering the wrong question.** It
was dropped on *"the console uses fifteen colours, not eight"*. True, and not
the same claim as *eight is not enough*: `WIDTH 80` plus `ATTR` gives eight
per-cell foreground hues — **measured against a real ROM in XRoar**, from a
palette reprogrammable out of 64 — and those eight carry all eight
information-bearing colours, Mongol ship types included. It fits with nothing to
spare; seven decorative colours fold.

**Nothing here is a decision.** The Falcon, the CoCo 3 and now the **Apple
IIgs** were each re-opened against a written argument in this file, and all
three shipped — the IIgs against two of them, one on colour and one on width.
A measurement settles how hard, never whether.


### The order, decided 2026-08-23, rewritten 2026-09-05 — HISTORY

> **All eight of these shipped, and the list below is kept for its reasoning
> rather than as a plan.** Two of the machines it rules out have since been
> re-opened and released. What is live is *What is left* above.

The original split targets into "text-mode siblings first, the two bitmap ones
last together". **Measuring them dissolved that grouping.** VBXE turned out to
have a real text mode, the Amiga turned out not to need the dirty-cell scheme
it was penalised for, and three machines left the list altogether. What is left
is ordered by **what each actually costs**, cheapest first:

1. **Commander X16** — the same compiler family as both existing ports, so
   everything learned about llvm-mos carries over. The core compiles for it
   today, untouched. Its code space is the same shape as the C128's, so the
   overlay machinery transfers rather than being redesigned, and **every KERNAL
   call the C128 disk seam uses exists for it** — that is the seam that cost
   four bugs on the C128 and was, when this was written, unfinished on the
   MEGA65 — it has shipped there since, SAVE included. Sixteen
   colours, so no mapping. `x16emu` gives `-dump` of RAM, banked RAM and VRAM,
   so the string pool and overlays can be checked byte-exact.

2. **Amiga** (OCS/ECS, KS2.0+) — because it **deletes** the most expensive
   machinery in the project rather than porting it. `core/farmem.h` has always
   said "Amiga — no banking needed, a plain array": no overlays, no far memory,
   no staging regions, no build stamp, none of the budget arithmetic. It is the
   only remaining target with more memory than the game needs. Its portability
   contract is already tested on every build — `make port-check` has compiled
   the core for the 68000 since before any port existed — and Amiberry is the
   best instrument here: screenshots, memory read/write, breakpoints, frame
   stepping and key injection over one socket, plus host-directory mounting so
   there is no disk image to build. Costs: thirteen original box-drawing glyphs
   (~104 bytes) and a sound re-fit for Paula's sampled audio.

3. **Atari 800XL + VBXE** — last, and **not** because of the display. Its text
   mode is real: 80×25 with per-cell foreground and background from 1024
   colours, confirmed on hardware emulation including that all twenty-five rows
   fit — and now confirmed again by this port's own first light. It is last
   because it has **the tightest code budget of any target**: VBXE's VRAM
   window occupies part of the 6502 address space, and even narrowed to 4K at
   `$2000–$2FFF` that leaves 36,864 bytes against the 37,612 the C128 build
   needs. More code has to move into overlays than on any port yet built. It
   also needs hardware the base machine does not have.

   **It has a separate home for its variables after all** — that sentence used
   to end "unlike the C128 this machine has no separate home for its
   variables", and dropping Atari DOS on 2026-09-11 made it false. `$0700–$1FFF`
   is free the moment no DOS is resident, and `.rodata`, `.data`, `.bss` and
   `.noinit` all live there now.

   **And starting it found something the other four ports never had to face: a
   seam costs more than its driver.** Swapping the video stubs for the real
   driver cost 4,636 bytes where the driver itself is 1,559 — the rest is
   `main()` and the `ui_draw_*` routines growing, because a stub that folds to
   one `volatile` write lets the optimiser collapse the argument setup at every
   call site. Budget a seam, not a file.

**The screen layer really was mechanical; nothing else was.** Doing the MEGA65
for real cost far more than a `vdc.c` rewrite, and none of it was display work:
a hand-written library clobbering the compiler's zero-page pseudo-registers, a
hypervisor that never freed a file descriptor, and a raster counter that wraps
twice per frame and so ran the music at double speed. Budget each of these for
its own three of those.

The core obeys 8-bit rules from line one even where the host doesn't force it —
no `float`, `double`, `malloc` or `long`; explicit-width types throughout; 8.8
fixed point with a precomputed distance table instead of runtime `sqrt`.
`core/` is compiled by each platform rather than copied into it, so it cannot
drift. `make port-check` compiles it for the 68000 with `-Werror` on every
build, so the portability contract is under test before the port exists.

## Building

Needs [llvm-mos](https://llvm-mos.org/) at `~/llvm-mos`; `make rund` also needs
[VICE](https://vice-emu.sourceforge.io/) for `x128` and `c1541`. (The port used
cc65 until August 2026 and left it: cc65 had 210 bytes of MAIN free where the
disk seam alone needed 815, and its character-set translation caused four
separate bugs here.)

```sh
cd c128 && make         # build/trek128.prg
cd c128 && make d64     # build/trek128.d64 -- the real thing
cd c128 && make rund    # launch the disk in x128 (the picture is on the VDC window)
cd c128 && make verify  # bounds and encodings the binary can be wrong about
cd c128 && make test    # the native suites: colour mapping, panels, sound
make test               # and the core's own, from the repository root
make ports              # every port's own gate, with the exit status checked
make all                # the native tests, the audits, and `ports`
```

**`make ports` skips what it cannot build.** It runs each port's real gate --
`make verify`, or `make` on the Amiga, which has no overlay budget for a
verify to check -- and a port whose cross compiler is not
installed is reported as a skip rather than a failure, naming the variable and
path it looked for. So a fresh clone with no toolchains still gets a green
`make all`; a machine with every toolchain gets **eight real gates in about
ten seconds**. `make ports P=atari` for one.

**Use `rund`, not `run`.** A bare PRG has no drive, and the string pool, the
music, the eleven code overlays and the twelve-page briefing all load from the
disk. `make run` boots a game with no words in it.

The MEGA65 port needs the same llvm-mos plus
[mega65-libc](https://github.com/MEGA65/mega65-libc), and
[Xemu](https://github.com/lgblgblgb/xemu)'s `xmega65` to run it. Its data files
go on an SD-card image rather than a disk:

```sh
cd mega65 && make         # build/egatrek.prg + build/OVERLAYS.BIN
cd mega65 && make verify  # load address, resident space, overlays, build stamp
cd mega65 && make d81     # build/egatrek.d81 -- the whole game, one image
cd mega65 && make release # build/egatrek-mega65.d81 + .txt
cd mega65 && make run     # launch it
cd mega65 && make drive   # headless: script the keys, screenshot the result
```

`make d81` runs `verify` on the way through, so a binary that cannot run —
wrong load address, stale overlay images, an overlay calling out of its own
window — cannot reach the disk.

The other three ports:

```sh
cd x16 && make game       # build/trekx16.prg -- NOT `make`, which builds the smoke test
cd x16 && make verify     # load address, window, overlay images, soft stack
cd x16 && make release    # a folder of files; the X16 has no disk image

cd amiga && make          # build/egatrek, the data files, and the tempo test
cd amiga && make release  # build/egatrek-amiga.zip

cd atari && make verify   # window, staging regions, the four overlay rules
cd atari && make atr      # build/egatrek.atr -- self-booting, no DOS on it
cd atari && make release  # build/egatrek-atari.atr + .txt
```

The X16 is the one port whose default target is not the game: `all: smoke`.
The Amiga needs bebbo's `m68k-amigaos-gcc` at `$AMIGA_TOOLCHAIN` (default
`~/amiga-toolchain`); the Atari needs the `mos-atari8-dos` half of llvm-mos.
`make -C atari atr` builds the Atari's disk: one self-booting `.ATR` with no
Atari code on it — this port's own boot record, directory and storage seam. It
needed Atari DOS 2.5 until 2026-09-11, which is why it had no release artefact.

## Reference material is not in this repository

`reference/` is deliberately **not tracked**, and since 2026-09-02 the build no
longer needs it: the music is the port's own (`tools/make_music.py`), so a
fresh clone gets a disk with sound on it. Only the measurement tools want the
original.

It holds the original shareware
package and material derived from it, and the licence in `EGATREK.DOC` requires
that any distributed copy include `egatrek.exe`, `egatrek.doc`, `egatrek.ref`,
`orderfrm.txt`, `egatrek.txt` and `file_id.diz` **unmodified**. A working
reference set is neither complete nor unmodified — the manual has control codes
stripped for grepping, the binary is LZEXE-unpacked for disassembly, and the
strings are extracted. Publishing that would be redistribution on terms the
licence does not grant.

To assemble your own copy:

- **EGA Trek** — Internet Archive item [`EGATrek`](https://archive.org/details/EGATrek),
  which holds every release from 1988 to 3.0. If you play it, register it.
- **Super Star Trek** (1978 BASIC listing, used only as calibration material) —
  [coding-horror/basic-computer-games](https://github.com/coding-horror/basic-computer-games)
- **unlzexe** — [mywave82/unlzexe](https://github.com/mywave82/unlzexe), for
  unpacking the LZEXE-compressed original

Message prose in the original is copyrightable, and the shareware notice says
so itself: it licenses redistribution of the complete unmodified package and
then ends *"The author retains all other rights to the program."* So the
extracted string catalogue is used here as a checklist of *which situations
need a message*, never as text to copy, and the screen captures of the twelve
briefing pages were used as a specification of what each page covers. Every
word this port puts on screen is its own. The mechanics are a different matter
and are used freely -- they are facts about a program, and reading them out of
the binary is what this project is.

## Licence

[MIT](LICENSE) — covers this remake's own code and documentation only. EGA Trek
itself remains © Nels Anderson under its own shareware terms, and nothing here
relicenses it.

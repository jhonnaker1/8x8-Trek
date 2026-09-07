# 8x8 Trek

A remake of **EGA Trek** (Nels Anderson, 1988–1992, shareware) for vintage
hardware — one byte-identical shared core plus a per-platform video, sound and
input layer, following the architecture of
[`commodore-uno`](https://github.com/jhonnaker1/commodore-uno).

The name is the galaxy: 8×8 quadrants of 8×8 sectors.

> **Status: two ports released, a third running.** The **Commodore 128** and
> **Commander X16** ports are feature complete and released as
> [v0.10.0](../../releases/latest). The **MEGA65** port plays — console, sound,
> overlays and the briefing — with saving still to come; see
> [`mega65/README.md`](mega65/README.md) for what works and what is still open.
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

## Where the C128 port stands (2026-09-02)

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

Four things are deliberately absent, each decided rather than left undone.

Two are **deferred to roomier targets** rather than cut. The **MAIN VIEWER's
other nine instrument pages** carry live data and two of them are wider than
this port's seventeen-column panel; what cycles them was read on 2026-09-02 and
is no longer a blocker -- a fresh `Random(10)` roughly every six seconds, or the
page number typed as a command. And **a colour per message**: EGA Trek has no
department palette at all, every message site picks its own colour, and
matching that means a byte per pooled string plus a second far read at every
drawing site. The bytes are nearly free in bank 1; the resident code is not.
This port keeps a four-way department map instead and `ui.c` says so plainly.

The **CP437 charset** and **boss mode** were ruled out for this platform
outright. The reasoning for all four is in `NOTES.md`.

What it needs is **play**, and the little it has had has been the most
productive thing in the project. Four sessions at the keyboard have found four
bugs no build check could see: FIX was missing half its command, nine letters
of the alphabet could not be typed (the self-destruct password JAMIE has a J
and an I in it), a yes/no question was drawn in the wrong panel, and the last
page of the briefing never waited for a key. Each one lived exactly where an
automated check does not go, and each fix shipped the check that closes its
class. Whether the game they add up to is survivable, readable and fair is
still an open question, and only playing it will answer that one too.

## Targets

**Tier 1 — 80 columns and 16 colours.** The console is a 80×25 grid of
per-cell colour; anything that can hold that runs the game as designed.

| Platform | Display | CPU | Status |
|---|---|---|---|
| **Commodore 128** (VDC) | 80×25 text, 16 colours per cell | 8502 | **Released** — [v0.10.0](../../releases/latest) |
| **Commander X16** | VERA text 80×60, per-cell fg+bg from 256 | 65C02 | **Released** — [v0.10.0](../../releases/latest) |
| **MEGA65**, native C65 mode | 80×25, VIC-IV H640, colour on all 2000 cells | 45GS02 | **Playable** — console, sound and overlays; no save yet |
| **Amiga** (OCS/ECS, KS2.0+) | 640×200 bitmap, 16 colours | 68000 | Designed |
| **Atari 800XL + [VBXE](https://vbxe.atari.org/)** | 80×25 text, per-cell fg+bg from 1024 colours | 6502 | Designed |

**How much colour the console actually needs: fifteen.** Counted from the
shared sources on 2026-09-05 rather than assumed — every EGA colour except
black, which is the background. It distinguishes `RED` from `LTRED`, `BLUE`
from `LTBLUE`, and so on, and it uses those pairs to mean different things:
the four Mongol ship types are told apart by colour alone. An earlier version
of this section claimed the console needed only eight. **It does not, and that
claim is what had kept a machine on the list that cannot run it.**

**Tier 3 and out.** Each of these was ruled out for its own reason, and three
of them were settled by measurement in September 2026.

- **Tandy CoCo 3** — `WIDTH 80` plus `ATTR` gives **eight** foreground colours
  per cell, and measured in MAME on an RGB monitor those eight are exact EGA
  hues rather than approximations. Eight is still not fifteen: every
  dark/bright pair would collapse, and with it the distinction between Mongol
  ship types. Add a fourth CPU family (6809) whose only compiler, CMOC, is
  non-conforming — it silently miscompiled a shift in the shared core, and
  cannot evaluate the save-record static assert — and the cost stops being
  worth it. (Both of those were found here and fixed for everyone; see
  `NOTES.md`.)
- **Foenix F256** — llvm-mos has no F256 platform, so the toolchain would have
  to be built before the port could start, and cc65 is the only route today.
  This project left cc65 in August with **210 bytes** of the C128's main
  budget free, and llvm-mos bought about 6K back. Whether the game fits at all
  under cc65 is genuinely unknown rather than merely tight.
- **Apple IIgs** — measured in MAME on 2026-09-05. Its 640 mode shows sixteen
  colours on a scanline but only **four** that can be placed at any x, because
  the palette groups are bound to a pixel's position within a byte. Four
  against fifteen.
- **The whole Atari ST line** — 640×200 costs all but four colours (Jamie's
  call). **MSX2** has 80 columns but the colour collapses. The **stock Atari
  800XL** stops at 40 columns; only VBXE brings it back. The 40-column colour
  machines (C64, Plus/4, CBM-II) are viable but would need a paged UI, because
  a nine-panel console does not fit in 40 columns.

### The order, decided 2026-08-23, rewritten 2026-09-05

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
   four bugs on the C128 and is still unfinished on the MEGA65. Sixteen
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
   fit. It is last because it has **the tightest code budget of any target so
   far**: VBXE's VRAM window occupies `$2000–$3FFF` of the 6502 address space,
   leaving 32,768 bytes against the 37,612 the C128 build needs. More code
   would have to move into overlays than on any port yet built. It also needs
   hardware the base machine does not have.

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
```

**Use `rund`, not `run`.** A bare PRG has no drive, and the string pool, the
music, the ten code overlays and the twelve-page briefing all load from the
disk. `make run` boots a game with no words in it.

The MEGA65 port needs the same llvm-mos plus
[mega65-libc](https://github.com/MEGA65/mega65-libc), and
[Xemu](https://github.com/lgblgblgb/xemu)'s `xmega65` to run it. Its data files
go on an SD-card image rather than a disk:

```sh
cd mega65 && make         # build/egatrek.prg + build/OVERLAYS.BIN
cd mega65 && make verify  # load address, resident space, overlays, build stamp
cd mega65 && make sd      # put the data files on Xemu's own card
cd mega65 && make run     # launch it
cd mega65 && make drive   # headless: script the keys, screenshot the result
```

`make sd` runs `verify` on the way through, so a binary that cannot run — wrong
load address, stale overlay images, an overlay calling out of its own window —
cannot reach the card.

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

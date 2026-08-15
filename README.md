# 8x8 Trek

A remake of **EGA Trek** (Nels Anderson, 1988–1992, shareware) for vintage
hardware — one byte-identical shared core plus a per-platform video, sound and
input layer, following the architecture of
[`commodore-uno`](https://github.com/jhonnaker1/commodore-uno).

The name is the galaxy: 8×8 quadrants of 8×8 sectors.

> **Status: first code landed.** The C128-VDC port has a working build — VDC
> driver, EGA→VDC colour mapping, and the nine-panel console skeleton. Research
> and decisions are recorded in [`NOTES.md`](NOTES.md).

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

## Targets

| Platform | Display | Status | Notes |
|---|---|---|---|
| **Commodore 128 (VDC)** | 80×25 text, 8×8 cell | In progress | The console is an 80×25 grid at EGA's 8×14 — the VDC maps it **1:1**. Same sixteen colours as EGA but at different indices (`I R G B` vs `R G B I`), so conversion is a 4-bit rotate; 15 of 16 exact, EGA's brown reads olive. Per-cell foreground only; coloured panel fills need the reverse-video bit. |
| **Amiga** (OCS/ECS, KS2.0+) | 640×200 hires, 16 colours | Designed | Same width and colour count as mode 10h, and EGA's 2-bit-per-gun levels land exactly on `$0/$5/$A/$F`, so the palette is reproduced rather than approximated. One binary for PAL and NTSC. |
| **Atari 800XL + [VBXE](https://vbxe.atari.org/)** | 640×200 bitmap, 16 colours | Designed | VBXE's **HR** overlay mode is 640×N at 4bpp — the same spec as the Amiga target, with a blitter. Verified against Altirra's `vbxe.cpp`, not the documentation. |

Ordering between the Amiga and VBXE legs is still open. The 40-column colour
machines (C64, Plus/4, CBM-II, MEGA65, CoCo 3) are viable but need a paged UI,
since a nine-panel console does not fit in 40 columns.

The core obeys 8-bit rules from line one even where the host doesn't force it —
no `float`, `double`, `malloc` or `long`; explicit-width types throughout; 8.8
fixed point with a precomputed distance table instead of runtime `sqrt`.
`core/` is compiled by each platform rather than copied into it, so it cannot
drift.

## Building

Needs [cc65](https://cc65.github.io/) on your `PATH`; `make run` also needs
[VICE](https://vice-emu.sourceforge.io/).

```sh
cd c128 && make        # build/trek128.prg
cd c128 && make run    # launch it in x128 (the picture is on the VDC window)
cd c128 && make test    # prove the EGA->VDC colour mapping natively
```

## Reference material is not in this repository

`reference/` is deliberately **not tracked**. It holds the original shareware
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

Message prose in the original is copyrightable. The extracted string catalogue
is used here as a checklist of *which situations need a message*, never as text
to copy.

## Licence

[MIT](LICENSE) — covers this remake's own code and documentation only. EGA Trek
itself remains © Nels Anderson under its own shareware terms, and nothing here
relicenses it.

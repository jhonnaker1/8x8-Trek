# 8x8 Trek

A remake of **EGA Trek** (Nels Anderson, 1988–1992, shareware) for vintage
hardware — one byte-identical shared core plus a per-platform video, sound and
input layer, following the architecture of
[`commodore-uno`](https://github.com/jhonnaker1/commodore-uno).

The name is the galaxy: 8×8 quadrants of 8×8 sectors.

> **Status: design only. No code yet.** Research and decisions are recorded in
> [`NOTES.md`](NOTES.md); this README summarises where things stand.

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

| Platform | Display | Notes |
|---|---|---|
| **Commodore 128 (VDC)** | 80×25 text, 8×8 cell | The console is an 80×25 grid at EGA's 8×14 — the VDC maps it **1:1**, and shares EGA's 16-colour RGBI palette, so the original colours reproduce exactly. Per-cell foreground only; coloured panel fills need the reverse-video bit. |
| **Amiga** (OCS/ECS, KS2.0+) | 640×200 hires, 16 colours | Same width and colour count as mode 10h, and EGA's 2-bit-per-gun levels land exactly on `$0/$5/$A/$F`, so the palette is reproduced rather than approximated. One binary for PAL and NTSC. |
| **Atari 800XL + [VBXE](https://vbxe.atari.org/)** | 640×200 bitmap, 16 colours | VBXE's **HR** overlay mode is 640×N at 4bpp — the same spec as the Amiga target, with a blitter. Verified against Altirra's `vbxe.cpp`, not the documentation. |

Ordering between the Amiga and VBXE legs is still open. The 40-column colour
machines (C64, Plus/4, CBM-II, MEGA65, CoCo 3) are viable but need a paged UI,
since a nine-panel console does not fit in 40 columns.

The core obeys 8-bit rules from line one even where the host doesn't force it —
no `float`, `double`, `malloc` or `long`; explicit-width types throughout; 8.8
fixed point with a precomputed distance table instead of runtime `sqrt`.

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

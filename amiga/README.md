# EGA Trek — Amiga (OCS/ECS, KS2.0+)

Fourth port. **Started 2026-09-06; first light works.** The nine-panel console
frame draws in EGA colours through the shared `c128/src/layout.c`, on a
640×200 four-bitplane screen, with box glyphs this port authored.

    make smoke                    build build/smoke
    # then, at the Amiga shell:
    work:smoke

## Why this target is the cheapest left

**No overlays, no far memory, no banking.** `core/farmem.h` has said "Amiga —
no banking needed, a plain array" since the seam was designed, and this machine
has more memory than the game needs. That deletes the single most expensive
machinery in all three 8-bit ports: the window, ten staging regions, the
resident/overlay split, the build stamp, `ovl_load`, the four overlay rules and
the budget arithmetic. **Three of the four bugs found in the v0.10.0 cycle came
out of that machinery**, and none of them can exist here.

640×200 in four bitplanes is the console exactly — 80 cells of 8 pixels by 25
rows of 8, nothing left over — and four planes at 640 wide is the OCS/ECS hires
maximum, so sixteen colours against the fifteen the console uses.

## The glyph set: fifteen, not eleven

Re-derived from the shared sources rather than counted by eye — a sweep of
every `scr_put`/`hline`/`vline`/`fill_rect` argument in `ui.c`, `layout.c` and
`main.c`. The eleven box-drawing characters are the obvious ones; **the other
four live in panels nothing had drawn yet**, so an eyeball count missed them:

    98   BADGE_DISC_TOP    lower half filled -- rounds the badge disc's top
    226  BADGE_DISC_BOTTOM its reverse, and needs no entry of its own
    160  BADGE_DISC_BODY   solid; also the block cursor, reverse of 32
    228  SYS_BAR_GLYPH     seven rows on an eight-pixel pitch, which is what
                           leaves a hairline under each systems-status bar
    81   the ship's saucer, next to four G_HLINE and a solid block

Reverse video is a **rule, not entries**: codes 128..255 are their base glyph
inverted, so 160, 226 and 228 fall out of 32, 98 and 100. Writing those three
by hand would be three more chances to disagree with the rule.

**A code nobody drew renders as a hollow box, not as nothing.** Every other
port hands these to a charset that has something at every position; here the
set is finite and authored, so a missed code would be invisible — which is
exactly the bug that leaves a panel looking merely empty. Two of the fifteen
were missed on the first pass, so this is not hypothetical. `smoke.c` draws
one deliberately undrawn code every run, because a marker that has never fired
is not a marker.

## The glyphs are this port's own artwork

`layout.h` names the box-drawing characters by **C64 screen code** — `G_HLINE`
is 64, `G_VLINE` 93 — and topaz has `@` and `]` at those code points. The
MEGA65 gets away with reusing them because the C65 charset is character-for-
character the C64's; this machine does not.

So `src/amigagfx.c` carries the set as 8×8 bit patterns: a
line sits on rows 3 and 4 and in columns 3 and 4, which is what makes a
vertical meet a horizontal in the middle of a cell and two adjacent cells join.
**What was checked and what was copied.** The C128 chargen ROM was read to find
out what SHAPE each code is meant to be — the same thing
`c128/test/test_panels.c` does to check our screen codes. Three of them are
pure geometry and can only look one way (a half block is a half block, a
seven-row bar is a seven-row bar), so knowing the shape is knowing the bytes.
The rest are drawn here, including the disc, which is this port's own circle
rather than Commodore's. Checking against that ROM is fine; shipping it is not
— the same rule that made `tools/make_music.py` compose the music rather than
extract it.

Letters, digits and punctuation come out of `topaz.font` 8 in ROM, read
straight from `tf_CharData`. No font to author for ASCII.

## Why the planes are written by hand

`graphics.library`'s `Text()` draws in one pen through a RastPort, and every
cell here carries its own colour — a `SetAPen`, `Move` and `Text` per cell,
two thousand times. A cell is 32 bytes of plane data (eight rows, four planes)
and writing it directly is both simpler and exact. The RastPort is still
opened, because the ROM font is read through it.

## Running it

Amiberry mounts a **host directory** as an Amiga volume, so there is no ADF to
build and nothing to copy — `tools/egatrek.uae` points `WORK:` straight at
`amiga/build`. Build on the host, type `work:smoke` on the Amiga.

    /Applications/Amiberry.app/Contents/MacOS/Amiberry -G -f amiga/tools/egatrek.uae

`tools/amiga_type.py` types at the shell and takes screenshots through
Amiberry's IPC socket. **Two things nothing documents**: the protocol is
TAB-separated — a space-separated command answers `ERROR Unknown command`,
which reads exactly like an unsupported feature — and `SEND_KEY` takes raw
Amiga keycodes with separate press and release, not characters.

## What is built, and what is next

    DONE   video seam: vdc_init, scr_put/puts/clear/hline/vline/fill_rect,
           wait_vsync, vdc_shutdown, plat_exit, the EGA palette
           glyphs: all fifteen, verified on the machine, with a marker for
           any sixteenth nobody has noticed yet
    NEXT   input   -- IDCMP VANILLAKEY into kb_waitkey; there is no scan table
                      to hand-transcribe and no encoding to measure
           storage -- AmigaDOS Open/Read/Write/Close onto the five plat_*
                      functions. plat_write_all() will actually work here; it
                      still does not on the MEGA65
           strings -- strpool.c against a plain array, no far memory
           sound   -- LAST. Paula is four channels of SAMPLED audio, the
                      furthest from the SID of any target. Budget a tempo bug:
                      the C128 lost time to a driver three semitones from its
                      cause and the MEGA65 to a raster that wraps twice a frame

The smoke build links the shared `layout.c` and supplies a **stub `S()`** for
the seven panel titles, because the string pool needs the file seam that is not
built yet. It is replaced by `c128/src/strpool.c` when storage lands.

# EGA Trek — Atari ST and STE

Ninth port, built **2026-09-14**, and it is **one file**. Every seam this
machine needs was already written for the Falcon; the ST's video chip is the
only thing it does not share.

It boots on a **1MB ST under EmuTOS** and draws the whole nine-panel console:
short range scan, status, lasers, the command line, the main viewer and the
message log, in sixteen colours at 320×200.

**It has been played**, the same day it was built, and the sitting found
exactly one thing: **a green block across the word MONGOL on the title
screen**. That was the GEM mouse handler, still running and still restoring
the desktop pixels it had saved under the pointer — the game painted the cell
and an interrupt painted over it. `$A00A` hides it; `$A009` gives it back on
the way out. Fixed before the port shipped.

```sh
make            # build/EGATREK.PRG + the three data files
make verify     # the cheap gate, no emulator: what `make ports` runs
make run        # Hatari, 1MB ST, EmuTOS
make run-ste    # the same on an STE -- the palette gets its fourth bit back
make run-tos206 # real TOS 2.06, to check the port is not leaning on one ROM
python3 tools/shot.py            # boot, screenshot, LEAVE IT RUNNING
python3 tools/shot.py --keys 'RETURN,n,RETURN,n,RETURN,RETURN,1,RETURN,a,b,c,RETURN' --tail 12
```

## This line was excluded for a reason that no longer exists

The whole Atari 16-bit line was ruled out of this project on **2026-08-22**,
and the reason was width: *"320×200 in 16 colours (only 40 columns)"*. The
console was an 80×25 grid then. It is forty columns now — 320/8 = 40 and
200/8 = 25 — so **the mode that was never wide enough is exactly the right
size**, and nothing about the machine changed.

640×200 is the other colour mode and it is still no good: four colours against
the **eight** the game needs to tell a Mongol battleship from a command ship.
That is a colour failure, not a width one, and it is why the two axes have to
be judged separately.

## What this port actually cost

| | |
|---|---:|
| Shared code it links | 10,522 lines |
| From the Falcon, unchanged | 5 files |
| Written here | `stvid.c`, `stlog.c` |

`vc +tos` **is** the ST target — the Falcon port already uses it. Sound is the
YM2149 through XBIOS `Giaccess` at the same measured 2 MHz clock. Storage is
GEMDOS, far memory is a plain array, and at 1MB there are no overlays.

## The video driver, and what it borrowed from where

**The planes are word-interleaved, exactly as on the Falcon.** Four planes of
the same sixteen pixels sit in four consecutive words, so one cell's eight
pixels are one byte in each of four words eight bytes apart, and which half of
each word depends on whether the column is even or odd. `cell_byte()` is
`falconvid.c`'s, unchanged but for the stride.

**The glyphs are the Amiga's.** The Falcon's box-drawing set is 8×16 and does
not scale; the Amiga draws at 8×8 for the same reason this port does, so its
artwork is already the right size. Seventeen glyphs, this project's own.

**The font comes out of ROM and is never shipped.** Line-A hands back three
system fonts and this driver takes the 8×8 one **by measuring the headers**,
not by taking an index — the same rule `falconvid.c` uses, because a ROM is
free to disagree about the order.

**One palette encoding, correct on both machines.** An STE gun is four bits,
but the extra bit lives in bit 3 of the nibble as the *least* significant one,
so a plain ST — which reads only bits 2–0 — sees the top three and shows the
nearest of its 512 colours, while an STE reads all four and gets 4096. Writing
the ST's own three-bit encoding would work on both and throw the STE's extra
bit away for nothing. Read off commodore-uno's `ste/src/stevid.c`.

**ST monochrome is refused, not squeezed.** One plane and no colour is a
different driver, not a different mode.

## Watch out for

**`tools/verify_st.py` IS the Falcon's checker, imported.** Same glyph sweep,
same geometry rule, pointed at `layout40.c` and `stvid.c`. That is deliberate:
a copy would pass for months after the original grew a check. It also means a
missing or broken `falcon/tools/verify_prg.py` fails this port's gate rather
than skipping it.

**Every `box[]` entry must start its own line.** The checker reads the table
out of the source with a regex anchored at the line start — the same one the
Falcon uses — so an entry hiding behind a comment on the same line is
invisible to the gate. The first draft of `stvid.c` did exactly that and the
checker saw 6 of 17.

**Hatari's `keypress` takes a single ASCII character or an ST scancode**, not
a key name. `tools/shot.py` maps `RETURN`, `SPACE`, `ESC` and the arrows and
passes single characters through. It also keeps Hatari's own output in
`build/shots/hatari.log` and prints any errors before the pictures — an
unrecognised event is reported there and nowhere else.

**Never SIGTERM Hatari.** `hatari-shortcut quit` down the command fifo, then
`kill -9` if that does not take. And Hatari **creates** the fifo itself, so the
path must not exist beforehand — pre-creating it makes the first write block
for ever.

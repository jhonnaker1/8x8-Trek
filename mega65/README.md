# EGA Trek — MEGA65 (native C65 mode)

The second port, and the first of the four **text-mode siblings** the target
order puts ahead of the bitmap machines. Status: **first light** — the console
frame draws from the shared layout, in 80 columns, on an exact EGA palette.

```sh
make          # build/smoke.prg
make run      # in Xemu
```

## Why native mode, not C64 mode

A `$0801` binary runs the MEGA65 in **C64 mode**, where 80 columns do not
exist — the C64 never had them, and setting VIC-IV's H640 bit from inside
changes nothing. Colour RAM makes it concrete: 80×25 is 2000 cells and a
C64-mode program reaches only the 1K window at `$D800`.

llvm-mos's `mega65` target links at **`$2001` with a BASIC 65 header**, which
is what actually selects C65/native mode. There H640 is real and mega65-libc
reaches the full colour RAM at `$FF80000` through the 45GS02's 32-bit
addressing, so every one of the 2000 cells gets its own colour.

## The EGA palette, exactly — including brown

The first thing this machine buys over the C128. The VDC is a fixed RGBI chip:
the C128 port maps EGA onto it with a 4-bit rotate, fifteen colours land
exactly, and **EGA's brown comes out olive** — recorded in `core/ega.h` as the
one colour that port cannot have.

The VIC-IV's palette is programmable, so `m65vid.c` writes EGA's own values
into entries 0..15 and the mapping becomes the identity.

Measured off a screenshot rather than assumed: all sixteen land on their EGA
index, brown at `(170, 85, 0)` **exact**. Eight read one LSB low (84 for 85,
254 for 255), which is Xemu's rendering — a wrong palette byte would be off by
a whole nibble step, not by one.

A happy accident makes the palette bytes revision-proof: EGA's two-bits-per-gun
levels are `0x00`, `0x55`, `0xAA`, `0xFF`, every one a duplicated nibble. They
are the same colour whether the core reads the register as eight bits or as the
low nibble of a VIC-III-compatible four.

## What is shared, and what this port has to supply

`core/` and the console layout compile unchanged — the smoke build already
links `../c128/src/layout.c` and uses `layout.h`'s glyph constants as they
stand, because the MEGA65 uses the same C64-family screen codes the C128's VDC
does.

`ui.c` and `main.c` — 4,500 lines of console and command handling — touch the
platform through a seam that is already abstracted at core level:

| seam | calls | MEGA65 |
|---|---|---|
| `scr_*` | 224 | **done** — `m65vid.c` |
| `ovl_load` | 25 | a **no-op**: 40MHz and far more RAM, so nothing needs overlaying |
| `kb_*` | 17 | to write |
| `snd_*` | 15 | the MEGA65 has real SIDs, so `sid.c` should port nearly as-is |
| `vdc_data_*` | 9 | the one genuinely C128-shaped thing: the message log lives in spare **VDC RAM**. Here it becomes plain RAM |
| `plat_*` | 5 | SD-card file I/O via mega65-libc |
| `far_*` | — | plain RAM; the bank-1 seam exists because the C128 has 42K, and this machine does not |

The three big C128 subsystems — **ten 4K code overlays**, the **bank-1 far
memory** for strings and music, and the **byte-at-a-time disk streaming** —
exist because of a 42K address space and a 2MHz 8502. None of them is needed
here. That is what "the friendliest target on the list" means in practice: most
of the C128 port's complexity is answering questions this machine does not ask.

## Verifying it

Xemu flushes `-screenshot` **and** `-dumpscreen` on **SIGTERM**, which is what
makes a program with no exit path checkable — `-prgexit` never fires, because
llvm-mos binaries do not reliably return to BASIC.

`-dumpscreen` writes the character matrix as text, so a layout can be asserted
by reading it rather than by looking at a picture.

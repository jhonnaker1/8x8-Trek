# EGA Trek for the MSX2

The fourteenth port, and the first Z80. An MSX-DOS 2 `.COM` built with SDCC,
**with no overlays at all**, installed as the boot disk's `COMMAND2.COM`. The
player's instructions are in [`README-release.txt`](README-release.txt); the
whole record — every measurement and every wrong turn — is NOTES.md's
`## SCOPE: an MSX2 port`.

## Building

SDCC 4.6 from Homebrew (`brew install sdcc`). The boot files — Nextor's
`MSXDOS2.SYS` and `COMMAND2.COM`, from the Nextor v2.1.1 source tree — live in
`~/msx-toolchain/nextor-boot/`, outside this repository. openMSX is at
`/Applications/openMSX.app`, and every run below uses a throwaway settings
file, because openMSX saves its settings on exit.

    make            the game, build/EGATREK.COM
    make verify     the static gate `make ports` runs: the budget against
                    crt0's stack reserve, and the string count
    make release    build/egatrek-msx2.zip -- the disk image, README.txt and
                    NEXTOR-LICENSE.txt
    make dsk        build/egatrek-msx2.dsk alone; `make dskshot` boots it
    make probe      the budget: image against the TPA

And the instruments, each of which boots openMSX headless over its control
channel (`-script` runs stalled on 2026-09-24, cause unknown):

    make gameshot   photograph the running game from VRAM (SHOT_TIMES,
                    SHOT_KEYS "t:text,..."; tools/vram2png.py decodes SCREEN 7)
    make profile    PC samples every 2ms, mapped to functions through
                    relocated listings -- where a repaint's time goes
    make stackrun   the running game's stack, by sentinel, over a full session
    make listen     record the PSG and check pitch and tempo against MUSIC.DAT
    make kbd        the keyboard, typed at through the emulated key matrix
    make memcheck   the startup memory check REFUSING, from a COMMAND2 prompt
    make stack / stackbound / shell / dostime   the measurements behind the
                    design decisions below

## Why it is shaped this way

**No overlays.** SDCC's `#pragma codeseg` is file-scoped, so it cannot express
this project's per-function overlays; z88dk's compiler can, at 1.38x the code.
The whole shared half with no overlays linked at 51,751 bytes, and every driver
then fitted: **343 bytes spare** above a 256-byte stack reserve, against a
stack measured at 208 on the running game.

**The game is the shell.** A program COMMAND2 runs gets `($0006) = $D606`;
COMMAND2 itself gets `$DB06`. The 1,280 bytes between are COMMAND2's resident
part, and every BDOS call goes through them. Installed as `COMMAND2.COM`, the
game gets them and calls BDOS direct. `crt0.s` checks at startup and refuses,
with a message, if a machine leaves too little.

**The BIOS shares the VDP and PSG latches.** Its interrupt handler reads the VDP
and writes the PSG's address latch every frame, so every two-byte sequence
runs under `di`/`ei` — never SDCC's `__critical`. CALSLT puts the BIOS ROM in
page 0, and an interrupt taken then runs on OUR stack, so the keyboard reads
the BIOS key ring directly instead of calling it.

**Far memory is VRAM page 1**, which SCREEN 7 never shows — not the memory
mapper, because the code spans all four pages.

**The console is drawn by the V9938's command engine**: HMMV for every blank
rectangle, and one HMMC a character from `hmmc_cell`, in assembly, with
sprites off. A full console is about two seconds; it was 8.5.

**Timing is the BIOS's JIFFY**, and a jiffy is 50.159Hz or 59.923Hz, not 50
or 60 — the V9938's 1368 clocks a line at 21.477MHz. The music measured 0.35%
fast before that was known.

**A DOS2 call costs about 5ms**, so storage reads ahead 64 bytes; the briefing
asks for one byte at a time.

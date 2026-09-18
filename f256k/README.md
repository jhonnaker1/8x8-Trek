# EGA Trek on the Foenix F256K — STARTED 2026-09-17

**First light: llvm-mos builds a PGZ that FoenixMCP loads and runs, and it
draws to the text matrix in colour.** That was the risk worth retiring before
anything else, because the fallback — cc65 — would have decided the whole
shape of the port.

| | state |
|---|---|
| toolchain: llvm-mos → PGZ → pexec | **works**, `make hello` |
| rig: MAME `f256k`, Lua, SD-card image | **works**, `make run P=hello` / `make check P=hello` |
| video: text matrix + per-cell colour | **proved by the hello**, no driver yet |
| sound: SN76489 PSG | **calibrated at three points**, no driver yet |
| storage, keyboard, far memory, overlays | **not started** |

## THE RIG: MAME, AND WHY THERE IS NOTHING BETTER TO AUTOMATE

Three F256 emulators exist. **MAME's `f256k` driver (dtremblay's fork) is the
only one this machine can run, and it is also the only one with a scripting
interface** — so the choice `commodore-uno` made is the choice there is:

| | | |
|---|---|---|
| **MAME `f256k`** (dtremblay) | macOS/Linux/Windows | Lua: memory, registers, breakpoints, key injection, snapshots, `-autoboot_script` |
| **FoenixIDE** (Trinity-11) | **Windows only** — C#/WinForms `.msi` | GUI debugger; nothing to drive from a script |
| **Foenijs** (wf2) | browser | `?url=&run=` launches a program, but its debugger is for a human at the keyboard |

MAME is not the compromise here — it is the one with the automation. The
friction was never MAME; it was **getting the PGZ onto the SD image.**
`commodore-uno`'s recipe (and this port's first one) attached the image as a
device and asked `diskutil` to mount it: seconds of disk arbitration per run,
and a leaked device node whenever a run died mid-way. **`mcopy` writes the FAT
partition in place instead — 7ms, no mounting, no privileges** — and FoenixMCP
still reads the same filesystem and pexec still loads the same file. The
partition offset is read out of the MBR rather than assumed, because 2048 is
right for this image and would be wrong for the next one.

`make check` is the automated half: it boots, types `/- <name>` at the
SuperBASIC prompt the way a person does, reads the completion marker, takes a
snapshot and exits. **The marker's address comes from the ELF**, not a
constant — a stale one reports somebody else's byte as the answer. Whole cycle,
including the boot: **3.6 seconds.**

The control matters more than the pass. Launching a name pexec cannot find
reads `$00` where a real run reads `$5A`, so the rig can tell "ran" from
"never started" — which is the thing a green light is worthless without.

`make run` leaves the machine up at the prompt, throttled, for a person.
`make check` is the one that passes `-nothrottle`, and it is never for a human.

## WHY llvm-mos AND NOT cc65, WHICH IS THE PROVEN ROUTE HERE

`commodore-uno/f256` is a complete working F256 port built with cc65, and its
toolchain directory is vendored from the FoenixMCP kernel repo. Taking it would
have been the cheap start. **Two measurements decided against it**, neither
about the thing everyone quotes:

  * **Code size is NOT the discriminator.** On a fair full link of `core/` —
    the same 78 functions pinned into both, because cc65 links whole modules
    while llvm-mos runs LTO and `--gc-sections` — cc65 came out at **31,035
    bytes against llvm-mos's 31,222. Within 1%.** The "13% smaller" this
    project has quoted, and uno's "2.3× tighter", are both unreproduced.
  * **What actually differs is the memory model.** cc65 carves its stack out
    of the program's own space; llvm-mos puts `__stack` outside it. Against
    the ~40K this machine gives a program, that is the number that decides.
  * **And cc65 would put C89 on the SHARED half forever.** `c128/src/ui.c` has
    eight `declaration-after-statement` sites; every other shared file is
    already C89 and so is `core/`. One port must not constrain twelve ports'
    shared sources in perpetuity — though **extending the root `c89-check` to
    the shared half is worth doing anyway**, since it guards `core/` only and
    the shared UI drifted to C99 with nothing noticing.

## WHAT THE MACHINE IS LIKE

**Its MMU is the opposite of the Plus/4's.** Eight slots of 8K, four MLUTs,
`$0000` the control register: you choose what is in each slot, so **nothing has
to be hidden to reach RAM and no kernel call needs a banking shim**. That one
cost is what made the Plus/4 a fourth seam bigger than the other three.

**But the kernel does take something away.** FoenixMCP's IRQ handler expects
I/O page 0, so an interrupt landing while the page is 2 or 3 reads matrix RAM
instead of its registers. Every screen write is bracketed `sei`/`cli` and
leaves the page at 0 — uno documents this and it is not optional.

    $0000 MMU_MEM_CTRL   $0001 MMU_IO_CTRL (0=regs 2=char 3=colour)
    $0002-$003F MCP's    $0040-$00EF free zp   $00F0-$00FF kernel args
    $2000-$9FFF program  $C000 matrix (paged)  $D800/$D840 colour LUTs
    $D608 PSG (page 0)   $FF00 kernel jump table

**80×60 cells of 8×8**, so the console's native **80 columns** fit with rows to
spare — `layout.c`, not `layout40.c`. **6.29 MHz**, three to six times every
other 8-bit port here.

## SOUND IS THE PSG, AND THAT IS A DELIBERATE CHOICE

MAME emulates two SIDs, two SN76489 PSGs and an OPL3, so the emulator cannot
decide it. **The PSGs are inside the Beatrix FPGA — every owner has them.**
The SIDs are the only audio anyone describes as optional (the jr's are
sockets, the K2's are FPGA, the K's are undocumented either way). Jamie's call,
and the right one: *we have to have sound, and neither of us owns the machine.*

Measured on the hardware, three points, because this project shipped a port an
octave flat off one check:

    wanted   predicted      MEASURED    error
      440    440.4 (N=254)  440.8 Hz   +0.09%
     1000    998.8 (N=112)  998.4 Hz   -0.04%
      200    200.1 (N=559)  200.2 Hz   +0.05%

`f = 111,860.78 / N` — **the same numerator `plus4/src/tedsnd.c` already
carries**, without TED's `1024 - N` inversion.

## THE ONE SEAM WITH A GENUINELY NEW SHAPE

**Storage is event-driven.** A jump table at `$FF00`, `error` is the carry
flag, and every operation is asynchronous: issue the call, then pump
`NextEvent` until your event arrives. Three traps, all visible in uno's loops:

  1. **Read is two steps** — `File.Read` requests, and on `file.DATA` you must
     call `ReadData` to copy the payload. The data is not in the event.
  2. **`delivered == 0` means 256, not EOF** — `buflen` is a `uint8_t`, so a
     full read wraps to zero. A reader that treats 0 as end-of-file truncates
     every file at its first full block, silently.
  3. **One queue for everything.** Keyboard, mouse, clock and file events all
     arrive through `NextEvent`, and a file loop that `continue`s past what is
     not its own **discards keystrokes**. On this machine input and storage are
     coupled in a way no other port here has.

## NOT YET TRUE

No `verify`, so this port is in neither `RELEASE_PORTS` nor
`tools/check_ports.py` — **a port with no gate makes `make ports` true and
meaningless**, and it joins when it has one. Nothing but `hello.c` exists: no
driver, no storage, no keyboard, no overlays. The `$A000-$BFFF` region may be
reachable and is **not assumed** until measured.

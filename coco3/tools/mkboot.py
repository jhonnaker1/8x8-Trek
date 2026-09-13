#!/usr/bin/env python3
"""Build TREKLDR.BIN -- the two-block loader Disk BASIC can actually place --
and EGATREK.RAW, the headerless image it reads.

WHY TWO BLOCKS. The loader body has to live where the game never goes, which
is the gap above the overlay window ($E400). But BASIC's EXEC jumps there with
the ROM still mapped, and $E400 reads ROM, so the body cannot be entered
directly. A DECB file may carry several blocks and LOADM places them all, so
the file carries a THIRTEEN-BYTE STUB low down -- where BASIC can put it and
where the CPU can execute it -- whose only job is to take the machine and jump
up to the body.

    ORCC #$50      interrupts off: Disk BASIC's handler is about to vanish
    LDS  #$7E00    a stack below $8000, valid in either mapping
    STA  $FFDF     all RAM
    JMP  $E400     into the body

THE BODY GETS THERE BY WRITE-THROUGH, and that was measured rather than
assumed: LOADM runs with the ROM mapped, and a CPU write to ROM space lands in
the RAM underneath -- $11/$22/$33 written to $8000/$A000/$C000 with ROM mapped
all read back after switching to all-RAM. See NOTES.md item 35.

EGATREK.RAW is the linked image with its DECB block headers stripped, so
plat_read_all drops it at $2800 and the loader needs to know nothing about
its shape.
"""
import os, struct, sys

STUB_ORG = 0x2800      # where BASIC drops the stub; the game's own org
BODY_LOAD = 0x4000     # where LOADM can place the body
BODY_ORG  = 0xE400     # where the body is LINKED to run
GAME_ORG = 0x2800
STACK    = 0x7E00

# THE CEILING THE HARNESSES AND THE README TYPE, and the body must fit under
# it. `LOADM` SILENTLY REFUSES A BLOCK ABOVE BASIC'S MEMORY TOP -- no error,
# no load, and EXEC then runs a stale address. That is item 35's lesson and it
# BIT AGAIN on 2026-09-13: adding plat_write_all to coco3storage.c grew this
# loader, which links it, from 3,427 bytes to 4,406 -- and at the old
# BODY_LOAD of $6000 that reached $715D, past $6FFF. The game stopped booting
# and the first thing I blamed was the SAVE command I had just written.
# So it is an ASSERTION now, not a number somebody remembers to check.
CLEAR_TOP = 0x6FFF


def decb_blocks(path):
    d = open(path, "rb").read()
    i, out, ex = 0, [], 0
    while i + 5 <= len(d):
        t, ln, ad = struct.unpack(">BHH", d[i:i + 5])
        if t == 0xFF:
            ex = ad
            break
        out.append((ad, d[i + 5:i + 5 + ln]))
        i += 5 + ln
    return out, ex


def stub(nbytes):
    """Hand-encoded, because this does not need an assembler and this way the
    file has no build-order dependency on one.

    IT HAS TO COPY, not just jump. LOADM WILL NOT PLACE A BLOCK ABOVE BASIC'S
    MEMORY TOP -- after a CLEAR it refuses $E400 outright, and the symptom is
    silent: the load fails, EXEC uses a stale address and the CPU ends up at
    $0028 with BASIC's stack still intact. So the body is LOADED at $6000,
    where BASIC allows it, and copied up to $E400 here. The copy runs with the
    ROM still mapped and works because a CPU write to ROM space passes through
    to the RAM underneath -- measured, not assumed."""
    end = BODY_LOAD + nbytes
    return bytes([
        0x1A, 0x50,                                     # ORCC #$50
        0x10, 0xCE, (STACK >> 8) & 0xFF, STACK & 0xFF,  # LDS  #$7E00
        0x8E, (BODY_LOAD >> 8) & 0xFF, BODY_LOAD & 0xFF,    # LDX #$6000
        0x10, 0x8E, (BODY_ORG >> 8) & 0xFF, BODY_ORG & 0xFF,  # LDY #$E400
        0xA6, 0x80,                                     # copy: LDA ,X+
        0xA7, 0xA0,                                     #       STA ,Y+
        0x8C, (end >> 8) & 0xFF, end & 0xFF,            #       CMPX #end
        0x25, 0xF7,                                     #       BLO copy
        0xB7, 0xFF, 0xDF,                               # STA  $FFDF
        0x7E, (BODY_ORG >> 8) & 0xFF, BODY_ORG & 0xFF,  # JMP  body
    ])


def check_fits(nbytes):
    end = BODY_LOAD + nbytes - 1
    if end > CLEAR_TOP:
        sys.exit("mkboot: the loader body is %d bytes at $%04X, ending $%04X -- "
                 "ABOVE the CLEAR 25,&H%04X ceiling the harnesses type.\n"
                 "        LOADM will refuse it silently: no error, no load, and "
                 "EXEC runs a stale address.\n"
                 "        Lower BODY_LOAD, raise the CLEAR everywhere, or take "
                 "something out of the loader."
                 % (nbytes, BODY_LOAD, end, CLEAR_TOP))


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: mkboot.py BOOT.BIN GAME.BIN OUTDIR")
    bootbin, gamebin, outdir = sys.argv[1:]

    body, _ = decb_blocks(bootbin)
    if len(body) != 1:
        sys.exit("mkboot: expected one block in %s, got %d" % (bootbin, len(body)))
    bad, bytes_ = body[0]
    if bad != BODY_ORG:
        sys.exit("mkboot: %s loads at $%04X, expected $%04X" % (bootbin, bad, BODY_ORG))
    check_fits(len(bytes_))

    game, _ = decb_blocks(gamebin)
    if len(game) != 1:
        sys.exit("mkboot: expected one block in %s, got %d" % (gamebin, len(game)))
    gad, gbytes = game[0]
    if gad != GAME_ORG:
        sys.exit("mkboot: %s loads at $%04X, expected $%04X" % (gamebin, gad, GAME_ORG))

    # THE STUB SITS AT THE GAME'S OWN ORG, DELIBERATELY. It is twelve bytes
    # and it jumps to the body before a byte of the game is read, so being
    # overwritten afterwards costs nothing -- and it means BASIC only has to
    # CLEAR to $27FF, which it will. Asking for $25FF got ?OM ERROR, the same
    # refusal that made the old $1200 org unloadable in the first place.
    if STUB_ORG != gad:
        sys.exit("mkboot: the stub is at $%04X but the game loads at $%04X; "
                 "they are meant to be the same address" % (STUB_ORG, gad))
    if gad <= BODY_ORG < gad + len(gbytes):
        sys.exit("mkboot: the loader body at $%04X is inside the game image "
                 "($%04X..$%04X)" % (BODY_ORG, gad, gad + len(gbytes) - 1))

    st = stub(len(bytes_))
    ldr = os.path.join(outdir, "TREKLDR.BIN")
    with open(ldr, "wb") as f:
        f.write(struct.pack(">BHH", 0x00, len(st), STUB_ORG));  f.write(st)
        f.write(struct.pack(">BHH", 0x00, len(bytes_), BODY_LOAD)); f.write(bytes_)
        f.write(struct.pack(">BHH", 0xFF, 0, STUB_ORG))
    raw = os.path.join(outdir, "EGATREK.RAW")
    open(raw, "wb").write(gbytes)

    print("  TREKLDR.BIN  stub %d bytes at $%04X + body %d bytes loaded at $%04X, "
          "copied to $%04X, exec $%04X"
          % (len(st), STUB_ORG, len(bytes_), BODY_LOAD, BODY_ORG, STUB_ORG))
    print("  EGATREK.RAW  %d bytes, loads at $%04X..$%04X"
          % (len(gbytes), gad, gad + len(gbytes) - 1))


if __name__ == "__main__":
    main()

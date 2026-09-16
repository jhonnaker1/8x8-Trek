; BLOCK 0 OF THE DISK: the loader, and the first code of ours the machine runs.
;
; A ProDOS-order block device boots by reading BLOCK 0 into $0800 and jumping
; to $0801 with X holding slot*16. So everything on this disk is ours from the
; first instruction: no ProDOS, no filesystem, no licence question about
; redistributing somebody else's system file -- the same argument that got the
; Atari VBXE port its own boot record and SIO seam.
;
; WHAT IT DOES. Reads PAYLOAD_BLOCKS blocks starting at block 1 into $2000 and
; jumps there. The count is patched into this block by tools/mkdisk.py, which
; finds it by a signature rather than by an offset somebody has to keep in
; step -- see `hdr` at the end.
;
; HOW A BLOCK IS READ WITHOUT PRODOS. Every ProDOS block device exposes a
; driver in its slot firmware: the byte at $CnFF is the driver's offset within
; $Cn00, and the call takes command in $42, unit in $43, buffer in $44/$45 and
; block number in $46/$47, returning carry clear on success. That is the whole
; interface, it is in ROM, and it is what ProDOS itself calls.
;
; Emulation mode, 8-bit everything, which is what the firmware hands us.

/* START_BLOCK is 2 for a real disk: block 0 is this loader and block 1 is the
   directory, so the image starts at 2. The only reason it is a knob at all is
   that the FAILED-READ path cannot otherwise be tested: every block a
   1,600-block image has reads successfully, so the only way to make the drive
   say no is to ask for one that is not there. A failure path nothing has ever
   executed is a comment, not a behaviour. */
#ifndef START_BLOCK
#define START_BLOCK 2
#endif

/* WHERE THE IMAGE GOES, AND IT IS $0800 BECAUSE NOTHING SAYS OTHERWISE.
   ProDOS 8 enters a SYS file at $2000; there is no ProDOS here, so the image
   goes as low as an Apple II program can and the port keeps the 6,144 bytes
   the convention was costing. It must match iigs.ld's ORIGIN(ram) -- and
   nothing enforces that, which is why both say so out loud. */
#ifndef LOAD_ADDR
#define LOAD_ADDR 0x0800
#endif

/* THE HANDOFF. The loader knows two things the game cannot re-derive for
   free -- which drive the machine booted from, and where that slot's block
   driver lives -- and zero page will not survive the C runtime's arrival.
   Four bytes at $0280, in the second half of the monitor's GETLN input
   buffer, which nothing this port runs ever touches.

   THE SIGNATURE IS NOT DECORATION. tools/run.lua pokes an image straight into
   memory and sets the PC, which is how every probe here was tested -- and in
   that path the boot block never runs, so these bytes are whatever the ROM
   left. Without a signature the storage layer would read a garbage driver
   address and hang; with it, it can say "not booted from our loader" and the
   difference between the two rigs stops being invisible.

   Written out as four literals rather than as `HANDOFF + n`: the assembler
   reads `sta (0x0280 + 0)` as an INDIRECT addressing mode and rejects the
   operand as needing to be eight bits. A parenthesised expression is not a
   number here. */
#define HANDOFF_UNIT   0x0280
#define HANDOFF_DRVLO  0x0281
#define HANDOFF_DRVHI  0x0282
#define HANDOFF_SIG    0x0283

        .section .boot,"ax",@progbits
        .globl _boot

; ProDOS block-driver call block, at the addresses the interface defines.
        cmd     = 0x42
        unit    = 0x43
        buflo   = 0x44
        bufhi   = 0x45
        blklo   = 0x46
        blkhi   = 0x47
; Ours, chosen below the driver's own zero page use.
        entlo   = 0x3C
        enthi   = 0x3D
        count   = 0x3E

; RELOCATE FIRST, BECAUSE THE IMAGE WANTS THE GROUND THIS CODE IS STANDING ON.
; The firmware loads block 0 to $0800 and the game image also starts at $0800,
; so a loader that stays where it was put overwrites itself halfway through
; the first block. It copies its own page down to $0300 and continues there.
;
; THE WHOLE BLOCK IS ASSEMBLED FOR $0300 and the copier still works while
; running at $0801, because every instruction in it is absolute or relative --
; neither cares where it is executing. Only the `jmp` at the end needs the
; assembled address, and by then the copy exists.
;
; ONE PAGE, NOT ALL 512 BYTES. $0400..$07FF is text page 1, and the slot
; firmware keeps its SCREEN HOLES in it ($0478..$07F8) -- the block driver we
; are about to call reads and writes them. Copying 512 bytes from $0800 would
; land on top of the driver's own state.
_boot:
        sei
        cld

; THE UNIT NUMBER IS IN X AND THE RELOCATION LOOP IS ABOUT TO DESTROY IT.
; Stored first, into zero page, which the copy below does not touch. The
; comment twenty lines down says X "is the only thing that says which drive we
; came from" -- and the relocation was added after that comment was written and
; broke it in the same edit. The machine ended in ROM at $FF/BD53 with unit 0.
        stx unit

        ldx #$00
reloc:
        lda 0x0800,x
        sta 0x0300,x
        inx
        bne reloc
        jmp relocated
relocated:

; Driver entry: high byte is $C0 + slot, low byte comes from $CnFF. The slot
; comes back out of the unit number saved above -- losing it means guessing a
; slot, and a guess that happens to be right on the machine it was written on
; is the worst kind.
        lda unit
        lsr
        lsr
        lsr
        lsr                     ; A = slot
        ora #$C0
        sta enthi               ; $CnFF is built in entlo/enthi, then entlo is
        lda #$FF                ; overwritten with the offset read through it
        sta entlo
        ldy #$00
        lda (entlo),y           ; the driver's offset within $Cn00
        sta entlo               ; entlo/enthi now point at the driver

; Hand the drive and the driver to whatever we are about to run.
        lda unit
        sta HANDOFF_UNIT
        lda entlo
        sta HANDOFF_DRVLO
        lda enthi
        sta HANDOFF_DRVHI
        lda #0x5A
        sta HANDOFF_SIG

; Read PAYLOAD_BLOCKS blocks from START_BLOCK to LOAD_ADDR upward.
        lda #(LOAD_ADDR & 0xFF)
        sta buflo
        lda #(LOAD_ADDR >> 8)
        sta bufhi
        lda #(START_BLOCK & 0xFF)
        sta blklo
        lda #(START_BLOCK >> 8)
        sta blkhi
        lda nblocks
        sta count

rdloop:
        lda #$01                ; 1 = READ
        sta cmd
        jsr driver
        bcs failed
        inc bufhi               ; 512 bytes is two pages
        inc bufhi
        inc blklo
        bne nocarry
        inc blkhi
nocarry:
        dec count
        bne rdloop

        jmp LOAD_ADDR

; A FAILED READ MUST NOT FALL THROUGH INTO WHAT IT DID NOT LOAD. The C128's
; overlay seam learned this: a load that fails and returns runs whatever was in
; the window before. Paint the screen one flat colour and stop, so "the disk
; did not read" is visibly different from "the game crashed".
;
; TESTED, and the first attempt at testing it was not a test. Asking for block
; 1598 of a 1,600-block disk READ FINE and the loader jumped into a page of
; zeros -- the machine ended up in ROM at $FE/00F2 and the screen stayed dark,
; which looked like the failure path failing. It was the CONTROL failing: the
; only block a 1,600-block image cannot deliver is one past the end. With
; START_BLOCK=1600 the driver returns carry set, this code runs, and the
; screen is #aa00aa across all 128,000 content pixels.
failed:
        lda #$C1
        sta 0xC029
        ldx #199
        lda #$00
scbloop:
        sta 0xE19D00,x
        dex
        bpl scbloop
        ; EVERY palette entry set to the same value, so the screen is one
        ; flat colour whatever the pixel bytes hold. $0A into both bytes of an
        ; entry is R=$A G=0 B=$A -- MAGENTA, measured as #aa00aa filling all
        ; 128,000 content pixels, and a colour the console never draws.
        ldx #31
palloop:
        sta 0xE19E00,x
        dex
        bpl palloop
        lda #0x00
        sta 0x06
        lda #0x20
        sta 0x07
        lda #0xE1
        sta 0x08
        ldx #125
fpage:
        lda #0x44
        ldy #0x00
fbyte:
        sta [0x06],y
        iny
        bne fbyte
        inc 0x07
        dex
        bne fpage
stop:   bra stop

driver:
        jmp (entlo)

; THE HEADER mkdisk.py PATCHES, found by SIGNATURE rather than by an offset
; kept in step by hand. If the signature is not in the image the tool fails
; loudly instead of writing a byte into the middle of an instruction.
        .globl hdr
hdr:
        .ascii "GSBOOT1"
nblocks:
        .byte 1

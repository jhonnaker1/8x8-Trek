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

/* START_BLOCK is 1 for a real disk. The only reason it is a knob at all is
   that the FAILED-READ path cannot otherwise be tested: every block a
   1,600-block image has reads successfully, so the only way to make the drive
   say no is to ask for one that is not there. A failure path nothing has ever
   executed is a comment, not a behaviour. */
#ifndef START_BLOCK
#define START_BLOCK 1
#endif

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

_boot:
        sei
        cld

; THE UNIT NUMBER IS IN X AND IT IS THE ONLY THING THAT SAYS WHICH DRIVE WE
; CAME FROM. Losing it means guessing a slot, and a guess that happens to be
; right on the machine it was written on is the worst kind.
        stx unit

; Driver entry: high byte is $C0 + slot, low byte comes from $CnFF.
        txa
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

; Read PAYLOAD_BLOCKS blocks from block 1 to $2000 upward.
        lda #$00
        sta buflo
        lda #$20
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

        jmp 0x2000

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

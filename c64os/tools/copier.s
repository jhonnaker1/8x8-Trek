; The installer's copier: whole blocks where BASIC moved a byte at a time.
;
; tools/install.bas opened files byte by byte through GET# and PRINT#, and
; the game's 27 files took 18 minutes even in warp -- 57,000 round trips for
; main.o alone, each one a TALK or LISTEN on the serial bus. This moves them
; in blocks: fill a buffer from one channel, then empty it into the other,
; so the bus changes direction once per block instead of once per byte.
;
; BASIC opens the files and calls in (install.bas, after POKE 56,64 puts the
; top of BASIC below the buffers):
;
;     SYS 49152   copy  logical file 1 -> logical file 2, to the end
;     SYS 49155   read logical file 3 through: its length at $C3F0/1/$C3F6, the
;                 16-bit sum of its bytes at $C3F3/4, and $C3F2 nonzero on a
;                 read error
;
; KERNAL entry points only; loads at $C000, which BASIC never touches.

CHKIN   = 0xFFC6
CHKOUT  = 0xFFC9
CLRCHN  = 0xFFCC
CHRIN   = 0xFFCF
CHROUT  = 0xFFD2
STATUS  = 0x90

BUFA    = 0x40          ; $4000-$7FFF: the copy buffer, 16K
BUFB    = 0x60          ; verify reads file 3 into $6000-$7FFF
PTR     = 0xFB          ; two zero-page bytes BASIC does not use

COUNT   = 0xC3F0
DIFF    = 0xC3F2
DIFFAT  = 0xC3F3
CHANERR = 0xC3F5        ; nonzero: a CHKIN or CHKOUT failed (the carry), and
                        ; which: $01/$03 input from file 1/3, $02 output to 2
COUNTHI = 0xC3F6        ; the length's third byte. The first archive over 64K
                        ; (egatrek.car, 102,523 bytes) read back as 36,987
                        ; with the right sum: the count had wrapped.

    .section .text,"ax",@progbits
    jmp     copy
    jmp     verify

; ---- copy: 1 -> 2 -----------------------------------------------------------
copy:
    lda     #0
    sta     CHANERR
1:  ldx     #1
    jsr     CHKIN
    bcs     90f
    lda     #BUFA
    ldx     #0x40               ; up to $40 pages: 16K
    jsr     fill                ; -> the byte count in cnt, eof in eof
    jsr     CLRCHN
    lda     cnt
    ora     cnt+1
    beq     3f
    ldx     #2
    jsr     CHKOUT
    bcs     91f                 ; NOT CHECKED AT FIRST: a failed CHKOUT sends
                                ; the file to the SCREEN, which is how
                                ; brief.txt appeared there instead of on disk
    lda     #BUFA
    jsr     empty
    jsr     CLRCHN
3:  lda     eof
    beq     1b
    rts
90: lda     #1
    .byte   0x2C                ; BIT abs: skip the next two bytes
91: lda     #2
    sta     CHANERR
    jmp     CLRCHN

; ---- verify: length and 16-bit sum of logical file 3, read straight through.
; NOT A COMPARISON WITH FILE 1, which it was: resuming a read on the CMD HD
; after reading device 8 in between returned one byte and then fell through
; to the KEYBOARD -- the second 8K block of main.o sat at $E5CD waiting for a
; key. One device, one pass; the Mac compares the numbers.
verify:
    lda     #0
    sta     COUNT
    sta     COUNT+1
    sta     COUNTHI
    sta     DIFF
    sta     DIFFAT
    sta     DIFFAT+1
    sta     CHANERR
    ldx     #3
    jsr     CHKIN
    bcs     93f
1:  jsr     CHRIN
    clc
    adc     DIFFAT              ; the sum, low byte (DIFFAT reused as SUM)
    sta     DIFFAT
    bcc     2f
    inc     DIFFAT+1
2:  inc     COUNT
    bne     3f
    inc     COUNT+1
    bne     3f
    inc     COUNTHI
3:  lda     STATUS
    beq     1b
    and     #0xBF               ; anything but end-of-file is an error
    beq     4f
    lda     #1
    sta     DIFF
4:  jmp     CLRCHN
93: lda     #3
    sta     CHANERR
    lda     #1
    sta     DIFF
    jmp     CLRCHN

; ---- fill: CHRIN into page A.. for up to X pages; cnt = bytes, eof = 1 at
; the end of the file (STATUS bit 6) or on any error --------------------------
fill:
    sta     PTR+1
    stx     pages
    lda     #0
    sta     PTR
    sta     cnt
    sta     cnt+1
    sta     eof
    ldy     #0
1:  jsr     CHRIN
    sta     (PTR),y
    inc     cnt
    bne     2f
    inc     cnt+1
2:  lda     STATUS
    bne     4f
    inc     PTR
    bne     1b
    inc     PTR+1
    dec     pages
    bne     1b
    rts
4:  lda     #1
    sta     eof
    rts

; ---- empty: CHROUT cnt bytes from page A.. ------------------------------------
empty:
    sta     PTR+1
    lda     #0
    sta     PTR
    lda     cnt
    sta     left
    lda     cnt+1
    sta     left+1
    ldy     #0
1:  lda     left
    ora     left+1
    beq     3f
    lda     (PTR),y
    jsr     CHROUT
    inc     PTR
    bne     2f
    inc     PTR+1
2:  lda     left
    bne     4f
    dec     left+1
4:  dec     left
    jmp     1b
3:  rts

    .data
cnt:    .short 0
cnt1:   .short 0
left:   .short 0
eof:    .byte 0
eof1:   .byte 0
pages:  .byte 0
tmp:    .byte 0
ptrb:   .short 0

; The C64 OS probe: the smallest application that answers the questions the
; port cannot be sized without. Assembly only, so nothing between it and
; C64 OS is in doubt. See c64os/README.md and NOTES.md, "C64 OS PORT".
;
; It pushes one screen layer and keeps a RECORD in its own memory, which
; c64os/tools/rig.py finds by its magic and reads back through VICE's monitor:
;
;   * THAT IT RAN AT ALL -- the init count. It is launched by a double-click:
;     naming it in //os/settings/homebase.t crashes C64 OS's booter instead
;     (see c64os/tools/rig.py);
;   * THAT C64 OS DRAWS IT -- the draw count, and "EGA TREK PROBE" on row 2;
;   * THAT A KEY WRITTEN INTO C64 OS'S OWN BUFFER ARRIVES, and AS WHAT: for
;     each of the first four Kprnt events, the A the callback is handed and
;     what readkprnt_ then returns in A, X, Y and the carry. The first run
;     wrote $41 and the callback's A was $09 -- so A is not the key, and
;     uno's app.s reads the queue instead;
;   * $01 IN EACH CALLBACK, because the game's drawing and sound depend on
;     which of RAM, I/O and ROM each one sees.
;
; The page map, the REU variables and the characters C64 OS itself puts on
; screen are read live by the harness; this app does not need to copy them.
;
; The shape of the header, the layer and the link table is the one
; commodore-uno's c64os-llvm/src/app.s uses, read out of the SDK.

initextern = 0x02FC
raw_rts    = 0x02B2
sec_rts    = 0x02B3

SCREEN     = 0x0400          ; C64 OS's screen BUFFER, not the VIC's
COLOUR     = 0xD800          ; its colour buffer, RAM under I/O
ROW        = 2 * 40 + 13

    .section .app_header,"a",@progbits
    .short  init
    .short  msgcmd
    .short  willquit
    .short  raw_rts             ; freeze
    .short  raw_rts             ; thaw

    .text
init:
    lda     0x01
    sta     rec_init01
    ldx     #mos16lo(externs)
    ldy     #mos16hi(externs)
    jsr     initextern
    ldx     #mos16lo(layer)
    ldy     #mos16hi(layer)
    jsr     os_layerpush
    inc     rec_inits
    rts

msgcmd:
willquit:
    rts

; C64 OS calls this with the I/O banked out, so COLOUR is its buffer.
draw:
    lda     0x01
    sta     rec_draw01
    inc     rec_draws
    bne     1f
    inc     rec_draws + 1
1:  ldx     #text_end - text - 1
2:  lda     text,x
    sta     SCREEN + ROW,x
    lda     #1                  ; white
    sta     COLOUR + ROW,x
    dex
    bpl     2b
    rts

; A -> something, not the key (see above). Carry clear: handled.
kprnt:
    sta     evt_a
    lda     0x01
    sta     rec_key01
    lda     rec_keys
    cmp     #4
    bcs     9f                  ; only the first four are kept
    asl     a
    asl     a
    clc
    adc     rec_keys            ; x5
    tax
    lda     evt_a
    sta     rec_evts,x
    stx     evt_x
    jsr     os_readkprnt
    php
    stx     evt_t
    ldx     evt_x
    sta     rec_evts+1,x
    lda     evt_t
    sta     rec_evts+2,x
    tya
    sta     rec_evts+3,x
    pla
    and     #1                  ; the carry: set = the queue was empty
    sta     rec_evts+4,x
    jsr     os_deqkprnt
9:  inc     rec_keys
    clc
    rts

    .data
layer:
    .short  draw
    .short  sec_rts             ; mouse: not handled
    .short  sec_rts             ; Kcmd: not handled
    .short  kprnt
    .byte   0

externs:
os_layerpush:   .byte 0xF6      ; lscr
                .short 0x0006   ; layerpush_
os_readkprnt:   .byte 0xFC      ; linp
                .short 0x0018   ; readkprnt_
os_deqkprnt:    .byte 0xFC      ; linp
                .short 0x001B   ; deqkprnt_
                .byte 0xFF

; "EGA TREK PROBE" as screen codes in C64 OS's set: capitals at 65-90.
text:   .byte 69,71,65,32,84,82,69,75,32,80,82,79,66,69
text_end:

; THE RECORD. The magic is how the harness finds it without a map file.
    .globl  record
record:     .ascii "EGATREKPROBE!"
rec_inits:  .byte 0
rec_draws:  .short 0
rec_keys:   .byte 0
rec_lastkey: .byte 0            ; unused since the per-event record below
rec_init01: .byte 0
rec_draw01: .byte 0
rec_key01:  .byte 0
rec_evts:   .fill 4 * 5, 1, 0   ; per event: callback A, readkprnt A X Y C
evt_a:      .byte 0
evt_x:      .byte 0
evt_t:      .byte 0

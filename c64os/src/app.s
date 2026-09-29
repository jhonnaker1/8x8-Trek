; EGA Trek's C64 OS side: the application header, the KERNAL link table, and
; the crossings between C64 OS and the game's C.
;
; THE SHAPE IS commodore-uno's c64os-llvm/src/app.s, which proved a whole C
; game can live inside C64 OS; the constants are the SDK's (//os/s/app.s,
; //os/h/*.h in C64 OS 1.09). What differs is the game: uno is a state machine
; driven by events, and EGA Trek is a C program with a main() that runs until
; the player quits and blocks in kb_waitkey(). So here init RUNS THE GAME and
; returns only when it is over -- C64 OS's event loop is not entered while the
; game plays. Its IRQ still runs: the clock, the mouse, the keyboard scan into
; the queue kb_waitkey() reads. Menus do not open mid-game; that is the later
; step, and NOTES.md says why.
;
; ZERO PAGE. llvm-mos keeps 32 imaginary registers at $4E-$6D, BASIC's float
; workspace, which nothing on C64 OS's IRQ path touches (uno measured it, and
; //os/docs/memory.t agrees). They are swapped with C64 OS's at every
; crossing: C64 OS's copy is saved on the way into C, and restored around
; every call C makes back into the OS.
;
; THE HARDWARE STACK is C64 OS's, shared. The soft stack is this file's own
; .stack section, inside the loaded file -- C64 OS allocates only the pages
; the file covers.

initextern = 0x02FC
raw_rts    = 0x02B2
sec_rts    = 0x02B3

ZP_BASE    = 0x4E               ; must match __rc0 in c64os.ld
ZP_LEN     = 32

    .section .app_header,"a",@progbits
    .short  app_init
    .short  app_msgcmd
    .short  app_willquit
    .short  raw_rts             ; freeze: the game never yields, so never asked
    .short  raw_rts             ; thaw

    .text

; THE WHOLE GAME, from here. Returns to C64 OS when main() returns or
; plat_exit() unwinds to exit_sp.
app_init:
    ldx     #mos16lo(externs)
    ldy     #mos16hi(externs)
    jsr     initextern

    ldx     #mos16lo(layer)
    ldy     #mos16hi(layer)
    jsr     os_layerpush_

    tsx
    stx     exit_sp             ; plat_exit() comes back to this depth

    ldx     #ZP_LEN-1           ; C64 OS's zero page, kept for the way out
1:  lda     ZP_BASE,x
    sta     os_zp,x
    dex
    bpl     1b
    lda     #mos16lo(__stack)   ; a fresh soft stack
    sta     ZP_BASE
    lda     #mos16hi(__stack)
    sta     ZP_BASE+1
    cld
    jsr     c64os_start         ; the overlays into the REU -- src/osovl.c
    jsr     main                ; main() is an overlay: this becomes a thunk

; plat_exit() lands here too, with the stack cut back to where init had it.
    .globl  app_exit
app_exit:
    ldx     exit_sp
    txs
    ldx     #ZP_LEN-1
1:  lda     os_zp,x
    sta     ZP_BASE,x
    dex
    bpl     1b

; Give back what src/osfile.c took: the two REU banks, then memory.lib. The
; docs say banks are freed when a switched-out app is expunged, and nothing
; about a quit, so do not leave it to C64 OS. bkfree_ takes the bank as
; bkalloc_ gave it, counted from appreubk. unldlib_ gets flags 0: with
; slunload set it would call the library's offset $03, which in memory.lib
; is realloc_, not an unload routine.
    lda     c64os_libpage
    beq     4f
    sta     5f+2
    lda     c64os_bank
    beq     3f
    sec
    sbc     0x0282              ; appreubk
    tay
    ldx     #2
5:  jsr     0x0006              ; bkfree_
3:  ldx     #0x4D               ; "me"
    ldy     #0x45
    lda     #0
    jsr     os_unldlib_

; QUIT FROM INSIDE THE EVENT LOOP, NOT FROM HERE. quitapp_ only points the
; event-loop break vector ($0336, loopbrkvec) at C64 OS's run-home routine;
; the loop jumps through it at the end of a pass and resets it at the start
; of the next. Called from init, before the loop's first pass, the reset
; wiped it: the first build returned here and C64 OS carried on running the
; game as the current app, menu bar and all, with no way out. So ask for a
; redraw and let layer_draw -- called in the first pass -- make the request.
4:  lda     #1
    sta     quit_pending
    ldx     #0                  ; every layer
    jsr     os_markredraw_
    rts                         ; back into C64 OS, from init

; A = message, X = menu action. The one message the game answers is its
; menu's Quit (menu.json: action "!"), which only exists once the event loop
; runs -- which, while the game plays, it does not.
app_msgcmd:
    cmp     #0                  ; mc_mnu, a menu action
    bne     1f
    cpx     #0x21               ; "!", Quit
    bne     1f
    jsr     os_quitapp_
    clc                         ; handled
    rts
1:  sec                         ; not handled
    rts

app_willquit:
    rts

; C64 OS would call these from its event loop, which does not run while the
; game plays. They are here because a layer must have them.
layer_draw:
    lda     quit_pending        ; app_exit asked: see there
    beq     1f
    jsr     os_quitapp_
1:  rts
layer_kprnt:
    sec                         ; not handled: kb_waitkey() reads the queue
    rts

; ---------------------------------------------------------------------------
; C -> OS. Park C's zero page, give C64 OS its own back; and the reverse.
; Preserve A, X, Y -- they carry the call's arguments and results.

to_os:
    sta     sav_a
    stx     sav_x
    ldx     #ZP_LEN-1
1:  lda     ZP_BASE,x
    sta     app_zp,x
    lda     os_zp,x
    sta     ZP_BASE,x
    dex
    bpl     1b
    lda     sav_a
    ldx     sav_x
    rts

from_os:
    php
    sta     sav_a
    stx     sav_x
    ldx     #ZP_LEN-1
1:  lda     ZP_BASE,x
    sta     os_zp,x
    lda     app_zp,x
    sta     ZP_BASE,x
    dex
    bpl     1b
    lda     sav_a
    ldx     sav_x
    plp
    rts

; unsigned int os_readkey(void): the next key off C64 OS's queues, taken off
; it, or 0xFFFF when there is none. THE KEY IS THE READ CALL'S A -- the value
; a Kprnt callback is handed is not the key (the probe read $09 for an $41).
;
; TWO QUEUES. C64 OS's keyboard driver sends RUN/STOP and F1-F7 (scan codes
; 03 04 05 06 3F, a table in the driver itself) and anything with CTRL or C=
; held to the COMMAND queue, and the rest to the printable one. The first
; game build read only the printable queue, so ESC -- VICE's Esc is RUN/STOP
; -- never arrived. CTRL and C= combinations are C64 OS's menu shortcuts, not
; the game's keys, so they are taken off and skipped.
;
; c64os_keylog is this routine's own working pair -- the last key read, and
; the modifiers of the last COMMAND key -- which tools/rig.py prints.
    .globl  os_readkey
os_readkey:
    jsr     to_os
    jsr     os_readkcmd_
    bcs     1f
    sta     c64os_keylog
    sty     c64os_keylog+1
    jsr     os_deqkcmd_
    jsr     from_os
    lda     c64os_keylog+1
    and     #0x06               ; C= or CTRL
    bne     os_readkey
    beq     3f
1:  jsr     os_readkprnt_
    bcs     2f
    sta     c64os_keylog
    jsr     os_deqkprnt_
    jsr     from_os
3:  lda     c64os_keylog
    ldx     #0
    rts
2:  jsr     from_os
    lda     #0xFF
    ldx     #0xFF
    rts

; unsigned char os_loadlib(unsigned char c1, unsigned char c2, unsigned char a)
; -- llvm-mos passes the three in A, X and __rc2. Returns the library's
; first page, or 0.
    .globl  os_loadlib
os_loadlib:
    sta     lib_x
    stx     lib_y
    lda     ZP_BASE+2           ; __rc2, before the swap takes it away
    sta     lib_a
    jsr     to_os
    ldx     lib_x
    ldy     lib_y
    lda     lib_a
    jsr     os_loadlib_
    sta     key_a
    jsr     from_os
    lda     key_a
    rts

; unsigned char os_bkalloc(unsigned char page, unsigned char banks): calls
; memory.lib's bkalloc_ at page*256+9. Returns the first bank AS bkalloc_
; GIVES IT -- "normal", counted from appreubk ($0282), so 0 is a real answer
; -- or $FF on failure. memory.lib says so in its code: it scans from
; appreubk and subtracts appreubk before it returns.
    .globl  os_bkalloc
os_bkalloc:
    sta     2f+2                ; the library's page, into the JSR below
    stx     lib_x
    jsr     to_os
    ldx     lib_x
2:  jsr     0x0009
    bcs     3f
    sty     key_a
    jsr     from_os
    lda     key_a
    rts
3:  jsr     from_os
    lda     #0xFF
    rts

; ---------------------------------------------------------------------------
; Buffers -> screen. C64 OS keeps the characters at $0400 and the colours in
; the RAM under I/O at $D800, and the VIC shows the RAM under I/O at $DC00
; ($DD00 = $C4, $D018 = $75). Its event loop copies one to the other; with the
; loop not running, osvid.c calls this. uno's os_present, unchanged: stop at
; $DFE7, because $DFF8 up are the sprite pointers and the mouse is a sprite.

    .globl  os_present
os_present:
    php
    sei
    lda     0x01
    pha
    and     #0xF8
    ora     #0x04               ; all RAM
    sta     0x01
    ldx     #0
1:  lda     0x0400,x
    sta     0xDC00,x
    lda     0x0500,x
    sta     0xDD00,x
    lda     0x0600,x
    sta     0xDE00,x
    inx
    bne     1b
2:  lda     0x0700,x
    sta     0xDF00,x
    inx
    cpx     #0xE8
    bne     2b

    lda     #0xD8
    sta     5f+2
    sta     6f+2
    ldy     #4
3:  ldx     #0
5:  lda     0xD800,x
    sta     bounce,x
    inx
    bne     5b
    lda     0x01
    ora     #0x01               ; %101: I/O in
    sta     0x01
4:  lda     bounce,x
6:  sta     0xD800,x
    inx
    bne     4b
    lda     0x01
    and     #0xF8
    ora     #0x04
    sta     0x01
    inc     5b+2
    inc     6b+2
    dey
    bne     3b

    pla
    sta     0x01
    plp
    rts

; ---------------------------------------------------------------------------

    .data
layer:
    .short  layer_draw
    .short  sec_rts             ; mouse: not handled
    .short  sec_rts             ; Kcmd: not handled
    .short  layer_kprnt
    .byte   0

; The KERNAL link table. C64 OS rewrites each record into a JMP.
externs:
os_layerpush_:  .byte 0xF6      ; lscr
                .short 0x0006   ; layerpush_
os_markredraw_: .byte 0xF6      ; lscr
                .short 0x0003   ; markredraw_
os_quitapp_:    .byte 0xF2      ; lser
                .short 0x0021   ; quitapp_
os_loadlib_:    .byte 0xF2      ; lser
                .short 0x002A   ; loadlib_
os_unldlib_:    .byte 0xF2      ; lser
                .short 0x002D   ; unldlib_
os_readkcmd_:   .byte 0xFC      ; linp
                .short 0x0012   ; readkcmd_
os_deqkcmd_:    .byte 0xFC      ; linp
                .short 0x0015   ; deqkcmd_
os_readkprnt_:  .byte 0xFC      ; linp
                .short 0x0018   ; readkprnt_
os_deqkprnt_:   .byte 0xFC      ; linp
                .short 0x001B   ; deqkprnt_
                .byte 0xFF

    .globl  c64os_keylog
c64os_keylog: .byte 0, 0
exit_sp: .byte 0
quit_pending: .byte 0
sav_a:   .byte 0
sav_x:   .byte 0
key_a:   .byte 0
lib_x:   .byte 0
lib_y:   .byte 0
lib_a:   .byte 0
os_zp:   .fill ZP_LEN, 1, 0
app_zp:  .fill ZP_LEN, 1, 0
    .globl  bounce              ; also far_load's page buffer: src/osfile.c
bounce:  .fill 256, 1, 0

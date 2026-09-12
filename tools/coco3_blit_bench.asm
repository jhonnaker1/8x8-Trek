; EGA Trek -- CoCo 3 + SuperSprite FM+ blit cost benchmark.
;
; The CoCo 3 target is DROPPED (Jamie, 2026-09-05). This exists because the
; scope for it called for a benchmark rather than arithmetic, and the number
; it produces is what any future reopening would turn on. Results and the
; derived figures are in NOTES.md under "SCOPE: the CoCo 3 + SuperSprite FM+".
;
; Build:  lwasm --raw -o bench.bin --list=bench.lst tools/coco3_blit_bench.asm
;
; Run under MAME, driving it from a debugscript -- the marker addresses come
; out of the listing, and `totalcycles` is read at each one:
;
;   mame coco3 -rompath <Ample roms> -ext ssfm -video none \
;        -debug -debugscript <script> -debuglog -nothrottle
;
;   gtime 3000          ; NOTE: debugger numbers are HEX. 3000 = 12288 ms.
;   load bench.bin,2000
;   bp <m1> ... bp <m6>
;   pc=2000
;   go / printf "%d",totalcycles   ; once per marker
;   dump rb.txt,2200,40,1,0        ; the dropped-byte check reads back here
;
; V9958 confirmed at $FF78 (data) / $FF79 (address/status), DP=$FF so
; every port access is 4-cycle direct addressing -- the fastest the 6809 has.
VDATA   equ $78
VADDR   equ $79
CNT     equ $2300

        org  $2000
start   orcc #$50               ; no interrupts: cycle counts must be ours
        sta  $FFD9              ; GIME fast mode: 6809 at 1.78977 MHz
        lda  #$ff
        tfr  a,dp

        clra                    ; R#14 = 0 (VRAM address bits 16-14)
        sta  <VADDR
        lda  #$8E
        sta  <VADDR

; ---- BENCH 1: linear fill, 240 bytes = one 480-pixel screen row ----
        clra
        sta  <VADDR
        lda  #$40               ; write address $0000, auto-increment
        sta  <VADDR
        ldx  #$3000
        ldb  #30                ; 30 x 8 bytes
m1      nop
b1loop
        lda  ,x+
        sta  <VDATA
        lda  ,x+
        sta  <VDATA
        lda  ,x+
        sta  <VDATA
        lda  ,x+
        sta  <VDATA
        lda  ,x+
        sta  <VDATA
        lda  ,x+
        sta  <VDATA
        lda  ,x+
        sta  <VDATA
        lda  ,x+
        sta  <VDATA
        decb
        bne  b1loop
m2      nop

; ---- BENCH 2: one 6x8 character cell -- 8 rows of 3 bytes, each row
;      a fresh VRAM address because rows are 240 bytes apart ----
        ldy  #$0000
        ldx  #$3000
        lda  #8
        sta  CNT
m3      nop
b2loop  tfr  y,d
        stb  <VADDR
        ora  #$40
        sta  <VADDR
        lda  ,x+
        sta  <VDATA
        lda  ,x+
        sta  <VDATA
        lda  ,x+
        sta  <VDATA
        leay 240,y
        dec  CNT
        bne  b2loop
m4      nop

; ---- BENCH 3: does anything drop bytes written faster than the
;      V9958's VRAM recovery time? 64 writes, 6 cycles apart ----
        clra
        sta  <VADDR
        lda  #$40
        sta  <VADDR
        clra
m5      nop
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
        sta  <VDATA
        inca
m6      nop
        clra                    ; read back from $0000
        sta  <VADDR
        clra
        sta  <VADDR
        ldx  #$2200
        ldb  #64
rdloop  lda  <VDATA
        sta  ,x+
        decb
        bne  rdloop
done    bra  done

; Hypervisor calls, with llvm-mos's imaginary registers saved across them.
;
; WHY THIS FILE EXISTS. mega65-libc's fileio.s is hand-written assembly that
; uses __rc4 and __rc5 as scratch around the hyppo trap, and the trap itself
; returns through registers the compiler was never told about. llvm-mos
; therefore believes read512() clobbers nothing and is free to keep a live
; value in a pseudo-register across the call.
;
; WHAT THAT COST: plat_read()'s loop counter. The generated code kept the
; remaining count in $81/$82, tested it there, and emitted its decrement
; against __rc24 -- so the count was never reduced and every plat_read()
; swallowed the whole file and returned its length modulo 256. Measured on the
; machine 2026-09-03: one call per file, returning 116 for a 7,284-byte
; STRINGS.DAT. far_load's destination pointer then never advanced, the string
; pool held one chunk, the overlay images were never written to banked RAM at
; all, and ui_title() -- which is fine -- was called into empty memory.
;
; Saving all 32 costs about 130 cycles against a hypervisor trap and a 512-byte
; DMA. It is not worth working out which ones actually matter, and a shorter
; list would rot the first time the library changed.

.global trek_read512, trek_open, trek_close

.section .text.trek_hyppo,"ax",@progbits

; Save __rc0..__rc31, call through, restore, and hand back A (and X for the
; calls that return sixteen bits).
save_rc:
	ldx #31
1:	lda __rc0,x
	sta rcsave,x
	dex
	bpl 1b
	rts

restore_rc:
	ldx #31
1:	lda rcsave,x
	sta __rc0,x
	dex
	bpl 1b
	rts

; uint16_t trek_read512(uint8_t *buf)   -- buf in __rc2/__rc3, count in A/X
trek_read512:
	jsr save_rc
	jsr read512
	sta retlo
	stx rethi
	jsr restore_rc
	lda retlo
	ldx rethi
	rts

; uint8_t trek_open(char *name)         -- name in __rc2/__rc3, fd in A
trek_open:
	jsr save_rc
	jsr open
	sta retlo
	jsr restore_rc
	lda retlo
	rts

; void trek_close(uint8_t fd)           -- fd in A
trek_close:
	jsr save_rc
	jsr close
	jsr restore_rc
	rts

.section .bss.trek_hyppo,"aw",@nobits
rcsave:	.zero 32
retlo:	.zero 1
rethi:	.zero 1

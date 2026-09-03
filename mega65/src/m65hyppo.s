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

.global trek_read512, trek_open, trek_closeall

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

; void trek_closeall(void)
;
; There was a trek_close(fd) here. It is gone rather than left unused: hyppo
; will not accept the descriptor its own openfile returns, so m65storage.c
; closes with closeall() instead and nothing can call it. Its one lesson is
; kept, because it applies to any shim written here: THE ARGUMENT MAY BE IN A,
; AND save_rc DESTROYS A. That version handed `close` whatever save_rc's last
; `lda` left behind -- __rc0, the soft stack pointer low byte -- so hyppo was
; asked to close descriptor $B2 three times.
trek_closeall:
	jsr save_rc
	jsr closeall
	jsr restore_rc
	rts

.section .bss.trek_hyppo,"aw",@nobits
rcsave:	.zero 32
retlo:	.zero 1
rethi:	.zero 1

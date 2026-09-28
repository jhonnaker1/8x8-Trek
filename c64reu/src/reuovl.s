; The REU overlay manager: overlays that may call each other.
;
; THE DISK RULES DO NOT APPLY HERE, AND THIS FILE IS WHY. core/overlay.h
; forbids an overlay from calling another because a swap from a 1541 costs
; seconds. From an REU it is a DMA at about a byte a cycle -- ~4ms for a full
; 4K window -- so every call that crosses from one overlay into another goes
; through a resident THUNK instead, and the thunk makes it safe:
;
;     thunk:  jsr ovl_far          ; six bytes, written by tools/reu_thunks.py
;             .byte id             ; into the table at the bottom of this file,
;             .word body           ; one per overlay function called from
;                                  ; outside its own overlay
;
; ovl_far reads the three bytes after the JSR, and then:
;
;   * THE CALLEE'S OVERLAY IS ALREADY IN THE WINDOW: jump to it. Nothing
;     changes, so there is nothing to restore, and the return goes straight
;     back to the caller.
;   * IT IS NOT: push the id that IS in the window, DMA the callee's overlay
;     in, call it, and on the way back DMA the pushed one in again before
;     returning.
;
; IT ALWAYS PUTS BACK WHAT WAS THERE, even for a resident caller, and that is
; not caution. A resident helper called FROM overlay A returns INTO A, so a
; call it makes into overlay B must leave A in the window afterwards. Whether
; the immediate caller is resident says nothing about who is waiting above it.
;
; A TAIL CALL IS SAFE FOR THE SAME REASON. `jmp body` from overlay A into B
; arrives here with A's caller's return address on the stack; A is pushed,
; restored after B returns, and A's caller -- resident, A itself, or the
; thunk frame of whoever called A -- finds the window as it left it.
;
; REGISTERS. llvm-mos passes arguments in A, X and the __rc imaginary
; registers and returns in A, X and __rc2..; this file touches no zero page
; at all (self-modifying absolute loads instead), and carries A, X and Y
; through both the entry and the return. Flags are not preserved; the C ABI
; does not pass anything in them.
;
; INTERRUPTS never reach overlay code on this port: the IRQ is the KERNAL's
; and the music is polled from kb_waitkey. The scratch bytes below are
; therefore safe to share between nesting levels -- each is consumed before
; the next level can begin.

REU_CMD   = 0xDF01
REU_C64L  = 0xDF02
REU_C64H  = 0xDF03
REU_REUL  = 0xDF04
REU_REUH  = 0xDF05
REU_BANK  = 0xDF06
REU_LENL  = 0xDF07
REU_LENH  = 0xDF08
REU_IMR   = 0xDF09
REU_ACR   = 0xDF0A

WINDOW_HI = 0xC0        ; the window is $C000 -- c64reu.ld
OVL_NONE  = 0xFF
MAXOVL    = 32          ; table size; c64reu.c asserts OVL_COUNT fits
RSDEPTH   = 16          ; nested cross-overlay calls; deeper is a halt
NTHUNKS   = 64

	.section .text.reuovl,"ax",@progbits

; void reu_xfer(uint8_t id, uint8_t cmd) -- A = id, X = the REU command.
; $90 stashes the window into the REU, $91 fetches it back; bit 4 set means
; "start now", not on a write to $FF00.
;
; THE I/O PAGE IS MAPPED IN FOR THE TRANSFER AND PUT BACK AFTER. On the bare
; C64 it is always in, but C64 OS runs an app's draw callback with $01 = $34,
; all RAM, and the REU's registers vanish with it. $35 is RAM everywhere
; except I/O -- BASIC and the KERNAL out, which is also true of this port's
; own program region -- and interrupts are off while it holds, because the
; KERNAL is not there to take one.
	.globl reu_xfer
reu_xfer:
	tay
	php
	sei
	lda 1
	pha
	and #0xF8
	ora #0x05
	sta 1
	lda #0
	sta REU_C64L
	sta REU_REUL
	sta REU_IMR
	sta REU_ACR
	lda #WINDOW_HI
	sta REU_C64H
	lda ovl_rhi,y
	sta REU_REUH
	lda ovl_rbank,y
	sta REU_BANK
	lda ovl_llo,y
	sta REU_LENL
	lda ovl_lhi,y
	sta REU_LENH
	stx REU_CMD             ; the CPU stops here until the DMA is done
	pla
	sta 1
	plp
	rts

	.globl ovl_far
ovl_far:
	sta save_a
	stx save_x
	sty save_y
	pla                     ; the JSR pushed thunk+2: its last byte
	sta rd + 1
	pla
	sta rd + 2
	ldy #3
1:
rd:	lda 0xFFFF,y            ; thunk+3..+5: id, body lo, body hi
	sta tgt_id - 1,y
	dey
	bne 1b

	lda tgt_id
	cmp ovl_live
	bne 2f
	lda save_a              ; already in the window: a plain jump
	ldx save_x
	ldy save_y
	jmp (tgt_lo)

2:
	ldx ovl_sp
	cpx #RSDEPTH
	bcs overflow
	lda ovl_live
	sta ovl_rs,x
	inx
	stx ovl_sp
	lda tgt_id
	sta ovl_live
	ldx #0x91
	jsr reu_xfer
	lda save_a
	ldx save_x
	ldy save_y
	jsr 3f                  ; call the body; it returns here
	sta save_a
	stx save_x
	sty save_y
	ldx ovl_sp
	dex
	stx ovl_sp
	lda ovl_rs,x
	cmp #OVL_NONE           ; nothing was loaded before: leave the callee
	beq 4f
	cmp ovl_live
	beq 4f
	sta ovl_live
	ldx #0x91
	jsr reu_xfer
4:
	lda save_a
	ldx save_x
	ldy save_y
	rts
3:
	jmp (tgt_lo)

; NESTED MORE THAN RSDEPTH DEEP, which the game never is -- its deepest
; chain is four. Stop with the border flashing rather than overwrite the
; return stack and return into the wrong overlay somewhere far from here.
overflow:
	inc 0xD020
	jmp overflow

; THE THUNK TABLE. Every byte is $00 -- BRK -- until the build patches it,
; so a call into an unpatched slot stops the machine instead of running on.
	.globl __ovl_thunks
	.globl __ovl_thunks_end
__ovl_thunks:
	.fill NTHUNKS * 6, 1, 0x00
__ovl_thunks_end:

	.section .data.reuovl,"aw",@progbits
; WHICH OVERLAY IS IN THE WINDOW. Starts as none, not as zero: zero is
; OVL_EVAL, and a .bss byte would claim the evaluation was loaded.
	.globl ovl_live
ovl_live:	.byte OVL_NONE

	.section .bss.reuovl,"aw",@nobits
	.globl ovl_rhi
	.globl ovl_rbank
	.globl ovl_llo
	.globl ovl_lhi
ovl_rhi:	.fill MAXOVL, 1, 0      ; REU address, middle byte
ovl_rbank:	.fill MAXOVL, 1, 0      ; REU bank
ovl_llo:	.fill MAXOVL, 1, 0      ; image length, as loaded
ovl_lhi:	.fill MAXOVL, 1, 0
ovl_sp:		.byte 0
ovl_rs:		.fill RSDEPTH, 1, 0     ; the ids to put back, innermost last
save_a:		.byte 0
save_x:		.byte 0
save_y:		.byte 0
; THE THREE BYTES COPIED OUT OF A THUNK, in the thunk's own order -- the
; copy loop writes tgt_id+2 down to tgt_id. JMP (ind) reads its high byte
; from the same page as its low byte, so the vector must not straddle one:
; the pad puts tgt_lo on an even address, which is never $xxFF.
	.balign 2
		.byte 0
tgt_id:		.byte 0
tgt_lo:		.byte 0
tgt_hi:		.byte 0

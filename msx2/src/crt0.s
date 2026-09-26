	;; MSX-DOS .COM startup for SDCC. DOS loads the file at $0100 and jumps
	;; there. The stack goes at ($0006), the top of the TPA, EXPLICITLY:
	;; under COMMAND2 that is $D606 and DOS had put SP just below it, but a
	;; program loaded AS THE SHELL (the boot disk's COMMAND2.COM) is entered
	;; with SP=$DCFE, up in DOS's own area above the $DB06 BDOS entry --
	;; measured 2026-09-25. `ld sp,(6)` is right in both.
	;;
	;; DOS does NOT zero memory, so _DATA is zeroed and initialisers copied
	;; here -- the same two fixups uno's cartridge crt0 does, for the same
	;; reason: nothing else will.
	.module crt0
STACK_RESERVE = 256		; bytes; see the memory check in init
	.globl	_main
	.globl	s__HEAP
	.globl	s__DATA
	.globl	l__DATA
	.globl	s__INITIALIZED
	.globl	s__INITIALIZER
	.globl	l__INITIALIZER

	.area	_HEADER (ABS)
	.org	0x0100
	jp	init			; 3 bytes; _CODE follows at $0103

	.area	_HOME
	.area	_CODE
init::
	ld	sp, (0x0006)

	;; THE MEMORY CHECK. The stack grows down from ($0006) into whatever the
	;; image leaves, and the game needs 256 bytes of it -- 208 measured on the
	;; running game, ~225 the static worst case (NOTES, "THE STACK"). How much
	;; there IS depends on the machine: every disk interface and driver
	;; takes page-3 work area and lowers ($0006), and so does COMMAND2 if the
	;; game is run from a prompt instead of installed as the shell (1,280
	;; bytes). s__HEAP is the linker's end of the image, so this cannot go
	;; stale behind a build. Too little: say so, wait for a key, _TERM0 --
	;; which returns to a prompt, or, as the shell, reloads and says it again.
	ld	hl, (0x0006)
	ld	de, #s__HEAP + STACK_RESERVE
	or	a
	sbc	hl, de
	jr	c, nomem
	call	gsinit
	call	_main
	ld	c, #0x00		; main returned: _TERM0
	jp	0x0005

nomem:
	ld	de, #nomem_msg
	ld	c, #0x09		; _STROUT
	call	0x0005
	ld	c, #0x08		; _INNOE: wait for a key, no echo
	call	0x0005
	ld	c, #0x00		; _TERM0
	jp	0x0005
nomem_msg:
	.ascii	"NOT ENOUGH MEMORY FOR EGA TREK."
	.db	13, 10
	.ascii	"BOOT ITS OWN DISK. PRESS A KEY."
	.db	13, 10
	.ascii	"$"

	.area	_INITIALIZER
	.area	_GSINIT
	.area	_GSFINAL
	.area	_DATA
	.area	_INITIALIZED
	.area	_BSEG
	.area	_BSS
	.area	_HEAP

	.area	_GSINIT
gsinit::
	ld	bc, #l__DATA
	ld	a, b
	or	a, c
	jr	Z, gsinit_init
	ld	hl, #s__DATA
	ld	(hl), #0x00
	dec	bc
	ld	a, b
	or	a, c
	jr	Z, gsinit_init
	ld	de, #s__DATA + 1
	ldir
gsinit_init:
	ld	bc, #l__INITIALIZER
	ld	a, b
	or	a, c
	jr	Z, gsinit_done
	ld	de, #s__INITIALIZED
	ld	hl, #s__INITIALIZER
	ldir
gsinit_done:
	.area	_GSFINAL
	ret

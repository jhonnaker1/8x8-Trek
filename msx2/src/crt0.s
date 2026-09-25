	;; MSX-DOS .COM startup for SDCC. DOS loads the file at $0100 and jumps
	;; there with SP already just below BDOS, at the top of the TPA -- so the
	;; stack is left where DOS put it rather than nominated.
	;;
	;; DOS does NOT zero memory, so _DATA is zeroed and initialisers copied
	;; here -- the same two fixups uno's cartridge crt0 does, for the same
	;; reason: nothing else will.
	.module crt0
	.globl	_main
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
	call	gsinit
	call	_main
	ld	c, #0x00		; main returned: _TERM0
	jp	0x0005

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

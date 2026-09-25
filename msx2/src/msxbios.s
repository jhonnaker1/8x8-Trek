	;; The MSX BIOS, reached from an MSX-DOS program.
	;;
	;; uno's msx2 port is a CARTRIDGE, so page 0 is the BIOS ROM and a BIOS
	;; call is a plain `call`. THIS PORT IS AN MSX-DOS .COM: page 0 is RAM
	;; holding the TPA, so every BIOS routine is reached through CALSLT at
	;; $001C, which MSX-DOS keeps in page 0 for exactly this. IYH names the
	;; slot -- EXPTBL at $FCC1 holds the main ROM's -- and IX the routine.
	;;
	;; CALSLT RETURNS WITH INTERRUPTS DISABLED. The BIOS handler is what
	;; drives JIFFY and the keyboard, so every wrapper puts them back.
	;;
	;; SDCC 4.x's default convention passes the first char in A and returns a
	;; char in A, which is what these routines want. IX is the register SDCC
	;; expects a callee to preserve, and CALSLT uses it for the address.

	.module msxbios
	.area	_CODE

	;; CHGMOD -- screen mode in A. Sets table bases and mode bits correctly on
	;; every MSX2 variant and hides the sprites; hand-poking R#0..R#10 is the
	;; silent failure uno's port chose not to risk.
_bios_chgmod::
	push	ix
	ld	iy, (0xFCC0)		; IYH = EXPTBL = the main BIOS's slot
	ld	ix, #0x005F		; CHGMOD
	call	0x001C			; CALSLT
	ei
	pop	ix
	ret

	;; Back to the DOS prompt: text mode first (CHGMOD 0, at DOS's own
	;; width), then _TERM0.
_plat_exit::
	xor	a
	call	_bios_chgmod
	ld	c, #0x00		; _TERM0
	jp	0x0005			; BDOS -- does not return

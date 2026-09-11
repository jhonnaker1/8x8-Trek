; The boot record for a disk with NO ATARI DOS ON IT.
;
; Sector 1 of a bootable Atari disk is not a program, it is a descriptor. The
; OS reads it into the cassette buffer, takes four things out of its first six
; bytes -- flags, a SECTOR COUNT, a load address and an init vector -- loads
; that many sectors from sector 1 onward to the load address, and JSRs to
; load address + 6. Everything below offset 6 here is that descriptor; the
; code starts at 6 because that is where control arrives.
;
; WHAT RUNS THERE IS AN XEX LOADER, and that choice is the point of the file.
; It could have been a flat image -- read N sectors to $3000, jump -- which is
; less code. But the game's link emits three segments and two of them are not
; code at all: two bytes at $02E5 that lower MEMTOP before _start reads it
; (see atari.ld, the soft stack) and two at $02E0 that are the run vector.
; A flat loader would have to know about both and set them by hand, and the
; disk build would then diverge from the DOS build in exactly the place this
; port has already been bitten once. Honouring the segment format instead
; means THE SAME EGATREK.XEX BOOTS HERE AND LOADS UNDER DOS, byte for byte.
;
; The file is found by NAME in the directory tools/nodos.py writes at sector
; 4, not at a sector this file was told about at build time. Nothing here has
; to be patched after assembly, and the loader cannot go stale against the
; disk builder.
;
; Assembled with llvm-mc and linked flat at $0700 -- see the Makefile. Hex is
; 0x, not $: llvm-mos's assembler reads $12 as a symbol named "$12" and the
; error arrives from the LINKER, which is a long way from the typo.

	.section .text,"ax",@progbits

BOOTAD	= 0x0700	; where the OS is told to put us
BUFFER	= 0x0900	; one sector, clear of the loaded image -- boot.ld
			; asserts it. Free RAM at boot, and NOT part of the
			; image, so it costs no disk sector.

; Zero page. $CB..$D1 are the OS's spare bytes -- nothing in the ROM touches
; them -- and only the segment pointer actually needs to be down here, for
; the (zp),y store. Everything else lives in the image.
dst	= 0xCB

; The Device Control Block, and SIO itself. The same registers src/atarisio.c
; fills from C; src/siotest.c proved the primitive with no DOS resident.
DDEVIC	= 0x0300
DUNIT	= 0x0301
DCOMND	= 0x0302
DSTATS	= 0x0303
DBUFLO	= 0x0304
DBUFHI	= 0x0305
DTIMLO	= 0x0306
DBYTLO	= 0x0308
DBYTHI	= 0x0309
DAUX1	= 0x030A
DAUX2	= 0x030B
SIOV	= 0xE459

RUNAD	= 0x02E0	; the run vector, written by one of the XEX's segments
COLBK	= 0xD01A	; background colour -- the only report a failed boot has

DIRSEC	= 4
DIRSECS	= 2		; the directory spans two sectors -- tools/nodos.py
ENTPSEC	= 8		; 128 / 16
SECSIZE	= 128

	.globl bootrec
bootrec:
	.byte 0					; flags, ignored by the OS
	.byte (imgend - bootrec + 127) / 128	; SECTORS TO READ, computed so
						; that growing the loader can
						; never silently truncate it
	.word BOOTAD				; load address
	.word dosini				; init vector, JSR'd on reset

; +6: where the OS hands over.
	jmp start

dosini:	rts		; nothing to re-initialise: by the time anything could
			; press RESET the game owns the machine, not this code.

; ---------------------------------------------------------------- find it
; BOTH DIRECTORY SECTORS ARE SCANNED even though nodos.py writes EGATREK.XEX
; into the first entry it has. Knowing where the builder happens to put it
; today is exactly the coupling the name lookup exists to avoid.
start:
	lda #DIRSEC
	sta secnum
	lda #0
	sta secnum+1
	lda #DIRSECS
	sta dirleft

findsec:
	jsr readsec
	inc secnum			; the next directory sector, if the
	bne findent			; name is not in this one
	inc secnum+1
findent:
	ldx #0				; byte offset of the entry under test
findnxt:
	ldy #0
findchr:
	lda BUFFER,x
	cmp fname,y
	bne nextent
	inx
	iny
	cpy #11
	bne findchr
	; matched all eleven: x is at entry+11, where the extent lives.
	lda BUFFER,x
	sta secnum
	lda BUFFER+1,x
	sta secnum+1
	lda BUFFER+2,x
	sta left
	lda BUFFER+3,x
	sta left+1
	jmp load
nextent:
	txa
	and #0xF0			; back to this entry's base...
	clc
	adc #16				; ...and on to the next
	tax
	cpx #ENTPSEC*16
	bcc findnxt
	dec dirleft
	bne findsec
	; No EGATREK.XEX on the disk. Falls through to the same report a disk
	; error gets -- there is no screen driver yet and nowhere to print.

; A red screen and a halt. NOT a bare `jmp *`: a machine that hangs with the
; boot's own blue background is indistinguishable from a machine that hung
; somewhere else, and this port has mistaken a silent no-op for a pass more
; than once.
fail:
	lda #0x34
	sta COLBK
	jmp fail

; ---------------------------------------------------------------- load it
; The XEX format: 0xFFFF, then segments of "first address, last address,
; bytes". A repeated 0xFFFF between segments is legal and ignored.
load:
	lda #SECSIZE
	sta bufidx			; empty, so the first getb fills
	jsr getb			; 0xFF
	jsr getb			; 0xFF

segment:
	jsr getword
	lda tmp
	and tmp+1			; both 0xFF, and only both, gives 0xFF
	cmp #0xFF
	beq segment			; a marker, not an address
	lda tmp
	sta dst
	lda tmp+1
	sta dst+1
	jsr getword
	lda tmp
	sta endad
	lda tmp+1
	sta endad+1

copy:
	jsr getb
	ldy #0
	sta (dst),y
	lda dst
	cmp endad
	bne bump
	lda dst+1
	cmp endad+1
	beq segdone
bump:
	inc dst
	bne copy
	inc dst+1
	jmp copy

segdone:
	lda left			; the directory said how long the file
	ora left+1			; is; when it is spent, so are we
	bne segment
	jmp (RUNAD)

getword:
	jsr getb
	sta tmp
	jsr getb
	sta tmp+1
	rts

getb:
	ldy bufidx
	cpy #SECSIZE
	bcc gotsec
	jsr readsec
	inc secnum
	bne nowrap
	inc secnum+1
nowrap:
	ldy #0
gotsec:
	lda BUFFER,y
	iny
	sty bufidx
	pha				; the byte is the return value, so the
	lda left			; countdown has to step around it
	bne nolo
	dec left+1
nolo:
	dec left
	pla
	rts

; ---------------------------------------------------------------- the drive
readsec:
	lda #0x31			; disk
	sta DDEVIC
	lda #1				; D1:
	sta DUNIT
	lda #0x52			; 'R'
	sta DCOMND
	lda #0x40			; read into memory
	sta DSTATS
	lda #(BUFFER & 0xFF)
	sta DBUFLO
	lda #(BUFFER >> 8)
	sta DBUFHI
	lda #15
	sta DTIMLO
	lda #SECSIZE
	sta DBYTLO
	lda #0
	sta DBYTHI
	lda secnum
	sta DAUX1
	lda secnum+1
	sta DAUX2
	jsr SIOV
	lda DSTATS
	cmp #1				; 1 is the only success
	bne fail
	rts

fname:	.ascii "EGATREK XEX"		; as tools/nodos.py spells it: no dot,
					; the fields are fixed width

secnum:	.word 0
left:	.word 0
tmp:	.word 0
endad:	.word 0
bufidx:	.byte 0
dirleft:.byte 0
imgend:

; TWO THINGS BOUND THIS FILE'S LENGTH AND boot.ld ASSERTS BOTH: three sectors,
; which is what tools/nodos.py reserves before the directory, and the sector
; buffer at $0900, which the OS would otherwise load this image over -- and
; only sometimes, depending on what was in flight.

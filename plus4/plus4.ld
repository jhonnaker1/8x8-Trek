/* Scoping link script for the Plus/4. NOT the shipping layout -- this exists
   to get one probe onto the machine and answer the banking question below.

   THE QUESTION THIS PORT TURNS ON. On a C64 the KERNAL is $E000..$FFFF, so a
   program at $0801..$BFFF can call it without hiding a byte of itself; only
   the far store under it needs care, which is all c64mem.c does. On a Plus/4
   the ROMs are BASIC $8000..$BFFF and KERNAL $C000..$FCFF, and $FF3E/$FF3F
   switch BOTH AT ONCE. So a program filling the address space cannot call the
   KERNAL without hiding half of itself.

   Derived from c64.ld and commodore.ld. */

/* KERNAL ENTRY POINTS. Each PLATFORM defines these and there is no plus4
   platform, so they are stated here. The Plus/4 keeps the CBM KERNAL jump
   table at the same addresses as the C64 -- CHROUT $FFD2 -- which is a large
   part of why "the C64's KERNAL model comes free" is true for this machine.
   Verified as needed, not copied wholesale: only the ones the link actually
   asks for are defined, so a wrong guess shows up as an undefined symbol
   rather than as a jump into nothing. */
__CHROUT = 0xFFD2;

__basic_zp_start = 0x0002;
__basic_zp_end = 0x0090;

MEMORY {
    /* $1001 is where a Plus/4 PRG loads. Kept small for the probe; the real
       layout is decided by what the probe finds. */
    ram (rw) : ORIGIN = 0x1001, LENGTH = 0x4000
}

__rc0 = __basic_zp_start;
INCLUDE imag-regs.ld
__basic_zp_size = __basic_zp_end - __basic_zp_start;
MEMORY { zp : ORIGIN = __rc31 + 1, LENGTH = __basic_zp_end - (__rc31 + 1) }

/* NO basic-header.o -- it lives in each PLATFORM's lib and encodes that
   machine's SYS address, and there is no plus4 platform. The probe is started
   with SYS 4097 instead, so the crt0 entry sits at the load address. A
   shipping build wants a real header; see NOTES. */
REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", ram)

SECTIONS {
    /* KEEP, or --gc-sections drops it: nothing REFERENCES the header, the
       machine reads it. Without this the section vanished silently and the
       program started at crt0 with no BASIC line in front of it. */
    .basic_header : { KEEP(*(.basic_header)) } >ram
    INCLUDE c.ld
}

/* $7F00 for now -- below the ROM, so the probe's own stack is never hidden. */
__stack = 0x7F00;

OUTPUT_FORMAT {
    SHORT(ORIGIN(ram))
    TRIM(ram)
}

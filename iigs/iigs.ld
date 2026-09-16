/* Scoping link script for the Apple IIgs. NOT the shipping layout -- this
   exists to answer ONE question: does llvm-mos's mosw65816 produce code that
   runs on a real IIgs and can reach Super Hi-Res in bank $E1?

   THE QUESTION THIS PORT TURNS ON. llvm-mos generates 8-bit 6502-shaped code
   with the 65816's extra opcodes; it never emits rep/sep or long addressing
   on its own. But SHR pixels live at $E1/2000, palettes at $E1/9E00 and SCBs
   at $E1/9D00 -- all outside the 16-bit address space the compiler can see.
   So the whole video seam is hand-written long addressing, and the first
   thing to establish is that the assembler encodes it and the machine runs it.

   NO PRODOS IN THIS BUILD. The image is poked straight into bank 0 by the
   MAME rig and the PC is set to the entry, so zero page is entirely ours and
   $BF00 is not yet a global page. Both of those change the day this loads
   from a disk; see iigs/README.md.

   Derived from plus4.ld, which was derived from c64.ld. */

MEMORY {
    /* $2000..$BEFF. $2000 because that is where ProDOS 8 loads and enters a
       SYS file, which is the intended shipping shape -- choosing it now means
       the boot work later does not move the image. It stops at $BEFF because
       $BF00..$BFFF is the ProDOS global page. */
    ram (rw) : ORIGIN = 0x2000, LENGTH = 0x9F00
}

/* ZERO PAGE IS ENTIRELY OURS IN THIS BUILD AND WILL NOT BE LATER.
   $0002..$0090 is c64.ld's range, inherited through plus4.ld, and on the
   Plus/4 that inheritance went unexamined and became the port's open
   question. It is written down as a question here on day one instead:
   ProDOS 8's MLI uses zero page during a call and a SYS program does not own
   all of it. NOTHING HAS ESTABLISHED WHAT IS FREE -- this build has no ProDOS
   in it at all, so the range below is safe for exactly this rig and no more.
   Measure it before the first MLI call, not after a bug. */
__basic_zp_start = 0x0002;
__basic_zp_end   = 0x0090;

__rc0 = __basic_zp_start;
INCLUDE imag-regs.ld
__basic_zp_size = __basic_zp_end - __basic_zp_start;
MEMORY { zp : ORIGIN = __rc31 + 1, LENGTH = __basic_zp_end - (__rc31 + 1) }

REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", ram)

SECTIONS {
    /* A JMP AT $2000, for the reason plus4.ld's .entry exists: whatever
       section the linker happens to place first becomes the entry point
       otherwise, and on the Plus/4 that was hand-written banking assembly --
       which looked exactly like a program that had never been written to.
       Three bytes that say where the C runtime really is. */
    .entry : { BYTE(0x4C); SHORT(__gs_start); } >ram

    __gs_start = .;
    INCLUDE c.ld
}

/* THE SOFT STACK, AND IT IS NOT $BF00. It grows DOWN from here, and the
   linker checks neither how much room is below it nor that it is outside the
   program -- both halves of [[llvm-mos-soft-stack]], the second of which cost
   the Plus/4 an hour of blaming a BASIC header. verify_gs.py asserts it lies
   outside every loaded section. */
__stack = 0xBF00;

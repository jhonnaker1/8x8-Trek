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
    /* $0800..$AFFF. IT WAS $2000..$BEFF, WHICH WAS PRODOS'S ANSWER TO A
       QUESTION THIS PORT NO LONGER ASKS. $2000 is where ProDOS 8 loads and
       enters a SYS file, and $BF00 is its global page -- and there is no
       ProDOS on this disk. Our own block 0 decides where the image goes, so
       it goes as low as an Apple II program can: $0800, above the text pages
       and the input buffer. That is 6,144 bytes the convention was costing,
       for nothing.

       MEASURED, because the number is the reason: the all-resident image
       overflowed $2000..$BEFF by 16,098 bytes -- about 56,289 bytes of game
       against 40,191 of room. The Plus/4 overflowed by 16,497 on the same
       sources, which is the cross-check that the figure is the game's and not
       this port's. Six kilobytes does not close that, so the port takes the
       overlay split every other 8-bit target here takes. */
    ram (rw) : ORIGIN = 0x0800, LENGTH = 0xB800

    /* THE OVERLAY WINDOW, IN THE LANGUAGE CARD, and that is worth 4,096
       bytes of resident space rather than being tidy.

       The C64 puts its window at $C000 because that is the 4K its $01 never
       covers. On a IIgs $C000..$CFFF is I/O in every configuration, so the
       window was at $B000 -- the top of RAM -- and every byte of it came out
       of the program's own region. $D000..$DFFF is RAM once the language card
       soft switches say so (measured: src/gsram.c writes and reads both
       $D000 banks and proves them separate), it is OUTSIDE `ram`, and code
       there executes with ordinary sixteen-bit calls because it is still
       bank 0. So `ram` grows from $0800..$AFFF to $0800..$BFFF and the window
       costs the program nothing at all.

       src/boot.S switches the card in before it jumps, so the window is RAM
       from the first instruction the game runs.

       Exactly 4K, so an overlay that outgrows it is a LINK ERROR rather than
       something that quietly overwrites resident code. */
    window (rwx) : ORIGIN = 0xD000, LENGTH = 0x1000

    /* ONE STAGING REGION PER OVERLAY. A single shared AT> region does not
       advance its pointer when the section also carries an explicit run
       address, so every overlay would be dumped from the same bytes of the
       ELF and eleven images would be written out identical. That happened on
       the C128 and the game ran the hall of fame when it asked for the
       evaluation. */
    ovlimg0 (rw) : ORIGIN = 0x10000, LENGTH = 0x1000
    ovlimg1 (rw) : ORIGIN = 0x11000, LENGTH = 0x1000
    ovlimg2 (rw) : ORIGIN = 0x12000, LENGTH = 0x1000
    ovlimg3 (rw) : ORIGIN = 0x13000, LENGTH = 0x1000
    ovlimg4 (rw) : ORIGIN = 0x14000, LENGTH = 0x1000
    ovlimg5 (rw) : ORIGIN = 0x15000, LENGTH = 0x1000
    ovlimg6 (rw) : ORIGIN = 0x16000, LENGTH = 0x1000
    ovlimg7 (rw) : ORIGIN = 0x17000, LENGTH = 0x1000
    ovlimg8 (rw) : ORIGIN = 0x18000, LENGTH = 0x1000
    ovlimg9 (rw) : ORIGIN = 0x19000, LENGTH = 0x1000
    ovlimg10 (rw): ORIGIN = 0x1a000, LENGTH = 0x1000
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

    .ovl_eval   ORIGIN(window) : { *(.ovl.eval)   } >window AT>ovlimg0
    .ovl_hof    ORIGIN(window) : { *(.ovl.hof)    } >window AT>ovlimg1
    .ovl_front  ORIGIN(window) : { *(.ovl.front)  } >window AT>ovlimg2
    .ovl_info   ORIGIN(window) : { *(.ovl.info)   } >window AT>ovlimg3
    .ovl_repair ORIGIN(window) : { *(.ovl.repair) } >window AT>ovlimg4
    .ovl_msgs   ORIGIN(window) : { *(.ovl.msgs)   } >window AT>ovlimg5
    .ovl_planet ORIGIN(window) : { *(.ovl.planet) } >window AT>ovlimg6
    .ovl_cmds   ORIGIN(window) : { *(.ovl.cmds)   } >window AT>ovlimg7
    .ovl_title  ORIGIN(window) : { *(.ovl.title)  } >window AT>ovlimg8
    .ovl_events ORIGIN(window) : { *(.ovl.events) } >window AT>ovlimg9
    .ovl_xtra   ORIGIN(window) : { *(.ovl.xtra)   } >window AT>ovlimg10
}

/* overlay.c takes the window address from here rather than repeating $B000. */
__ovl_start = ORIGIN(window);

/* THE SOFT STACK, GROWING DOWN FROM THE WINDOW. It grows DOWN from here, and the
   linker checks neither how much room is below it nor that it is outside the
   program -- both halves of [[llvm-mos-soft-stack]], the second of which cost
   the Plus/4 an hour of blaming a BASIC header. verify_gs.py asserts it lies
   outside every loaded section. */
/* $C000, NOT ORIGIN(window), AND THE DIFFERENCE IS HARDWARE. The stack grows
   DOWN from here; with the window at $D000 it would descend through
   $CFFF..$C000, which is I/O in every configuration -- so a deep enough call
   chain would not corrupt memory, it would hit slot registers. $C000 is the
   top of `ram` and the stack grows down into the gap above the image. */
__stack = 0xC000;

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
   table at the same addresses as the C64, which is a large part of why "the
   C64's KERNAL model comes free" is true for this machine.

   ASSERTED, NOT YET PROVED. These are the documented CBM vectors and the
   link needs them to resolve, but a linker script cannot tell a right address
   from a plausible one -- a wrong entry here is a jump into the middle of a
   routine, not an error. What proves them is the port LOADING A FILE, which
   exercises SETLFS, SETNAM and LOAD together; until then treat this block as
   the most likely thing to be wrong. */
__ACPTR  = 0xFFA5;  __CIOUT  = 0xFFA8;  __UNTLK  = 0xFFAB;  __UNLSN  = 0xFFAE;
__LISTEN = 0xFFB1;  __TALK   = 0xFFB4;  __READST = 0xFFB7;  __SETLFS = 0xFFBA;
__SETNAM = 0xFFBD;  __OPEN   = 0xFFC0;  __CLOSE  = 0xFFC3;  __CHKIN  = 0xFFC6;
__CKOUT  = 0xFFC9;  __CLRCHN = 0xFFCC;  __CLRCH  = 0xFFCC;  __CHRIN  = 0xFFCF;  __CHROUT = 0xFFD2;  __BSOUT  = 0xFFD2;
__LOAD   = 0xFFD5;  __SAVE   = 0xFFD8;  __SETTIM = 0xFFDB;  __RDTIM  = 0xFFDE;
__STOP   = 0xFFE1;  __GETIN  = 0xFFE4;  __CLALL  = 0xFFE7;  __SCNKEY = 0xFF9F;
__PLOT   = 0xFFF0;

__basic_zp_start = 0x0002;
__basic_zp_end = 0x0090;

MEMORY {
    /* $1001..$DCFF. THE PROGRAM SPANS $8000 AND THAT IS FORCED, not chosen:
       $1001..$7FFF is 28,671 bytes and the resident image is about 40K. Above
       $8000 the ROM hides it -- which costs nothing, because the ROM is only
       ever banked in around a KERNAL call and the only code executing then is
       .lowtext below.
       It stops at $DCFF because $DD00..$FCFF is the far store (p4mem.c owns
       it, the linker must not) and $FD00..$FF3F is I/O whatever is banked. */
    ram (rw) : ORIGIN = 0x1001, LENGTH = 0xCBFF

    /* THE OVERLAY WINDOW, AND IT IS ABOVE $8000 ON PURPOSE. The C64's window
       is at $C000 because that is the 4K nothing in its $01 ever covers; here
       every address above $8000 is covered by ROM during a KERNAL call, so
       there is no such place and the question is whether it matters. It does
       not: overlay code CAN call the storage layer, but it is not executing
       while the ROM is in -- .lowtext is -- and its return address becomes
       visible again the moment the ROM goes out. The same argument that lets
       the resident image span $8000 lets the window sit up here.
       Exactly 4K, so an overlay that outgrows it is a LINK ERROR rather than
       something that quietly overwrites resident code. */
    window (rwx) : ORIGIN = 0xCC00, LENGTH = 0x1000

    /* ONE STAGING REGION PER OVERLAY. A single shared AT> region does not
       advance its pointer when the section also has an explicit run address,
       so every overlay would be dumped from the same bytes of the ELF and
       eleven images would be written out identical. That happened on the C128
       and the game ran the hall of fame when it asked for the evaluation. */
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

    /* .lowtext FIRST, WHICH IS HOW IT IS GUARANTEED LOW. `ram` grows upward
       from $1001, so anything placed before the general code cannot land
       above $8000 -- and everything in here must stay visible while the ROM
       is banked in, because that is where the KERNAL calls are made from and
       the addresses they return to.
       Placed by ATTRIBUTE, not by object file: this build uses LTO, so every
       translation unit becomes one `.lto.o` and `*p4mem.o(.text*)` would
       match nothing. verify_p4.py asserts the section really did end below
       $8000 rather than trusting the ordering. */
    /* A JMP AT THE SYS ADDRESS, AND IT IS NOT DECORATION. The BASIC header
       sends RUN to $100D, which is whatever section follows it -- and the
       moment .lowtext was placed there, RUN jumped into the middle of the
       KERNAL banking assembly and the program never started. It looked
       exactly like a probe that had not been written to.
       So the entry is three bytes that say where the C runtime really is,
       and .lowtext keeps its low placement without owning the entry point.
       verify_p4.py checks this cell is a $4C and that the address after it
       matches the runtime, because the old check -- "the SYS number equals
       the load address plus the header length" -- was arithmetic that stayed
       true while pointing at the wrong code. */
    .entry : { BYTE(0x4C); SHORT(__p4_start); } >ram

    .lowtext : { *(.lowtext) } >ram
    .lowbss  : { *(.lowbss)  } >ram

    /* THE SOFT STACK, AND THE LINKER OWNS IT. Every previous home for it was
       an ADDRESS SOMEBODY BELIEVED WAS FREE -- $7F00 (inside .text), $CC00
       (under the ROM, so reads returned ROM), then $0500 growing down into
       $0400. The last one was measured, and the measurement asked the wrong
       question: src/lowfree.c counts which blocks still hold a fill, so it
       sees WRITES. $0400..$04FF is never written while this program runs --
       it is READ AND EXECUTED, by the ROM:

           $0473-$0478  CHRGET      $0494-$04A1  INDSUB  "shared ROM fetch sub"
           $0479-$0484  CHRGOT      $04A5-$04AF  INDTXT
           $0485-$0493  QNUM        $04B0-$04E6  INDIN1/INDIN2/INDST1/
                                                 INDLOW/INDFMO

       That is this machine's OWN banking machinery: because the ROM can be
       switched out, Commodore put the indirect-fetch subroutines in RAM so
       ROM code can reach RAM underneath it. Two independent memory maps agree
       (floodgap's 264memory.txt and Butterfield's). The port had the soft
       stack growing down through all of it, and the CPU ended up executing
       zero page -- BREAK at PC $000E.

       SO STOP NOMINATING ADDRESSES. A reserved section cannot be wrong about
       what is free: nothing else can be here by construction, it is below
       $8000 so it is never ROM-shadowed, and it moves with the program
       instead of having to be re-measured every time the layout shifts.
       2048 BYTES, AND THE SIZE IS MEASURED NOW RATHER THAN BORROWED. It was
       512, on the 143-184 bytes other ports measure on their deepest path --
       and the game reached BASIC's monitor with BREAK at PC $0BFA, which is
       COLOUR RAM: the stack had underflowed past $1200, down through .lowbss,
       .lowtext, the BASIC header and the screen. That only started happening
       once STRINGS.DAT actually loaded, because until then the pool was empty
       and the deep parse-and-draw path was never taken.
       p4bank.c fills this region with $A5 before main(), so the high-water
       mark can be read straight out of memory. Keep the size a whole number
       of pages: that fill is eight unrolled stores. */
    .stack (NOLOAD) : {
        . = ALIGN(256);
        __stack_bottom = .;
        . += 0x0800;                 /* EIGHT PAGES -- see p4bank.c's fill */
        __stack = .;
    } >ram

    __p4_start = .;
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

/* overlay.c takes the window address from here rather than repeating $CC00. */
__ovl_start = ORIGIN(window);

/* __stack is defined by the .stack section above, not nominated here. */

/* NO OUTPUT_FORMAT, AND THAT IS DELIBERATE. The platform templates end with a
   SHORT/TRIM block that makes the linker emit a finished PRG -- convenient,
   and it leaves NO ELF for llvm-objcopy to cut the overlay images out of.
   c64.ld has none for the same reason. The link produces an ELF; the Makefile
   strips it to a body, prepends the $1001 load address, and dumps each
   .ovl_* section separately. */

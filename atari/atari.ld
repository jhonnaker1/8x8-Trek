/* Atari 800XL + VBXE. After the stock atari8-dos link.ld, with one change
 * that is the whole reason this file exists.
 *
 * THE PROGRAM STARTS AT $3000, BECAUSE VBXE'S WINDOW IS IN THE WAY.
 * MEMAC maps VBXE's VRAM INTO the 6502 address space. commodore-uno opens it
 * with MEMAC_CONTROL = 0x29 -- "window at $2000, CPU enable, 8K" -- which puts
 * $2000..$3FFF out of reach; this port narrows it to 4K ($28), so only
 * $2000..$2FFF belongs to video memory and the program starts at $3000. That
 * narrowing was the first lever this port spent and it is worth 4,096 bytes;
 * see src/vbxevid.c.
 *
 * (This comment said $4000 until 2026-09-11, describing uno's 8K window after
 * this port had already narrowed it. The MEMORY block below has said $3000 all
 * along -- a header and the code under it disagreeing, which is the shape to
 * watch for.)
 *
 * FOUND BY CRASHING INTO IT, in uno rather than here: a build that loaded at
 * $2000 had vbxe_init() map the window over its own code, and execution fell
 * into VRAM. PC=$25ED, illegal opcode. The emulator's Program Error dialog is
 * what reported it, which is a long way from where the mistake was.
 *
 * WHAT IT COSTS: $4000..$BFFF is 32,768 bytes, about 4.8K LESS than the C128,
 * which holds this same game in 37,823 with 211 spare. That makes this the
 * tightest target in the project and it is why the FIRST thing built here was
 * a measurement rather than a driver -- see tools/budget.py.
 *
 * $C000 up is the XL's OS ROM. BASIC at $A000 must be disabled, which is what
 * holding OPTION at boot does and what the emulator profile already sets.
 */
__rc0 = 0x80;
INCLUDE imag-regs.ld
ASSERT(__rc31 == 0x9f, "Inconsistent zero page map.")

/* THE OVERLAY WINDOW, and on this machine it is not optional. The whole game
   is about 61,000 bytes of 6502 code -- measured, by linking it with every
   seam stubbed and reading the overflow -- against 32,768 bytes of address
   space. So roughly half of it has to be paged, where the C128 pages about a
   third. Same machinery as the other three 6502 ports: one window, one
   staging region per overlay, and images cut from the same link. */
__ovl_size = 0x1200;

/* The soft stack's reserve, just below the window -- see the MEMORY block. */
__stack_reserve = 0x100;

MEMORY {
    zp : ORIGIN = __rc31 + 1, LENGTH = 0x100 - (__rc31 + 1)

    /* THE OS SPARE AREA. $0480..$06FF is 640 bytes that the OS itself does
       not use -- measured intact across a whole-file read, two hundred
       streamed reads and a write, back when DOS was still resident above it.
       Nothing here is zeroed at startup, so what goes in it must be
       write-before-read. See core/lowmem.h.

       IT IS NO LONGER THE ONLY LOW MEMORY THIS PORT HAS -- see lowdata below
       -- but it is still the only region under $0700, which is where the
       boot record loads. io_buf lives here and can stay. */
    lowram (rw) : ORIGIN = 0x0480, LENGTH = 0x0280

    /* WHAT DOS USED TO SIT ON, minus what the boot record needs.
       $0700..$1FFF is free on this machine the moment no DOS is loaded, and
       that is 6,400 bytes that were never competing with code -- see
       tools/nodos.py and src/boot.s. The writable half of the program moves
       down here and gives every byte of it back to $3000 for CODE, which is
       the only pool this port has ever been short of.

       IT STARTS AT $0A00, NOT $0700, AND THAT IS NOT ROUNDING. The boot
       record is loaded to $0700 and its sector buffer sits at $0900; both
       are still LIVE while the program's segments are being read in. A
       .data section placed at $0700 would be loaded over the loader that is
       loading it. $0A00 is the first address past the buffer. */
    lowdata (rw) : ORIGIN = 0x0A00, LENGTH = 0x2000 - 0x0A00

    /* THE SOFT STACK GETS ITS OWN 256 BYTES, AND THIS IS THE C128'S BUG
       ARRIVING ON A SECOND MACHINE.

       llvm-mos's _start sets the soft stack to MEMTOP+1 ($02E5) and grows it
       DOWN. On this port MEMTOP is $BC1F -- so the stack started at $BC20,
       INSIDE the overlay window, and every local written there landed on live
       overlay code. MEASURED 2026-09-10: ui_setup()'s `char fname[18]` sat at
       $BBE8, read_field() wrote the save filename onto it through ($E5),Y, the
       running ovl_front image was corrupted under its own feet, execution
       reached the string and hit $52 -- kil. That JAM stopped the CPU, which
       is why a restore "never came back" and why the bridge's frame gate
       never released.

       c128/trek128.ld has carried the warning since v0.9.0: "the stock script
       lets a build succeed and then corrupt itself, with the linker none the
       wiser". It even names the X16 as carrying the same hazard. Nobody
       checked the Atari, which had NO guard at all.

       DEPTH IS MEASURED, NOT GUESSED -- the C128's first guess of 64 was
       overrun by 79. Sampling $80/$81 across title, setup, console, SAVE,
       chart and a warp put the deepest at 28 bytes below the top. 256 is an
       order of magnitude over that, and it costs 256 of the 693 bytes that
       were free below the window. See tools/probe_stack.py; re-run it rather
       than reasoning if a deeper path is ever found. */
    /* $3000 up to the stack reserve, which sits just below the window. */
    ram (rw) : ORIGIN = 0x3000, LENGTH = 0x9000 - __ovl_size - __stack_reserve

    /* The window sits at the TOP of the region, below the OS ROM at $C000. */
    window (rwx) : ORIGIN = 0x3000 + 0x9000 - __ovl_size, LENGTH = __ovl_size

    /* Staging: each overlay is linked to RUN at the window and to LOAD at its
       own address, so one link produces every image. Addresses above any real
       memory, exactly as the other three ports do it. */
    ovlimg0 (rw) : ORIGIN = 0x100000, LENGTH = __ovl_size
    ovlimg1 (rw) : ORIGIN = 0x102000, LENGTH = __ovl_size
    ovlimg2 (rw) : ORIGIN = 0x104000, LENGTH = __ovl_size
    ovlimg3 (rw) : ORIGIN = 0x106000, LENGTH = __ovl_size
    ovlimg4 (rw) : ORIGIN = 0x108000, LENGTH = __ovl_size
    ovlimg5 (rw) : ORIGIN = 0x10a000, LENGTH = __ovl_size
    ovlimg6 (rw) : ORIGIN = 0x10c000, LENGTH = __ovl_size
    ovlimg7 (rw) : ORIGIN = 0x10e000, LENGTH = __ovl_size
    ovlimg8 (rw) : ORIGIN = 0x110000, LENGTH = __ovl_size
    ovlimg9 (rw) : ORIGIN = 0x112000, LENGTH = __ovl_size
    ovlimg10 (rw): ORIGIN = 0x114000, LENGTH = __ovl_size
    /* THE TWO OPT-IN WINDOWS -- see core/overlay.h. The Makefile
       defines TREK_OVL_ENEMY and TREK_OVL_MOVE; on a port that does
       not, these sections are empty and cost nothing. */
    ovlimg11 (rw): ORIGIN = 0x116000, LENGTH = __ovl_size
    ovlimg12 (rw): ORIGIN = 0x118000, LENGTH = __ovl_size
}

REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", lowdata)

SECTIONS {
    .lowbss (NOLOAD) : { *(.lowbss) } >lowram

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
    .ovl_enemy  ORIGIN(window) : { *(.ovl.enemy)  } >window AT>ovlimg11
    .ovl_move   ORIGIN(window) : { *(.ovl.move)   } >window AT>ovlimg12

    /* c.ld IS EXPANDED HERE rather than included, for one line of it.
       READ-ONLY DATA GOES LOW TOO. .rodata is 2,161 bytes of tables and
       literals that never compete for anything except address space, and
       address space below $2000 is what dropping DOS bought. Moving it out
       of $3000 is 2,161 more bytes of CODE on the tightest target in the
       project -- which had 218 free without this.

       It is placed BEFORE .data so it sits at the bottom of the region, and
       so OUTPUT_FORMAT's second segment can be `$0A00 .. __rodata_end`: the
       only part of lowdata with contents to load. .bss and .noinit are
       NOLOAD and .data is loaded at its LMA, up in $3000 with the code. */
    INCLUDE zp.ld
    INCLUDE text.ld
    .rodata : { INCLUDE rodata-sections.ld } >lowdata
    __rodata_end = .;
    INCLUDE data.ld
    INCLUDE bss.ld
    INCLUDE noinit.ld
}

__ovl_start = ORIGIN(window);

OUTPUT_FORMAT {
    SHORT(0xffff)    /* XEX magic number. */

    /* LOWER MEMTOP BEFORE _start EVER READS IT. An XEX segment is just
       "start, end, bytes", so two bytes loaded at $02E5 set the OS's
       top-of-free-RAM at LOAD time -- no code, no INITAD, and it happens
       before the run vector is taken. _start then computes MEMTOP+1 and the
       soft stack begins at the window base, growing DOWN into the reserve
       above instead of into the overlay window. */
    SHORT(0x02e5)                 /* first byte: MEMTOP low  */
    SHORT(0x02e6)                 /* last byte:  MEMTOP high */
    SHORT(ORIGIN(window) - 1)     /* = $ADFF, so SP starts at the window base */

    SHORT(0x02e0)    /* First byte of Run vector. */
    SHORT(0x02e1)    /* Last byte of Run vector. */
    SHORT(_start)    /* Segment to be loaded to Run vector. */
    /* THE LOW SEGMENT: read-only data, in the space DOS used to occupy.
       Loaded before the main one only because that is the order written; the
       two do not overlap and neither depends on the other. */
    SHORT(0x0a00)
    SHORT(__rodata_end - 1)
    TRIM(lowdata)

    SHORT(0x3000)    /* Where the main segment loads -- see the note above. */

    /* THE LAST BYTE IS THE END OF .data's LOAD ADDRESS, NOT OF .data.
       The stock script writes `--__data_end - 1`, which is the end of .data's
       VMA -- correct only while c_writeable and c_readonly are the same
       region, as they were here until the writable half moved to $0A00.
       After that split __data_end is $0A08 and the XEX claimed a segment
       running DOWNWARD from $3000, which no loader can read and which the
       link reported as success. LOADADDR is where the bytes actually are. */
    SHORT(__data_load_start + __data_size - 1)
    TRIM(ram)
}

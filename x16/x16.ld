/* Commander X16 with a code-overlay window, after c128/trek128.ld.
 *
 * WHY THE WINDOW IS IN LOW RAM AND THE IMAGES ARE NOT. The C128 loads each
 * overlay straight off disk into its window. This port keeps the images in
 * BANKED RAM instead and copies them in, the way the MEGA65 does -- the X16
 * has 512K of it and a swap becomes a memcpy rather than a disk load. But the
 * banked window IS $A000..$BFFF, which is how far memory reaches those images,
 * so the overlay window cannot live there too. It sits at the top of low RAM
 * instead, immediately below the soft stack.
 *
 * TEN STAGING REGIONS, one per overlay, exactly as trek128.ld has them and for
 * the same reason: lld does not advance a region's pointer for a section that
 * carries an explicit run address, so sharing one staging region gives every
 * overlay the same LOAD address, and objcopy then dumps the same bytes ten
 * times under ten names. That shipped on the C128 for ten silent minutes.
 * These addresses are positions in the OUTPUT FILE, not in the machine.
 */

__basic_zp_start = 0x0002;
__basic_zp_end = 0x0080;

/* 4K, matching the C128's and the MEGA65's, so an overlay that fits one fits
   all three and the budget arithmetic transfers. */
/* 3776, MEASURED against this port's own images rather than the C128's. The
   window costs resident space 1:1 and the largest image here is ovlmsgs at
   3,766 bytes, so anything above 3,776 is slack. (4,096 was the C128's number
   and 4,032 was a guess from its largest image; both were wrong for this port
   in the same direction.) MUST MATCH OVL_WINDOW in the Makefile,
   which passes it to the loader and enforces it when cutting images: an
   overlay that outgrows this fails the build with its own name. */
__ovl_size = 0x0ec0;

MEMORY {
    /* GOLDEN RAM, $0400..$07FF. The X16 reserves this kilobyte for
       applications and the stock linker script does not claim it, so it is a
       free kilobyte that costs the main region nothing. The two biggest
       buffers go here, which is what closes the last 813 bytes.

       NOTE the C128 could not do this: its far store is bank 1 through
       FETCH/STASH, not directly addressable, which is why "io_buf cannot move
       to far memory" is recorded for that port. This is not far memory -- it
       is ordinary RAM at a fixed address, and the objection does not carry. */
    golden (rw)  : ORIGIN = 0x0400, LENGTH = 0x0400

    /* $0801 up to the window. The stock script gives ram LENGTH 0x96ff ending
       at $9F00 where __stack begins; the window takes the last 4K of that. */
    ram    (rw)  : ORIGIN = 0x0801, LENGTH = 0x96ff - 0x0ec0

    /* Runs here. Every overlay shares this address -- that is the whole idea. */
    window (rwx) : ORIGIN = 0x0801 + 0x96ff - 0x0ec0, LENGTH = 0x0ec0

    ovlimg0 (rw) : ORIGIN = 0x100000, LENGTH = 0x0ec0
    ovlimg1 (rw) : ORIGIN = 0x101000, LENGTH = 0x0ec0
    ovlimg2 (rw) : ORIGIN = 0x102000, LENGTH = 0x0ec0
    ovlimg3 (rw) : ORIGIN = 0x103000, LENGTH = 0x0ec0
    ovlimg4 (rw) : ORIGIN = 0x104000, LENGTH = 0x0ec0
    ovlimg5 (rw) : ORIGIN = 0x105000, LENGTH = 0x0ec0
    ovlimg6 (rw) : ORIGIN = 0x106000, LENGTH = 0x0ec0
    ovlimg7 (rw) : ORIGIN = 0x107000, LENGTH = 0x0ec0
    ovlimg8 (rw) : ORIGIN = 0x108000, LENGTH = 0x0ec0
    ovlimg9 (rw) : ORIGIN = 0x109000, LENGTH = 0x0ec0
    ovlimg10 (rw): ORIGIN = 0x10a000, LENGTH = 0x0ec0
}

SECTIONS {
    /* PLACED BY SECTION NAME, so no shared source is touched. Each object
       gets its own .bss.<name>, so the two biggest buffers can be moved from
       the X16's linker script alone -- ui.c and strpool.c stay identical for
       the C128 and MEGA65.

       ZERO-INIT: these leave the range the CRT's __do_zero_bss covers, so
       x16vera.c's vdc_init() clears all of golden RAM before anything uses
       it. Skip that and `hof` starts as garbage rather than an empty table. */
    .golden (NOLOAD) : {
        *(.bss.io_buf)      /* 626 -- the save/load scratch buffer   */
        *(.bss.hof)         /* 280 -- the hall-of-fame table         */
        *(.bss.planets)     /*  95 -- the planet table               */
        *(.bss.cmd)         /*  16 -- the command-word buffer         */
        *(.bss.inventory)   /*   5 -- the landing-party inventory     */
                            /* 1022 of 1024 used, and the names above are
                               the ones LTO actually emits -- an earlier
                               attempt listed .bss.view_dept, which does not
                               survive LTO under that name and silently did
                               nothing. Read the MAP, do not guess a symbol. */
    } > golden

    /* EXPLICITLY >ram, and it must come after .golden. A section with no
       region named inherits the LAST one used, so with .golden declared above
       and nothing between, the BASIC header landed in golden RAM and overflowed
       it by 10 bytes. trek128.ld places this section explicitly too. */
    .basic_header : { *(.basic_header) } >ram

    /* ORIGIN(window) is written out rather than left implicit so that the run
       address is stated at every one of these, not inferred. */
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

__ovl_start = ORIGIN(window);

INCLUDE commodore.ld

/* BELOW THE WINDOW, NOT ABOVE IT. The stock script puts __stack at $9F00, the
   top of the whole region -- but the overlay window now occupies the last 4K,
   so a stack growing down from $9F00 grows straight through the code it is
   about to run. mega65.ld carries the same warning after that port hit it for
   real: "__stack is $D000 and grows DOWN, so a window at $C000..$CFFF is
   exactly where the stack lives. The screen went purple and stayed there." */
__stack = 0x0801 + 0x96ff - 0x0ec0;

/* NO OUTPUT_FORMAT BLOCK, DELIBERATELY. The stock cx16 script ends with
   SHORT(ORIGIN(ram)) + TRIM(ram), which emits a raw PRG -- and then there is
   no ELF for objcopy to cut the ten window images out of, which shows up as
   "the file was not recognized as a valid object file" on a link that
   actually succeeded. mega65.ld drops it for the same reason; its Makefile
   says "THE OBJCOPY STEP IS NOT OPTIONAL". The Makefile prepends the $0801
   load address itself. */

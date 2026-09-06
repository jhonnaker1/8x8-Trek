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
__ovl_size = 0x1000;

MEMORY {
    /* $0801 up to the window. The stock script gives ram LENGTH 0x96ff ending
       at $9F00 where __stack begins; the window takes the last 4K of that. */
    ram    (rw)  : ORIGIN = 0x0801, LENGTH = 0x96ff - 0x1000

    /* Runs here. Every overlay shares this address -- that is the whole idea. */
    window (rwx) : ORIGIN = 0x0801 + 0x96ff - 0x1000, LENGTH = 0x1000

    ovlimg0 (rw) : ORIGIN = 0x100000, LENGTH = 0x1000
    ovlimg1 (rw) : ORIGIN = 0x101000, LENGTH = 0x1000
    ovlimg2 (rw) : ORIGIN = 0x102000, LENGTH = 0x1000
    ovlimg3 (rw) : ORIGIN = 0x103000, LENGTH = 0x1000
    ovlimg4 (rw) : ORIGIN = 0x104000, LENGTH = 0x1000
    ovlimg5 (rw) : ORIGIN = 0x105000, LENGTH = 0x1000
    ovlimg6 (rw) : ORIGIN = 0x106000, LENGTH = 0x1000
    ovlimg7 (rw) : ORIGIN = 0x107000, LENGTH = 0x1000
    ovlimg8 (rw) : ORIGIN = 0x108000, LENGTH = 0x1000
    ovlimg9 (rw) : ORIGIN = 0x109000, LENGTH = 0x1000
}

SECTIONS {
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
}

__ovl_start = ORIGIN(window);

INCLUDE commodore.ld

__stack = 0x9f00;

OUTPUT_FORMAT {
    SHORT(ORIGIN(ram))
    TRIM(ram)
}

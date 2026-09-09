/* Atari 800XL + VBXE. After the stock atari8-dos link.ld, with one change
 * that is the whole reason this file exists.
 *
 * THE PROGRAM STARTS AT $4000, NOT $2000, BECAUSE VBXE'S WINDOW IS IN THE WAY.
 * MEMAC maps VBXE's VRAM INTO the 6502 address space -- commodore-uno's
 * vbxevid.c opens it with MEMAC_CONTROL = 0x29, "window at $2000, CPU enable,
 * 8K" -- so $2000..$3FFF belongs to video memory and not to this program.
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

MEMORY {
    zp : ORIGIN = __rc31 + 1, LENGTH = 0x100 - (__rc31 + 1)

    /* $4000 up to the window. */
    ram (rw) : ORIGIN = 0x3000, LENGTH = 0x9000 - __ovl_size

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
}

REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", ram)

SECTIONS {
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

    INCLUDE c.ld
}

__ovl_start = ORIGIN(window);

OUTPUT_FORMAT {
    SHORT(0xffff)    /* XEX magic number. */
    SHORT(0x02e0)    /* First byte of Run vector. */
    SHORT(0x02e1)    /* Last byte of Run vector. */
    SHORT(_start)    /* Segment to be loaded to Run vector. */
    SHORT(0x3000)    /* Where the main segment loads -- see the note above. */
    SHORT(--__data_end - 1)
    TRIM(ram)
}

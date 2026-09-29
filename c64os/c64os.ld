/* EGA Trek as a C64 OS application, linked by llvm-mos.
 *
 * THE CONTRACT, from the SDK (//os/s/app.s in C64 OS 1.09): a PRG whose load
 * address is $0900, five vectors in its first ten bytes, and a KERNAL link
 * table C64 OS rewrites into JMPs -- src/app.s supplies all three.
 *
 * EVERYTHING IS INSIDE THE LOADED FILE: .bss, .noinit, the overlay window and
 * the soft stack. C64 OS allocates the pages an application's FILE covers and
 * no more (commodore-uno measured it), so a NOLOAD section after the image
 * would sit in memory C64 OS may hand to something else. They are loaded as
 * zeros -- the Makefile's objcopy fills every gap up to the stack's last
 * byte -- which also stands in for crt0's zero-bss, which -nostartfiles drops.
 *
 * THE CEILING IS $7A00. The probe read the page map from inside a running app
 * on 2026-09-27: $0A00-$79FF free, a system page at $7A00. A file that grows
 * past it is a LINK ERROR here rather than an app that loads over C64 OS.
 *
 * THE OVERLAYS are c64reu's twenty, run from a 4K window placed right after
 * the resident image, and swapped from the REU by ../c64reu/src/reuovl.s
 * through the thunks tools' reu_thunks.py patches in. Each is staged at its
 * own address above 64K so the Makefile can cut it out -- see c64reu.ld for
 * why one staging region per overlay.
 */

/* The imaginary registers at $4E-$6D: BASIC's float workspace, which nothing
 * on C64 OS's IRQ path touches. src/app.s swaps them with C64 OS's at every
 * crossing and must agree on the address. */
__rc0 = 0x4E;
INCLUDE imag-regs.ld

MEMORY {
    /* No zero page beyond the registers: a static placed here would be
       swapped out along with them. */
    zp  : ORIGIN = __rc31 + 1, LENGTH = 0
    ram : ORIGIN = 0x0900, LENGTH = 0x7A00 - 0x0900

    ovlimg0  (rw) : ORIGIN = 0x10000, LENGTH = 0x1000
    ovlimg1  (rw) : ORIGIN = 0x11000, LENGTH = 0x1000
    ovlimg2  (rw) : ORIGIN = 0x12000, LENGTH = 0x1000
    ovlimg3  (rw) : ORIGIN = 0x13000, LENGTH = 0x1000
    ovlimg4  (rw) : ORIGIN = 0x14000, LENGTH = 0x1000
    ovlimg5  (rw) : ORIGIN = 0x15000, LENGTH = 0x1000
    ovlimg6  (rw) : ORIGIN = 0x16000, LENGTH = 0x1000
    ovlimg7  (rw) : ORIGIN = 0x17000, LENGTH = 0x1000
    ovlimg8  (rw) : ORIGIN = 0x18000, LENGTH = 0x1000
    ovlimg9  (rw) : ORIGIN = 0x19000, LENGTH = 0x1000
    ovlimg10 (rw) : ORIGIN = 0x1a000, LENGTH = 0x1000
    ovlimg11 (rw) : ORIGIN = 0x1b000, LENGTH = 0x1000
    ovlimg12 (rw) : ORIGIN = 0x1c000, LENGTH = 0x1000
    ovlimg13 (rw) : ORIGIN = 0x1d000, LENGTH = 0x1000
    ovlimg14 (rw) : ORIGIN = 0x1e000, LENGTH = 0x1000
    ovlimg15 (rw) : ORIGIN = 0x1f000, LENGTH = 0x1000
    ovlimg16 (rw) : ORIGIN = 0x20000, LENGTH = 0x1000
    ovlimg17 (rw) : ORIGIN = 0x21000, LENGTH = 0x1000
    ovlimg18 (rw) : ORIGIN = 0x22000, LENGTH = 0x1000
    ovlimg19 (rw) : ORIGIN = 0x23000, LENGTH = 0x1000
    ovlimg20 (rw) : ORIGIN = 0x24000, LENGTH = 0x1000
    ovlimg21 (rw) : ORIGIN = 0x25000, LENGTH = 0x1000
    ovlimg22 (rw) : ORIGIN = 0x26000, LENGTH = 0x1000
}

REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", ram)

SECTIONS {
    .app_header : { KEEP(*(.app_header)) } >ram

    INCLUDE c.ld

    /* THE WINDOW, page-aligned after everything resident. */
    .window (NOLOAD) : ALIGN(0x100) {
        __ovl_start = .;
        . += 0x1000;
    } >ram

    /* The soft stack: 256 bytes, the C64 port's guard against a measured
       demand of 144. Its top byte is the one real byte that makes the file
       run this far -- the stack grows down from __stack, so the zero it holds
       at load is simply the first byte the stack will overwrite. */
    .stack : {
        __stack_bottom = .;
        . += 255;
        BYTE(0)
        __stack = .;
    } >ram

    .ovl_eval   __ovl_start : { *(.ovl.eval)   } AT>ovlimg0
    .ovl_hof    __ovl_start : { *(.ovl.hof)    } AT>ovlimg1
    .ovl_front  __ovl_start : { *(.ovl.front)  } AT>ovlimg2
    .ovl_info   __ovl_start : { *(.ovl.info)   } AT>ovlimg3
    .ovl_repair __ovl_start : { *(.ovl.repair) } AT>ovlimg4
    .ovl_msgs   __ovl_start : { *(.ovl.msgs)   } AT>ovlimg5
    .ovl_planet __ovl_start : { *(.ovl.planet) } AT>ovlimg6
    .ovl_cmds   __ovl_start : { *(.ovl.cmds)   } AT>ovlimg7
    .ovl_title  __ovl_start : { *(.ovl.title)  } AT>ovlimg8
    .ovl_events __ovl_start : { *(.ovl.events) } AT>ovlimg9
    .ovl_xtra   __ovl_start : { *(.ovl.xtra)   } AT>ovlimg10
    .ovl_enemy  __ovl_start : { *(.ovl.enemy)  } AT>ovlimg11
    .ovl_move   __ovl_start : { *(.ovl.move)   } AT>ovlimg12
    .ovl_view   __ovl_start : { *(.ovl.view)   } AT>ovlimg13
    .ovl_panel  __ovl_start : { *(.ovl.panel)  } AT>ovlimg14
    .ovl_nav    __ovl_start : { *(.ovl.nav)    } AT>ovlimg15
    .ovl_time   __ovl_start : { *(.ovl.time)   } AT>ovlimg16
    .ovl_turn   __ovl_start : { *(.ovl.turn)   } AT>ovlimg17
    .ovl_laser  __ovl_start : { *(.ovl.laser)  } AT>ovlimg18
    .ovl_torp   __ovl_start : { *(.ovl.torp)   } AT>ovlimg19
    .ovl_main   __ovl_start : { *(.ovl.main)   } AT>ovlimg20
    .ovl_dlg    __ovl_start : { *(.ovl.dlg)    } AT>ovlimg21
    .ovl_io     __ovl_start : { *(.ovl.io)     } AT>ovlimg22
}

/* MEGA65 native mode, with the overlay window and its images.
 *
 * WHY THIS PORT STILL HAS OVERLAYS, which the first commit here said it would
 * not. Measured rather than assumed: llvm-mos's mega65 region is $2001..$CFFF,
 * 45,055 bytes, and the whole game is about 66,500 bytes of code. Unmapping
 * the KERNAL as well would buy 8K and still leave it short. So a flat build
 * does not fit and the window stays.
 *
 * WHAT CHANGES IS WHERE THE IMAGES LIVE. On the C128 each 4K image is read off
 * a 1541 -- hundreds of milliseconds, and the reason ovl_load is called as
 * rarely as the code can manage. Here they are held in banked RAM at $50000
 * and copied in by DMAgic, which is microseconds. Same mechanism, and the cost
 * that shaped the C128 port's structure is gone.
 */
__basic_zp_start = 0x0002;
__basic_zp_end = 0x0090;

MEMORY {
    /* Resident code, stopping short of the window. */
    /* SHORT OF THE STACK ON PURPOSE, the same guard trek128.ld keeps. The soft
     * stack starts at $C000 and grows DOWN, so the last kilobyte below it is
     * left unallocated: overflowing .bss into the stack is then a LINK ERROR
     * rather than a program that runs and quietly corrupts itself. */
    ram (rw) : ORIGIN = 0x2001, LENGTH = 0x9bff       /* $2001..$BBFF */

    /* The window sits at $B000, NOT $C000, and the reason is the soft stack.
     *
     * __stack is $D000 and grows DOWN, so a window at $C000..$CFFF is exactly
     * where the stack lives. This script said $C000 for an afternoon with a
     * comment calling the stack "a tripwire" that this code never uses --
     * which was true only while nothing took the address of a local. far_read
     * DMAs into a `uint8_t note[2]` on the stack, and the moment the music
     * driver started calling it every frame the overlay window and the stack
     * were the same memory. The screen went purple and stayed there.
     *
     * trek128.ld carries the same warning about __stack and guards for it.
     * Same trap, same machine-shaped reason, second port.
     *
     * RESOLVED by moving the STACK instead of the window: __stack is $C000 now
     * and grows down through the guard above, so the window has $C000..$CFFF
     * to itself and the two can never meet. */
    window (rwx) : ORIGIN = 0xC000, LENGTH = 0x1000   /* $C000..$CFFF */

    /* Image staging, exactly as trek128.ld does it: one region per overlay so
       that two overlays sharing a load address is a LINK error rather than a
       silent aliasing bug. These are addresses in the OUTPUT FILE, not in the
       machine -- objcopy cuts each one out and they end up in banked RAM. */
    ovlimg0 (rw) : ORIGIN = 0x50000, LENGTH = 0x1000
    ovlimg1 (rw) : ORIGIN = 0x51000, LENGTH = 0x1000
    ovlimg2 (rw) : ORIGIN = 0x52000, LENGTH = 0x1000
    ovlimg3 (rw) : ORIGIN = 0x53000, LENGTH = 0x1000
    ovlimg4 (rw) : ORIGIN = 0x54000, LENGTH = 0x1000
    ovlimg5 (rw) : ORIGIN = 0x55000, LENGTH = 0x1000
    ovlimg6 (rw) : ORIGIN = 0x56000, LENGTH = 0x1000
    ovlimg7 (rw) : ORIGIN = 0x57000, LENGTH = 0x1000
    ovlimg8 (rw) : ORIGIN = 0x58000, LENGTH = 0x1000
    ovlimg9 (rw) : ORIGIN = 0x59000, LENGTH = 0x1000
}

INCLUDE commodore.ld
INPUT(unmap-basic.o)

/* $C000, growing DOWN into the kilobyte of guard reserved above -- NOT $D000,
   which would grow straight through the overlay window. */
__stack = 0xc000;

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
}

__ovl_start = ORIGIN(window);

/* NO OUTPUT_FORMAT, deliberately, and for the same reason trek128.ld has none:
   it would emit the PRG directly, and then objcopy could not cut the ten
   window images out of the link. One link produces an ELF; two objcopy passes
   make the PRG and OVERLAYS.BIN out of it, so the resident half and the images
   can never disagree about where anything is. */

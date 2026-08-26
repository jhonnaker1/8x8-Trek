/* Custom link script for the C128 port.
 *
 * llvm-mos's stock c128 script puts everything -- code, rodata, data, bss --
 * in one region starting at $1C01, and ignores the RAM below it. That was
 * fine until SAVE: the state serialiser costs about 2.5K of code, and the
 * release build came down to EIGHT bytes of headroom while the debug build
 * would not link at all.
 *
 * So writable sections move to the low RAM between the KERNAL's work areas
 * and the start of BASIC text. This is not a new idea here -- the cc65 build
 * did exactly this in trek128.cfg, and the block was VERIFIED then: filled
 * with a rolling pattern through VICE's binary monitor while the port ran and
 * read back three times over thirty seconds with zero bytes changed.
 *
 * THE LIMIT OF THAT TEST, unchanged: the port had done no disk I/O when it
 * ran, because it had none. It does now. Re-run it across an open-read-close
 * before trusting this further -- KERNAL file handling is the one subsystem
 * that allocates buffers.
 *
 * Derived from mos-platform/c128/lib/link.ld and commodore.ld. The ONLY
 * substantive change is REGION_ALIAS("c_writeable", lowram); everything else
 * is those two files inlined so the alias can be overridden.
 *
 * THAT ONE CHANGE HAS A DEBT, AND NOT PAYING IT COST THE SOUND BUG. Moving
 * c_writeable off c_readonly gives .data a run address that differs from its
 * load address -- and llvm-mos only bridges the two if copy-data.c.obj is
 * LINKED IN. The Commodore targets never link it, because with both aliases
 * on the same region their .data needs no copy at all; c128/lib/libcrt0.a
 * bundles init-stack, zero-bss, zero-zp-bss and copy-ZP-data, and no
 * copy-data. So .data was simply never initialised, and sid.c's
 * `static uint8_t enabled = 1;` came up 0.
 *
 * It has been latent since the llvm-mos migration on 2026-08-23, which is
 * when this script was written. Why it SURFACED alongside the bank-1 move a
 * day later is not established -- uninitialised RAM is uninitialised, and
 * $1301 may simply have held a nonzero byte before. Do not read the timing as
 * evidence about the far store; the bank-1 dump cleared it outright.
 *
 * c128/Makefile links -lcopy-data for exactly this reason, and `make verify`
 * fails the build if .data is non-empty, has VMA != LMA, and no
 * __do_copy_data in the map. Do not drop either without moving c_writeable
 * back. The failure is silent, the map looks correct, and both test suites
 * run natively where .data is the host's problem.
 */

__basic_zp_start = 0x000A;
__basic_zp_end = 0x0090;

MEMORY {
    /* $1300..$1BFF, 2,304 bytes. Code and read-only data stay above. */
    lowram (rw) : ORIGIN = 0x1300, LENGTH = 0x0900
    /* THE OVERLAY WINDOW, and the region below it is what shrank to make
     * room. One overlay at a time is copied here from the disk; see
     * core/overlay.h. Sizing it at what the largest overlay actually needs is
     * deliberate -- outgrowing it is then a LINK ERROR, the same tripwire as
     * the stack guard below, rather than an overlay that quietly runs off the
     * end into resident code.
     *
     * The overlays' LOAD addresses go in `ovlimg`, which exists only inside
     * the linker: it gives each overlay a distinct place in the ELF so their
     * file ranges do not collide, while their RUN addresses all coincide at
     * the window. That overlap is why the link needs --no-check-sections. */
    window (rwx) : ORIGIN = 0x1C01 + 0xA3FF - 0x40 - 0x0F00, LENGTH = 0x0F00

    /* ONE STAGING REGION PER OVERLAY, and they must not be merged back into
     * one. A single `AT>ovlimg` shared by every overlay does NOT advance the
     * region pointer when the section also carries an explicit run address --
     * both sections came out at load address $10000, so they occupied the
     * same bytes of the ELF and llvm-objcopy dumped the SAME image twice
     * under two names. The build was silent, the link was silent, and the
     * game ran the hall of fame when it asked for the evaluation.
     *
     * Each region is exactly the window's length, so an overlay too big for
     * the window is a link error here as well as there. Adding an overlay
     * means adding a region; `make verify` fails the build if two of them
     * ever share a load address again. */
    ovlimg0 (rw) : ORIGIN = 0x10000, LENGTH = 0x0F00
    ovlimg1 (rw) : ORIGIN = 0x10F00, LENGTH = 0x0F00
    ovlimg2 (rw) : ORIGIN = 0x11E00, LENGTH = 0x0F00
    ovlimg3 (rw) : ORIGIN = 0x12D00, LENGTH = 0x0F00
    ovlimg4 (rw) : ORIGIN = 0x13C00, LENGTH = 0x0F00
    ovlimg5 (rw) : ORIGIN = 0x14B00, LENGTH = 0x0F00

    /* SHORT OF $C000 ON PURPOSE.
     *
     * 0xA3FF would run the region right up to $C000, which is where __stack
     * starts and grows DOWN -- so the stock script lets a build succeed and
     * then corrupt itself, with the linker none the wiser. This reserves a
     * guard so that overflowing it is a LINK ERROR instead.
     *
     * Why only 64 bytes: llvm-mos gives a non-reentrant program a STATIC
     * stack, and this one has 181 bytes of `.Lstatic_stack` in .noinit. The
     * soft stack at $C000 is for recursion and alloca, neither of which this
     * code uses, and nothing in the link map references it. So the guard is a
     * tripwire rather than a real allocation.
     *
     * If that assumption ever breaks -- recursion, a function pointer table,
     * alloca -- raise this and expect to have to free space for it. */
    ram    (rw) : ORIGIN = 0x1C01, LENGTH = 0xA3FF - 0x40 - 0x0F00
}

__rc0 = __basic_zp_start;
INCLUDE imag-regs.ld

__basic_zp_size = __basic_zp_end - __basic_zp_start;
MEMORY { zp : ORIGIN = __rc31 + 1, LENGTH = __basic_zp_end - (__rc31 + 1) }

INPUT(basic-header.o)

REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", lowram)

SECTIONS {
    /* >ram is NOT optional here, and leaving it off cost a debugging round.
       A section with no region goes to the FIRST region declared, and this
       script declares lowram first -- so the BASIC header silently landed at
       $1300 while the PRG still claimed to load at $1C01. It loaded and then
       sat at READY, because RUN found no BASIC line to run. */
    .basic_header : { *(.basic_header) } >ram
    INCLUDE c.ld

    /* One section per overlay, every one of them starting at the window.
       ORIGIN(window) is written out rather than left implicit because a
       section with no address takes the current dot, which after c.ld is the
       end of .bss. */
    .ovl_eval   ORIGIN(window) : { *(.ovl.eval)   } >window AT>ovlimg0
    .ovl_hof    ORIGIN(window) : { *(.ovl.hof)    } >window AT>ovlimg1
    .ovl_front  ORIGIN(window) : { *(.ovl.front)  } >window AT>ovlimg2
    .ovl_info   ORIGIN(window) : { *(.ovl.info)   } >window AT>ovlimg3
    .ovl_repair ORIGIN(window) : { *(.ovl.repair) } >window AT>ovlimg4
    .ovl_msgs   ORIGIN(window) : { *(.ovl.msgs)   } >window AT>ovlimg5
}

/* c128/src/overlay.c takes the window's address from here rather than
   repeating the arithmetic above. */
__ovl_start = ORIGIN(window);

INPUT(init-mmu.o)

__stack = 0xC000;

/* NO OUTPUT_FORMAT ANY MORE. It produced the PRG directly, trimmed to `ram`,
   which was fine until the overlays needed extracting individually. The link
   emits an ELF now and llvm-objcopy makes both the PRG and the overlay images
   out of it -- ONE link, so there is no way for two of them to disagree about
   where anything went. `-O binary` over the ELF was checked byte for byte
   against the PRG this used to write. See the $(OUT) rule in Makefile. */

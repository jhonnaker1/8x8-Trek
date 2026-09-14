/* Custom link script for the Commodore 64 port.
 *
 * THE C64 IS A ROOMIER MACHINE FOR THIS PORT THAN THE C128 IS, which is the
 * opposite of what everyone expects and is worth the paragraph.
 *
 * The C128 port lives at $1C01 because BASIC text starts there in C128 mode,
 * and its overlay window is carved OUT of that same region -- 4K of the
 * program's own address space, given back to the disk. Its far store is bank
 * 1, reached a byte at a time through the KERNAL's FETCH.
 *
 * On a C64 the three pieces separate cleanly:
 *
 *     $0801..$BEFF   program: code, rodata, data, bss, and the message log
 *     $BF00..$BFFF   soft-stack guard; __stack = $C000, growing down
 *     $C000..$CFFF   the overlay window -- the 4K NOTHING EVER COVERS
 *     $D000..$DFFF   I/O
 *     $E000..$FFF9   the far store, RAM under the KERNAL
 *     $FFFA..$FFFF   RAM copies of the NMI/RESET/IRQ vectors -- see c64mem.c
 *
 * That makes the program region 46,847 bytes against the C128's 39,935,
 * because BASIC's 8K at $A000 is plain RAM once LORAM is cleared (llvm-mos's
 * unmap-basic.o does it in .init.010) and the window no longer costs the
 * program anything. MEASURED: the 40-column C128 build is 37,327 bytes of
 * code and rodata plus 2,264 writable, so this region starts with about 7,200
 * bytes spare where the C128 has 304.
 *
 * WHAT IS TIGHT INSTEAD IS THE FAR STORE. 8,186 bytes hold STRINGS.DAT
 * (7,492) and MUSIC.DAT (412) with 282 to spare, and the Makefile FAILS the
 * build when they stop fitting rather than letting a truncated pool load.
 * The escape hatch is the 7,200 bytes above: MUSIC.DAT can become resident
 * data again. Do not solve it by shrinking the prose.
 *
 * Derived from mos-platform/c64/lib/link.ld and commodore.ld, inlined so the
 * regions can be redeclared. Unlike trek128.ld this does NOT alias
 * c_writeable away from c_readonly -- both are `ram` -- so there is no .data
 * copy to arrange and no -lcopy-data. See trek128.ld for what that debt cost
 * on the C128.
 */

__basic_zp_start = 0x0002;
__basic_zp_end = 0x0090;

MEMORY {
    /* $0801..$BEFF. The top stops 256 bytes short of the window so the soft
       stack has somewhere to go -- the same guard trek128.ld carries, and for
       the same measured reason: the C128's deepest path was 143 bytes below
       __stack, and a 64-byte guard shipped in v0.9.0 was overrun by 79 into
       the overlay window. The linker cannot see a runtime stack; only a
       sentinel fill can. Re-measure here rather than assume the C128's
       number transfers -- the call graph is the same but the ABI spills
       differently at a different optimisation of the same code. */
    ram    (rw) : ORIGIN = 0x0801, LENGTH = 0xB6FF

    /* THE 4K NOTHING EVER COVERS. $C000..$CFFF is RAM in every $01 setting
       the machine has -- no BASIC, no KERNAL, no I/O, no character ROM -- so
       an overlay running here is never banked out from under itself. On the
       C128 this window had to be stolen from the program; here it is free.
       Sizing it at exactly 4K makes an overlay that outgrows it a LINK ERROR
       rather than something that quietly overwrites resident code. */
    window (rwx) : ORIGIN = 0xC000, LENGTH = 0x1000

    /* ONE STAGING REGION PER OVERLAY. A single shared AT> region does not
       advance its pointer when the section also has an explicit run address,
       so every overlay would be dumped from the same bytes of the ELF and
       llvm-objcopy would write the SAME image out under eleven names. That
       happened on the C128 and the game ran the hall of fame when it asked
       for the evaluation. Adding an overlay means adding a region here. */
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

INPUT(basic-header.o)

REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", ram)

SECTIONS {
    /* >ram stated, not left implicit. A section with no region goes to the
       FIRST region declared; `ram` is first here so it would come out right
       by luck, and trek128.ld lost a debugging round to exactly that luck
       running out. */
    .basic_header : { *(.basic_header) } >ram
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

/* c64/src/c64ovl.c takes the window from here rather than repeating $C000. */
__ovl_start = ORIGIN(window);

/* .init.010 clears LORAM so $A000..$BFFF is RAM; .fini.990 puts it back. */
INPUT(unmap-basic.o)

__stack = 0xC000;

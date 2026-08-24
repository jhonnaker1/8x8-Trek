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
 */

__basic_zp_start = 0x000A;
__basic_zp_end = 0x0090;

MEMORY {
    /* $1300..$1BFF, 2,304 bytes. Code and read-only data stay above. */
    lowram (rw) : ORIGIN = 0x1300, LENGTH = 0x0900
    ram    (rw) : ORIGIN = 0x1C01, LENGTH = 0xA3FF
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
}

INPUT(init-mmu.o)

__stack = 0xC000;

OUTPUT_FORMAT {
    SHORT(ORIGIN(ram))
    TRIM(ram)
}

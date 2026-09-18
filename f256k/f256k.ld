/* The Foenix F256K. THE TWELFTH-AND-A-HALF PORT, started 2026-09-17.
 *
 * llvm-mos has no `f256` platform -- `mos-platform/` has cx16 and atari8-*
 * and nothing for Foenix -- so this script, the PGZ container and the kernel
 * shim are the port's own. THE PLUS/4 PROVED THAT IS ENOUGH: a platform is a
 * convenience, not a requirement, and `mos-common-clang -mcpu=mos65c02` plus
 * a hand-written script is the pattern already in this tree.
 *
 * WHY NOT cc65, WHICH commodore-uno USES AND WHICH IS PROVEN HERE. Two
 * reasons, neither about code size -- measured 2026-09-17, the two compilers
 * are within 1% on a fair full link of core/:
 *
 *   - THE MEMORY MODEL, which is where llvm-mos's real advantage is. cc65
 *     carves its stack out of the program's own space; llvm-mos puts __stack
 *     outside it. Against the ~40K this machine gives a program, that is the
 *     number that decides.
 *   - cc65 WOULD PUT C89 ON THE SHARED HALF FOREVER. `ui.c` has eight
 *     declaration-after-statement sites today and the root Makefile's
 *     `c89-check` guards `core/` only. One port must not constrain the other
 *     twelve's shared sources in perpetuity.
 *
 * THE MEMORY MAP, from uno's `toolchain/f256jr_pgz.cfg` -- which is the jr's
 * and is a starting point rather than a drop-in:
 *
 *     $0000       MMU_MEM_CTRL          $0001  MMU_IO_CTRL
 *     $0002-$003F MCP's                 NOT OURS
 *     $0040-$00EF free zero page        $00F0-$00FF kernel args
 *     $0400-$1FFF MCP leaves this       $2000-$9FFF the program
 *     $C000-$DFFF I/O window (paged by $0001)
 *     $FF00       the kernel jump table
 */

/* ZERO PAGE STARTS AT $40, AND THAT IS NOT THE USUAL $02. On a C64 or a
   Plus/4 the imaginary registers sit at $0002 because BASIC's workspace is
   free once BASIC is out. Here $0000 and $0001 are the MMU's control
   registers -- writing $0000 REMAPS THE ADDRESS SPACE -- and $0002..$003F
   belongs to FoenixMCP. */
__basic_zp_start = 0x0040;
__basic_zp_end   = 0x00F0;

MEMORY {
    /* $2000..$9FFF. uno's config stops at $9FFF with __HIMEM__ at $BFFF, so
       $A000..$BFFF may be reachable too -- NOT ASSUMED until measured. */
    ram (rw) : ORIGIN = 0x2000, LENGTH = 0x8000
}

__rc0 = __basic_zp_start;
INCLUDE imag-regs.ld
__basic_zp_size = __basic_zp_end - __basic_zp_start;
MEMORY { zp : ORIGIN = __rc31 + 1, LENGTH = __basic_zp_end - (__rc31 + 1) }

REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", ram)

SECTIONS {
    __f256_start = .;
    INCLUDE c.ld

    /* THE SOFT STACK IS A RESERVED SECTION, NOT A NOMINATED ADDRESS, and the
       Plus/4 is why. Its stack had three homes -- $7F00 (inside .text), $CC00
       (under a ROM, so writes passed through and READS RETURNED ROM) and
       $0500 (on top of the ROM's own RAM-resident fetch routines) -- and every
       one of them was an address somebody believed was free. A reserved
       section cannot be wrong about what is free, and it MOVES WITH THE
       PROGRAM, so a layout shift cannot change its safety.
       Sized by sentinel later; 1K to begin with. */
    .stack (NOLOAD) : {
        . = ALIGN(256);
        __stack_bottom = .;
        . += 0x0400;
        __stack = .;
    } >ram
}

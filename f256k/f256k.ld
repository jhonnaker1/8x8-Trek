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

/* THE OVERLAY WINDOW IS SLOT 5, AND THAT IS THE WHOLE POINT OF THIS MACHINE.
   Every other port copies an image INTO a window -- off disk on the C128, out
   of banked RAM on the X16 and the MEGA65, out of video RAM on the Atari. Here
   each overlay image IS a RAM bank, and the window is the MMU slot it maps to,
   so ovl_load is ONE STORE and nothing is copied at all.
   8K rather than the 4K every other port uses, because a slot is 8K -- there
   is no smaller unit to map. That is 8,192 bytes of resident space spent, and
   it buys every overlay more room than any other port has. */
__ovl_window = 0xA000;
__ovl_size   = 0x2000;

MEMORY {
    /* $2000..$BFFF -- FORTY KILOBYTES, AND THE LAST 8K OF IT IS MEASURED NOW.
       This said "$A000..$BFFF may be reachable, NOT ASSUMED until measured"
       from the day the port started. src/bankprobe.c measured it: the active
       map is MLUT 3, whose slots 0-6 are RAM banks $00-$06, so $A000-$BFFF is
       bank $05 and it is ours. FoenixMCP's own maps (MLUT 0 and 1) put FLASH
       in that slot, which is why it needed checking and why the answer is
       specifically about the map the PROGRAM runs in.
       Slot 6 ($C000-$DFFF) is not in here: it is the I/O window, and the
       display driver needs it. Slot 7 is the kernel. */
    ram (rw) : ORIGIN = 0x2000, LENGTH = 0x8000

    /* $0400-$1FFF. MEASURED (src/lowprobe.c): FoenixMCP neither writes to it
       nor depends on it while our program runs, and a PGZ segment loads
       straight into it -- all 7,168 bytes arrive intact even though pexec's
       own workspace is there. So it is RESIDENT space, which is worth more
       than a bank because it is always mapped. */
    low (rw) : ORIGIN = 0x0400, LENGTH = 0x1C00

    /* Where every overlay RUNS. All eleven share this address; that is the
       idea. It is slot 5, so mapping a bank here is a single store. */
    window (rwx) : ORIGIN = 0xA000, LENGTH = 0x2000

    /* Where the images are LINKED, one per bank, at addresses no CPU ever
       sees. The Makefile cuts them out of the ELF; the loader copies each
       into its own RAM bank at startup and never reads the disk again. */
    ovlimg0  (rw) : ORIGIN = 0x100000, LENGTH = 0x2000
    ovlimg1  (rw) : ORIGIN = 0x102000, LENGTH = 0x2000
    ovlimg2  (rw) : ORIGIN = 0x104000, LENGTH = 0x2000
    ovlimg3  (rw) : ORIGIN = 0x106000, LENGTH = 0x2000
    ovlimg4  (rw) : ORIGIN = 0x108000, LENGTH = 0x2000
    ovlimg5  (rw) : ORIGIN = 0x10A000, LENGTH = 0x2000
    ovlimg6  (rw) : ORIGIN = 0x10C000, LENGTH = 0x2000
    ovlimg7  (rw) : ORIGIN = 0x10E000, LENGTH = 0x2000
    ovlimg8  (rw) : ORIGIN = 0x110000, LENGTH = 0x2000
    ovlimg9  (rw) : ORIGIN = 0x112000, LENGTH = 0x2000
    ovlimg10 (rw) : ORIGIN = 0x114000, LENGTH = 0x2000
}

__rc0 = __basic_zp_start;
INCLUDE imag-regs.ld
__basic_zp_size = __basic_zp_end - __basic_zp_start;
/* THE OVERLAY WINDOW IS SLOT 5, AND THAT IS THE WHOLE POINT OF THIS MACHINE.
   Every other port copies an image INTO a window -- off disk on the C128, out
   of banked RAM on the X16 and the MEGA65, out of video RAM on the Atari. Here
   each overlay image IS a RAM bank, and the window is the MMU slot it maps to,
   so ovl_load is ONE STORE and nothing is copied at all.
   8K rather than the 4K every other port uses, because a slot is 8K -- there
   is no smaller unit to map. That is 8,192 bytes of resident space spent, and
   it buys every overlay more room than any other port has. */
__ovl_window = 0xA000;
__ovl_size   = 0x2000;

MEMORY { zp : ORIGIN = __rc31 + 1, LENGTH = __basic_zp_end - (__rc31 + 1) }

REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", ram)

SECTIONS {
    /* THE LOW 7K TAKES THE BIGGEST BUFFERS, exactly as the X16's golden RAM
       does, and for the same reason: it is resident space the main region
       would otherwise have to find. Placed BY SECTION NAME so no shared
       source is touched -- and the names are the ones LTO actually emits,
       which the X16 learned by listing one (.bss.view_dept) that does not
       survive LTO and silently did nothing. READ THE MAP, do not guess. */
    /* THE LOW 7K TAKES EVERYTHING THAT IS NOT CODE, which is what buys the
       main region back. Read-only data, all of .bss, and the soft stack:
       5,314 bytes that $2000-$9FFF no longer has to find.

       BY CATCH-ALL, NOT BY NAME. The X16's golden RAM lists five section
       names and one of them (.bss.view_dept) does not survive LTO and
       silently did nothing -- its own comment says READ THE MAP, do not
       guess. A catch-all cannot be wrong about a name it never writes down. */
    .lowrodata : { *(.rodata .rodata.*) } >low

    .lowbss (NOLOAD) : {
        __low_bss_start = .;
        *(.bss .bss.*) *(COMMON)
        __low_bss_end = .;
    } >low

    /* THE SOFT STACK, reserved as a SECTION and not nominated as an address --
       the Plus/4 burned three homes learning that, and two of them were
       addresses somebody believed were free. It takes whatever is left of the
       low region, so the slack sits where an overflow would land rather than
       being wasted somewhere it cannot help.

       __stack GROWS DOWN from the top of low, into its own reserve and then
       into .lowbss. That is deliberate: an overrun lands in this program's
       own variables, where a sentinel can see it -- not in MCP's workspace at
       $0200-$03FF and not in the 6502 stack page. */
    .lowstack (NOLOAD) : {
        __stack_bottom = .;
        . = ORIGIN(low) + LENGTH(low);
        __stack = .;
    } >low

    __f256_start = .;
    INCLUDE c.ld

    /* ORIGIN(window) is written out rather than left implicit, so the run
       address is STATED at every one of these and not inferred from whatever
       section happened to come before. The X16's script carries the same note
       after a section with no region named inherited the last one used and
       put a BASIC header in golden RAM. */
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

    /* THE SOFT STACK USED TO BE HERE, IN ram. It moved to the low region when
       $0400-$1FFF was measured -- see .lowstack above. Leaving both in place
       defined __stack TWICE and reserved 1,067 bytes of the main region that
       nothing was ever going to use. */

}

__ovl_start = ORIGIN(window);

/* .bss IS NOT WHERE THE C RUNTIME EXPECTS IT. -lzero-bss clears c.ld's own
   .bss, which is now empty, so NOTHING ZEROES THE REAL ONE. f256vid.c's
   vdc_init() clears __low_bss_start..__low_bss_end as its first act, and
   main() calls vdc_init() as ITS first act -- checked, not assumed. Skip that
   and the hall of fame starts as garbage rather than as an empty table, which
   is the exact fault the X16 records for its golden RAM. */

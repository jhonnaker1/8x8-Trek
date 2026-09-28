/* The C64 + REU port's link script: ../c64/c64.ld with twenty overlays
 * instead of eleven.
 *
 * EVERYTHING ELSE IS c64.ld's AND IS NOT RESTATED -- read that file for the
 * memory map and why it is shaped this way. What differs is only the overlay
 * list: c64.ld's eleven, the two opt-in hot-path groups, and the seven
 * TREK_OVL_REU groups in core/overlay.h, which take the engine and the
 * console drawing out of resident memory. They may call each other because src/reuovl.s swaps
 * through a thunk on every such call; tools/reu_thunks.py writes the thunks
 * into this link after it is made.
 *
 * If this and c64.ld differ anywhere but the overlay list, one of them is
 * wrong.
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
    ovlimg10(rw) : ORIGIN = 0x1a000, LENGTH = 0x1000
    ovlimg11(rw) : ORIGIN = 0x1b000, LENGTH = 0x1000
    ovlimg12(rw) : ORIGIN = 0x1c000, LENGTH = 0x1000
    ovlimg13(rw) : ORIGIN = 0x1d000, LENGTH = 0x1000
    ovlimg14(rw) : ORIGIN = 0x1e000, LENGTH = 0x1000
    ovlimg15(rw) : ORIGIN = 0x1f000, LENGTH = 0x1000
    ovlimg16(rw) : ORIGIN = 0x20000, LENGTH = 0x1000
    ovlimg17(rw) : ORIGIN = 0x21000, LENGTH = 0x1000
    ovlimg18(rw) : ORIGIN = 0x22000, LENGTH = 0x1000
    ovlimg19(rw) : ORIGIN = 0x23000, LENGTH = 0x1000
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

    .ovl_eval    ORIGIN(window) : { *(.ovl.eval) } >window AT>ovlimg0
    .ovl_hof     ORIGIN(window) : { *(.ovl.hof) } >window AT>ovlimg1
    .ovl_front   ORIGIN(window) : { *(.ovl.front) } >window AT>ovlimg2
    .ovl_info    ORIGIN(window) : { *(.ovl.info) } >window AT>ovlimg3
    .ovl_repair  ORIGIN(window) : { *(.ovl.repair) } >window AT>ovlimg4
    .ovl_msgs    ORIGIN(window) : { *(.ovl.msgs) } >window AT>ovlimg5
    .ovl_planet  ORIGIN(window) : { *(.ovl.planet) } >window AT>ovlimg6
    .ovl_cmds    ORIGIN(window) : { *(.ovl.cmds) } >window AT>ovlimg7
    .ovl_title   ORIGIN(window) : { *(.ovl.title) } >window AT>ovlimg8
    .ovl_events  ORIGIN(window) : { *(.ovl.events) } >window AT>ovlimg9
    .ovl_xtra    ORIGIN(window) : { *(.ovl.xtra) } >window AT>ovlimg10
    .ovl_enemy   ORIGIN(window) : { *(.ovl.enemy) } >window AT>ovlimg11
    .ovl_move    ORIGIN(window) : { *(.ovl.move) } >window AT>ovlimg12
    .ovl_view    ORIGIN(window) : { *(.ovl.view) } >window AT>ovlimg13
    .ovl_panel   ORIGIN(window) : { *(.ovl.panel) } >window AT>ovlimg14
    .ovl_nav     ORIGIN(window) : { *(.ovl.nav) } >window AT>ovlimg15
    .ovl_time    ORIGIN(window) : { *(.ovl.time) } >window AT>ovlimg16
    .ovl_turn    ORIGIN(window) : { *(.ovl.turn) } >window AT>ovlimg17
    .ovl_laser   ORIGIN(window) : { *(.ovl.laser) } >window AT>ovlimg18
    .ovl_torp    ORIGIN(window) : { *(.ovl.torp) } >window AT>ovlimg19
}

/* src/c64reu.c takes the window from here rather than repeating $C000;
   src/reuovl.s cannot, and says $C0 with a pointer back to this line. */
__ovl_start = ORIGIN(window);

/* .init.010 clears LORAM so $A000..$BFFF is RAM; .fini.990 puts it back. */
INPUT(unmap-basic.o)

__stack = 0xC000;

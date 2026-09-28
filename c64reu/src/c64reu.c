#include <cbm.h>
#include <stdint.h>

#include "../../core/overlay.h"
#include "vdc.h"
#include "egavdc.h"
#include "../../core/ega.h"

/* The REU build's overlays: every image loaded from the disk ONCE, into the
 * REU, and swapped into the window by DMA from then on.
 *
 * This file replaces c128/src/overlay.c in this port and nowhere else. The
 * swapping itself is src/reuovl.s: a thunk on every call that crosses from
 * one overlay into another, patched in after the link by tools/reu_thunks.py.
 * What is left for C is getting the images into the REU in the first place.
 *
 * SO ovl_load() NO LONGER LOADS ANYTHING. The shared code still calls it --
 * `load_title(); ui_title();` -- and on the disk builds that pairing is what
 * makes the call safe. Here the thunk on ui_title does the swap, and does it
 * whoever the caller is, so the explicit load has nothing left to do. Doing it
 * anyway would be WRONG, not merely wasteful: run_turn is itself an overlay
 * here, and a real load from inside it would overwrite the code making the
 * call. The first call does the one-time load instead; main() makes it,
 * before any overlay has run, so the window is free.
 *
 * THE LOOP BELOW IS THE ONLY PART THAT IS THIS MACHINE'S. A C64 OS port
 * would fill the REU through C64 OS's file API, into banks C64 OS allocates
 * it, and keep the thunks and reu_xfer as they are -- which is why the
 * manager is in its own file. */

#define DEV      8
#define LFN_OVL  1

extern char __ovl_start[];

/* reuovl.s */
extern uint8_t ovl_live;
extern uint8_t ovl_rhi[], ovl_rbank[], ovl_llo[], ovl_lhi[];
void reu_xfer(uint8_t id, uint8_t cmd);
#define REU_STASH 0x90
#define REU_MAXOVL 32

/* INDEXED BY THE IDS THEMSELVES, not by position, so the table cannot drift
   out of order with core/overlay.h the way the disk loader's did when
   OVL_XTRA was added -- see c128/src/overlay.c. The count check below still
   catches a missing name, and tools/reu_thunks.py reads the same ids from the
   same header, so the thunks and the loader cannot disagree about which image
   is which. */
static const char *const ovl_name[] = {
    [OVL_EVAL]   = "0:OVLEVAL",
    [OVL_HOF]    = "0:OVLHOF",
    [OVL_FRONT]  = "0:OVLFRONT",
    [OVL_INFO]   = "0:OVLINFO",
    [OVL_REPAIR] = "0:OVLREPAIR",
    [OVL_MSGS]   = "0:OVLMSGS",
    [OVL_PLANET] = "0:OVLPLANET",
    [OVL_CMDS]   = "0:OVLCMDS",
    [OVL_TITLE]  = "0:OVLTITLE",
    [OVL_EVENTS] = "0:OVLEVENTS",
    [OVL_XTRA]   = "0:OVLXTRA",
    [OVL_ENEMY]  = "0:OVLENEMY",
    [OVL_MOVE]   = "0:OVLMOVE",
    [OVL_VIEW]   = "0:OVLVIEW",
    [OVL_PANEL]  = "0:OVLPANEL",
    [OVL_NAV]    = "0:OVLNAV",
    [OVL_TIME]   = "0:OVLTIME",
    [OVL_TURN]   = "0:OVLTURN",
    [OVL_LASER]  = "0:OVLLASER",
    [OVL_TORP]   = "0:OVLTORP"
};

struct ovl_reu_checks {
    int ovl_name_has_one_entry_per_overlay :
        1 - 2 * !(sizeof ovl_name / sizeof ovl_name[0] == OVL_COUNT);
    int reuovl_tables_hold_every_overlay :
        1 - 2 * !(OVL_COUNT <= REU_MAXOVL);
};

#define REG(a) (*(volatile uint8_t *)(a))

/* An REU answers at $DF02..$DF05 with registers that read back what was
   written; with none fitted, $DF00 is open I/O space and reads float. Two
   patterns, both inverted, so a bus that happens to echo one value cannot
   pass. */
static uint8_t reu_present(void) {
    REG(0xDF02) = 0x55; REG(0xDF03) = 0xAA;
    if (REG(0xDF02) != 0x55 || REG(0xDF03) != 0xAA) return 0;
    REG(0xDF02) = 0xAA; REG(0xDF03) = 0x55;
    return REG(0xDF02) == 0xAA && REG(0xDF03) == 0x55;
}

static void halt(const char *what, const char *detail) {
    scr_clear();
    scr_puts(2, 2, what, EGA_TO_VDC(EGA_LTRED));
    scr_puts(2, 4, detail, EGA_TO_VDC(EGA_WHITE));
    for (;;) { }
}

static uint8_t ready;

static void ovl_reu_init(void) {
    uint8_t id;
    char count[3];

    if (!reu_present())
        halt("THIS BUILD NEEDS A RAM EXPANSION UNIT",
             "NO REU ANSWERED AT $DF00.");

    scr_clear();
    scr_puts(2, 2, "LOADING OVERLAYS INTO THE REU", EGA_TO_VDC(EGA_LTCYAN));
    for (id = 0; id < OVL_COUNT; id++) {
        uint16_t end, stamp, len;
        const uint8_t *tail;

        /* The progress is drawn, not decoration: a test watches the screen
           for stillness to decide a step is over, and twenty silent loads
           would read as a finished one. */
        count[0] = (char)('0' + (id + 1) / 10);
        count[1] = (char)('0' + (id + 1) % 10);
        count[2] = 0;
        scr_puts(2, 4, count, EGA_TO_VDC(EGA_WHITE));

        cbm_k_setlfs(LFN_OVL, DEV, 0);
        cbm_k_setnam(ovl_name[id]);
        end = (uint16_t)(uintptr_t)cbm_k_load(0, __ovl_start);

        /* THE SAME TWO CHECKS AS THE DISK LOADER, and for the same reasons
           -- see c128/src/overlay.c. Here they matter more: an image is
           checked once, at startup, and then trusted for the whole game. */
        if (end <= (uint16_t)(uintptr_t)__ovl_start)
            halt("CANNOT LOAD AN OVERLAY FROM THE DISK", ovl_name[id] + 2);
        tail  = (const uint8_t *)(uintptr_t)(end - 2);
        stamp = (uint16_t)(tail[0] | ((uint16_t)tail[1] << 8));
        if (stamp != (uint16_t)(uintptr_t)&ovl_load)
            halt("OVERLAY FILES ARE FROM A DIFFERENT BUILD", ovl_name[id] + 2);

        /* 4K apiece: id 16 and up land in bank 1. Twenty images need 80K,
           which a 128K 1700 -- the smallest REU there is -- holds. */
        len = (uint16_t)(end - (uint16_t)(uintptr_t)__ovl_start);
        ovl_rhi[id]   = (uint8_t)(id << 4);
        ovl_rbank[id] = (uint8_t)(id >> 4);
        ovl_llo[id]   = (uint8_t)len;
        ovl_lhi[id]   = (uint8_t)(len >> 8);
        reu_xfer(id, REU_STASH);
    }
    /* The last image is still in the window, so say so rather than claim
       none: the first thunk into it then costs nothing. */
    ovl_live = (uint8_t)(OVL_COUNT - 1);
    ready = 1;
}

void ovl_load(uint8_t which) {
    (void)which;
    if (!ready) ovl_reu_init();
}

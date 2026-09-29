#include <cbm.h>
#include <stdint.h>
#include <string.h>

#include "input.h"

#include "../../core/overlay.h"
#include "vdc.h"
#include "egavdc.h"
#include "../../core/ega.h"
#include "c64os.h"

/* The C64 OS port's overlays: c64reu/src/c64reu.c, with the files and the
 * REU where C64 OS says they are.
 *
 * THE SAME DESIGN AS c64reu, AND THE SAME MANAGER: ../c64reu/src/reuovl.s is
 * linked unchanged -- the thunks, the return stack and reu_xfer are all it
 * ever needed from the machine. What moves is the loop below: every image is
 * read from the app's own folder (osfile.c's prefix, not "0:" on drive 8), and
 * stashed into the banks bkalloc_ gave the game rather than from bank 0.
 *
 *     bank A      images 0-15, 4K each
 *     bank A+1    images 16-22 to $6FFF, the message log at $7000, and
 *                 osfile.c's far store from $8000
 *
 * Everything else -- the stamp check, the names indexed by the ids, why
 * ovl_load() does nothing after the first call -- is c64reu's, and its file
 * says why. */

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
    [OVL_EVAL]   = "OVLEVAL",
    [OVL_HOF]    = "OVLHOF",
    [OVL_FRONT]  = "OVLFRONT",
    [OVL_INFO]   = "OVLINFO",
    [OVL_REPAIR] = "OVLREPAIR",
    [OVL_MSGS]   = "OVLMSGS",
    [OVL_PLANET] = "OVLPLANET",
    [OVL_CMDS]   = "OVLCMDS",
    [OVL_TITLE]  = "OVLTITLE",
    [OVL_EVENTS] = "OVLEVENTS",
    [OVL_XTRA]   = "OVLXTRA",
    [OVL_ENEMY]  = "OVLENEMY",
    [OVL_MOVE]   = "OVLMOVE",
    [OVL_VIEW]   = "OVLVIEW",
    [OVL_PANEL]  = "OVLPANEL",
    [OVL_NAV]    = "OVLNAV",
    [OVL_TIME]   = "OVLTIME",
    [OVL_TURN]   = "OVLTURN",
    [OVL_LASER]  = "OVLLASER",
    [OVL_TORP]   = "OVLTORP",
    [OVL_MAIN]   = "OVLMAIN",
    [OVL_DLG]    = "OVLDLG",
    [OVL_IO]     = "OVLIO"
};

struct ovl_reu_checks {
    int ovl_name_has_one_entry_per_overlay :
        1 - 2 * !(sizeof ovl_name / sizeof ovl_name[0] == OVL_COUNT);
    int reuovl_tables_hold_every_overlay :
        1 - 2 * !(OVL_COUNT <= REU_MAXOVL);
};

static void halt(const char *what, const char *detail) {
    scr_clear();
    scr_puts(1, 2, what, EGA_TO_VDC(EGA_LTRED));
    scr_puts(1, 4, detail, EGA_TO_VDC(EGA_WHITE));
    scr_puts(1, 6, "PRESS A KEY TO RETURN TO C64 OS.", EGA_TO_VDC(EGA_LTCYAN));
    kb_waitkey();
    plat_exit();
}

void c64os_banks(void);
static char path[48];   /* prefix (39) + the longest name, OVLREPAIR (9) */

static uint8_t ready;

static void ovl_reu_init(void) {
    uint8_t id;
    char count[3];

    /* No REU test of our own: C64 OS found it, and bkalloc_ fails without
       one -- osfile.c says so and stops. */
    c64os_banks();

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
        c64os_stage = (unsigned char)(10 + id);
        osv_flush();        /* the count is seen as it goes, not at the end */

        strcpy(path, c64os_prefix());
        strcat(path, ovl_name[id]);
        cbm_k_setlfs(LFN_OVL, c64os_dev, 0);
        cbm_k_setnam(path);
        end = (uint16_t)(uintptr_t)cbm_k_load(0, __ovl_start);

        /* THE SAME TWO CHECKS AS THE DISK LOADER, and for the same reasons
           -- see c128/src/overlay.c. Here they matter more: an image is
           checked once, at startup, and then trusted for the whole game. */
        if (end <= (uint16_t)(uintptr_t)__ovl_start)
            halt("CANNOT LOAD AN OVERLAY FROM THE DISK", ovl_name[id]);
        tail  = (const uint8_t *)(uintptr_t)(end - 2);
        stamp = (uint16_t)(tail[0] | ((uint16_t)tail[1] << 8));
        if (stamp != (uint16_t)(uintptr_t)&ovl_load)
            halt("OVERLAY FILES ARE FROM A DIFFERENT BUILD", ovl_name[id]);

        /* 4K apiece: id 16 and up land in the second bank, below the far
           store. */
        len = (uint16_t)(end - (uint16_t)(uintptr_t)__ovl_start);
        ovl_rhi[id]   = (uint8_t)(id << 4);
        ovl_rbank[id] = (uint8_t)(c64os_bank + (id >> 4));
        ovl_llo[id]   = (uint8_t)len;
        ovl_lhi[id]   = (uint8_t)(len >> 8);
        reu_xfer(id, REU_STASH);
    }
    /* The last image is still in the window, so say so rather than claim
       none: the first thunk into it then costs nothing. */
    ovl_live = (uint8_t)(OVL_COUNT - 1);
    ready = 1;
    c64os_stage = 40;
}

void ovl_load(uint8_t which) {
    (void)which;
    if (!ready) ovl_reu_init();
}

/* STARTUP, from src/app.s, BEFORE main(). main() is itself an overlay here
   (OVL_MAIN), so the images must be in the REU before anything calls it --
   and the one-time load cannot run from inside main(), which would be
   loading over the window it runs from. The screen comes first because the
   load draws its count; vdc_init() is safe to call again from main(). */
void c64os_start(void) {
    vdc_init();
    ovl_reu_init();
}

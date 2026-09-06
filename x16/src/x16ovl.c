/* Code overlays for the Commander X16.
 *
 * SHAPED LIKE THE MEGA65'S, NOT THE C128'S. The C128 loads each overlay off
 * disk on every swap; this port holds all ten images in banked RAM and copies
 * the wanted one into a low-RAM window, so a swap is a memcpy rather than a
 * disk load. The images ride in the far store as one more tenant, beside the
 * string pool and the music -- which means they arrive through the storage and
 * far-memory seams that are already tested, rather than through a third path.
 *
 * The window is in LOW RAM because $A000..$BFFF is the banked window the
 * images are read THROUGH; see x16.ld.
 */
#include <stdint.h>
#include "../../core/overlay.h"
#include "../../core/farmem.h"
#include "../../core/ega.h"
#include "x16mem.h"
#include "x16vera.h"

#define OVL_SIZE 0x1000U

/* UNSIGNED, AND NOT WRITTEN AS OVL_COUNT * OVL_SIZE. `int` is 16 bits here,
   and 10 * 4096 = 40,960 overflows a SIGNED 16-bit int -- -Werror caught it,
   which is the same hazard core/'s "stage the arithmetic so intermediates stay
   inside 16 bits" rule exists for. 40,960 is fine unsigned. */
#define OVL_TOTAL ((uint16_t)OVL_COUNT * OVL_SIZE)

extern char __ovl_start[];

static uint8_t  live     = OVL_NONE;
static uint16_t ovl_base = FAR_NONE;

static void die(const char *a, const char *b, const char *c) {
    scr_clear();
    scr_puts(2, 2, a, EGA_LTRED);
    scr_puts(2, 4, b, EGA_WHITE);
    scr_puts(2, 6, c, EGA_LTCYAN);
    for (;;) { }
}

/* THE STAMP, from day one rather than after it bites. An overlay is linked
   WITH the resident half, so every call it makes into resident code is a fixed
   address from that same link. Put yesterday's images beside today's program
   and the game jumps into the middle of some other function -- no error, and
   nothing naming either file. That cost an afternoon on the MEGA65 and
   produced two wrong diagnoses before the disk was even suspected, and it
   caught a real mismatch on the C128 the day it went in.

   The stamp is the low sixteen bits of ovl_load's address in the link the
   images were cut from, written by the Makefile into the last two bytes. */
void ovl_init(void) {
    uint16_t stamp;
    uint8_t  tail[2];

    ovl_base = far_load("OVERLAYS.BIN");
    if (ovl_base == FAR_NONE)
        die("OVERLAYS.BIN IS MISSING",
            "THE GAME CANNOT RUN WITHOUT IT.",
            "PUT IT BESIDE THE PROGRAM AND START AGAIN.");

    if (far_size() < (uint16_t)(ovl_base + OVL_TOTAL))
        die("OVERLAYS.BIN IS TOO SHORT",
            "IT MUST HOLD TEN 4K IMAGES.",
            "REBUILD AND START AGAIN.");

    far_read((uint16_t)(ovl_base + OVL_TOTAL - 2U), tail, 2);
    stamp = (uint16_t)(tail[0] | ((uint16_t)tail[1] << 8));
    if (stamp != (uint16_t)(uintptr_t)&ovl_load)
        die("OVERLAYS.BIN IS FROM A DIFFERENT BUILD",
            "THE IMAGES AND THE PROGRAM MUST COME FROM ONE LINK.",
            "REBUILD AND START AGAIN.");
}

void ovl_load(uint8_t which) {
    if (which >= OVL_COUNT || which == live) return;
    if (ovl_base == FAR_NONE) return;

    /* Marked absent BEFORE the copy, as on the C128: if it fails half way the
       window holds a mixture, and claiming it holds `which` would be a lie
       that survives into the next call. */
    live = OVL_NONE;
    far_bulk((uint16_t)(ovl_base + (uint16_t)which * OVL_SIZE),
             __ovl_start, OVL_SIZE);
    live = which;
}

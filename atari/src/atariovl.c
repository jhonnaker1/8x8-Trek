/* Code overlays for the Atari 800XL + VBXE.
 *
 * SHAPED LIKE THE X16'S AND THE MEGA65'S, NOT THE C128'S. The C128 loads each
 * image off disk on every swap; here all of them sit in VBXE's VRAM and a swap
 * is a copy through the MEMAC window. They ride in the far store as one more
 * tenant beside the string pool and the music, so they arrive through the
 * seams that already exist rather than through a third path.
 *
 * AND ON THIS TARGET THAT IS NOT MERELY TIDIER, IT IS WHAT MAKES THE TWELFTH
 * OVERLAY POSSIBLE. The enemy-turn split measured on the C128 frees 3,520
 * bytes and this port needs them, but `run_turn` calls the enemy turn on
 * essentially every command, so it is a window swap on the hot path. A disk
 * read there would be ruinous. A VRAM copy is not.
 *
 * The window is at the TOP of the program's space, below the OS ROM -- see
 * atari.ld -- because $2000..$2FFF is already spoken for by the VRAM window
 * the images are read THROUGH.
 */
#include <stdint.h>

#include "../../core/overlay.h"
#include "../../core/farmem.h"
#include "../../core/ega.h"
#include "atarimem.h"
#include "vbxevid.h"

#define OVL_SIZE OVL_WINDOW   /* -D from the Makefile, which reads atari.ld */

/* UNSIGNED, AND NOT WRITTEN AS OVL_COUNT * OVL_SIZE. `int` is 16 bits here and
   the product is over 32,767, so the signed form is undefined -- -Werror
   caught exactly this on the X16. */
#define OVL_TOTAL ((uint16_t)OVL_COUNT * (uint16_t)OVL_SIZE)

extern char __ovl_start[];

/* THE STAMP'S ANCHOR IS A DATA SYMBOL, not a function. The stamp needs an
   address that moves whenever the link moves, and `ovl_load` is the obvious
   choice -- but LTO inlines it into main.c's loader stubs and drops the
   symbol, so the Makefile finds nothing to stamp with and `&ovl_load` folds
   to a constant. `used` did not save it. A volatile global cannot be folded
   away and its address is in the symbol table. */
volatile uint8_t ovl_anchor;

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
   nothing naming either file. That cost the MEGA65 an afternoon and two wrong
   diagnoses before the disk was even suspected, and it caught a real mismatch
   on the C128 the day it went in.

   STATIC AND CALLED LAZILY, because a public ovl_init() that main() does not
   know the name of is a function LTO deletes -- taking the anchor with it. */
static void ovl_init(void) {
    uint16_t stamp;
    uint8_t  tail[2];

    /* A REAL ACCESS, or the symbol does not survive: `volatile` constrains how
       an access is emitted, not whether the object exists. This write is what
       keeps the anchor in the binary. */
    ovl_anchor = 0;

    ovl_base = far_load("OVERLAYS.BIN");
    if (ovl_base == FAR_NONE)
        die("OVERLAYS.BIN IS MISSING",
            "THE GAME CANNOT RUN WITHOUT IT.",
            "PUT IT BESIDE THE PROGRAM AND START AGAIN.");

    if (far_size() < (uint16_t)(ovl_base + OVL_TOTAL))
        die("OVERLAYS.BIN IS TOO SHORT",
            "IT MUST HOLD ONE IMAGE PER OVERLAY.",
            "REBUILD AND START AGAIN.");

    far_read((uint16_t)(ovl_base + OVL_TOTAL - 2U), tail, 2);
    stamp = (uint16_t)(tail[0] | ((uint16_t)tail[1] << 8));
    if (stamp != (uint16_t)(uintptr_t)&ovl_anchor)
        die("OVERLAYS.BIN IS FROM A DIFFERENT BUILD",
            "THE IMAGES AND THE PROGRAM MUST COME FROM ONE LINK.",
            "REBUILD AND START AGAIN.");
}

/* IDEMPOTENT, and on this port that is load-bearing rather than a nicety. The
   enemy overlay is paged on the hot path, so on a turn where nothing else
   touched the window this call has to cost nothing at all. */
void ovl_load(uint8_t which) {
    if (which >= OVL_COUNT || which == live) return;
    if (ovl_base == FAR_NONE) ovl_init();      /* first call brings them in */

    /* Marked absent BEFORE the copy, as on every other port: if it fails half
       way the window holds a mixture, and claiming it holds `which` would be a
       lie that survives into the next call. */
    live = OVL_NONE;
    far_bulk((uint16_t)(ovl_base + (uint16_t)which * (uint16_t)OVL_SIZE),
             __ovl_start, OVL_SIZE);
    live = which;
}

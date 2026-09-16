/* Code overlays for the Apple IIgs: eleven 4K images, read from the disk into
 * the language card.
 *
 * SHAPED LIKE THE C128'S, NOT THE ATARI'S. The Atari holds every image in
 * VBXE's VRAM and a swap is a copy, because its twelfth overlay sits on the
 * hot path and a disk read there would be ruinous. This port has no such
 * overlay and no spare bank big enough: bank $01 holds the string pool and has
 * about 16K left, against 45,056 for eleven padded images. So a swap is a
 * read -- eight blocks off a 3.5" drive, which is what the C128 does over
 * IEC and it was never the thing anyone noticed.
 *
 * THE WINDOW IS IN THE LANGUAGE CARD, at $D000, and that is why this port has
 * room at all: the window used to be the top of `ram` and every byte of it
 * came out of the program. See iigs.ld. src/boot.S reads $C083 twice before
 * jumping, so $D000 is RAM and writable from the game's first instruction --
 * without that, ovl_load would write into ROM space, read back whatever was
 * already there, and the game would run whichever overlay the ROM resembles.
 *
 * WHERE THE WINDOW IS is the linker's business, not a number repeated here:
 * iigs.ld sets __ovl_start, so an overlay that outgrows the window is a LINK
 * ERROR rather than something that corrupts resident code at run time.
 */
#include <stdint.h>

#include "../../core/overlay.h"
#include "../../core/storage.h"
#include "../../c128/src/vdc.h"

#define OVL_BLOCKS 8              /* 4,096 bytes at 512 a block */
#define OVL_FILE   "OVERLAYS.BIN"

extern char __ovl_start[];
uint8_t blk_read_to(unsigned int blk, void *dst);
uint8_t blk_extent(const char *name, unsigned int *start, unsigned int *len);

static unsigned int ovl_start;    /* first block of OVERLAYS.BIN, 0 = unknown */
static uint8_t live = OVL_NONE;

/* A FAILED LOAD MUST NOT RETURN, and the shared main.c already says so and
   already does it -- core/overlay.h declares ovl_fatal and c128/src/main.c
   defines it for every port. This file MUST NOT define its own.

   It did, briefly, and the duplicate-symbol error was the cheap half of what
   that was worth. The expensive half is in the shared version's comment: it
   writes the overlay number through scr_puts because scr_put takes a SCREEN
   code and a digit is ASCII. My version used scr_put -- which happens to be
   right on machines where digits map to themselves, and this port's font is
   indexed by C128 screen code where they do not. A private copy of a shared
   routine loses every lesson the shared one has learned. */

void ovl_load(uint8_t which)
{
    unsigned int blk, len, i;
    char *dst = __ovl_start;

    if (which >= OVL_COUNT || which == live) return;

    if (!ovl_start) {
        if (blk_extent(OVL_FILE, &ovl_start, &len) != STOR_OK || !ovl_start)
            ovl_fatal(which);
        /* THE FILE MUST BE LONG ENOUGH FOR EVERY IMAGE, checked once rather
           than trusted: a short OVERLAYS.BIN reads blocks past its own extent,
           which on this format is the NEXT FILE, and the window would hold
           somebody else's data with no error anywhere. */
        if (len < (unsigned int)OVL_COUNT * (OVL_BLOCKS * 512U))
            ovl_fatal(which);
    }

    /* Marked absent BEFORE the read, as on every other port: if it fails half
       way the window holds a mixture, and claiming it holds `which` would be a
       lie that survives into the next call. */
    live = OVL_NONE;

    blk = ovl_start + (unsigned int)which * OVL_BLOCKS;
    for (i = 0; i < OVL_BLOCKS; i++) {
        if (blk_read_to(blk + i, dst) != STOR_OK) ovl_fatal(which);
        dst += 512;
    }

    /* THE STAMP, AND IT IS THE GUARD THE MEGA65 PORT PAID FOR. Every slot
       carries the low sixteen bits of ovl_load's address in the link it was
       cut from. An overlay is linked WITH the resident half, so yesterday's
       OVERLAYS.BIN beside today's program jumps into the middle of some other
       function -- which on the MEGA65 reset the machine to BASIC and could
       mimic any bug you cared to name. Two bytes and a compare. */
    {
        const unsigned char *t = (const unsigned char *)__ovl_start + 4094;
        unsigned int stamp = (unsigned int)(t[0] | ((unsigned int)t[1] << 8));
        if (stamp != (unsigned int)(uintptr_t)&ovl_load) ovl_fatal(which);
    }

    live = which;
}

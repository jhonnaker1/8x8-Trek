/* Overlays for the CoCo 3: images read from disk into a fixed window.
 *
 * THE C128'S DESIGN, NOT THE GIME'S. Paging an 8K block with the MMU would
 * make a swap one store instead of a disk read -- but enabling MMUEN breaks
 * standalone DSKCON on this machine (isolated to that single bit; see
 * coco3bank.h and NOTES.md), so the port does not enable the MMU at all. That
 * leaves the shape four shipping ports here already use: a window in ordinary
 * RAM, and an image loaded into it on demand.
 *
 * ONE LINK, NOT SEPARATE ONES. coco3/tools/build_ovl.py renames an overlay
 * source's `code` section and places it at the window, so the linker resolves
 * calls IN BOTH DIRECTIONS -- overlay code calls resident functions by name
 * and vice versa, and an overlay is ordinary C rather than a jump-table
 * dialect. The window address and the image names come from the generated
 * ovlmap.h, because a window address written down twice is one that drifts.
 *
 * RULE 4 STILL APPLIES, and nothing here enforces it: only resident code may
 * call into the window, and it must load the right image first. A function
 * inside an overlay cannot call ovl_load, because it would page itself out
 * mid-call. core/overlay.h states it for every port; it is no softer here.
 */
#include <stdint.h>

#include "../../core/overlay.h"
#include "../../core/storage.h"
#include "ovlmap.h"          /* generated: window address and image names */

/* Which image is in the window, so a repeated load costs nothing. 0xFF means
   "nothing yet" -- and it must NOT be a valid index. */
static uint8_t resident_image = 0xFF;

void ovl_load(uint8_t which)
{
    uint16_t got;

    if (which >= OVL_COUNT || which == resident_image)
        return;

    /* A failed load leaves the window holding the WRONG image, so the cache
       is invalidated first: better to re-read than to call into whatever
       happens to be there. */
    resident_image = 0xFF;
    if (plat_read_all(ovl_name[which], (void *)OVL_WINDOW, OVL_SIZE, &got)
        == STOR_OK)
        resident_image = which;
}

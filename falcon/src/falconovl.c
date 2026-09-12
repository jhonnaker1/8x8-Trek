/* Overlays for the Falcon: THERE ARE NONE, and this is the empty function
 * core/overlay.h asks for.
 *
 * Same as the Amiga, and for the same reason: this machine has far more
 * memory than the game needs, so nothing is paged. What that deletes is the
 * 4K window, eleven staging regions, the resident/overlay split, the build
 * stamp, eleven files on the disk, and `make verify`'s layout, call-graph,
 * image and stamp checks.
 *
 * See amiga/src/amigaovl.c: three of the four bugs found in the v0.10.0 cycle
 * came out of that machinery, and none of them can exist here either.
 */
#include <stdint.h>

#include "../../core/overlay.h"

void ovl_load(uint8_t which) { (void)which; }

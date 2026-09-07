/* Overlays for the Amiga: THERE ARE NONE, and this is the empty function
 * core/overlay.h asks for.
 *
 * "A platform with no overlays implements this as an empty function" -- and
 * this machine has more memory than the game needs, so nothing is paged. What
 * that deletes, compared with the three 8-bit ports: a 4K window, eleven
 * staging regions, the resident/overlay split, a build stamp tying images to
 * the link, eleven files on the disk, `make verify`'s layout, call-graph,
 * image and stamp checks, and the four rules in overlay.h.
 *
 * THREE OF THE FOUR BUGS FOUND IN THE v0.10.0 CYCLE CAME OUT OF THAT
 * MACHINERY: a resident function calling into a window that had been swapped
 * under it, a name table with ten entries for eleven overlays, and a soft
 * stack growing down into the window. None of them can exist here.
 *
 * OVL_CODE compiles to nothing off-target too, so every function the other
 * ports page in is simply resident.
 */
#include <stdint.h>

#include "../../core/overlay.h"

void ovl_load(uint8_t which) { (void)which; }

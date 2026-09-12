/* Overlays for the CoCo 3 -- EMPTY FOR NOW, AND THAT IS A MEASUREMENT, NOT A
 * DESIGN.
 *
 * The scope says overlays are "probably needed" here: a 64K address space,
 * the GIME paging 8K blocks (coarser than any current port uses), and cmoc
 * with no LTO. But the staging rule is to LINK THE WHOLE GAME RESIDENT FIRST
 * AND READ THE OVERFLOW -- the number decides, not the expectation. Every
 * 6502 port opened this way.
 *
 * If the resident build fits, this file stays as it is and the port is the
 * Amiga's shape rather than the C128's. If it does not, the overflow is the
 * budget the overlay scheme has to find, and that is a far more useful
 * starting point than guessing at windows.
 */
#include <stdint.h>

#include "../../core/overlay.h"

void ovl_load(uint8_t which) { (void)which; }

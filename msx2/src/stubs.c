/* The one seam this port does not implement: overlays. The game runs with
   NO overlays at all (NOTES.md, "an MSX2 port"), so nothing loads one, and
   this exists only so the shared code's references link. `make probe`
   counts it. */
#include <stdint.h>
#include "overlay.h"

void ovl_load(uint8_t w) { (void)w; }

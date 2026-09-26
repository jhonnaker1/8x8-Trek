#ifndef MSX2_H
#define MSX2_H

/* What this port's drivers share that no seam carries. */

/* FAR MEMORY LIVES IN VRAM PAGE 1 ($10000-$1FFFF), which SCREEN 7 never
   displays. Points the VDP at far offset `off` for a run of reads
   (write = 0x00) or writes (0x40) on port $98. msx2vid.c owns it because it
   owns the VDP's discipline -- the blitter wait, the latch under di -- and
   because the message log caches where the VDP points: this invalidates
   that cache, or a string fetched mid-redraw would move the pointer under
   the log's feet. */
void vram_far_at(unsigned int off, unsigned char write);

#endif

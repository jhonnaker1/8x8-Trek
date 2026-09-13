#ifndef COCO3VDP_H
#define COCO3VDP_H

#include <stdint.h>

/* THE VDP'S ADDRESS COUNTER IS ONE RESOURCE WITH THREE TENANTS -- the screen,
 * the message log and far memory -- and only one of them remembers where it
 * left it. coco3vid.c's log writer CACHES the fact that the counter is already
 * positioned for its next sequential byte, so anything that moves the counter
 * must say so or the log's next character lands in the middle of the picture.
 * These two entry points do that; they are the only sanctioned way in from
 * outside coco3vid.c.
 *
 * FAR MEMORY LIVES IN THE SECOND 64K of the card's 128K, VRAM $10000 up. That
 * is clear of the 54,272-byte display at 0 and the 2K message log at $E000,
 * and it is unreachable from coco3vid.c on purpose: that file is 16-bit
 * throughout by rule, because 32-bit VRAM addresses once turned a screen clear
 * into twelve seconds of emulated time. R#14 counts 16K banks, so the bank
 * arithmetic lives here instead and the rule stays intact.
 */

#define VDP_PORT (*(unsigned char *)0xFF78)     /* VRAM data, auto-incrementing */

void vdp_far_write_at(uint16_t off);
void vdp_far_read_at(uint16_t off);

#endif

/* Far memory for the Commander X16: the string pool and the music, out of the
 * 38,655 bytes of code space and into banked RAM.
 *
 * THE SHAPE IS DIFFERENT FROM THE C128'S AND THAT IS THIS PORT'S TOP RISK.
 * The C128 has a FLAT 64K bank reached through the KERNAL's FETCH/STASH, so a
 * read never crosses anything. The X16 pages **8K at a time** into a window at
 * $A000, selected by the RAM bank register at $00. So a read of len bytes at
 * offset off can span two banks, and the C128 implementation has no code for
 * that case because it cannot happen there.
 *
 * The string pool is 7,275 bytes -- just under one 8K page -- so the FIRST
 * read to cross $2000 is the one that would break, and it would break for the
 * music rather than the prose. Both far_read and far_write below therefore
 * loop, clipping each pass at the window edge, and there is a test that
 * deliberately straddles.
 *
 * Offsets are uint16_t by the seam's contract, so the store tops out at 64K =
 * eight banks. The pool and music together are under 8K today.
 */
#include <stdint.h>
#include <string.h>
#include "../../core/farmem.h"
#include "../../core/storage.h"

#define BANK_REG  (*(volatile unsigned char *)0x0000)
#define WIN       ((unsigned char *)0xA000)
#define WIN_BITS  13                    /* 8192 bytes per bank */
#define WIN_SIZE  (1U << WIN_BITS)
#define WIN_MASK  (WIN_SIZE - 1U)

/* Bank 0 is the KERNAL's. The store starts at bank 1. */
#define FIRST_BANK 1

static uint16_t far_len = 0;

static void far_move(uint16_t off, unsigned char *p, uint8_t len, uint8_t writing) {
    while (len) {
        uint16_t within = (uint16_t)(off & WIN_MASK);
        uint16_t avail  = (uint16_t)(WIN_SIZE - within);
        uint8_t  n      = (len < avail) ? len : (uint8_t)avail;

        BANK_REG = (unsigned char)(FIRST_BANK + (off >> WIN_BITS));
        if (writing) memcpy(WIN + within, p, n);
        else         memcpy(p, WIN + within, n);

        p   += n;
        off  = (uint16_t)(off + n);
        len  = (uint8_t)(len - n);
    }
}

void far_read(uint16_t off, void *dst, uint8_t len) {
    far_move(off, (unsigned char *)dst, len, 0);
}

uint16_t far_size(void) { return far_len; }

/* Streams a file into the store, APPENDING -- the store has more than one
   tenant and an earlier version of the C128's loaded at offset 0 every time,
   so the music silently overwrote the prose. Reads go through the storage
   seam, which is already tested, rather than duplicating KERNAL calls here. */
uint16_t far_load(const char *name) {
    uint16_t base = far_len;
    unsigned char buf[64];
    uint16_t n;

    if (plat_open(name) != STOR_OK) return FAR_NONE;
    for (;;) {
        n = plat_read(buf, (uint16_t)sizeof buf);
        if (n == 0) break;
        far_move(far_len, buf, (uint8_t)n, 1);
        far_len = (uint16_t)(far_len + n);
    }
    plat_close();

    if (far_len == base) return FAR_NONE;
    return base;
}

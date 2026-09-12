/* Far memory for the Falcon: there isn't any, and that is the implementation.
 *
 * core/farmem.h has said "no banking needed, a plain array" (the Amiga's entry) since the
 * seam was designed, and this is that array. The C128 reaches bank 1 through a
 * KERNAL call per byte; the MEGA65 goes through DMA; the X16 pages 8K windows
 * at $A000 and has to split every read that straddles a page boundary. Here a
 * far offset is an index and far_read is a memcpy.
 *
 * THE SEAM'S SHAPE IS THE 8-BIT MACHINES', not this one's, and it stays that
 * way on purpose: c128/src/strpool.c is shared by all five ports and it is the
 * only caller that matters. Making this machine's version return pointers
 * straight into the buffer would be faster and would fork the pool reader,
 * which is a bad trade for a memcpy of sixty-odd bytes.
 */
#include <string.h>

#include "../../core/farmem.h"
#include "../../core/storage.h"

/* STRINGS.DAT is about 7.3K and MUSIC.DAT about 0.4K, and far offsets are
   uint16_t, so the whole store can never exceed 64K by the seam's own
   definition. 32K is comfortably more than the game can ask for and is
   nothing on a machine with at least 1MB, and this one has 14. Static rather than
   AllocMem: there is no failure path to get wrong, and no cleanup to forget
   on a quit that resets the machine on one of its siblings. */
#define FAR_CAP 32768u

static unsigned char store[FAR_CAP];
static uint16_t used;

uint16_t far_load(const char *name) {
    uint16_t base = used;
    uint16_t got = 0;

    /* APPENDS, and returns where it put the file -- see farmem.h. More than
       one file lives in the store and the caller keeps the offset. */
    if (plat_read_all(name, store + used, (uint16_t)(FAR_CAP - used), &got)
        != STOR_OK)
        return FAR_NONE;

    used = (uint16_t)(used + got);
    return base;
}

uint16_t far_size(void) { return used; }

void far_read(uint16_t off, void *dst, uint8_t len) {
    if ((uint32_t)off + len > used) {
        /* Past the end of what was loaded. The 8-bit ports would read whatever
           was in the bank; refusing is better, and the pool's own guards
           already treat an empty answer as "no words" rather than a crash. */
        memset(dst, 0, len);
        return;
    }
    memcpy(dst, store + off, len);
}

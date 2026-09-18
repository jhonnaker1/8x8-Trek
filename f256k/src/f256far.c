/* Far memory for the F256K: read-only bulk data in RAM banks.
 *
 * WHAT LIVES HERE. The string pool and the music, which is what farmem.h's
 * "more than one tenant" is about -- far_load APPENDS and returns where it
 * put the file, because an earlier version of the C128's loaded at offset 0
 * every time and the music silently overwrote the prose.
 *
 * THE BRIEFING DOES NOT LIVE HERE, and that is worth saying because on this
 * machine it could. It is streamed through plat_open/plat_read like every
 * other port, since the seam already exists and works; moving it into a bank
 * is an optimisation for later, not a requirement. Far offsets are uint16_t
 * by contract, so the whole store is capped at 64K anyway -- eight banks of
 * the forty-five this port has free.
 *
 * THE WINDOW IS BORROWED FROM THE OVERLAY LOADER. There is no ninth slot; see
 * f256ovl.h for why that is safe and what would not be.
 *
 * READ IN CHUNKS, NOT BYTES -- farmem.h says so, and here the reason is that
 * every far_read costs two MMU stores whether it moves one byte or two
 * hundred.
 */
#include <stdint.h>
#include "../../core/farmem.h"
#include "../../core/storage.h"
#include "f256ovl.h"

/* $2000 bytes per bank, so the offset splits as bank = off >> 13 and
   position = off & $1FFF. Written as shifts rather than / and % because the
   compiler cannot know the divisor is a power of two through a uint16_t on a
   6502 without help, and this is on the path every string in the game takes. */
#define BANK_SHIFT 13
#define BANK_MASK  0x1FFFU

static uint16_t far_used;

uint16_t far_size(void) { return far_used; }

/* Streams a file into the store, appending. Returns its base offset, or
   FAR_NONE if it could not be read.

   IT READS STRAIGHT INTO THE BANK, not into a buffer and then out again: the
   window IS the destination, so the storage layer writes where the data will
   live. That is the same trick the overlay loader uses, and it is the reason
   neither of them needs a scratch buffer on a machine that would have to find
   one somewhere. */
uint16_t far_load(const char *name)
{
    uint16_t base = far_used;

    if (plat_open(name) != STOR_OK) return FAR_NONE;

    for (;;) {
        uint16_t pos = (uint16_t)(far_used & BANK_MASK);
        uint16_t room = (uint16_t)(F256_WIN_SIZE - pos);
        uint16_t got;

        f256_win_borrow((unsigned char)(F256_FAR_BANK0 + (far_used >> BANK_SHIFT)));
        got = plat_read((void *)(F256_WIN + pos), room);
        f256_win_return();

        if (got == 0) break;                 /* end of file */
        far_used = (uint16_t)(far_used + got);
        /* A SHORT READ IS NOT END OF FILE -- plat_read fills the request and
           returns short only at EOF, but leaning on that here would make this
           loop depend on a promise made in another file. Looping until a read
           returns nothing is true whichever way plat_read behaves. */
        if (got < room) continue;
    }
    plat_close();

    /* A FILE THAT READ NOTHING IS NOT A FILE THAT LOADED. Returning `base`
       here would hand the caller an offset into somebody else's data with no
       error anywhere -- and the string pool's first read would come back as
       whatever the music left. */
    if (far_used == base) return FAR_NONE;
    return base;
}

void far_read(uint16_t off, void *dst, uint8_t len)
{
    unsigned char *d = (unsigned char *)dst;

    /* STRADDLES ARE THE WHOLE JOB. A record does not care where a bank ends,
       and a version of this that mapped one bank and copied `len` bytes would
       be correct for every read except the ones that cross $2000 -- which is
       one read in thirty-two, arrives late, and looks like corrupt data
       rather than like a boundary. */
    while (len) {
        uint16_t pos  = (uint16_t)(off & BANK_MASK);
        uint16_t room = (uint16_t)(F256_WIN_SIZE - pos);
        uint8_t  n = len;
        uint8_t  i;

        /* `room` is up to 8192 and `n` is a byte. Narrowing only happens on
           the branch where room is already smaller than len, and len is at
           most 255 -- so the cast cannot lose anything. Written as two
           statements rather than a ternary because the ternary's safety
           depends on that argument and nothing said it. */
        if (room < len) n = (uint8_t)room;

        f256_win_borrow((unsigned char)(F256_FAR_BANK0 + (off >> BANK_SHIFT)));
        for (i = 0; i < n; i++) d[i] = F256_WIN[pos + i];
        f256_win_return();

        d += n;
        off = (uint16_t)(off + n);
        len = (uint8_t)(len - n);
    }
}

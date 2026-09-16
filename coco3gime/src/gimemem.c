/* Far memory on a machine with none: THE STRING POOL STAYS ON THE DISK.
 *
 * The SuperSprite carried this. Its 128K of VRAM was the pool's home on the
 * card-carrying port, and this machine has neither that nor a usable MMU --
 * setting MMUEN permanently breaks disk access here (NOTES.md item 30,
 * unexplained, parked). So there is no banked store to put 7,483 bytes in.
 *
 * AND THERE IS NO ROOM FOR IT IN RAM EITHER, which is measured rather than
 * assumed. With the overlay split wired, the resident image and its bss reach
 * $FB11, leaving 1,007 bytes under the I/O page for a 4,096-byte overlay
 * window, a 4,000-byte screen, a 2,048-byte message log and a 1K stack. A
 * `static unsigned char store[7936]` was 7,936 of the 10,161 that did not fit.
 *
 * SO far_read READS THE DISK. core/farmem.h allows exactly this -- "a platform
 * with no far memory may implement this as a read into a static array; the
 * contract does not care where the bytes live" -- and it says the other half
 * too: READ IN CHUNKS, NOT BYTES. strpool.c already obeys that. It takes two
 * bytes for the offset and then one slot's worth of text, so a string costs
 * at most two of the reads below and usually zero disk access at all.
 *
 * ONE SECTOR OF CACHE, 256 BYTES, and that is what makes it affordable. The
 * pool's text is 6,822 bytes -- twenty-seven sectors -- and strings average
 * about twenty characters, so eight to twelve consecutive fetches land in the
 * same sector. The offset table is 668 bytes at the front of the file and is
 * hit constantly, which is why the cache is checked before anything else.
 */
#include "../../core/farmem.h"
#include "../../coco3/src/coco3storage.h"

#define SEC_SIZE 256

/* THE TENANTS, and there are exactly two: STRINGS.DAT then MUSIC.DAT, in the
   order main() loads them. far_load appends, so each gets a base offset and
   far_read maps a flat offset back to a file. Two is not a general solution
   and does not pretend to be -- core/farmem.h's contract is that the store
   has more than one tenant, not that it has many. */
#define MAX_TENANTS 2

static struct {
    unsigned char first;        /* first granule, 0xFF if absent */
    unsigned int  base;         /* where this file starts in the flat store */
    unsigned long len;
} ten[MAX_TENANTS];

static unsigned char ntenants = 0;
static unsigned int far_len = 0;

/* TWO WAYS, AND ONE WAS THE WHOLE PROBLEM. strpool.c fetches a string in two
   far_reads that are ALWAYS in different parts of the file: the index, in the
   668-byte offset table at the front, and then the text, hundreds of sectors
   later. A single sector cache cannot hold both, so it evicted one for the
   other twice per string, for ever -- TWO DISK READS FOR EVERY LABEL ON THE
   SCREEN. Measured on the machine at real CoCo speed: the title screen drew
   about fifteen cells every twenty seconds and every sampled PC was inside
   plat_raw_sector or dskcon_processSector. Jamie's words were "loading
   extremely slow", and it was not loading -- that was the game running.
   Two ways with a one-bit LRU fixes it exactly, because the pattern is
   exactly two streams: the index sector settles in one way and the text
   sector rotates through the other. 256 bytes. */
#define WAYS 2
static unsigned char cache[WAYS][SEC_SIZE];
static unsigned char cache_first[WAYS] = { 0xFF, 0xFF };
static unsigned int  cache_sec[WAYS] = { 0xFFFF, 0xFFFF };
static unsigned char lru = 0;               /* the way to evict next */

unsigned int far_load(const char *name)
{
    unsigned long len = 0;
    unsigned char first;

    if (ntenants >= MAX_TENANTS) return FAR_NONE;

    first = plat_raw_open(name, &len);
    if (first == 0xFF || len == 0) return FAR_NONE;

    ten[ntenants].first = first;
    ten[ntenants].base  = far_len;
    ten[ntenants].len   = len;
    ntenants++;

    {
        unsigned int base = far_len;
        far_len = (unsigned int)(far_len + (unsigned int)len);
        return base;
    }
}

unsigned int far_size(void) { return far_len; }

/* One byte, through the cache. Kept separate so far_read below is obviously
   correct across a sector boundary rather than cleverly correct. */
static unsigned char fetch(unsigned char t, unsigned long pos)
{
    unsigned int sec = (unsigned int)(pos >> 8);
    unsigned char off = (unsigned char)(pos & 0xFF);

    unsigned char w;

    for (w = 0; w < WAYS; w++)
        if (cache_first[w] == ten[t].first && cache_sec[w] == sec) {
            lru = (unsigned char)(w ^ 1);    /* the other way is the older */
            return cache[w][off];
        }

    w = lru;
    lru = (unsigned char)(w ^ 1);
    if (!plat_raw_sector(ten[t].first, sec, cache[w])) {
        /* A READ THAT FAILS RETURNS ZERO, NOT GARBAGE. strpool.c treats a
           NUL as the end of a string, so a disk error shows as a short or
           empty label -- the same failure the pool already plans for when
           STRINGS.DAT is missing entirely. */
        cache_first[w] = 0xFF;
        cache_sec[w] = 0xFFFF;
        return 0;
    }
    cache_first[w] = ten[t].first;
    cache_sec[w] = sec;
    return cache[w][off];
}

void far_read(unsigned int off, void *dst, unsigned char len)
{
    unsigned char *d = (unsigned char *)dst;
    unsigned char i;
    unsigned char t;

    for (i = 0; i < len; i++) {
        unsigned int at = (unsigned int)(off + i);

        /* Which tenant. Two of them, so a loop is a branch. */
        t = 0;
        if (ntenants > 1 && at >= ten[1].base) t = 1;

        if (t >= ntenants || at < ten[t].base ||
            at >= (unsigned int)(ten[t].base + (unsigned int)ten[t].len)) {
            d[i] = 0;
            continue;
        }
        d[i] = fetch(t, (unsigned long)(at - ten[t].base));
    }
}

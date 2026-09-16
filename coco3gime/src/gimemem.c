/* Far memory with no far memory: the string pool in ordinary RAM.
 *
 * The SuperSprite carried this. Its 128K of VRAM was the pool's home on
 * [[coco3-port]] and it is not in this machine, and the GIME's own MMU is not
 * available either -- setting MMUEN permanently breaks disk access here
 * (NOTES.md item 30, unexplained, parked). So there is no banked store at all
 * and the pool has to live in the 64K the program already occupies, which
 * makes far_read a memcpy and far_load a whole-file read.
 *
 * THAT IS THE BUDGET QUESTION OF THIS PORT, and it is why this file exists
 * before the sound driver does: `make early` links the whole game with the
 * real array in it and prints what is left.
 */
#include "../../core/farmem.h"
#include "../../core/storage.h"

/* STRINGS.DAT is 7,483 bytes and MUSIC.DAT 412. Sized from the files rather
   than rounded up, so that outgrowing it is a link failure here rather than a
   truncated pool on the disk. */
#define FAR_SIZE 8192

static unsigned char store[FAR_SIZE];
static unsigned int far_len = 0;

unsigned int far_load(const char *name)
{
    unsigned int base = far_len;
    unsigned int got = 0;

    if (base >= FAR_SIZE) return FAR_NONE;
    if (plat_read_all(name, store + base, (unsigned int)(FAR_SIZE - base), &got)
        != STOR_OK)
        return FAR_NONE;
    if (got == 0) return FAR_NONE;

    far_len = (unsigned int)(base + got);
    return base;
}

unsigned int far_size(void) { return far_len; }

void far_read(unsigned int off, void *dst, unsigned char len)
{
    unsigned char *d = (unsigned char *)dst;
    unsigned char i;
    for (i = 0; i < len; i++) d[i] = store[off + i];
}

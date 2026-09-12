/* See coco3bank.h. */
#include "coco3bank.h"

#define INIT0  (*(unsigned char *)0xFF90)
#define MMU0   ((unsigned char *)0xFFA0)

#define INIT0_MMUEN 0x40
#define INIT0_TR    0x01            /* 0 selects task 0, which is what we use */

static unsigned char started;

/* cmoc HAS NO `volatile`, and it says so: "the `volatile' keyword is not
   supported by this compiler". Hardware accesses therefore have to be written
   so that an optimiser cannot fold them -- straight-line stores through a
   pointer to a literal address survive, and a loop that reads the same
   hardware address repeatedly is NOT safe. A first version of the MMU probe
   read a paged window inside a loop and the reads were folded away; it looked
   exactly like the banking not working. Keep hardware access straight-line. */

void bank_init(void)
{
    unsigned char i;

    if (started) return;

    /* Write the map the machine is ALREADY using, then enable -- see the
       header. Enabling first would swap in whatever task 0 happened to hold. */
    for (i = 0; i < BANK_SLOTS; i++)
        MMU0[i] = (unsigned char)(0x38 + i);

    INIT0 = (unsigned char)((INIT0 | INIT0_MMUEN) & (unsigned char)~INIT0_TR);
    started = 1;
}

void bank_map(unsigned char slot, unsigned char blk)
{
    MMU0[slot & 7] = blk;
}

unsigned char bank_get(unsigned char slot)
{
    return MMU0[slot & 7];
}

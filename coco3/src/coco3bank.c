/* See coco3bank.h. */
#include "coco3bank.h"

#define INIT0  (*(unsigned char *)0xFF90)
#define MMU0   ((unsigned char *)0xFFA0)

#define INIT0_MMUEN 0x40
#define INIT0_TR    0x01            /* 0 selects task 0, which is what we use */

static unsigned char started;
static unsigned char init0_shadow;   /* $FF90 is write-only: keep our own copy */

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

    /* WRITE A LITERAL. NEVER READ-MODIFY-WRITE $FF90.
     *
     * The GIME's control registers are WRITE-ONLY from the CPU: reading
     * $FF90 returns floating bus, not the value last written. The first
     * version of this said `INIT0 = (INIT0 | MMUEN) & ~TR`, which with a
     * floating $FF computes $FE -- and bit 7 of INIT0 is the CoCo 1/2
     * COMPATIBILITY bit, so that one line quietly threw the machine into a
     * different memory map. Every read through the paged window then came
     * back $FF and it looked exactly like the banking not working.
     *
     * MAME's DEBUGGER READS $FF90 AS $1B, because it shows the internal
     * latch rather than what the 6809 would see -- which is how the wrong
     * model survived: the instrument agreed with it.
     *
     * So: a literal, and a shadow in RAM for anything that needs to change
     * one bit later. $5A = MMUEN on, task 0, FEN, MC3, MC1:MC0 = 10, which
     * is the boot value $1B with MMUEN added and TR cleared. */
    init0_shadow = 0x5A;
    INIT0 = init0_shadow;
    started = 1;
}

void bank_map(unsigned char slot, unsigned char blk)
{
    MMU0[slot & 7] = blk;
}

/* The window is slot 3, $6000..$7FFF -- see BANK_WINDOW in the header. These
   are deliberately in this file and not inlined anywhere: that is what stops
   a caller caching the load. */
unsigned char bank_peek(unsigned int off)
{
    return *((unsigned char *)(0x6000 + (off & 0x1FFF)));
}

void bank_poke(unsigned int off, unsigned char v)
{
    *((unsigned char *)(0x6000 + (off & 0x1FFF))) = v;
}

unsigned char bank_get(unsigned char slot)
{
    return MMU0[slot & 7];
}

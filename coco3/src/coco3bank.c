/* See coco3bank.h. */
#include "coco3bank.h"

#define INIT0  (*(unsigned char *)0xFF90)
#define INIT1  (*(unsigned char *)0xFF91)
#define MMU0   ((unsigned char *)0xFFA0)

#define INIT0_MMUEN 0x40
#define INIT1_TR    0x01            /* in INIT1 ($FF91): 0 = task 0 = $FFA0-$FFA7 */

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

    /* THE TASK SELECT IS IN $FF91, NOT $FF90 -- and getting that wrong is
     * what cost a day. INIT0 ($FF90) bit 6 is MMUEN; its bits 1-0 are MC1/MC0,
     * the ROM MAP CONTROL. The first version of this cleared bit 0 of INIT0
     * believing it was the task select, which actually switched the machine
     * from 32K external ROM ($1B = MC1:MC0 = 11) to 32K internal (10) -- and
     * never selected task 0 at all.
     *
     * TR is bit 0 of INIT1 ($FF91): 0 selects $FFA0-$FFA7, 1 selects
     * $FFA8-$FFAF. At boot BASIC is using task 1.
     *
     * So: preserve INIT0's low bits, OR IN MMUEN ONLY, and clear TR in INIT1.
     * (INIT0 and INIT1 are both READABLE -- an earlier note here claimed they
     * were write-only and that MAME's debugger was showing an internal latch.
     * That was invented and it was wrong.) */
    INIT1 = (unsigned char)(INIT1 & (unsigned char)~INIT1_TR);   /* task 0 */
    INIT0 = (unsigned char)(INIT0 | INIT0_MMUEN);                /* MMU on */
    started = 1;
}



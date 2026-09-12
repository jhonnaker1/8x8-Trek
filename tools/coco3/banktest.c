/* coco3bank.c on the machine: map a block, write, page away, page back, read
 * it intact, restore the original. Expect A5 3B 30 22 11 3B 5A at $2F00.
 *
 *   cmoc --coco --org=2000 -fomit-frame-pointer -o banktest.bin \
 *        banktest.c ../../coco3/src/coco3bank.c
 *
 * and drive it with run.lua. Results live at $2F00 -- slot 1, with this code,
 * and NOT in the window being paged.
 */
#include "../../coco3/src/coco3bank.h"

#define SLOT 3

/* NOT A LOCAL. A local pointer lives in the STACK FRAME at -2,U, and cmoc
   reloads it with `LDX -2,U` before every store through it -- so if the frame
   is inside the window, paging the window away makes the variable itself
   unreadable and the stores land anywhere. Moving S with `lds` is not enough:
   U is the frame pointer and it has to be outside the window too. */
#define r ((unsigned char *)0x2F00)



int main(void)
{
    r[0] = 0xA5;
    asm { orcc #$50 }
    /* THE STACK MUST NOT LIVE IN THE WINDOW. cmoc left S inside $6000-$7FFF,
       so the first bank_map() paged away its own return address and the
       machine died on the RTS. A rule the port has to keep. */
    asm { lds #$3F00 }

    bank_init();
    r[1] = bank_get(SLOT);

    /* Read immediately after each write, then page back and read again --
       the shape the raw probe used. A store with no read between it and the
       next store to the same address is dead-store-eliminated by cmoc, which
       has no `volatile`; this ordering gives the optimiser nothing to remove
       and still proves retention across a page-out. */
    bank_map(SLOT, 0x30);
    r[2] = bank_get(SLOT);      /* what does $FFA3 read RIGHT NOW? expect 30 */
    bank_poke(0, 0x11);
    r[3] = bank_peek(0);        /* expect 11 */
    r[4] = bank_get(SLOT);      /* still 30? */
    bank_map(SLOT, 0x31); bank_poke(0, 0x22); r[3] = bank_peek(0);
    bank_map(SLOT, 0x30);                r[4] = bank_peek(0);  /* 11 = retained */

    bank_map(SLOT, 0x3B); r[5] = bank_get(SLOT);
    r[6] = 0x5A;
    for (;;) ;
}

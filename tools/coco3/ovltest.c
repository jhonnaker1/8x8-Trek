/* THE OVERLAY SCHEME, END TO END: read an image off the disk into a spare RAM
 * block, page it into the window, call code inside it, page it away, page it
 * back and call it again.
 *
 * Results at $2F00 and NOT in a local -- a local lives in the stack frame at
 * -2,U, and if the frame is ever in the window the variable itself vanishes
 * when the window pages. That cost a day; see coco3bank.h.
 */
#include "../../coco3/src/coco3bank.h"
#include "../../core/storage.h"

#define r    ((unsigned char *)0x2F00)
#define SLOT BANK_WINDOW
#define OVL_BLOCK 0x30
#define CALL(x)  ((*(unsigned char (*)(unsigned char))0x6000)(x))

/* STATIC, NOT A LOCAL -- and this is the same bug that cost a day, one level
   up. main()'s frame pointer U is established at entry, BEFORE the `lds` that
   moves S, so main's locals stay wherever BASIC left the stack: inside
   $6000-$7FFF, the window. Passing &got to plat_read_all then hands the disk
   code a pointer into the block that is about to be paged away, and it hangs.
   The rule is not "move the stack" -- it is NO AUTOMATIC STORAGE AT ALL in
   code that pages. */
static unsigned int got;
static unsigned char scratch[512];

int main(void)
{
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$3F00 }          /* S out of the window; U too -- see below */

    bank_init();
    r[1] = 0xB1;                 /* survived bank_init */

    /* Page the spare block in and read the image straight into it. The
       storage seam's own buffers are resident, so nothing it touches is in
       the window. */
    /* SPLIT THE QUESTION: read the file with the MMU ON but the window NOT
       paged, into a resident buffer. If this works, the MMU is innocent and
       something the disk code touches lives in $6000-$7FFF. */
    r[2] = 0xB2;
    r[3] = plat_read_all("OVLDEMO.BIN", scratch, sizeof scratch, &got);
    r[4] = (unsigned char)(got & 0xFF);

    /* Call into the overlay. 0x3C ^ 0xA5 = 0x99. */
    r[5] = CALL(0x3C);

    /* Page it away, put something else there, and come back. */
    bank_map(SLOT, 0x31);
    r[6] = bank_get(SLOT);
    bank_map(SLOT, OVL_BLOCK);
    r[7] = CALL(0x3C);          /* still 0x99 -- the image survived */

    bank_map(SLOT, 0x3B);
    r[8] = 0x5A;
    for (;;) ;
}

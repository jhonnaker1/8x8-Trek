/* THE BRACKET. Disk I/O fails with the MMU on, so turn it off around the
   access and back on afterwards. If this works the blocker is solved in
   practice: the port loads everything before enabling the MMU, and SAVE --
   the one thing that needs the disk later -- pays a bracket. */
#include "../../coco3/src/coco3bank.h"
#include "../../core/storage.h"

#define r ((unsigned char *)0x1000)
static unsigned int got;
static unsigned char buf[512];

int main(void)
{
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$3F00 }

    r[1] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);   /* MMU off */
    bank_init();
    r[2] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);   /* MMU on: fails */

    /* THE BRACKET */
    bank_off();
    r[3] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);
    r[4] = (unsigned char)(got & 0xFF);
    r[5] = buf[0];
    bank_on();

    /* and banking still works afterwards */
    bank_map(BANK_WINDOW, 0x30);
    r[6] = bank_get(BANK_WINDOW);
    bank_map(BANK_WINDOW, 0x3B);
    r[7] = 0x5A;
    for (;;) ;
}

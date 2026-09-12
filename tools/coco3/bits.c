/* WHICH BIT KILLS THE DISK: the task select, or the MMU enable? One variable
   at a time, which is the lesson of this whole target. */
#include "../../core/storage.h"

#define INIT0 (*(unsigned char *)0xFF90)
#define INIT1 (*(unsigned char *)0xFF91)
#define MMU0  ((unsigned char *)0xFFA0)
#define r ((unsigned char *)0x1000)
static unsigned int got;
static unsigned char buf[512];
extern void plat_disk_reset(void);

int main(void)
{
    unsigned char i;

    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$3F00 }

    r[1] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);   /* baseline */

    /* (a) write the task-0 map only -- no control bits touched at all */
    for (i = 0; i < 8; i++) MMU0[i] = (unsigned char)(0x38 + i);
    r[2] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    /* (b) clear TR in INIT1, still no MMUEN */
    INIT1 = (unsigned char)(INIT1 & 0xFE);
    r[3] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    /* (c) now MMUEN */
    INIT0 = (unsigned char)(INIT0 | 0x40);
    r[4] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    r[5] = 0x5A;
    for (;;) ;
}

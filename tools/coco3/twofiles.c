/* ONE VARIABLE AT A TIME. Read STRINGS.DAT (tracks 0-1, known good) and then
   OVLDEMO.BIN (granule 4 = track 2), both with the MMU OFF. If the first
   works and the second hangs, the MMU was never the problem and the earlier
   "banking breaks the disk" conclusion was drawn from changing two things at
   once. */
#include "../../core/storage.h"

#define r ((unsigned char *)0x2F00)
static unsigned int got;
static unsigned char buf[8192];

int main(void)
{
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$3F00 }

    r[1] = 0xB1;
    r[2] = plat_read_all("STRINGS.DAT", buf, sizeof buf, &got);
    r[3] = (unsigned char)(got >> 8);
    r[4] = (unsigned char)(got & 0xFF);

    r[5] = 0xB5;
    r[6] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);
    r[7] = (unsigned char)(got & 0xFF);
    r[8] = buf[0];

    r[9] = 0x5A;
    for (;;) ;
}

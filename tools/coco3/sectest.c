/* Does standalone DSKCON read a real sector, with NO Disk BASIC ROM involved?
   Reads track 17 sector 3 -- the first directory sector -- into $3000 and
   leaves the status at $3100 for the host to dump. */
#include <dskcon-standalone.h>

int main(void)
{
    unsigned char *dst = (unsigned char *) 0x3000;
    unsigned char *sta = (unsigned char *) 0x3100;
    unsigned long h;

    sta[0] = 0xEE;                    /* so "never ran" is distinguishable */
    asm { orcc #$50 }                 /* init must run with interrupts masked */
    h = dskcon_init(dskcon_nmiService);

    DCOPC = 2;                        /* 2 = read */
    DCDRV = 0;
    DCTRK = 17;
    DCSEC = 3;
    DCBPT = dst;
    dskcon_processSector();
    sta[0] = DCSTA;
    sta[1] = 0x5A;                    /* reached the end */

    dskcon_shutdown(h);
    for (;;) ;
}

/* WHY DID THE TIMER STOP TICKING BETWEEN tmrtest.c AND tmr2.c?
 *
 * tmrtest.c counted timer expiries happily. tmr2.c wedged in a loop waiting
 * for the first one. THREE things changed at once between them -- a disk read
 * happened first, INIT0 went from $04 to $2C, and the timer period went from
 * $3FF to $FFF -- which is the same shape of mistake this target has punished
 * three times today. One variable at a time, and every loop BOUNDED so the
 * probe reports rather than hangs.
 *
 * Each case reports, in three bytes: timer expiries seen, V-BORD flags seen,
 * and whether the budget ran out. The budget is fixed, so the counts compare.
 */
#include "../../core/storage.h"

#define INIT0   (*(unsigned char *)0xFF90)
#define INIT1   (*(unsigned char *)0xFF91)
#define IRQENR  (*(unsigned char *)0xFF92)
#define FIRQENR (*(unsigned char *)0xFF93)
#define TMRHI   (*(unsigned char *)0xFF94)
#define TMRLO   (*(unsigned char *)0xFF95)
#define TMR_FLAG   0x20
#define VBORD_FLAG 0x08

#define r    ((unsigned char *)0x5F00)
#define DONE 0x5A

static unsigned int got;
static unsigned char buf[512];

/* Counts both flags over a fixed budget of polling passes. */
static void count(unsigned char slot, unsigned char hi, unsigned char lo)
{
    unsigned int pass, tmrs = 0, vbs = 0;

    IRQENR  = TMR_FLAG;
    FIRQENR = VBORD_FLAG;
    TMRHI = hi; TMRLO = lo;
    (void)IRQENR; (void)FIRQENR;

    for (pass = 0; pass < 20000; pass++) {
        if (IRQENR  & TMR_FLAG)   { if (tmrs != 65535) tmrs++; }
        if (FIRQENR & VBORD_FLAG) { if (vbs  != 65535) vbs++;  }
    }
    r[slot]     = (unsigned char)(tmrs > 255 ? 255 : tmrs);
    r[slot + 1] = (unsigned char)(vbs  > 255 ? 255 : vbs);
}

int main(void)
{
    unsigned int i;

    for (i = 0; i < 32; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$5E00 }
    asm { sta $FFDF }

    INIT1 = 0x20;

    INIT0 = 0x04;  count(1, 0x03, 0xFF);   /* A: as tmrtest had it        */
    INIT0 = 0x04;  count(3, 0x0F, 0xFF);   /* B: only the period changed  */
    INIT0 = 0x2C;  count(5, 0x0F, 0xFF);   /* C: only INIT0 changed       */

    INIT0 = 0x0C;
    r[7] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);
    INIT0 = 0x2C;  count(8, 0x0F, 0xFF);   /* D: after the disk has run   */

    INIT0 = 0x0C;
    r[31] = DONE;
    for (;;) ;
}

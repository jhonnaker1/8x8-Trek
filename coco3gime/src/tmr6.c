/* THE PIAs SHARE THE IRQ LINE, AND THE GIME's IEN DOES NOT GATE THEM.
 *
 * tmr5.c proved the handler IS reached -- its first statement lands -- and the
 * machine wedges anyway. That rules out the vector chain (and incidentally
 * showed $FFF8 holds $FEF7 and $FEF7 holds $16, an LBRA, so the CoCo 3's
 * vector page is LBRAs and a $7E JMP written over one works fine).
 *
 * A handler that is entered and never makes progress is a source that is not
 * being cleared. The GIME's own sources are cleared by reading $FF92 -- but
 * the two PIAs drive /IRQ DIRECTLY on this machine, outside IEN entirely, and
 * Disk BASIC leaves PIA0's 60 Hz field-sync interrupt enabled. Clearing I then
 * takes a PIA interrupt that reading $FF92 does nothing about, immediately and
 * for ever.
 *
 * So: disable every PIA interrupt enable and READ THE DATA REGISTERS to clear
 * what is already pending, before unmasking. The port polls its keyboard and
 * takes its tempo from the GIME's own V-BORD flag, so nothing wants them.
 *
 * Reported: whether the handler ran, how many times, and whether the disk
 * still works afterwards -- because a driver that silences the drive is no
 * better than no driver.
 */
#include "../../core/storage.h"

#define INIT0    (*(unsigned char *)0xFF90)
#define INIT1    (*(unsigned char *)0xFF91)
#define IRQENR   (*(unsigned char *)0xFF92)
#define TMR_FLAG 0x20

#define PIA0_DA  (*(unsigned char *)0xFF00)
#define PIA0_CRA (*(unsigned char *)0xFF01)
#define PIA0_DB  (*(unsigned char *)0xFF02)
#define PIA0_CRB (*(unsigned char *)0xFF03)
#define PIA1_DA  (*(unsigned char *)0xFF20)
#define PIA1_CRA (*(unsigned char *)0xFF21)
#define PIA1_DB  (*(unsigned char *)0xFF22)
#define PIA1_CRB (*(unsigned char *)0xFF23)

#define r    ((unsigned char *)0x5F00)
#define DONE 0x5A

static unsigned int got;
static unsigned char buf[512];
static unsigned int fired;
static unsigned char sink;

interrupt void tick(void)
{
    sink = IRQENR;                    /* the read is what clears the source */
    fired++;
}

/* Every PIA interrupt enable off, and every pending flag read away. The data
   register read is the clear; disabling the enable alone leaves a latched
   flag asserting the line for ever. */
static void pia_quiet(void)
{
    PIA0_CRA = (unsigned char)(PIA0_CRA & 0xFE);
    PIA0_CRB = (unsigned char)(PIA0_CRB & 0xFE);
    PIA1_CRA = (unsigned char)(PIA1_CRA & 0xFE);
    PIA1_CRB = (unsigned char)(PIA1_CRB & 0xFE);
    sink = PIA0_DA;
    sink = PIA0_DB;
    sink = PIA1_DA;
    sink = PIA1_DB;
}

int main(void)
{
    unsigned int i, pass;

    for (i = 0; i < 32; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$5E00 }
    asm { sta $FFDF }

    INIT1 = 0x20;
    INIT0 = 0x0C;
    r[1] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);
    asm { orcc #$50 }

    fired = 0;
    *((unsigned char *)0xFEF7) = 0x7E;
    *((void **)0xFEF8) = (void *)tick;

    pia_quiet();

    IRQENR = TMR_FLAG;
    *((unsigned char *)0xFF94) = 0x0F;
    *((unsigned char *)0xFF95) = 0xFF;
    sink = IRQENR;
    INIT0 = 0x2C;
    asm { andcc #$EF }

    for (pass = 0; pass < 20000; pass++) { r[13] = (unsigned char)pass; }

    asm { orcc #$10 }
    INIT0 = 0x0C;
    IRQENR = 0x00;

    r[10] = (unsigned char)(fired ? 0xE1 : 0x00);
    r[11] = (unsigned char)(fired >> 8);
    r[12] = (unsigned char)(fired & 0xFF);

    r[14] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    r[31] = DONE;
    for (;;) ;
}

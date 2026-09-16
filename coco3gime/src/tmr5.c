/* IS THE HANDLER REACHED AT ALL?
 *
 * tmr4.c case F wedged with a handler installed, interrupts unmasked and the
 * timer armed. The generated code for the handler is correct -- `LDB $FF92`
 * survives, so the source is cleared -- which leaves two very different
 * failures wearing one symptom:
 *
 *   the IRQ vector chain does not reach `tick`, so the CPU jumps into
 *   whatever $FEF7 happens to hold and never comes back;   OR
 *   `tick` runs, and runs, and the machine never leaves it.
 *
 * A marker written as the FIRST thing the handler does tells them apart, and
 * an OUTER GUARD means the probe reports either way instead of hanging. That
 * is the lesson from reading findings off a probe that never finished.
 *
 * $FEF7 is also checked BEFORE anything is enabled: if the ROM's IRQ vector
 * does not actually point there in all-RAM mode, the whole approach is wrong
 * and the bytes will say so.
 */
#include "../../core/storage.h"

#define INIT0   (*(unsigned char *)0xFF90)
#define INIT1   (*(unsigned char *)0xFF91)
#define IRQENR  (*(unsigned char *)0xFF92)
#define TMR_FLAG 0x20

#define r    ((unsigned char *)0x5F00)
#define DONE 0x5A

static unsigned int got;
static unsigned char buf[512];
static unsigned int fired;

interrupt void tick(void)
{
    r[10] = 0xE1;                     /* reached, and this is the first thing */
    (void)IRQENR;
    fired++;
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

    /* WHAT DOES THE MACHINE THINK THE IRQ VECTOR IS? Read before writing. */
    r[1] = *((unsigned char *)0xFFF8);
    r[2] = *((unsigned char *)0xFFF9);
    r[3] = *((unsigned char *)0xFEF7);          /* should be $7E, a JMP */
    r[4] = *((unsigned char *)0xFEF8);
    r[5] = *((unsigned char *)0xFEF9);

    r[6] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);
    asm { orcc #$50 }

    fired = 0;
    *((unsigned char *)0xFEF7) = 0x7E;
    *((void **)0xFEF8) = (void *)tick;
    IRQENR = TMR_FLAG;
    *((unsigned char *)0xFF94) = 0x0F;
    *((unsigned char *)0xFF95) = 0xFF;
    (void)IRQENR;
    INIT0 = 0x2C;
    asm { andcc #$EF }

    for (pass = 0; pass < 20000; pass++) { }    /* just let time go by */

    asm { orcc #$10 }
    INIT0 = 0x0C;
    IRQENR = 0x00;

    r[11] = (unsigned char)(fired >> 8);
    r[12] = (unsigned char)(fired & 0xFF);

    r[31] = DONE;
    for (;;) ;
}

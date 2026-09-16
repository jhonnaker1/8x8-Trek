/* DOES THE GIME TIMER TICK, AND UNDER WHAT ENABLE?
 *
 * dactest.c polled $FF92 for the TMR flag and never saw one in three tries.
 * The likely reason is that $FF92 is TWO registers at one address: a WRITE
 * sets which sources are enabled, a READ returns which are pending and clears
 * them -- and nothing had ever written the mask, so no source was armed. That
 * is a guess, and this measures it instead.
 *
 * Four configurations, one variable each, each given a bounded wait and asked
 * how many timer expiries it saw:
 *
 *   A  $FF92 mask = TMR, IEN clear     does the flag latch unarmed at INIT0?
 *   B  $FF92 mask = TMR, IEN set       armed, CPU interrupts still masked
 *   C  $FF93 mask = TMR, FEN set       the FIRQ side, same question
 *   D  no mask at all, IEN set         the control: is the mask what matters?
 *
 * The CPU's I and F flags stay SET throughout. Nothing here takes an
 * interrupt; the question is only whether the flag becomes readable, because
 * a driver that can poll needs no vectors and stays out of the way of the
 * standalone DSKCON NMI.
 *
 * Each result is the number of expiries seen in a fixed budget, capped at
 * 255, so "it ticks" and "it ticks at roughly the right rate" are not the
 * same answer.
 */
#include "../../core/storage.h"

#define INIT0  (*(unsigned char *)0xFF90)
#define INIT1  (*(unsigned char *)0xFF91)
#define IRQENR (*(unsigned char *)0xFF92)
#define FIRQENR (*(unsigned char *)0xFF93)
#define TMRHI  (*(unsigned char *)0xFF94)
#define TMRLO  (*(unsigned char *)0xFF95)
#define TMR_FLAG 0x20

#define r    ((unsigned char *)0x5F00)
#define DONE 0x5A

/* Counts expiries over a fixed number of polling passes. The budget is in
   passes, not in time, so the four cases are directly comparable. */
static unsigned char ticks(volatile unsigned char *reg)
{
    unsigned char seen = 0;
    unsigned int outer;
    unsigned int inner;

    TMRHI = 0x03;                     /* $3FF = 1023 counts */
    TMRLO = 0xFF;
    (void)*reg;

    for (outer = 0; outer < 200; outer++) {
        for (inner = 0; inner < 200; inner++) {
            if (*reg & TMR_FLAG) {
                if (seen != 255) seen++;
                break;
            }
        }
    }
    return seen;
}

int main(void)
{
    unsigned int i;

    for (i = 0; i < 32; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$5E00 }
    asm { sta $FFDF }

    INIT1 = 0x20;                     /* TINS: 1.7897725 MHz */

    INIT0 = 0x04;  IRQENR = 0x20;  FIRQENR = 0x00;
    r[1] = ticks((volatile unsigned char *)0xFF92);       /* A */

    INIT0 = 0x24;  IRQENR = 0x20;  FIRQENR = 0x00;
    r[2] = ticks((volatile unsigned char *)0xFF92);       /* B */

    INIT0 = 0x14;  IRQENR = 0x00;  FIRQENR = 0x20;
    r[3] = ticks((volatile unsigned char *)0xFF93);       /* C */

    INIT0 = 0x24;  IRQENR = 0x00;  FIRQENR = 0x00;
    r[4] = ticks((volatile unsigned char *)0xFF92);       /* D, the control */

    INIT0 = 0x04;                     /* leave the disk reachable */
    r[31] = DONE;
    for (;;) ;
}

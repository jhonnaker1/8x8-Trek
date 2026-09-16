/* THE LAST TWO UNKNOWNS BEFORE THE SOUND DRIVER EXISTS.
 *
 * tmrtest.c established that a GIME interrupt flag latches from its ENABLE
 * MASK alone -- $FF92/$FF93 written, IEN/FEN in INIT0 irrelevant to whether
 * the flag becomes readable. That gives the driver two independent sources:
 * the TIMER as a real IRQ to toggle the DAC, and V-BORD polled for tempo, on
 * the other register so the IRQ handler's read cannot steal it.
 *
 *   1. Does V-BORD (bit 3) latch in $FF93 with FEN CLEAR, at about 60 Hz?
 *      tmrtest proved the TIMER latches unarmed on $FF92. Assuming the other
 *      bit on the other register behaves the same way is exactly the step
 *      that has been wrong three times on this target.
 *   2. Does the disk still read with INIT0 = $2C -- MC2 + MC3 + IEN -- which
 *      is what the port has to run to take an interrupt at all? lowinit.c
 *      measured $04, $0C and $8C. $2C is a value nothing has tried.
 *
 * The V-BORD count is taken over a wall-clock interval measured by the TIMER
 * itself, so the answer is a RATE and not just "it moves".
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

int main(void)
{
    unsigned int i, tmrs, vborders;

    for (i = 0; i < 32; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    asm { orcc #$50 }                 /* the CPU takes nothing; this is polling */
    asm { lds #$5E00 }
    asm { sta $FFDF }

    INIT1 = 0x20;                     /* TINS: 1.7897725 MHz */
    INIT0 = 0x2C;                     /* MC2 + MC3 + IEN, the port's value */

    r[1] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);   /* Q2 */

    /* Q1. The timer at $FFF counts is 4096 ticks = 2.289 ms, so 1,000 of them
       is 2.289 s and a 60 Hz field should show about 137 borders. Anything
       near zero means the flag does not latch with FEN clear; anything near
       1,000 means I am reading a flag that is stuck on. */
    IRQENR  = 0x20;                   /* TMR on the IRQ side   */
    FIRQENR = VBORD_FLAG;             /* V-BORD on the FIRQ side, FEN clear */
    TMRHI = 0x0F; TMRLO = 0xFF;
    (void)IRQENR; (void)FIRQENR;

    tmrs = 0;
    vborders = 0;
    while (tmrs < 1000) {
        if (IRQENR & TMR_FLAG) tmrs++;
        if (FIRQENR & VBORD_FLAG) vborders++;
    }

    r[2] = (unsigned char)(vborders >> 8);
    r[3] = (unsigned char)(vborders & 0xFF);

    r[4] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);   /* still? */

    INIT0 = 0x0C;
    r[31] = DONE;
    for (;;) ;
}

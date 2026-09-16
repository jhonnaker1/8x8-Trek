/* DOES DSKCON HAND BACK A MACHINE WITH INTERRUPTS ENABLED?
 *
 * tmr3.c: arming the timer with IEN set works before any disk read and wedges
 * the machine after one. The counting loop is bounded, so it cannot be the
 * loop -- the machine must be leaving the code entirely. The obvious candidate
 * is that plat_read_all returns with the CPU's I flag CLEAR, so setting IEN
 * then produces a real IRQ that vectors through $FEF7 -> $010C, which nothing
 * has ever written.
 *
 * That is a guess. These two cases separate it, and they are also exactly the
 * two ways the driver could be built:
 *
 *   E  re-mask interrupts after the disk read, then arm      (polling only)
 *   F  install a handler at $FEF7, then UNMASK and arm       (the real thing)
 *
 * If E survives and F survives, the diagnosis holds and the driver can take a
 * real interrupt. If F wedges, the handler is not being reached and the vector
 * chain is wrong.
 *
 * The handler counts, so F also proves the interrupt is DELIVERED rather than
 * merely not crashing -- a handler that is never called looks exactly like a
 * machine that is working fine.
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
static unsigned int fired;

/* cmoc's `interrupt` emits RTI and saves NOTHING -- which is safe for IRQ,
   because the 6809 stacks the entire register set on an IRQ and RTI restores
   it. It would silently corrupt the interrupted code on an FIRQ, which stacks
   only PC and CC. That is why this is on the IRQ side. */
interrupt void tick(void)
{
    (void)IRQENR;                     /* the read is what clears the source */
    fired++;
}

static void count(unsigned char slot)
{
    unsigned int pass, tmrs = 0, vbs = 0;

    FIRQENR = VBORD_FLAG;
    TMRHI = 0x0F; TMRLO = 0xFF;
    (void)IRQENR; (void)FIRQENR;

    for (pass = 0; pass < 20000; pass++) {
        if (IRQENR  & TMR_FLAG)   { if (tmrs < 65535) tmrs++; }
        if (FIRQENR & VBORD_FLAG) { if (vbs  < 65535) vbs++;  }
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
    INIT0 = 0x0C;
    r[1] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    /* E: mask again, then arm. */
    asm { orcc #$50 }
    IRQENR = TMR_FLAG;
    INIT0 = 0x2C;
    count(2);

    /* F: a handler, then unmask, then arm. F stays SET -- FIRQ is not used
       and DSKCON's NMI is not maskable anyway. */
    INIT0 = 0x0C;
    IRQENR = 0x00;
    fired = 0;
    *((unsigned char *)0xFEF7) = 0x7E;              /* JMP */
    *((void **)0xFEF8) = (void *)tick;
    IRQENR = TMR_FLAG;
    INIT0 = 0x2C;
    asm { andcc #$EF }                              /* I clear: take IRQs */
    count(4);
    asm { orcc #$10 }

    r[6] = (unsigned char)(fired >> 8);
    r[7] = (unsigned char)(fired & 0xFF);

    INIT0 = 0x0C;
    r[8] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);   /* still? */

    r[31] = DONE;
    for (;;) ;
}

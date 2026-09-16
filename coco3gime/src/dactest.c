/* CAN THIS MACHINE MAKE A NOTE, AND CAN THE GIME TIMER PACE IT?
 *
 * There is no sound chip. The CoCo's audio is a 6-bit DAC in PIA1's side A at
 * $FF20 (bits 7..2), and a tone exists only while something toggles it. So
 * two questions, and the driver's whole shape depends on the second:
 *
 *   1. Does the PIA path actually reach the speaker? Mux select in PIA0's
 *      CA2/CB2, enable in PIA1's CB2, DDRA for the top six bits.
 *   2. **Do the GIME timer's interrupt FLAGS latch while IEN is clear?**
 *      $FF92 read returns pending sources and clears them. If the TMR flag
 *      (bit 5) latches with interrupts disabled, the driver can POLL it and
 *      needs no vectors at all -- which matters, because this port runs in
 *      all-RAM mode with a standalone DSKCON that takes an NMI per sector and
 *      I would rather not put a second handler in its way.
 *
 * A BUSY LOOP IS NOT AN ACCEPTABLE TIMING SOURCE HERE and that is why this
 * probe exists rather than a delay loop: cmoc's codegen decides the cycle
 * count, so the pitch would be whatever the optimiser felt like. The GIME
 * timer is a real clock.
 *
 *     TINS (INIT1 bit 5) = 1  ->  14.31818 MHz / 8 = 1.7897725 MHz
 *     counts = 1789772 / (2 * f)
 *
 * 200 Hz needs 4474 counts and the timer is 12 bits, so low notes take a
 * software divisor. That is measured here too, at the bottom of the range.
 *
 * THREE POINTS, NOT ONE. This project shipped an octave flat for four months
 * off a single-point check.
 *
 * The report says only whether the timer ticked; the PITCH is read off a WAV
 * recorded from the emulator, because "it makes a noise" is not a frequency.
 */
#include "../../core/storage.h"

#define INIT0  (*(unsigned char *)0xFF90)
#define INIT1  (*(unsigned char *)0xFF91)
#define IRQENR (*(unsigned char *)0xFF92)   /* read: pending, and clears */
#define TMRHI  (*(unsigned char *)0xFF94)   /* write starts the timer */
#define TMRLO  (*(unsigned char *)0xFF95)

#define PIA0_CRA (*(unsigned char *)0xFF01)
#define PIA0_CRB (*(unsigned char *)0xFF03)
#define PIA1_DA  (*(unsigned char *)0xFF20)
#define PIA1_CRA (*(unsigned char *)0xFF21)
#define PIA1_CRB (*(unsigned char *)0xFF23)

#define TMR_FLAG 0x20
#define DAC_HI   0xFC
#define DAC_LO   0x00

#define r    ((unsigned char *)0x5F00)
#define DONE 0x5A

static void sound_on(void)
{
    /* DDRA first: bit 2 of the control register picks DDR over data. */
    PIA1_CRA = (unsigned char)(PIA1_CRA & 0xFB);
    PIA1_DA  = 0xFC;                       /* top six bits are outputs */
    PIA1_CRA = (unsigned char)(PIA1_CRA | 0x04);

    /* The analogue mux picks DAC / cassette / cartridge from PIA0's CA2 and
       CB2. Both low is the DAC. Bit 2 keeps the data register selected; bits
       5 and 4 put the control line in output mode and bit 3 is its level. */
    PIA0_CRA = 0x34;
    PIA0_CRB = 0x34;
    PIA1_CRB = 0x3C;                       /* CB2 high: sound enabled */
    PIA1_DA  = DAC_LO;
}

static void sound_off(void)
{
    PIA1_DA  = DAC_LO;
    PIA1_CRB = 0x34;                       /* CB2 low again */
}

/* Toggles the DAC `halves` times, one per timer expiry, dividing the timer
   rate by `div` so notes below ~218 Hz are reachable. Returns 1 if the timer
   stopped answering. */
static unsigned char tone(unsigned int counts, unsigned char div,
                          unsigned int halves)
{
    unsigned char level = 0;
    unsigned char n;
    unsigned int guard;

    TMRHI = (unsigned char)((counts >> 8) & 0x0F);
    TMRLO = (unsigned char)(counts & 0xFF);
    (void)IRQENR;                          /* start from no pending flag */

    while (halves) {
        for (n = 0; n < div; n++) {
            guard = 0;
            while (!(IRQENR & TMR_FLAG))
                if (++guard == 0) { sound_off(); return 1; }
        }
        level = (unsigned char)(level ? 0 : 1);
        PIA1_DA = level ? DAC_HI : DAC_LO;
        halves--;
    }
    PIA1_DA = DAC_LO;
    return 0;
}

static void quiet(unsigned int halves)
{
    unsigned int guard;
    PIA1_DA = DAC_LO;
    while (halves) {
        guard = 0;
        while (!(IRQENR & TMR_FLAG)) if (++guard == 0) return;
        halves--;
    }
}

int main(void)
{
    unsigned int i;

    for (i = 0; i < 32; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$5E00 }
    asm { sta $FFDF }

    INIT0 = 0x04;                          /* MC2 kept -- see gimevid.c */
    INIT1 = 0x20;                          /* TINS: the 1.7897725 MHz clock */
    sound_on();

    /* 1789772 / (2f), and half a second of each. */
    r[1] = tone(2034, 1, 440);             /* 440 Hz  */
    quiet(200);
    r[2] = tone(895,  1, 1000);            /* 1000 Hz */
    quiet(200);
    r[3] = tone(2237, 2, 200);             /* 200 Hz, divided down */
    sound_off();

    r[31] = DONE;
    for (;;) ;
}

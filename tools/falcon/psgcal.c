/* Calibrate the Falcon's YM2149: play three known tone PERIODS in turn and
   let the host measure what comes out. clock = f * 16 * period, and three
   points separate a wrong clock from a wrong intercept -- the X16's beep
   looked like a pitch error and was partly a free first tick. */
#include <tos.h>
#include <stdio.h>

#define PSG_W(r, v)  Giaccess((WORD)(v), (WORD)((r) | 0x80))

static void tone(int period, int hold_ticks)
{
    long t0;

    PSG_W(0, period & 0xFF);
    PSG_W(1, (period >> 8) & 0x0F);
    PSG_W(7, 0xFE);              /* tone A on, everything else off */
    PSG_W(8, 15);                /* channel A at full, fixed volume */

    t0 = *(volatile long *)0x4BAL;
    while (*(volatile long *)0x4BAL - t0 < hold_ticks)
        ;
    PSG_W(8, 0);                 /* silence between tones */
    t0 = *(volatile long *)0x4BAL;
    while (*(volatile long *)0x4BAL - t0 < 100)
        ;
}

static long run(void)
{
    long t0 = *(volatile long *)0x4BAL;

    /* Four seconds of silence first, so the host can start recording after
       the program is already running and still catch every tone. */
    while (*(volatile long *)0x4BAL - t0 < 800)
        ;

    /* Periods chosen for the four corners of what the MUSIC actually uses --
       90Hz to 930Hz -- rather than round numbers. Calibrating inside the
       range that matters is the point; three points separate a wrong scale
       from a wrong intercept and four leaves one spare. */
    tone(1389, 300);       /* want  90.0 Hz at a 2MHz clock */
    tone(431,  300);       /* want 290.0 Hz -- the commonest note */
    tone(212,  300);       /* want 589.6 Hz */
    tone(134,  300);       /* want 933.0 Hz */
    return 0;
}

int main(void)
{
    printf("PSGCAL periods 1389 431 212 134\r\n");
    Supexec(run);                /* $4BA and the PSG both want supervisor */
    printf("DONE\r\n");
    return 0;
}

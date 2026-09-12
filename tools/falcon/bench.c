/* Falcon blit cost, timed on the 200Hz system tick. */
#include <tos.h>
#include <stdio.h>

#define W 640
#define H 480
#define STRIDE (W/2)
#define SCRBYTES ((long)STRIDE*H)

static long hz200(void) { return *(volatile long *)0x4BAL; }

int main(void)
{
    int  old = VsetMode(-1);
    long t0, t1, i;
    long *scr;
    long n;
    int  reps = 50;

    VsetMode(VGA | COL80 | BPS4);
    scr = (long *)Physbase();

    /* full-screen clear, long writes */
    t0 = Supexec(hz200);
    for (i = 0; i < reps; i++)
        for (n = 0; n < SCRBYTES / 4; n++)
            scr[n] = 0;
    t1 = Supexec(hz200);
    VsetMode(old);
    printf("full-screen clear: %ld reps in %ld ticks (200Hz)\r\n", (long)reps, t1 - t0);
    printf("  = %ld ms each, %ld bytes\r\n",
           ((t1 - t0) * 5) / reps, SCRBYTES);
    printf("DONE\r\n");
    return 0;
}

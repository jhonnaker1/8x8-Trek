/* Does the Amiga driver's tick arithmetic keep the original's tempo?
 *
 * WHY THIS EXISTS AS A HOST TEST. The tracks are written in ticks of the PC's
 * 18.2065Hz timer, and BOTH PREVIOUS PORTS SHIPPED A TEMPO BUG -- the C128
 * lost time to a driver three semitones from its cause, the MEGA65 to a
 * raster that wraps twice a frame. Neither was caught by listening, because
 * "a bit fast" is not obvious in music nobody has heard before; both were
 * caught by counting. So the conversion from the Amiga's 50Hz DateStamp ticks
 * to 18.2065Hz music ticks is counted here, on the host, where it costs
 * nothing to run on every build.
 *
 * This is the arithmetic from amigasnd.c's snd_poll(), copied deliberately:
 * a test that #includes the driver would drag in Paula, dos.library and the
 * whole Amiga toolchain, and the thing under test is six lines of integer
 * maths. If those six lines change, this must be changed with them -- which
 * is the point, because the change is exactly when it needs re-checking.
 */
#include <stdio.h>
#include <stdlib.h>

/* 18.2065 ticks a second against 50 of these: 18207/50000 per fiftieth. */
static unsigned long acc;

static int ticks_for(unsigned long fiftieths) {
    int n = 0;
    acc += fiftieths * 18207UL;
    while (acc >= 50000UL) { acc -= 50000UL; n++; }
    return n;
}

static int fails;

static void ok(const char *what, int cond) {
    printf("  %-52s %s\n", what, cond ? "ok" : "FAIL");
    if (!cond) fails++;
}

int main(void) {
    int i, total;
    double rate;

    printf("amiga tempo:\n");

    /* SIXTY SECONDS, one fiftieth at a time -- which is how snd_poll() is
       actually driven from the key loop. 18.2065 * 60 = 1092.4 ticks. */
    acc = 0;
    total = 0;
    for (i = 0; i < 50 * 60; i++) total += ticks_for(1);
    rate = total / 60.0;
    printf("  %d ticks in 60s = %.4f Hz (want 18.2065)\n", total, rate);
    ok("within 0.1% of the original's timer", rate > 18.188 && rate < 18.225);

    /* THE SAME ANSWER IN LUMPS. A disk load blocks for many fiftieths at once
       and snd_poll() then sees them together; if the accumulator dropped the
       remainder the music would run slow by whatever the game was doing. */
    acc = 0;
    total = 0;
    for (i = 0; i < 60 * 2; i++) total += ticks_for(25);
    printf("  %d ticks in 60s arriving 25 at a time\n", total);
    ok("lumpy arrival gives the same count", total >= 1090 && total <= 1094);

    /* And that a single fiftieth never produces a tick on its own more than
       once -- 18207/50000 is less than one, so at most one tick per call at
       this granularity. A driver that ticked per CALL rather than per elapsed
       time is exactly the MEGA65's bug. */
    acc = 0;
    for (i = 0; i < 200; i++)
        if (ticks_for(1) > 1) { ok("one fiftieth never yields two ticks", 0); break; }
    if (i == 200) ok("one fiftieth never yields two ticks", 1);

    printf(fails ? "amiga tempo: FAILURES\n" : "amiga tempo: all checks passed\n");
    return fails ? 1 : 0;
}

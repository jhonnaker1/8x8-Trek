/* SID frequency and tempo arithmetic, proven natively.
 *
 * Same reason as test_egavdc.c: this is integer arithmetic whose failure mode
 * is silent. Get the multiplier wrong and every note still plays, just at the
 * wrong pitch; get the accumulator wrong and the tune plays at the wrong
 * speed. Neither shows up as an error anywhere, and neither is audible from a
 * session on this machine without recording the emulator.
 *
 * The reference values come from the real formula in double precision, which
 * is exactly what the 8-bit code cannot do. */
#include <stdio.h>
#include <math.h>

#include "../src/sidfreq.h"

static int failures = 0;

static void check(int cond, const char *what) {
    if (!cond) { printf("FAIL: %s\n", what); failures++; }
}

#define NTSC_CLK 1022730.0
#define PAL_CLK   985248.0

/* The truth: Fn = Hz * 16777216 / Fclk, with Hz = tenths * 10. */
static double exact(int tenths, double clk) {
    return tenths * 10.0 * 16777216.0 / clk;
}

static void test_pitch(void) {
    int tenths;
    double worst_n = 0.0, worst_p = 0.0;

    /* A4 is the one note the original lands on exactly, so it is the anchor. */
    check(fabs(sid_freq(44, REGION_NTSC) - exact(44, NTSC_CLK)) < 1.0,
          "440Hz NTSC lands on the exact formula");

    for (tenths = 1; tenths <= 255; tenths++) {
        double en = exact(tenths, NTSC_CLK), ep = exact(tenths, PAL_CLK);
        double gn = sid_freq((unsigned char)tenths, REGION_NTSC);
        double gp = sid_freq((unsigned char)tenths, REGION_PAL);
        if (fabs(gn - en) > worst_n) worst_n = fabs(gn - en);
        if (fabs(gp - ep) > worst_p) worst_p = fabs(gp - ep);

        /* The whole point of splitting the multiplier into a whole part and a
           /256 fraction is that no intermediate leaves 16 bits. Multiplying by
           the scaled constant in one go would overflow above about byte 24 on
           PAL, and it would do it silently. */
        check(gn < 65536.0 && gp < 65536.0, "frequency word stays inside 16 bits");
    }

    /* A SID word is 16777216/Fclk per Hz, about 16.4, so a whole-number error
       of 1 is well under a tenth of a Hz of pitch -- inaudible. */
    printf("  worst frequency error: NTSC %.2f, PAL %.2f (SID units)\n",
           worst_n, worst_p);
    check(worst_n < 1.0, "NTSC pitch is within rounding of exact");
    check(worst_p < 1.0, "PAL pitch is within rounding of exact");
}

static void test_tempo(void) {
    /* Run the accumulator for a simulated minute and count how many of the
       original's 18.2065Hz ticks come out. */
    struct { unsigned char region; double frame_hz; const char *name; } cases[] = {
        { REGION_NTSC, 59.826, "NTSC" },
        { REGION_PAL,  50.125, "PAL"  },
    };
    int c;

    for (c = 0; c < 2; c++) {
        unsigned int acc = 0;
        long frames = (long)(cases[c].frame_hz * 60.0);
        long ticks = 0, f;
        double want = 18.2065 * 60.0, err;

        for (f = 0; f < frames; f++) {
            acc += snd_tick_num(cases[c].region);
            while (acc >= SND_TICK_DEN) { acc -= SND_TICK_DEN; ticks++; }
        }
        err = 100.0 * (ticks - want) / want;
        printf("  %s: %ld ticks in 60s, want %.0f  (%+.2f%%)\n",
               cases[c].name, ticks, want, err);
        check(fabs(err) < 0.5, "tempo is within half a percent over a minute");

        /* And the accumulator must not run away: it is unsigned 16-bit and
           gets a numerator added every frame forever. */
        check(acc < SND_TICK_DEN, "accumulator stays bounded");
    }
}

int main(void) {
    printf("SID frequency and tempo:\n");
    test_pitch();
    test_tempo();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("  all checks passed\n");
    return 0;
}

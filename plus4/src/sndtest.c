/* DOES IT PLAY THE RIGHT NOTES? The arithmetic in tedsnd.c says 440.4, 998.8
 * and 200.1 Hz; this finds out whether the machine agrees.
 *
 * THREE POINTS, NOT ONE. A single check cannot see an octave error, and this
 * project has shipped one in each direction -- the X16 an octave FLAT for four
 * months, the CoCo 3's first build an octave SHARP because the GIME's timer
 * clock was half what the datasheet reading suggested.
 *
 * TED's counter is the other way round from every other chip here -- N RISES
 * with pitch -- so an inverted formula would play the tune's intervals inside
 * out. That sounds wrong without sounding broken, and only a measurement
 * separates it from a tuning error.
 *
 * Includes the driver rather than linking it, so it can reach voice_note and
 * hold a known pitch; everything else goes through the public seam.
 */
#include "tedsnd.c"

static void fields(unsigned int n)
{
    unsigned int guard, r, last = TED_RASTER;
    while (n) {
        guard = 0;
        for (;;) {
            r = TED_RASTER;
            if (r < last) { last = r; break; }
            last = r;
            if (++guard == 0) return;          /* bounded: a dead raster must
                                                  not hang the probe */
        }
        n--;
    }
}

int main(void)
{
    snd_init();

    /* A second each, with a clear gap, so the analysis can segment them
       without guessing. Voice 1 -- the music voice. */
    voice_note(0, 44);   fields(50); voice_off(0); fields(25);
    voice_note(0, 100);  fields(50); voice_off(0); fields(25);
    voice_note(0, 20);   fields(50); voice_off(0); fields(25);

    /* And voice 2, to prove the second generator is separately addressable --
       the thing the CoCo 3 could not do. */
    voice_note(1, 44);   fields(50); voice_off(1);

    for (;;) { }
    return 0;
}

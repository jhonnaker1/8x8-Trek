#ifndef SIDFREQ_H
#define SIDFREQ_H

/* SID frequency and tempo arithmetic, in a header so native `cc` can test it.
 *
 * Same precedent as egavdc.h: this is pure integer arithmetic with a silent
 * failure mode -- get it wrong and every note still plays, just at the wrong
 * pitch or the wrong speed -- so it is proven on the build machine rather than
 * by listening to an emulator.
 *
 * MEASURED 2026-08-22 by recording VICE to a WAV and measuring the pitch by
 * autocorrelation, because two things here are commonly assumed and one of the
 * assumptions is wrong:
 *
 *   1. The C128's 2MHz mode does NOT change SID pitch. This port runs the
 *      whole game at 2MHz, so it mattered. A note written for 1MHz played at
 *      424.8Hz in both modes -- ratio 0.997 across the pair.
 *   2. PAL and NTSC clock the SID differently, and it is audible: the same
 *      frequency word measured 424.8Hz on PAL and 440.4Hz on NTSC, against
 *      423.9 and 440.1 predicted. Half a semitone apart.
 *
 * So the port detects the region and scales at runtime rather than baking one
 * region's numbers into the data.
 */

/* SID: Fout = Fn * Fclk / 16777216, so Fn = Hz * 16777216 / Fclk.
 * The source data stores frequency as Hz/10 in a byte (the original's own
 * format), so the multiplier folds the 10 in:
 *
 *   NTSC  Fclk 1022730 -> Fn = byte * 164.044
 *   PAL   Fclk  985248 -> Fn = byte * 170.284
 *
 * Split into a whole part and a /256 fraction so nothing leaves 16 bits.
 * The worst case is a byte of 255: 255 * 170 = 43350 and 255 * 76 = 19380,
 * both inside 16 bits. Multiplying by the scaled constant in one go would not
 * be -- 255 * 43596 needs 24 bits, and cc65 would wrap it silently.
 *
 * test_sid.c checks every byte value against the formula in double precision,
 * and it earned its place immediately: the first version of these constants
 * had the NTSC multiplier as 164.067 rather than 164.044 and the PAL one as
 * 170.310 rather than 170.284, both arithmetic slips, and the test found them
 * before a note was ever played. */
#define SID_NTSC_WHOLE  164
#define SID_NTSC_FRAC    11     /* 0.044 * 256 */
#define SID_PAL_WHOLE   170
#define SID_PAL_FRAC     73     /* 0.284 * 256 */

#define REGION_NTSC 0
#define REGION_PAL  1

static unsigned int sid_freq(unsigned char tenths, unsigned char region) {
    unsigned int whole = (unsigned int)(region == REGION_PAL
                                        ? SID_PAL_WHOLE : SID_NTSC_WHOLE);
    unsigned int frac  = (unsigned int)(region == REGION_PAL
                                        ? SID_PAL_FRAC : SID_NTSC_FRAC);
    /* +128 rounds the fractional part rather than truncating it, which halves
       the worst-case error for nothing. Under 0.7 SID units across every byte
       value in both regions -- far below a cent of pitch. */
    return (unsigned int)(tenths * whole)
         + (unsigned int)(((tenths * frac) + 128) >> 8);
}

/* Tempo. The original's player is an ISR on the stock PC timer, 18.2065Hz.
 * This port has no interrupt of its own and polls the VIC raster instead, so a
 * "frame" is 50.125Hz on PAL and 59.826Hz on NTSC and neither divides the
 * original's tick. An accumulator carries the remainder:
 *
 *     acc += SND_TICK_NUM(region);
 *     while (acc >= SND_TICK_DEN) { acc -= SND_TICK_DEN; one original tick; }
 *
 * 18.2065/50.125 = 0.36322 and 18.2065/59.826 = 0.30433, so the numerators
 * below are within 0.06% -- about a third of a second's drift over the whole
 * 45-second title track, which no one can hear. */
#define SND_TICK_DEN  1000
#define SND_TICK_NTSC  304
#define SND_TICK_PAL   363

static unsigned int snd_tick_num(unsigned char region) {
    return (unsigned int)(region == REGION_PAL ? SND_TICK_PAL : SND_TICK_NTSC);
}

#endif

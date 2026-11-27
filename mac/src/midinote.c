/* The track format's frequency -> the Sound Manager's MIDI note.
 *
 * MUSIC.DAT stores each note as Hz/10 in a byte (tools/make_music.py), and
 * the square-wave synthesizer takes a MIDI note number -- 69 is A440,
 * MEASURED on the Quadra 800 at three points (220.0/440.0/880.0 Hz for
 * 57/69/81). There is no fractional pitch to want: make_music.py composes on
 * equal-tempered semitones, and wherever a byte belongs to ONE semitone the
 * nearest note IS the note that was meant -- the byte's own rounding (2.2%
 * sharp at D3, its header says) is undone rather than reproduced.
 *
 * BELOW ABOUT G3 TWO NOTES CAN SHARE A BYTE -- a 10 Hz step is wider than a
 * semitone there -- and then nothing can say which was meant: E3 and D#3 both
 * store as 16. The nearest note to the stored frequency is taken, which is no
 * further from the composer's note than every other port's 160 Hz. Of the 25
 * bytes MUSIC.DAT uses, ONE is shared (9, 90 Hz, in the torpedo effect,
 * which is not tuned to anything), measured 2026-11-26.
 *
 * Integer only -- a 68000 has no FPU and this runs once, at start-up. The
 * boundary between note n and n+1 is half a semitone above n, a quarter of a
 * whole tone: 440 * 2^((2(n-69)+1)/24). Q16 values of 2^(r/24) for r = 0..23
 * make every boundary a multiply and a shift. Pure C with no Toolbox in it,
 * so test/test_midi.c checks all 255 bytes against floating point on the
 * host -- in uint32_t, so the host's arithmetic is the 68000's.
 */
#include <stdint.h>

#include "midinote.h"

static const uint32_t quarter[24] = {
    65536, 67456, 69433, 71468, 73562, 75717, 77936, 80220,
    82570, 84990, 87480, 90043, 92682, 95398, 98193, 101070,
    104032, 107080, 110218, 113448, 116772, 120194, 123715, 127341
};

/* The upper edge of MIDI note n, in Hz * 256. */
static uint32_t upper_edge(int n)
{
    int e = 2 * (n - 69) + 1;                  /* quarter-tone steps from A440 */
    int q = (e >= 0) ? e / 24 : -((-e + 23) / 24);
    int r = e - 24 * q;                        /* 0..23 */
    /* 440 * 256 * quarter >> 16, written as 440 * quarter >> 8: the long
       form is 1.4e10 at the top of the table and OVERFLOWED 32 bits. It
       shipped to the first recording that way -- the host test passed,
       because a host `unsigned long` is 64 bits. Everything here is
       uint32_t now, which wraps at 32 bits on the host exactly as on the
       68000, so test/test_midi.c sees what the Mac sees. */
    uint32_t v = (UINT32_C(440) * quarter[r]) >> 8;

    return (q >= 0) ? (v << q) : (v >> -q);
}

void midi_table(unsigned char out[256])
{
    int t, n = 0;

    out[0] = 0;                                /* a rest stays a rest */
    for (t = 1; t < 256; t++) {
        uint32_t hz256 = (uint32_t)t * UINT32_C(10) * UINT32_C(256);
        while (n < 127 && hz256 >= upper_edge(n))
            n++;
        out[t] = (unsigned char)n;
    }
}

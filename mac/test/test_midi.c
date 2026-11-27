/* Host check of src/midinote.c: every byte of the track format must map to
 * the MIDI note floating point says is nearest, and a note composed on a
 * semitone must come back as THAT semitone wherever its byte is its own.
 *
 * The second check is the one that matters. make_music.py stores
 * round(Hz / 10); the driver has to recover the note the composer wrote, not
 * the one the rounding nudged towards. Every note up to C8 whose byte is its
 * own is pushed through the same rounding and must come back unchanged.
 *
 * And it must be able to FAIL: `-DBREAK_IT` shifts the table a semitone,
 * which both checks have to catch.
 */
#include <math.h>
#include <stdio.h>

#include "../src/midinote.h"

int main(void)
{
    unsigned char tab[256];
    int t, n, bad = 0, checked = 0;

    midi_table(tab);
#ifdef BREAK_IT
    for (t = 1; t < 256; t++) tab[t]++;
#endif

    for (t = 1; t < 256; t++) {
        double f = t * 10.0;
        int want = (int)floor(69.0 + 12.0 * log2(f / 440.0) + 0.5);
        if (want < 0) want = 0;
        if (tab[t] != want) {
            printf("byte %3d (%4d Hz): table %d, nearest %d\n", t, t * 10, tab[t], want);
            bad++;
        }
    }

    /* Every note whose byte NO OTHER NOTE shares. Where two share one (E3
       and D#3 both store as 16), the byte cannot say which was meant and
       there is nothing to recover; midinote.c says what happens there.
       The first draft asserted this from G2 up and failed on three such
       pairs -- which is how the comment in midinote.c got corrected. */
    for (n = 24; n <= 108; n++) {
        double hz = 440.0 * pow(2.0, (n - 69) / 12.0);
        int byte = (int)floor(hz / 10.0 + 0.5);
        int m, shared = 0;
        if (byte < 1 || byte > 255) continue;
        for (m = 0; m <= 127; m++)
            if (m != n && (int)floor(440.0 * pow(2.0, (m - 69) / 12.0) / 10.0 + 0.5) == byte)
                shared = 1;
        if (shared) continue;
        checked++;
        if (tab[byte] != n) {
            printf("note %d (%.1f Hz) stored as %d comes back as %d\n", n, hz, byte, tab[byte]);
            bad++;
        }
    }

    printf("test_midi: 255 bytes, %d composed notes round-tripped, %d wrong\n",
           checked, bad);
    return bad ? 1 : 0;
}

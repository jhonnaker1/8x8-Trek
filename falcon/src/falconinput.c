/* Keyboard for the Falcon.
 *
 * The seam is one blocking call: kb_waitkey() returns a single key and the
 * shared UI does the rest. Here that is BIOS Bconstat/Bconin on device 2, the
 * keyboard -- Bconin blocks and returns a long with the IKBD scan code in bits
 * 16..23 and the ASCII in the low byte, which is more than the seam needs but
 * exactly what the two arrow keys require, since those have no ASCII at all.
 *
 * WHY NOT Cconin: GEMDOS's console read handles ^C and ^S/^Q, which would let
 * a player suspend or kill the game mid-turn from inside a dialog. The BIOS
 * call sees the keyboard and nothing else.
 *
 * THE POLL LOOP IS NOT AN IDLE SPIN, it is where two things happen that
 * nothing else does -- and both were learned the hard way on other ports:
 *
 *   kb_entropy is this port's ONLY source of randomness. It counts passes and
 *   is sampled when the player answers the setup screen, so how long a human
 *   takes to reach a key is what picks the galaxy. Before the C128 had this,
 *   every game was the same one.
 *
 *   snd_poll() is the sound driver's only chance to run. On the ATARI 8-bit
 *   port it was missing from BOTH loops of kb_waitkey and the port shipped
 *   SILENT -- the driver had even been dead-stripped, so nothing pointed at
 *   the cause. It is called here from the first build, while the driver it
 *   calls is still a stub, precisely so that adding the real one is not also
 *   a chance to forget this.
 */
#include <stdint.h>
#include <tos.h>

#include "../../c128/src/input.h"
#include "../../c128/src/sid.h"

#define DEV_KBD  2

/* IKBD scan codes, high word of Bconin's result. Only the two keys with no
   character of their own are needed; everything else arrives as ASCII. */
#define SCAN_UP    0x48
#define SCAN_DOWN  0x50

uint16_t kb_entropy;

void kb_init(void) { }

/* The shared UI compares against upper-case letters. Fold here rather than in
   the UI, exactly as every other port does. */
static char fold(unsigned char c)
{
    if (c >= 'a' && c <= 'z')
        return (char)(c - 'a' + 'A');
    return (char)c;
}

char kb_waitkey(void)
{
    for (;;) {
        if (Bconstat(DEV_KBD)) {
            long  k    = Bconin(DEV_KBD);
            unsigned char scan  = (unsigned char)((k >> 16) & 0xFF);
            unsigned char ascii = (unsigned char)(k & 0xFF);

            if (scan == SCAN_UP)   return KB_UP;
            if (scan == SCAN_DOWN) return KB_DOWN;
            if (ascii)
                return fold(ascii);
            continue;                  /* a key with neither: ignore it */
        }

        /* Wait on the display rather than spinning as fast as the CPU will
           go: Vsync() blocks until the next vertical blank, so this passes
           some tens of times a second, which is the rate kb_entropy wants. */
        snd_poll();
        kb_entropy++;
        Vsync();
    }
}

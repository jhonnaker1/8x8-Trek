/* How long does this port take to paint the console, and what does the CoCo 3's
 * high-speed mode buy?
 *
 * Jamie played the port and said the one thing wrong with it is that it is
 * SLOW -- "main screen redraws and redraws of each panel for each turn". This
 * measures the thing he is describing, before anything is changed: a full
 * 80x25 of glyphs, timed in VDP vertical blanks, at 0.89 MHz and again at
 * 1.78 MHz.
 *
 * $FFD9 IS THE CoCo 3's HIGH-SPEED LATCH and this port has never written it,
 * so the 6809 has been running at half the speed the machine offers for its
 * entire life. $FFD8 puts it back.
 */
#include <stdint.h>

#include "../../c128/src/vdc.h"

#define RESULT  ((unsigned char *)0x7F00)
#define SPEED_FAST (*(unsigned char *)0xFFD9)
#define SPEED_SLOW (*(unsigned char *)0xFFD8)
#define VDP_PORT1  (*(unsigned char *)0xFF79)

void snd_poll(void) { }             /* the video seam pulls nothing else in */

/* Wait for a blank, paint, then count blanks until the paint is done. Done by
   painting N times and counting blanks across the whole run, which is robust
   against the flag latching. */
static unsigned int timed_paint(unsigned char passes)
{
    unsigned int frames = 0;
    unsigned char p, x, y;

    VDP_PORT1 = 0x00;
    VDP_PORT1 = 0x8F;
    while ((VDP_PORT1 & 0x80) == 0) { }      /* start on a blank edge */

    for (p = 0; p < passes; p++) {
        for (y = 0; y < VDC_ROWS; y++) {
            for (x = 0; x < VDC_COLS; x++) {
                scr_put(x, y, (unsigned char)(33 + ((x + y) & 31)), 15);
                if (VDP_PORT1 & 0x80) frames++;
            }
        }
    }
    return frames;
}

int main(void)
{
    unsigned int slow, fast;

    asm { orcc #$50 }
    RESULT[31] = 0;

    vdc_init();

    SPEED_SLOW = 0;                          /* 0.89 MHz, where it has always been */
    slow = timed_paint(2);
    RESULT[0] = (unsigned char)(slow >> 8);
    RESULT[1] = (unsigned char)(slow & 0xFF);

    SPEED_FAST = 0;                          /* 1.78 MHz */
    fast = timed_paint(2);
    RESULT[2] = (unsigned char)(fast >> 8);
    RESULT[3] = (unsigned char)(fast & 0xFF);

    SPEED_SLOW = 0;                          /* hand the machine back slow */
    RESULT[31] = 0x5A;
    for (;;) ;
    return 0;
}

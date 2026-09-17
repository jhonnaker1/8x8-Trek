/* WHICH OF $0400..$07FF DOES A PLUS/4 LEAVE ALONE?
 *
 * The soft stack has to live below $8000 -- above it, a read comes back as ROM
 * whenever the ROM is mapped, which is what was killing startup. $0400..$07FF
 * is the only kilobyte down there that is not the screen ($0C00), the colour
 * RAM ($0800), the hardware stack or the program ($1001 up). __stack was moved
 * there ON THAT REASONING AND NOTHING ELSE, and the machine then broke at
 * PC $000E. Reasoning from a memory map is the one step in this sequence that
 * was never measured, and it is the one that was wrong.
 *
 * So: fill the kilobyte, do the things the game does at startup -- a KERNAL
 * LOAD through the banked wrappers, and a screenful of drawing -- and report
 * which of the sixty-four 16-byte blocks still hold the fill.
 *
 * AND IT HAS A CONTROL. -DDOCALL=0 does the fill and the read with NOTHING in
 * between. Every block must come back 16 of 16. If it does not, the probe is
 * measuring itself and its other column means nothing -- which is exactly how
 * this port's zpprobe.c came to report "32 of 32 clobbered" and report the
 * same with the thing under test REMOVED.
 */
#include <stdint.h>

#include "../../c128/src/vdc.h"
#include "../../core/ega.h"

uint16_t far_load(const char *name);

#ifndef FILL_PAGES
#define FILL_PAGES 4
#endif

#ifndef DOCALL
#define DOCALL 1
#endif

/* In .lowbss, which the linker places below $1200 -- nowhere near the region
   under test, so the report cannot overwrite the thing it reports on. */
__attribute__((used, retain, section(".lowbss"))) unsigned char blk[64];
__attribute__((used, retain, section(".lowbss"))) unsigned char lf_done;
__attribute__((used, retain, section(".lowbss"))) unsigned char lf_far;

#define LOW ((volatile unsigned char *)0x0400)

int main(void)
{
    unsigned int i;
    unsigned char b, n;

    /* FILL_PAGES limits the fill: filling all four pages STOPPED far_load
       from completing at all, which is itself the answer in outline -- the
       KERNAL needs something in this kilobyte. This narrows it to which
       page. */
    for (i = 0; i < (unsigned int)FILL_PAGES * 256; i++) LOW[i] = 0xA5;

#if DOCALL
    /* The two things startup actually does through the KERNAL and the TED. */
    lf_far = (far_load("strings.dat") == 0xFFFF) ? 0xEE : 0x01;
    vdc_init();
    for (i = 0; i < 1000; i++)
        scr_put((unsigned char)(i % 40), (unsigned char)(i / 40),
                (unsigned char)(33 + (i & 31)), EGA_WHITE);
#endif

    for (b = 0; b < 64; b++) {
        n = 0;
        for (i = 0; i < 16; i++)
            if (LOW[(unsigned int)b * 16 + i] == 0xA5) n++;
        blk[b] = n;
    }
    lf_done = 0x5A;
    for (;;) { }
    return 0;
}

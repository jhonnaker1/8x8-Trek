/* exit_bisect.c -- which of this port's hardware pokes stops cc65's exit path
 * from handing the C128 back to BASIC?
 *
 * NOTES.md open item 2: `return 0` from main() wedges the machine, so the
 * port parks in a for(;;) and tells the player to use RUN/STOP+RESTORE. Three
 * suspects were named and never separated: the 2MHz switch, the direct VDC
 * register writes, and scanning CIA1 behind the KERNAL's back.
 *
 * This is the smallest program that can tell them apart. Build one stage at a
 * time with -DSTAGE=n; every stage ends in `return 0`, so a stage that comes
 * back to a working BASIC exonerates everything it did.
 *
 *   0  nothing at all -- the control. If this wedges, the fault is cc65's
 *      c128 exit path or the way we launch, not anything the port does.
 *   1  + 2MHz on and back off      (vdc.c: C128_CLKRATE)
 *   2  + the VDC register writes   (vdc.c: vdc_init/vdc_shutdown)
 *   3  + a CIA1 matrix scan        (input.c: key_down)
 *
 * Driven by tools/exit_bisect.py, which decides "came back to BASIC" by
 * typing a sum at the READY prompt and looking for the answer in screen RAM,
 * not by looking at a picture. A wedged machine can still show READY -- it
 * was printed before the program ran.
 */
#include <6502.h>

#ifndef STAGE
#define STAGE 0
#endif

#define C128_CLKRATE (*(unsigned char *)0xD030)
#define CIA1_PRA     (*(volatile unsigned char *)0xDC00)
#define CIA1_PRB     (*(volatile unsigned char *)0xDC01)

#if STAGE >= 2
#include "../src/vdc.h"
#endif

#if STAGE >= 1
static void spin(void) {
    unsigned int n;
    for (n = 0; n < 20000u; n++) { }
}
#endif

int main(void) {
#if STAGE >= 1
    /* Exactly what vdc_init/vdc_shutdown do, bit 0 only. Run at 2MHz for a
       moment first -- a set/clear pair with nothing between it would not
       exercise whatever the speed change disturbs. */
    C128_CLKRATE |= 0x01;
    spin();
#endif

#if STAGE >= 2
    /* The real driver's own init and shutdown, not a copy: screen and
       attribute base, attribute mode, background, and a full screen clear. */
    vdc_init();
    scr_puts(20, 12, "EXIT BISECT STAGE 2", 15);
    spin();
    vdc_shutdown();
#endif

#if STAGE >= 3
    /* input.c's key_down(), inlined. The interesting part is not the read but
       the SEI/CLI pair and writing CIA1_PRA while the KERNAL's 60Hz IRQ is
       also strobing it for the cursor. */
    {
        unsigned char save, i;
        for (i = 0; i < 25; i++) {
            SEI();
            save = CIA1_PRA;
            CIA1_PRA = (unsigned char)~(1 << (i & 7));
            (void)CIA1_PRB;
            CIA1_PRA = save;
            CLI();
        }
    }
#endif

#if STAGE >= 1
    C128_CLKRATE &= (unsigned char)~0x01;
#endif

    return 0;
}

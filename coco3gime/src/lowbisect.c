/* WHICH BYTES OF LOW RAM BREAK THE DISK? lowram.c established that 4,000
 * bytes written at $1000 stop standalone DSKCON finding a file, and that the
 * pattern SURVIVES -- so nothing overwrote us; we overwrote something. It
 * never asked WHICH something, and I wrote "worth a bisect if the route is
 * ever wanted again" into NOTES.md. It is wanted: 80-column text is 1,608
 * bytes over, and there are 10,240 bytes below $2800.
 *
 * EACH BLOCK IS TESTED INDEPENDENTLY, which is the whole design. A sequential
 * fill would find the FIRST fatal block and then report failure for every
 * block after it, which is indistinguishable from "all of low RAM is fatal" --
 * the shape of answer this target has produced wrongly twice. So each block is
 * SAVED, filled, tested, RESTORED, and tested again:
 *
 *     high nibble   the read WHILE the block held $AA
 *     low nibble    the read AFTER the original bytes were put back
 *
 * If a low nibble is ever non-zero the run is void from that point, because
 * the machine did not come back to the state the next block is measured
 * against. The probe says so rather than reporting 19 numbers of which only
 * the first few mean anything.
 *
 * $0000..$01FF IS NOT TESTED and that is not an oversight: it is the direct
 * page the 6809 and cmoc's runtime both use, plus the interrupt vector table
 * at $0100..$010F that items 37, 38 and 49 were all, in the end, about.
 * Filling it would crash the probe rather than measure anything.
 *
 * ALL-RAM VIA `sta $FFDF` RATHER THAN THE REAL LOADER, and that is allowed
 * here because lowram.c ran BOTH and recorded "Identical to the LOADM run."
 * The two environments agree on this question, so the cheaper one is honest.
 *
 * STOR_OK is 0, STOR_NOTFOUND 1, STOR_ERROR 2.
 */
#include "../../core/storage.h"

#define FIRST   0x0200
#define BLOCK   512
#define NBLOCK  19                      /* $0200..$27FF */
#define r       ((unsigned char *)0x5F00)
#define DONE    0x5A

static unsigned int got;
static unsigned char buf[512];
static unsigned char save[BLOCK];

int main(void)
{
    unsigned char b, filled, back;
    unsigned int i;
    unsigned char *p;

    for (i = 0; i < 32; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$5E00 }
    asm { sta $FFDF }

    /* The control. If this is not STOR_OK nothing below means anything, and
       lowram.c's own first run is the reason that line is here. */
    r[1] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    for (b = 0; b < NBLOCK; b++) {
        p = (unsigned char *)(FIRST + (unsigned int)b * BLOCK);

        for (i = 0; i < BLOCK; i++) save[i] = p[i];
        for (i = 0; i < BLOCK; i++) p[i] = 0xAA;

        filled = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

        for (i = 0; i < BLOCK; i++) p[i] = save[i];

        back = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

        r[2 + b] = (unsigned char)((filled << 4) | (back & 0x0F));
    }

    r[31] = DONE;
    for (;;) ;
}

/* WHICH BIT KILLS THE DISK -- ASKED PROPERLY THIS TIME.
 *
 * bits.c concluded "it is MMUEN alone" and that conclusion cannot stand,
 * because of the line that produced it:
 *
 *     INIT0 = (unsigned char)(INIT0 | 0x40);
 *
 * $FF90 DOES NOT READ BACK. NOTES.md item 30 establishes that on this machine
 * and says in as many words "never read-modify-write these; keep a RAM
 * shadow" -- and then bits.c read-modify-writes it. The read returns $1B, the
 * floating bus, so `| 0x40` writes $5B and sets FIVE bits:
 *
 *     MMUEN, FEN, MC3, MC1, MC0
 *
 * **MC3 PUTS VECTOR RAM AT $FE00..$FEFF.** That is exactly where this port's
 * interrupt LBRAs live -- $FEEE SWI3, $FEF1 SWI2, $FEF4 FIRQ, $FEF7 IRQ,
 * $FEFA SWI, $FEFD NMI -- and the standalone DSKCON driver takes an NMI per
 * sector. A bit that moves the machine's vectors out from under a driver that
 * uses them is a far better suspect than the one the probe was named after.
 *
 * So this writes EXPLICIT VALUES FROM A SHADOW, never a read-modify-write,
 * and separates the two candidates:
 *
 *     r[1]  baseline, nothing touched
 *     r[2]  INIT0 = $00 explicitly -- a clean value, no MMU, no MC3
 *     r[3]  + the task-0 map and INIT1 = $00
 *     r[4]  INIT0 = $40 -- MMUEN ALONE, and nothing else
 *     r[5]  INIT0 = $00 -- does the disk COME BACK?
 *     r[6]  INIT0 = $08 -- MC3 ALONE, no MMU at all
 *     r[7]  INIT0 = $00 again
 *
 * STOR_OK is 0, STOR_NOTFOUND 1, STOR_ERROR 2.
 */
#include "../../core/storage.h"

#define INIT0 (*(unsigned char *)0xFF90)
#define INIT1 (*(unsigned char *)0xFF91)
#define MMU0  ((unsigned char *)0xFFA0)
#define r ((unsigned char *)0x1000)

static unsigned int got;
static unsigned char buf[512];

int main(void)
{
    unsigned char i;

    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$3F00 }

    r[1] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    /* A clean INIT0, written not derived. If THIS breaks the disk then the
       fault is in the other bits and never was MMUEN. */
    INIT0 = 0x00;
    r[2] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    for (i = 0; i < 8; i++) MMU0[i] = (unsigned char)(0x38 + i);
    INIT1 = 0x00;
    r[3] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    INIT0 = 0x40;                       /* MMUEN alone */
    r[4] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    INIT0 = 0x00;                       /* off again -- does it recover? */
    r[5] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    INIT0 = 0x08;                       /* MC3 alone, no MMU */
    r[6] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    INIT0 = 0x00;
    r[7] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    r[8] = 0x5A;
    for (;;) ;
}

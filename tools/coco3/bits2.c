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
/* THE REPORT LIVES AT $2F00, NOT $1000, and that is not arbitrary. This probe
   is LOADMed and EXECed, so BASIC is still live -- and $1000 is BASIC's own
   memory under `CLEAR 25,&H2FFF`. coco3gime/src/lowram.c measured that
   collision today from the other side: 4,000 bytes written at $1000 broke the
   disk outright. tools/coco3/run.lua already uses $2F00 for banktest's
   results, so this follows the rig that works -- $3F00 here, because this
   build's code runs to $331E and $2F00 would be inside it. */
#define r ((unsigned char *)0x3F00)

static unsigned int got;
static unsigned char buf[512];

int main(void)
{
    unsigned char i;

    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$3E00 }   /* below the report, above the code */

    r[1] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    /* ALL-RAM MODE FIRST, WHICH IS THE PORT'S ACTUAL ENVIRONMENT. The first
       run of this probe showed INIT0 = $00 breaking the disk on its own --
       before MMUEN, before MC3 -- which points at MC1:MC0, the ROM MAP. With
       the ROM still mapped, changing which ROM sits at $A000..$FFFF moves the
       vectors the driver's NMI goes through, and that is a fault of the
       probe's environment rather than of the MMU.
       THE REAL PORT RUNS IN ALL-RAM MODE, where the ROM map means nothing.
       So $FFDF goes first and the bisect happens where the port lives. */
    asm { sta $FFDF }
    r[9] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    /* BIT 7 IS COCO, AND BASIC BOOTS WITH IT SET. Writing a "clean" $00
       clears it and switches the GIME's whole mode; that is what broke the
       disk in the previous run, not MMUEN and not the ROM map -- $00 was
       never a neutral value, it was five changes at once wearing the
       disguise of a tidy constant.
       So the base is $80 -- COCO as BASIC left it -- and MMUEN is the ONLY
       thing that varies below. */
    INIT0 = 0x80;                       /* COCO alone: the control */
    r[2] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    INIT0 = 0xC0;                       /* COCO + MMUEN, one bit different */
    r[3] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    for (i = 0; i < 8; i++) MMU0[i] = (unsigned char)(0x38 + i);
    r[4] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    INIT0 = 0x80;                       /* MMUEN off -- does it recover? */
    r[5] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    INIT0 = 0x88;                       /* COCO + MC3 alone, no MMU */
    r[6] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    INIT0 = 0x80;
    r[7] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

    r[8] = 0x5A;
    for (;;) ;
}

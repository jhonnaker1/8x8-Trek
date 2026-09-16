/* WHAT BASIC ACTUALLY HAS IN INIT0 -- READ OUT OF THE ROM, NOT INVENTED.
 *
 * bits2.c improved on bits.c by refusing to read-modify-write $FF90, and then
 * made the same class of mistake one level up: it picked $80 as "COCO alone:
 * the control" and called the difference from there a single bit. $80 is not
 * a control. The CoCo 3 ROM's own writes to $FF90 are, by disassembly:
 *
 *     $8C1F  $0A      (native-mode init, before BASIC)
 *     $8C30  $CC
 *     $C024  $CE   $C0C4  $CA   $C11D  $C8   $C13C  $CA   $C18E  $CE
 *
 * EVERY value BASIC runs under has bit 7 COCO, **bit 6 MMUEN** and **bit 3
 * MC3** set. So:
 *
 *   - MMUEN IS ALREADY ON under Disk BASIC. NOTES.md item 30's "MMUEN alone
 *     kills disk access" cannot be right in the direction it is stated: the
 *     disk works, all day, with MMUEN set, because that is the state DECB
 *     boots into and never leaves.
 *   - $80 CLEARS MC3, which is the bit that puts RAM at $FE00..$FEFF -- the
 *     page holding the interrupt jump slots the standalone DSKCON driver's
 *     NMI goes through. It also clears MC2 and MC1:MC0, the SCS and ROM map.
 *     Four changes, none of them the one the step was named after.
 *
 * So this probe asks the PORT'S question first, with INIT0 never written at
 * all, and only afterwards asks which INIT0 values a live driver survives.
 *
 * STOR_OK is 0, STOR_NOTFOUND 1, STOR_ERROR 2. $FF is "never reached".
 */
#include "../../core/storage.h"

#define INIT0 (*(unsigned char *)0xFF90)
#define MMU0  ((unsigned char *)0xFFA0)
#define r     ((unsigned char *)0x3F00)

static unsigned int got;
static unsigned char buf[512];

/* One read, one report slot. Written out rather than looped because each step
   has to happen between two specific register writes. */
#define TRY(n) r[n] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got)

int main(void)
{
    unsigned char i;

    for (i = 0; i < 16; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$3E00 }

    TRY(1);                                  /* baseline, nothing touched   */

    asm { sta $FFDF }                        /* all-RAM: the port's world   */
    TRY(2);

    /* The identity map. $38..$3F is the top 64K of a 128K machine, which is
       what task 0 already selects -- writing it should change nothing, and if
       it does, the port's overlay scheme is dead before INIT0 is involved. */
    for (i = 0; i < 8; i++) MMU0[i] = (unsigned char)(0x38 + i);
    TRY(3);

    /* THE QUESTION THE PORT ACTUALLY ASKS. MMU0[6] covers $C000..$DFFF, where
       OVL_WINDOW lives. Swap that one 8K block to a different physical page
       and read a file through the driver. Nothing else moves; INIT0 has still
       never been written. */
    MMU0[6] = 0x30;
    TRY(4);

    MMU0[6] = 0x3E;                          /* put it back                 */
    TRY(5);

    /* Only now, INIT0 -- and only values the ROM itself writes. An identity
       write must be a no-op; if even these kill the driver, the fault is the
       write cycle, not the bits. */
    INIT0 = 0xC8;  TRY(6);
    INIT0 = 0xCA;  TRY(7);
    INIT0 = 0xCC;  TRY(8);
    INIT0 = 0xCE;  TRY(9);

    INIT0 = 0x80;  TRY(10);                  /* bits2's "control" -- expect 1 */
    INIT0 = 0xCC;  TRY(11);                  /* and does it come BACK?        */

    r[15] = 0x5A;
    for (;;) ;
}

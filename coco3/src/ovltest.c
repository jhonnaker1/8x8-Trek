/* Can this machine load ONE overlay off the diskette into the window?
 *
 * The full game reaches ovl_load through the title screen, the setup prompts
 * and a stubbed keyboard, so a failure there could be any of a dozen things.
 * This is the seam on its own: take the machine, ask for one image, and
 * report the return code, the byte count and the first bytes that landed.
 *
 * Results at $2F00.
 */
#include <stdint.h>
#include "../../core/overlay.h"
#include "../../core/storage.h"
#include "ovlmap.h"

#define R ((unsigned char *)0x2F00)

int main(void)
{
    unsigned char i;
    unsigned char rc;
    unsigned int  got = 0;

    asm { orcc #$50 }
    asm { lds #$3F00 }
    for (i = 0; i < 48; i++) R[i] = 0xEE;

    /* ALL-RAM MODE FIRST. The window at $C300 is underneath Disk BASIC ROM
       until this runs, and a CPU write is the only thing that moves the
       latch -- a debugger poke does nothing. */
    asm { sta $FFDF }
    R[0] = 0x01;                      /* got this far */

    /* plat_read_all directly, so the answer is not filtered through
       ovl_load's caching or its silent return on a table hole. */
    rc = plat_read_all(ovl_name[OVL_HOF], (void *)OVL_WINDOW, OVL_SIZE, &got);
    R[1] = rc;                        /* 0 = STOR_OK */
    R[2] = (unsigned char)(got >> 8);
    R[3] = (unsigned char)(got & 0xFF);
    for (i = 0; i < 16; i++) R[4 + i] = ((unsigned char *)OVL_WINDOW)[i];

    /* And a second, different image, to prove the window is really rewritten
       rather than holding whatever was there. */
    rc = plat_read_all(ovl_name[OVL_TITLE], (void *)OVL_WINDOW, OVL_SIZE, &got);
    R[20] = rc;
    R[21] = (unsigned char)(got >> 8);
    R[22] = (unsigned char)(got & 0xFF);
    for (i = 0; i < 16; i++) R[24 + i] = ((unsigned char *)OVL_WINDOW)[i];

    /* A name that is not on the disk must come back NOTFOUND, or the two
       successes above prove nothing. */
    R[40] = plat_read_all("NOSUCH.OVL", (void *)OVL_WINDOW, OVL_SIZE, &got);

    R[47] = 0x5A;
    for (;;) ;
}

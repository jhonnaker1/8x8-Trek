/* THE FIRST-STAGE LOADER, the way a CoCo does it.
 *
 * A 42K program cannot be placed by Disk BASIC's LOADM: it spans $2800..$CE5C,
 * which is under the BASIC and Disk BASIC ROMs, and `CLEAR` cannot move
 * BASIC's ceiling that far -- `CLEAR 25,&H11FF` answers ?OM ERROR. The CoCo
 * answer, and it is an old one, is a SMALL FIRST-STAGE LOADER that BASIC can
 * place, which then reads the real image into position itself and jumps.
 * See "CoCo 3 Loader for big programs" and NOTES.md item 35.
 *
 * WHAT MADE THIS SHAPE POSSIBLE, measured on the machine rather than assumed:
 * A CPU WRITE TO ROM SPACE PASSES THROUGH TO THE RAM UNDERNEATH. Writing $11,
 * $22, $33 to $8000/$A000/$C000 with the ROM mapped and reading them back
 * after switching to all-RAM returned all three. So the image can be placed
 * while the ROM is still there, and the machine taken only at the end.
 *
 * THIS RUNS AT $E400, above the overlay window and below the stack reserve --
 * space the game never uses. It gets there because a stub in the same DECB
 * file loads low, where BASIC can put it, and jumps up here; see
 * tools/mkboot.py. Once the game starts, nothing here is live.
 */
#include <stdint.h>

#include "../../core/storage.h"

#define GAME_ORG   0x2800
#define GAME_MAX   0xC000        /* far more than the image; the read is bounded
                                    by the file's own length on disk */

int main(void)
{
    uint16_t got = 0;

    /* Interrupts off and our own stack before anything else: from here on
       Disk BASIC's handler and its stack are not ours to rely on. */
    asm { orcc #$50 }
    asm { lds #$FDF0 }

    /* The image is RAW -- no DECB block headers -- so it lands exactly at
       GAME_ORG and the loader needs to know nothing about its shape. */
    if (plat_read_all("EGATREK.RAW", (void *)GAME_ORG, GAME_MAX, &got) != STOR_OK)
        for (;;) ;               /* nothing to jump to; stop rather than guess */

#ifdef BOOT_HALT
    /* HALT INSTEAD OF JUMPING, so the machine stays in all-RAM mode and the
       image can be READ. Every check of it so far ran after the game had
       crashed and the ROM was back, where a read above $8000 returns ROM
       whatever the RAM beneath holds -- inconclusive, not failing. This
       leaves the machine in exactly the state the game starts in.
       Answers at $2000, below the image and below BASIC's ceiling. */
    {
        unsigned char *r = (unsigned char *)0x2000;
        unsigned char i;
        r[0] = 0xA1;                       /* the loader got here */
        r[1] = (unsigned char)(got >> 8);  /* how many bytes it read */
        r[2] = (unsigned char)(got & 0xFF);
        for (i = 0; i < 4; i++) r[4 + i]  = ((unsigned char *)0x2800)[i];
        for (i = 0; i < 4; i++) r[8 + i]  = ((unsigned char *)0x6000)[i];
        for (i = 0; i < 4; i++) r[12 + i] = ((unsigned char *)0xA000)[i];
        for (i = 0; i < 4; i++) r[16 + i] = ((unsigned char *)0xC000)[i];
        for (i = 0; i < 4; i++) r[20 + i] = ((unsigned char *)0xCE50)[i];
    }
    for (;;) ;
#else
    /* ALL RAM, and only now. Everything above $8000 has been written through
       the ROM; this is what makes it readable. */
    asm { sta $FFDF }
    asm { jmp $2800 }
#endif
    return 0;
}

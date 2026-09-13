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

/* AN ABSOLUTE ADDRESS, NOT A LOCAL POINTER. The first version of the report
   below used `unsigned char *r = (unsigned char *)0x2000;` and cmoc emitted
   NO STORES AT ALL -- checked by looking for B7 20 00 in the binary, which
   was not there. README.md's "cmoc traps" says why: a local pointer lives in the
   stack frame and is reloaded before every store through it, and a run of
   stores nothing ever reads is free to vanish. Every working probe in this
   port uses a macro like this one. I had the note and ignored it.

   AND THE CHECK FOR IT WAS WRONG TOO: I looked for B7 20 00 (STA $2000) in
   the binary and reported the stores missing. cmoc emits C6 A1 / F7 20 00 --
   LDB/STB, because it prefers B for 8-bit values. The stores were there the
   second time and the instrument said they were not. Search for both, or
   read the generated assembly instead of guessing an encoding. */
#define BOOTR      ((unsigned char *)0x2000)

#define GAME_ORG   0x2800
#define GAME_MAX   0xC000        /* far more than the image; the read is bounded
                                    by the file's own length on disk */

int main(void)
{
    uint16_t got = 0;

    /* MARK EACH STAGE, and mark the first one BEFORE anything can fail.
       The previous version wrote its report only after plat_read_all
       returned, and the failure path loops forever before reaching it -- so a
       failed read and a loader that never started looked identical, both
       reporting $FF. */
    BOOTR[0] = 0xA1;                /* entered main() */

    /* Interrupts off and our own stack before anything else: from here on
       Disk BASIC's handler and its stack are not ours to rely on. */
    asm { orcc #$50 }
    asm { lds #$FDF0 }

    /* The image is RAW -- no DECB block headers -- so it lands exactly at
       GAME_ORG and the loader needs to know nothing about its shape. */
    BOOTR[1] = 0xA2;                /* about to call plat_read_all */
    BOOTR[2] = plat_read_all("EGATREK.RAW", (void *)GAME_ORG, GAME_MAX, &got);
    BOOTR[9]  = (unsigned char)(got >> 8);
    BOOTR[10] = (unsigned char)(got & 0xFF);
    BOOTR[11] = 0x5A;               /* the report is complete */
    if (BOOTR[2] != STOR_OK)
        for (;;) ;               /* nothing to jump to; stop rather than guess */

    /* REPORT BEFORE JUMPING, ALWAYS. `got` is the one number that separates
       "the image did not load" from "the image loaded and the game is at
       fault", and every attempt to read the image itself has been taken in
       ROM mode where the answer is meaningless. $2000 is below the image
       ($2800..$CE5B) and below anything the game touches, so it survives
       whatever happens next. */
    BOOTR[3] = ((unsigned char *)0x2800)[0];    /* first byte of the image */
    BOOTR[4] = ((unsigned char *)0x8000)[0];    /* and one from above $8000, */
    BOOTR[5] = ((unsigned char *)0xA000)[0];    /* read while all-RAM is live */
    BOOTR[6] = ((unsigned char *)0xCE50)[0];

#ifdef BOOT_HALT
    /* Extra samples, taken here because the machine never leaves all-RAM mode
       in this build: a read above $8000 means what it says. */
    BOOTR[7]  = ((unsigned char *)0x6000)[0];
    BOOTR[8]  = ((unsigned char *)0xC000)[0];

    /* HALT INSTEAD OF JUMPING, so the machine stays in all-RAM mode and the
       image can be READ. Every check of it so far ran after the game had
       crashed and the ROM was back, where a read above $8000 returns ROM
       whatever the RAM beneath holds -- inconclusive, not failing. This
       leaves the machine in exactly the state the game starts in.
       Answers at $2000, below the image and below BASIC's ceiling. */
    for (;;) ;
#else
    /* ALL RAM, and only now. Everything above $8000 has been written through
       the ROM; this is what makes it readable. */
    asm { sta $FFDF }
    asm { jmp $2800 }
#endif
    return 0;
}

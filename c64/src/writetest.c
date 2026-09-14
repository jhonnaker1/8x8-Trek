/* Can this port WRITE a file and READ IT BACK IN THE SAME RUN?
 *
 * THE QUESTION IS NOT "IS THERE A DIRECTORY ENTRY", and that distinction is
 * the whole reason this file exists. After the first C64 save, the host .d64
 * listed `0 "egatrek.sav" *seq` -- a splat file with no blocks -- and c1541
 * answered ERR = 62, FILE NOT FOUND when asked to read it. That looks
 * conclusive and is not: c128/src/storage.c carries a long note saying the
 * identical reading was chased for two sessions on the C128 and was an
 * artefact of looking at the HOST IMAGE while VICE's drive held its own view.
 * Running the same drive against the C128's own 40-column disk produced the
 * SAME splat, which rules the C64 in or out of nothing.
 *
 * So this asks the drive. Write a pattern, read it back through plat_read_all
 * without the machine stopping in between, and compare. That is the
 * measurement the C128's note says settled it there ("write 340 bytes, then
 * read them back in the same run"), and it is the same shape as
 * coco3/src/writetest.c.
 *
 * WHAT IT STILL CANNOT SEE: both halves are this port's own code, so a shared
 * wrong idea about the format would pass. That is acceptable here because the
 * format is CBM DOS's and not ours -- the KERNAL writes it.
 */
#include <stdint.h>

#include "../../core/storage.h"

/* A page nothing else uses: below the program at $0801 and above the KERNAL's
   own work areas. The runner reads it straight out of memory, so the probe
   needs no screen at all. */
#define R ((volatile unsigned char *)0x0340)
#define NBYTES 600

/* THE READ BUFFER IS BIGGER THAN THE FILE ON PURPOSE, and the first run of
 * this probe failed for want of it. plat_read_all treats a buffer it filled
 * EXACTLY as an error -- `if (n == max) return STOR_ERROR;` -- because it
 * cannot tell a 600-byte file from a longer one truncated at 600, and
 * core/storage.h says silently reading half a save is worse than refusing.
 * That is correct behaviour and the contract tells callers to "size with the
 * format's own length", which serial.c does. Passing NBYTES as max made the
 * probe report STOR_ERROR beside a comparison that MATCHED ALL 600 BYTES --
 * a red result from a green machine, and exactly the instrument fault this
 * project keeps finding: the probe's own rule, misread as the port's. */
#define BACKROOM (NBYTES + 64)

static unsigned char data[NBYTES];
static unsigned char back[BACKROOM];

int main(void)
{
    unsigned int i;
    uint16_t got = 0;

    R[31] = 0;

    /* A pattern with no run of equal bytes, so a file written twice, short,
       or in the wrong order cannot pass by looking plausible. 600 bytes is
       more than one 254-byte sector and not a multiple of it. */
    for (i = 0; i < NBYTES; i++)
        data[i] = (unsigned char)((i * 7u + (i >> 3)) ^ 0x5A);

    R[0] = plat_write_all("SAVETEST.DAT", data, NBYTES);

    R[1] = plat_read_all("SAVETEST.DAT", back, BACKROOM, &got);
    R[2] = (unsigned char)(got >> 8);
    R[3] = (unsigned char)(got & 0xFF);

    /* THE POSITION IS RECORDED WHERE IT IS KNOWN, not after the loop. Written
       the other way, the CoCo 3's copy of this reported "first difference at
       2927" on a run that MATCHED -- `i` after a completed loop is whatever
       the compiler left in it, and a number printed beside a pass is worse
       than no number at all. */
    R[4] = 1; R[5] = 0; R[6] = 0;
    for (i = 0; i < NBYTES; i++)
        if (back[i] != data[i]) {
            R[4] = 0;
            R[5] = (unsigned char)(i >> 8);
            R[6] = (unsigned char)(i & 0xFF);
            break;
        }

    /* THE STAMP IS LAST. Without it a runner cannot tell "the probe reported
       a failure" from "the probe never ran" -- and $0340 holds whatever the
       last program left there, so zeroes are not proof of anything. */
    R[31] = 0x5A;
    for (;;) { }
    return 0;
}

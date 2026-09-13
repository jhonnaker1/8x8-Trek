/* Can this port WRITE a file that a real Disk BASIC filesystem reader can
 * find and read back?
 *
 * TWO INDEPENDENT READERS HAVE TO AGREE, and that is the point. The machine
 * writes the file and reads it back with its own plat_read_all -- which
 * proves the port is self-consistent and nothing more, because the same
 * wrong idea about the format would be on both sides. Then tools/writecheck.py
 * opens the .dsk on the HOST and walks the directory and the FAT with code
 * that was written before any of this and already round-trips mkdisk's
 * output. A file both of them can read is a file on a Disk BASIC diskette.
 * The hall-of-fame write on the C128 was settled the same way: witnessed on
 * the disk, not inferred from a return code.
 */
#include <stdint.h>

#include "../../core/storage.h"

#define RESULT ((unsigned char *)0x7F00)
#define NBYTES 600

static unsigned char data[NBYTES];
static unsigned char back[NBYTES];

int main(void)
{
    unsigned int i;
    uint16_t got = 0;

    asm { orcc #$50 }
    /* OUR OWN STACK, AND IT IS NOT OPTIONAL. cmoc's CoCo runtime positions
       one from Disk BASIC's memory pointers, which on a machine that has not
       been CLEARed is up around $7FFF -- directly on top of the report at
       $7F00. The first symptom was a stamp reading $80: not the $00 this
       line writes and not the $5A the end writes, because the report was
       being used as stack. $6F00 is below the report and above the bss this
       file needs. */
    asm { lds #$6F00 }
    RESULT[31] = 0;

    /* A pattern with no run of equal bytes, so a sector written twice or in
       the wrong order cannot pass by looking plausible. 600 bytes is three
       sectors -- two full and one short -- which exercises the padding and
       the lastbytes arithmetic in one file. */
    for (i = 0; i < NBYTES; i++)
        data[i] = (unsigned char)((i * 7u + (i >> 3)) ^ 0x5A);

    RESULT[0] = plat_write_all("SAVETEST.DAT", data, NBYTES);

    RESULT[1] = plat_read_all("SAVETEST.DAT", back, NBYTES, &got);
    RESULT[2] = (unsigned char)(got >> 8);
    RESULT[3] = (unsigned char)(got & 0xFF);

    /* RECORD THE POSITION WHERE IT IS KNOWN, not after the loop. Written the
       other way this reported "first difference at 2927" on a run that
       MATCHED: `i` after a completed loop is whatever cmoc left in it, and a
       number printed beside a pass is worse than no number at all. */
    RESULT[4] = 1;
    RESULT[5] = 0;
    RESULT[6] = 0;
    for (i = 0; i < NBYTES; i++)
        if (back[i] != data[i]) {
            RESULT[4] = 0;
            RESULT[5] = (unsigned char)(i >> 8);
            RESULT[6] = (unsigned char)(i & 0xFF);
            break;
        }

    RESULT[31] = 0x5A;
    for (;;) ;
    return 0;
}

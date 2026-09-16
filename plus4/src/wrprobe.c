/* DOES A WRITE PASS THROUGH THE ROM TO THE RAM UNDERNEATH?
 *
 * The whole memory map depends on it. $1001..$7FFF is only 28,671 bytes and
 * the resident image is about 40K, so the program MUST span $8000 and up --
 * which is hidden whenever the ROM is banked in for a KERNAL call. That is
 * survivable for CODE, which simply is not executing at that moment. It is
 * not survivable for the FAR STORE unless KERNAL LOAD can write into it, and
 * on a C64 it can, because writes always reach RAM whatever $01 says.
 *
 * bankprobe.c showed reads: ROM in gives ROM bytes, RAM in gives what was
 * written, and the writes survive being hidden. It never wrote WITH THE ROM
 * MAPPED, which is the actual question.
 *
 *   R[1..2]  write with ROM in, then read with ROM in    -- ROM, obviously
 *   R[3..4]  bank RAM, read the same cells               -- DID IT LAND?
 *   R[5]     and the byte the ROM shows at $A000, for contrast
 */
#define ROM_IN  (*(volatile unsigned char *)0xFF3E)
#define RAM_IN  (*(volatile unsigned char *)0xFF3F)

volatile unsigned char report[16];
#define R report

int main(void)
{
    volatile unsigned char *a000 = (volatile unsigned char *)0xA000;
    volatile unsigned char *e000 = (volatile unsigned char *)0xE000;

    R[0] = 0xA1;

    /* Scrub, so a stale value cannot look like a success. */
    RAM_IN = 0;
    a000[0] = 0x11;
    e000[0] = 0x22;

    /* THE TEST: ROM mapped, write, and see where it went. */
    ROM_IN = 0;
    a000[0] = 0xBE;
    e000[0] = 0xEF;
    R[1] = a000[0];                 /* still ROM if reads come from ROM */
    R[2] = e000[0];

    RAM_IN = 0;
    R[3] = a000[0];                 /* $BE means the write passed through */
    R[4] = e000[0];                 /* $EF likewise; $11/$22 means it did not */

    ROM_IN = 0;
    R[5] = a000[0];
    R[6] = 0x5A;
    for (;;) { }
    return 0;
}

/* DOES A WRITE TO $FFFE REACH THE RAM UNDER THE ROM?
 *
 * p4bank.c sets the 6502's interrupt vectors BEFORE it banks RAM in, and
 * justifies it like this: "The vector stores happen with the ROM still mapped
 * and reach the RAM beneath it -- writes pass through, which src/wrprobe.c
 * measured."
 *
 * WRPROBE.C MEASURED $A000 AND $E000. It never touched $FFFE, and $FFFE is
 * not in the same country: $FF00..$FF3F is TED, present in every
 * configuration, and $FF40..$FFFF is the tail of the KERNAL ROM. A decode
 * that lets a write fall through at $E000 need not do so three bytes below
 * the top of memory, and the citation is to a measurement of somewhere else.
 *
 * If the writes do not land, p4bank.c banks RAM in with the CPU's vectors
 * pointing at whatever the program has at $FFFE -- and the port dies before
 * main(), which is exactly what it does.
 *
 *   R[1..2]  $FFFE/$FFFF as the ROM shows them
 *   R[3..4]  written with ROM in, read back with ROM in
 *   R[5..6]  THE ANSWER: the same cells with RAM banked in
 *   R[7..8]  and $E000, the region wrprobe DID measure, for contrast
 */
#define ROM_IN  (*(volatile unsigned char *)0xFF3E)
#define RAM_IN  (*(volatile unsigned char *)0xFF3F)

volatile unsigned char report[16];
#define R report

int main(void)
{
    volatile unsigned char *fffe = (volatile unsigned char *)0xFFFE;
    volatile unsigned char *e000 = (volatile unsigned char *)0xE000;

    __asm__ volatile("sei");
    R[0] = 0xA1;

    /* Scrub through RAM first, so a stale value cannot look like success. */
    RAM_IN = 0;
    fffe[0] = 0x11; fffe[1] = 0x22;
    e000[0] = 0x33;

    ROM_IN = 0;
    R[1] = fffe[0];                 /* what the ROM shows there */
    R[2] = fffe[1];

    /* THE TEST: ROM mapped, write, and see where it went. */
    fffe[0] = 0xBE; fffe[1] = 0xEF;
    e000[0] = 0x5A;
    R[3] = fffe[0];                 /* still ROM if reads come from ROM */
    R[4] = fffe[1];

    RAM_IN = 0;
    R[5] = fffe[0];                 /* $BE means it passed through */
    R[6] = fffe[1];                 /* $11/$22 means it did NOT */
    R[7] = e000[0];                 /* $5A -- the region wrprobe measured */
    R[8] = 0x00;

    ROM_IN = 0;
    R[9] = 0x5A;                    /* completed */
    for (;;) { }
    return 0;
}

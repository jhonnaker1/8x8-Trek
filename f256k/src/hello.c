/* FIRST LIGHT. Can llvm-mos produce a PGZ that FoenixMCP loads and runs?
 *
 * That is the whole question -- everything else about this port depends on it,
 * and if the answer is no the fallback is cc65 and uno's proven toolchain. So
 * it is retired first, before a line of driver code.
 *
 * It writes straight to the text matrix. $C000 is the character matrix ONLY on
 * I/O PAGE 2 and the colour matrix on PAGE 3, selected by $0001 -- and the
 * PSG tone probe was silent for a whole run because I forgot that for $D608.
 * On this machine the I/O page is part of the address, not part of the setup.
 *
 * `ran` is a completion marker. A blank screen and a program that never
 * started look identical, and this project has read findings off a probe whose
 * own completion byte said it never finished.
 */
#define IOCTRL (*(volatile unsigned char *)0x0001)
#define MATRIX ((volatile unsigned char *)0xC000)
#define COLS 80

__attribute__((used, retain)) volatile unsigned char ran;

static void put(unsigned char x, unsigned char y, const char *s, unsigned char colour)
{
    unsigned int off = (unsigned int)y * COLS + x;
    const char *p;
    unsigned int o;

    __asm__ volatile ("sei");
    IOCTRL = 2;
    for (p = s, o = off; *p; p++) MATRIX[o++] = (unsigned char)*p;
    IOCTRL = 3;
    for (p = s, o = off; *p; p++) MATRIX[o++] = colour;
    IOCTRL = 0;
    __asm__ volatile ("cli");
}

int main(void)
{
    ran = 0x11;
    put(2, 10, "EGA TREK -- F256K, LLVM-MOS, FIRST LIGHT", 0xF0);
    put(2, 12, "IF YOU CAN READ THIS THE TOOLCHAIN WORKS.", 0xE0);
    ran = 0x5A;
    for (;;) { }
}

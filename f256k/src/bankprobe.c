/* HOW MUCH RAM IS THERE, AND HOW MUCH OF IT IS OURS?
 *
 * The port has been scoped against ~40K of ADDRESS SPACE, which is the wrong
 * number to plan an architecture around on this machine. The F256K has 512K
 * of RAM in 64 banks of 8K, and an MMU with eight slots and four complete
 * mappings -- so "load an overlay" could be a single store to a slot register
 * instead of a disk read. That would delete the streaming briefing, the
 * disk-backed string pool (the slowest thing in the CoCo 3 GIME port) and
 * every mid-game overlay load, and it would move this port's shape from the
 * C128's to the Amiga's.
 *
 * That is worth having, so it is worth measuring rather than assuming. Three
 * questions, and the third is the one that decides:
 *
 *   1. THE MACHINE'S OWN LAYOUT. All four MLUTs, dumped before anything is
 *      touched. A bank that FoenixMCP has mapped is a bank in use.
 *   2. IS $A000-$BFFF REACHABLE? The README has said "NOT ASSUMED until
 *      measured" since the port started. Slot 5 is that window.
 *   3. WHICH BANKS ARE REAL, AND ARE THEY DISTINCT? 512K is what MAME is
 *      configured with; a machine with less would ALIAS, and aliasing is the
 *      failure that looks like everything working until two overlays quietly
 *      share a page.
 *
 * THIS PASS IS READ-ONLY WHERE IT MATTERS. Writing a signature into a bank
 * FoenixMCP is using would take the kernel down mid-probe, and a probe that
 * kills the machine reports nothing. So banks are fingerprinted first; the
 * write test comes after, and only where the fingerprint says nothing lives.
 */
#include "f256kern.h"

#define IOCTRL (*(volatile unsigned char *)0x0001)
#define MATRIX ((volatile unsigned char *)0xC000)
#define MMU_CTRL (*(volatile unsigned char *)0x0000)
#define MMU_SLOT ((volatile unsigned char *)0x0008)
#define WINDOW ((volatile unsigned char *)0xA000)    /* slot 5 */
#define COLS 80
#define ROWS 60
#define NBANK 64

TREK_SIGNATURE;
__attribute__((used, retain)) volatile unsigned char ran;
/* The four MLUTs as found, 8 slots each. */
__attribute__((used, retain)) volatile unsigned char mlut[32];
/* Per bank: a 16-bit sum of its first 256 bytes, and whether it is all zero. */
__attribute__((used, retain)) volatile unsigned char fp_lo[NBANK], fp_hi[NBANK];
__attribute__((used, retain)) volatile unsigned char zero[NBANK];
/* Per bank after the write test: 1 = held its own signature, 0 = did not. */
__attribute__((used, retain)) volatile unsigned char wrote_ok[NBANK];
__attribute__((used, retain)) volatile unsigned char slot5_live;
/* WHICH MAPPING THE CPU IS ACTUALLY RUNNING ON. Dumping four MLUTs without
   this says what the mappings ARE and not which one the program is in, and
   they disagree completely: MLUT 0 puts flash at $4000-$BFFF and MLUT 2 puts
   RAM there. Reading the table without this number is reading a map with no
   "you are here". */
__attribute__((used, retain)) volatile unsigned char mmu_ctrl_seen;

static unsigned char saved_slot5;

/* Map `bank` into slot 5. THE ACTIVE MLUT IS PRESERVED: bits 0-1 choose which
   mapping the CPU is running on and MCP picked it, so the edit target is set
   to whatever is already active rather than to 0. Clobbering that would
   remap the ground under the program's feet. */
static void map5(unsigned char bank)
{
    unsigned char save = MMU_CTRL;
    unsigned char act = (unsigned char)(save & 0x03);
    MMU_CTRL = (unsigned char)(0x80 | (act << 4) | act);
    MMU_SLOT[5] = bank;
    MMU_CTRL = save;
}

static unsigned char read_slot(unsigned char lut, unsigned char slot)
{
    unsigned char save = MMU_CTRL;
    unsigned char v;
    MMU_CTRL = (unsigned char)(0x80 | (lut << 4) | (save & 0x03));
    v = MMU_SLOT[slot];
    MMU_CTRL = save;
    return v;
}

static void cell(unsigned char x, unsigned char y, unsigned char ch, unsigned char col)
{
    unsigned int off = (unsigned int)y * COLS + x;
    IOCTRL = 2; MATRIX[off] = ch;
    IOCTRL = 3; MATRIX[off] = col;
    IOCTRL = 0;
}
static void text(unsigned char x, unsigned char y, const char *s, unsigned char col)
{ __asm__ volatile ("sei"); while (*s) cell(x++, y, (unsigned char)*s++, col);
  __asm__ volatile ("cli"); }
static void clear(void)
{
    unsigned int i;
    __asm__ volatile ("sei");
    IOCTRL = 2; for (i = 0; i < (unsigned int)COLS*ROWS; i++) MATRIX[i] = ' ';
    IOCTRL = 3; for (i = 0; i < (unsigned int)COLS*ROWS; i++) MATRIX[i] = 0xF0;
    IOCTRL = 0;
    __asm__ volatile ("cli");
}

int main(void)
{
    unsigned char b, l, s;
    unsigned int i, sum;

    ran = 0x11;
    clear();
    text(2, 1, "F256K BANK PROBE -- 64 BANKS OF 8K, AND WHICH ARE OURS", 0xE0);

    /* The machine as found, before anything is remapped. */
    __asm__ volatile ("sei");
    for (l = 0; l < 4; l++)
        for (s = 0; s < 8; s++)
            mlut[l * 8 + s] = read_slot(l, s);
    (void)0;
    mmu_ctrl_seen = MMU_CTRL;
    saved_slot5 = mlut[(mmu_ctrl_seen & 3) * 8 + 5];
    __asm__ volatile ("cli");

    /* IS SLOT 5 EVEN LIVE? Write through the window as MCP left it and read
       back. If $A000-$BFFF is not RAM in this configuration, everything below
       is measuring a hole. */
    __asm__ volatile ("sei");
    WINDOW[0] = 0x5A; WINDOW[1] = 0xA5;
    slot5_live = (unsigned char)(WINDOW[0] == 0x5A && WINDOW[1] == 0xA5);
    __asm__ volatile ("cli");

    /* FINGERPRINT EVERY BANK, READ ONLY. */
    for (b = 0; b < NBANK; b++) {
        unsigned char z = 1;
        __asm__ volatile ("sei");
        map5(b);
        sum = 0;
        for (i = 0; i < 256; i++) {
            unsigned char v = WINDOW[i];
            sum += v;
            if (v) z = 0;
        }
        map5(saved_slot5);
        __asm__ volatile ("cli");
        fp_lo[b] = (unsigned char)sum;
        fp_hi[b] = (unsigned char)(sum >> 8);
        zero[b] = z;
    }

    /* THE WRITE TEST, AND ONLY WHERE THE FINGERPRINT SAYS NOTHING LIVES.
       Each bank gets its own number written at four offsets; then a SECOND
       pass re-maps each and checks it still reads its own number. One pass
       could not detect aliasing -- two banks sharing a page would each verify
       correctly on the way past. */
    for (b = 0; b < NBANK; b++) {
        if (!zero[b]) { wrote_ok[b] = 2; continue; }   /* 2 = not attempted */
        __asm__ volatile ("sei");
        map5(b);
        WINDOW[0] = b; WINDOW[1] = (unsigned char)~b;
        WINDOW[0x1000] = b; WINDOW[0x1FFF] = (unsigned char)~b;
        map5(saved_slot5);
        __asm__ volatile ("cli");
    }
    for (b = 0; b < NBANK; b++) {
        if (wrote_ok[b] == 2) continue;
        __asm__ volatile ("sei");
        map5(b);
        wrote_ok[b] = (unsigned char)(WINDOW[0] == b &&
                                      WINDOW[1] == (unsigned char)~b &&
                                      WINDOW[0x1000] == b &&
                                      WINDOW[0x1FFF] == (unsigned char)~b);
        map5(saved_slot5);
        __asm__ volatile ("cli");
    }

    text(2, 3, "DONE -- SEE THE HOST FOR THE TABLE", 0xA0);
    ran = 0x5A;
    for (;;) { }
}

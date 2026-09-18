/* CAN A PGZ SEGMENT LOAD STRAIGHT INTO $0400, OR MUST IT BE COPIED DOWN?
 *
 * The low-memory probe showed the kernel neither writes to nor depends on
 * $0400-$1FFF while our program runs -- 7K of RESIDENT address space, which
 * is worth more than a bank because it is always mapped.
 *
 * But it also showed what is sitting there when we arrive: the ASCII string
 * "lowprobe.pgz". THAT IS pexec's OWN WORKSPACE -- the name of the file it is
 * in the middle of loading. So a segment targeted at $0400 would be written
 * INTO THE LOADER'S VARIABLES WHILE THE LOADER IS STILL RUNNING.
 *
 * Which is why this is a measurement and not an assumption. The PGZ here
 * carries a second segment at $0400 holding a known pattern; the program
 * checks it before touching anything. Three outcomes, all of them useful:
 *
 *   pattern intact   -- segments can load there; no copy-down needed
 *   pattern wrong    -- it loaded, then pexec overwrote part of it
 *   nothing runs     -- pexec did not survive, and the answer is copy-down
 */
#include "f256kern.h"

#define IOCTRL (*(volatile unsigned char *)0x0001)
#define MATRIX ((volatile unsigned char *)0xC000)
#define LOW ((volatile unsigned char *)0x0400)
/* THE WHOLE 7K, not a token 256 bytes. pexec survived a segment over its
   filename buffer at $0400 -- but its sector buffer and stack could be
   anywhere in $0400-$1FFF, and a test that covers the first page only proves
   the first page. */
#define NLOW 0x1C00

TREK_SIGNATURE;
__attribute__((used, retain)) volatile unsigned char ran;
__attribute__((used, retain)) volatile unsigned int  bad;      /* mismatches */
__attribute__((used, retain)) volatile unsigned char first_bad, first_bad_hi, was_bad, now_bad;

static void cell(unsigned char x, unsigned char y, unsigned char ch, unsigned char col)
{
    unsigned int off = (unsigned int)y * 80 + x;
    __asm__ volatile ("sei");
    IOCTRL = 2; MATRIX[off] = ch;
    IOCTRL = 3; MATRIX[off] = col;
    IOCTRL = 0;
    __asm__ volatile ("cli");
}
static void text(unsigned char x, unsigned char y, const char *s, unsigned char col)
{ while (*s) cell(x++, y, (unsigned char)*s++, col); }

int main(void)
{
    unsigned int i;

    ran = 0x11;
    /* CHECKED FIRST, before the screen is touched or anything is called --
       the question is what the LOADER left, and every instruction between
       here and the check is another chance to be the one that changed it. */
    bad = 0;
    first_bad = 0xFF;
    for (i = 0; i < NLOW; i++) {
        unsigned char want = (unsigned char)((i ^ 0xC3) + (unsigned char)(i >> 8));
        unsigned char got = LOW[i];
        if (got != want) {
            if (!bad) { first_bad = (unsigned char)i; was_bad = want; now_bad = got;
                        first_bad_hi = (unsigned char)(i >> 8); }
            bad++;
        }
    }

    text(2, 1, "F256K: DOES A PGZ SEGMENT REACH $0400?", 0xE0);
    text(2, 3, bad ? "NO -- SOMETHING OVERWROTE IT" : "YES -- IT ARRIVED INTACT",
         bad ? 0xC0 : 0xA0);
    ran = 0x5A;
    for (;;) { }
}

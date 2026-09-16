/* DO THE KERNAL VECTORS IN plus4.ld ACTUALLY WORK?
 *
 * They are the most likely thing in this port to be wrong and a linker script
 * cannot tell a right vector from a plausible one -- a bad entry is a jump
 * into the middle of a routine, not an error. Only a real load proves them,
 * and it proves SETLFS, SETNAM and LOAD together with the whole banking
 * window: ROM in, three calls, ROM out, return to code that was invisible
 * throughout.
 *
 *   R[1]    far_load returned FAR_NONE?      0 = the file came in
 *   R[2..3] the base it reported
 *   R[4..5] far_size() -- must be 7496, the exact file
 *   R[6..9] the first four bytes, read back through far_read
 *
 * THE FIRST FOUR BYTES ARE THE CHECK THAT MATTERS. STRINGS.DAT starts with
 * the string COUNT as a 16-bit little-endian word -- 334 today -- so a load
 * that lands at the wrong address, or drops the PRG header, or reads short,
 * produces a number that is not 334 and says so.
 */
#include <stdint.h>
#include "../../core/farmem.h"

/* used AND volatile: LTO can see that nothing READS this array and is
   entitled to delete every store to it. The first build did exactly that and
   the symbol was absent from the map -- a probe whose report the compiler had
   optimised out looks identical to a probe that never ran. */
__attribute__((used)) volatile unsigned char report[16];
#define R report

int main(void)
{
    uint16_t base, sz;
    unsigned char b[4];

    R[0] = 0xA1;
    base = far_load("STRINGS.DAT");
    R[1] = (unsigned char)(base == FAR_NONE);
    R[2] = (unsigned char)(base >> 8);
    R[3] = (unsigned char)(base & 0xFF);

    sz = far_size();
    R[4] = (unsigned char)(sz >> 8);
    R[5] = (unsigned char)(sz & 0xFF);

    if (base != FAR_NONE) {
        far_read(base, b, 4);
        R[6] = b[0]; R[7] = b[1]; R[8] = b[2]; R[9] = b[3];
    }
    R[10] = 0x5A;
    for (;;) { }
    return 0;
}

/* Does the input seam return what input.h names? Type, and read the screen.
 *
 * This is the discriminator the X16's keyprobe was: the driver's mapping is
 * built out of the OS's own KEYDEF table, so the way it can be wrong is in
 * the FOLD from ATASCII to the ASCII values c128/src/input.h uses -- the
 * uppercasing, EOL, delete and the two cursor keys. Each of those prints its
 * decimal value here, so a wrong one is visible rather than merely absent.
 */
#include "vbxevid.h"
#include "input.h"

static void put_num(unsigned char x, unsigned char y, unsigned int n) {
    char buf[6];
    unsigned char i = 0;
    if (!n) { scr_put(x, y, 16, 15); return; }      /* screen code for '0' */
    while (n && i < 5) { buf[i++] = (char)('0' + n % 10); n /= 10; }
    while (i--) scr_put(x++, y, (unsigned char)buf[i], 15);
}

int main(void) {
    unsigned char row = 2;

    vdc_init();
    kb_init();
    scr_puts(2, 0, "KEY ECHO -- CHAR, THEN ITS DECIMAL VALUE", 15);

    for (;;) {
        char c = kb_waitkey();
        char one[2];

        /* THROUGH scr_puts, NOT scr_put. The first version passed the ASCII
           value straight to scr_put, which takes a RAW SCREEN CODE -- so
           every letter came back as the font's missing-glyph marker while
           the digits and punctuation looked fine, because those two encodings
           agree on 32..63 and disagree above it. The marker firing on this
           test is exactly what it is for; the test was wrong, not the font. */
        one[0] = c;
        one[1] = 0;
        scr_puts(2, row, one, 14);
        put_num(6, row, (unsigned int)(unsigned char)c);
        if (++row >= VDC_ROWS) row = 2;
    }
    return 0;
}

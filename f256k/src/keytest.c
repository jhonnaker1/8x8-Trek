/* kb_waitkey through the REAL f256key.c, checked against the values the
 * shared header names.
 *
 * The probe measured what the KERNEL sends; this measures what the PORT
 * returns, and they are different questions. Between them sits translate():
 * the case fold, the two arrows, and RUN/STOP standing in for an ESC key this
 * keyboard does not have. Every one of those is a chance to be wrong in a way
 * the probe cannot see -- the X16's note says it plainly, delete the fold and
 * the console comes up saying NO SUCH ORDER.
 *
 * The host types a chosen sequence and reads back what kb_waitkey made of it.
 */
#include "../../c128/src/vdc.h"
#include "../../c128/src/input.h"
#include "../../core/ega.h"
#include "f256kern.h"

TREK_SIGNATURE;
__attribute__((used, retain)) volatile unsigned char ran;
__attribute__((used, retain)) volatile unsigned char got[32];
__attribute__((used, retain)) volatile unsigned char got_n;
__attribute__((used, retain)) volatile unsigned int  entropy_seen;

extern unsigned char f256_other_lost;
__attribute__((used, retain)) volatile unsigned char lost_seen;

/* scr_put takes a RAW SCREEN CODE, not ASCII -- digits are 48..57 as in
   ASCII but letters are 1..26, so a hex digit cannot come from a string
   literal here. Routing it through scr_puts instead would be worse: that
   applies the ASCII fold, which is what mangled hex into accented letters on
   the X16 the last time somebody tried it. */
static unsigned char hexdig(unsigned char v)
{
    return (unsigned char)(v < 10 ? 48 + v : 1 + v - 10);
}

static void hex2(unsigned char x, unsigned char y, unsigned char v, unsigned char col)
{
    scr_put(x, y, hexdig((unsigned char)(v >> 4)), col);
    scr_put((unsigned char)(x + 1), y, hexdig((unsigned char)(v & 15)), col);
}

int main(void)
{
    ran = 0x11;
    vdc_init();
    kb_init();

    scr_puts(2, 1, "F256K KEY TEST -- WHAT kb_waitkey RETURNS", EGA_YELLOW);
    scr_puts(2, 3, "N   VALUE  MEANING", EGA_LTGRAY);
    ran = 0x5A;

    for (;;) {
        unsigned char c = (unsigned char)kb_waitkey();
        if (got_n < 32) {
            unsigned char y = (unsigned char)(4 + got_n);
            got[got_n] = c;
            hex2(2, y, got_n, EGA_DKGRAY);
            hex2(6, y, c, EGA_WHITE);
            /* Name the ones that are NOT plain ASCII, because those are
               exactly the ones translate() had to invent. */
            scr_puts(11, y,
                c == KB_UP     ? "KB_UP"     :
                c == KB_DOWN   ? "KB_DOWN"   :
                c == KB_ESC    ? "KB_ESC"    :
                c == KB_DELETE ? "KB_DELETE" :
                c == KB_RETURN ? "KB_RETURN" :
                c == KB_SPACE  ? "KB_SPACE"  : "(ascii)", EGA_LTCYAN);
            if (c >= 33 && c < 127) scr_put(24, y, c, EGA_LTGREEN);
            got_n++;
        }
        entropy_seen = kb_entropy;
        lost_seen = f256_other_lost;
    }
}

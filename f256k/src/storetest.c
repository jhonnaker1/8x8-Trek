/* The disk seam, end to end, through the real plat_ functions.
 *
 * NINE CHECKS, and they are the ones that have actually gone wrong in this
 * project rather than the ones that are easy to write:
 *
 *   - A LENGTH THAT CROSSES BLOCKS. 600 bytes is 255 + 255 + 90, so a reader
 *     that mistakes a short block for end-of-file fails here and nowhere in a
 *     small test would.
 *   - THE CONTENT, not just the length. `delivered == 0 means 256` is a bug
 *     whose symptom is a correct byte COUNT over a buffer nobody filled --
 *     step two of the read is a separate kernel call and skipping it looks
 *     like success.
 *   - A SECOND, SHORTER WRITE TO THE SAME NAME. storage.h says a write
 *     REPLACES what is there. If the kernel's WRITE mode appends instead,
 *     every save after the first is a long file with a valid short save at
 *     the front -- which loads, and is wrong.
 *   - A FILE LONGER THAN THE BUFFER must be STOR_ERROR, not a truncation.
 *   - A MISSING FILE must be STOR_NOTFOUND, not STOR_ERROR: the restore
 *     prompt needs to tell "no save yet" from "the drive is broken".
 *   - AND THE KEYSTROKES MUST SURVIVE. Keys are typed BEFORE the file work
 *     and read back after it. uno's reference implementation drops every
 *     event it is not waiting for, keystrokes included, and nothing reports
 *     it -- this check is the reason f256evt.c exists.
 */
#include <stdint.h>
#include "../../c128/src/vdc.h"
#include "../../c128/src/input.h"
#include "../../core/ega.h"
#include "../../core/storage.h"
#include "f256kern.h"
#include "f256evt.h"

TREK_SIGNATURE;
__attribute__((used, retain)) volatile unsigned char ran;
#define NCHECK 10
__attribute__((used, retain)) volatile unsigned char res[NCHECK];
__attribute__((used, retain)) volatile unsigned int  detail[NCHECK];
__attribute__((used, retain)) volatile unsigned char done_n;

#define BIG 600
static unsigned char out_buf[BIG];
static unsigned char in_buf[BIG + 8];
/* THE "BSS REACHES ABOVE $A000" CHECK LIVED HERE AND IS GONE, because the
   claim it made is no longer this file's to make. When it was written, `ram`
   ran $2000-$BFFF and .bss was at the top of it. The overlay split moved
   .bss into the low region at $0400 and gave $A000-$BFFF to the window -- so
   a 28K array here would not reach $A000, it would overflow a 7K region.

   The claim itself is now carried by a STRONGER test: make check-ovl EXECUTES
   code at $A000-$BFFF out of eleven different RAM banks. Running there beats
   storing there. A check kept after its meaning moved is worse than no check
   -- see the sweep notes on stale assertions. */

static const char NAME[] = "TREKTEST.DAT";
static const char GONE[] = "NOSUCH.DAT";

static unsigned char hexdig(unsigned char v)
{ return (unsigned char)(v < 10 ? 48 + v : 1 + v - 10); }

static void hex4(unsigned char x, unsigned char y, unsigned int v, unsigned char c)
{
    scr_put(x, y, hexdig((unsigned char)(v >> 12)), c);
    scr_put((unsigned char)(x+1), y, hexdig((unsigned char)((v >> 8) & 15)), c);
    scr_put((unsigned char)(x+2), y, hexdig((unsigned char)((v >> 4) & 15)), c);
    scr_put((unsigned char)(x+3), y, hexdig((unsigned char)(v & 15)), c);
}

static void check(const char *label, unsigned char ok, unsigned int d)
{
    unsigned char y = (unsigned char)(4 + done_n);
    res[done_n] = ok;
    detail[done_n] = d;
    scr_puts(2, y, label, EGA_LTGRAY);
    scr_puts(34, y, ok ? "PASS" : "FAIL", ok ? EGA_LTGREEN : EGA_LTRED);
    hex4(41, y, d, EGA_DKGRAY);
    done_n++;
}

/* A pattern where EVERY byte depends on its position, so a block copied to
   the wrong offset -- or not copied at all -- shows up as a mismatch rather
   than as plausible-looking data. */
static unsigned char pat(uint16_t i)
{
    return (unsigned char)((i * 7u) ^ (i >> 3) ^ 0x5A);
}

int main(void)
{
    uint16_t i, got;
    uint8_t st;
    unsigned char keys[4], nkeys = 0, k;

    ran = 0x11;
    vdc_init();
    kb_init();
    scr_puts(2, 1, "F256K STORAGE TEST", EGA_YELLOW);
    ran = 0x5A;

    for (i = 0; i < BIG; i++) out_buf[i] = pat(i);

    /* THE KEYS GO IN FIRST, before any file work, and are read out after all
       of it.
     *
     * PACED BY FRAMES, NOT BY A LOOP COUNT. This was `for (i = 0; i < 40000;
     * i++) f256_pump();` -- a fixed iteration count standing in for a
     * duration, which is only a duration for as long as the code around it
     * stays the same size. The overlay split moved every one of these
     * functions and the loop stopped being long enough: the keystroke check
     * failed on a build whose keyboard was provably fine. A frame count means
     * the same wall time whatever the compiler does. */
    {
        unsigned char t0 = f256_frames();
        while ((unsigned char)(f256_frames() - t0) < 120) f256_pump();
    }

    st = plat_write_all(NAME, out_buf, BIG);
    check("WRITE 600 BYTES", st == STOR_OK, st);

    for (i = 0; i < BIG + 8; i++) in_buf[i] = 0;
    st = plat_read_all(NAME, in_buf, BIG + 8, &got);
    check("READ IT BACK", st == STOR_OK, st);
    check("LENGTH IS 600", got == BIG, got);

    {
        uint16_t bad = 0xFFFF;
        for (i = 0; i < BIG; i++)
            if (in_buf[i] != pat(i)) { bad = i; break; }
        check("EVERY BYTE MATCHES", bad == 0xFFFF, bad);
    }

    st = plat_read_all(GONE, in_buf, BIG, &got);
    check("MISSING FILE IS NOTFOUND", st == STOR_NOTFOUND, st);

    st = plat_read_all(NAME, in_buf, 100, &got);
    check("TOO BIG FOR BUFFER IS ERROR", st == STOR_ERROR, st);

    /* A SHORTER WRITE OVER A LONGER FILE. */
    st = plat_write_all(NAME, out_buf, 20);
    check("REWRITE SHORT", st == STOR_OK, st);
    st = plat_read_all(NAME, in_buf, BIG, &got);
    check("REPLACED, NOT APPENDED", st == STOR_OK && got == 20, got);

    /* Streaming, which is how the briefing is read. Put the long file back
       first, then take it in 140-byte bites so the reads straddle the
       kernel's blocks rather than lining up with them. */
    (void)plat_write_all(NAME, out_buf, BIG);
    {
        uint16_t total = 0, n;
        uint16_t bad = 0xFFFF;
        for (i = 0; i < BIG + 8; i++) in_buf[i] = 0;
        if (plat_open(NAME) == STOR_OK) {
            for (;;) {
                n = plat_read(in_buf + total, 140);
                if (n == 0) break;
                total = (uint16_t)(total + n);
                if (total > BIG) break;
            }
            plat_close();
        }
        for (i = 0; i < total && i < BIG; i++)
            if (in_buf[i] != pat(i)) { bad = i; break; }
        check("STREAMED 600 IN 140s", total == BIG && bad == 0xFFFF,
              (unsigned int)total);
    }

    /* AND NOW THE KEYS. Everything above ran through f256_wait_file, which
       pumps the same queue the keyboard arrives on. */
    while (nkeys < 4) {
        k = f256_getkey();
        if (k == KB_NONE) break;
        keys[nkeys++] = k;
    }
    check("KEYS SURVIVED THE DISK",
          nkeys >= 3 && keys[0] == 'A' && keys[1] == 'B' && keys[2] == 'C',
          (unsigned int)((nkeys << 8) | (nkeys ? keys[0] : 0)));

    scr_puts(2, (unsigned char)(6 + done_n), "DONE.", EGA_WHITE);
    for (;;) { f256_pump(); }
}

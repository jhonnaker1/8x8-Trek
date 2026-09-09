/* Far memory across VBXE's 4K MEMAC window, and the overlay-sized copy that
 * rides on it.
 *
 * THE CASE THAT MATTERS IS A READ THAT STRADDLES A BANK BOUNDARY -- impossible
 * on the C128, where the far store is one flat 64K bank, and therefore
 * untested by anything inherited from it. Here the window is 4K, so a 4,608-
 * byte overlay image crosses at least one boundary and usually two, and it
 * does so on the game's hot path.
 *
 * LABELS ARE UPPERCASE WITH NO UNDERSCORE, and that is a finding rather than
 * a style: scr_puts maps ASCII 64..95 to screen codes 0..31, so '_' (95)
 * becomes screen code 31 -- which this port's font draws as a left arrow,
 * because that is what a C64 screen code 31 IS. The first run of this test
 * printed "FAR+READ". Nothing in the game's own strings uses an underscore,
 * so it costs nothing there; it costs a misread screenshot here.
 *
 * IT PLANTS ITS OWN PATTERN rather than loading a file, because the storage
 * seam is still stubbed. That is not a weaker test of the addressing: far_load
 * pushes bytes through far_write, which is what this uses, so what is exercised
 * is the same arithmetic on the same window. What it does NOT cover is
 * far_load's chunking against a real file, and that waits on storage.
 */
#include <stdint.h>

#include "../../core/farmem.h"
#include "atarimem.h"
#include "vbxevid.h"

#define PLANTED 9000U          /* spans banks 4, 5 and 6 */

extern char __ovl_start[];

static unsigned char row = 2;

static void say(const char *s, unsigned char colour) {
    scr_puts(2, row, s, colour);
}

static void num(unsigned char x, uint16_t n) {
    char buf[6];
    unsigned char i = 0;
    if (!n) { scr_puts(x, row, "0", 15); return; }
    while (n && i < 5) { buf[i++] = (char)('0' + n % 10); n /= 10; }
    buf[i] = 0;
    while (i--) { char c[2]; c[0] = buf[i]; c[1] = 0; scr_puts(x++, row, c, 15); }
}

static unsigned char want(uint16_t j) {
    return (unsigned char)((j * 7U + 11U) & 0xFFU);
}

/* Reports the offset and the bad count as well as the verdict, because "FAIL"
   alone does not distinguish a wrong bank from a wrong offset within one. */
static void check(const char *tag, uint16_t off, uint16_t len,
                  const unsigned char *got) {
    uint16_t i, bad = 0;
    for (i = 0; i < len; i++)
        if (got[i] != want((uint16_t)(off + i))) bad++;
    say(tag, bad ? 12 : 10);
    num(22, off);
    num(30, len);
    num(38, bad);
    scr_puts(46, row, bad ? "FAIL" : "PASS", bad ? 12 : 10);
    row++;
}

int main(void) {
    unsigned char buf[64];
    unsigned char back[64];
    uint16_t off, i;

    vdc_init();
    scr_puts(2, 0, "FAR MEMORY -- VBXE VRAM THROUGH A 4K WINDOW", 15);
    scr_puts(22, 1, "OFFSET  LEN     BAD", 7);

    /* Plant the pattern, 64 bytes at a time, straight across three banks. */
    for (off = 0; off < PLANTED; off = (uint16_t)(off + 64)) {
        uint16_t n = (uint16_t)((PLANTED - off < 64U) ? (PLANTED - off) : 64U);
        for (i = 0; i < n; i++) buf[i] = want((uint16_t)(off + i));
        far_write(off, buf, n);
    }

    /* far_read: inside one bank, then across each of the two boundaries. */
    far_read(0, back, 64);              check("FAR READ  FLAT ", 0, 64, back);
    far_read(4064, back, 64);           check("FAR READ  X4096", 4064, 64, back);
    far_read(8160, back, 64);           check("FAR READ  X8192", 8160, 64, back);

    /* far_bulk at the size and shape the overlay loader actually uses: one
       image, starting where it crosses BOTH boundaries. Into the overlay
       window itself, which is where ovl_load puts it. */
    far_bulk(4000, __ovl_start, OVL_WINDOW > 4608 ? 4608 : OVL_WINDOW);
    check("FAR BULK  IMAGE", 4000,
          OVL_WINDOW > 4608 ? 4608 : OVL_WINDOW,
          (const unsigned char *)__ovl_start);

    /* AND THE WINDOW IS STILL THE SCREEN'S AFTERWARDS. far_* moves the MEMAC
       bank; if it did not put it back, the next scr_puts would write into
       whatever bank was left selected and this line would not appear. It is
       the check that the bank bookkeeping is shared correctly between two
       drivers, and it can only be made from here. */
    row++;
    scr_puts(2, row, "IF YOU CAN READ THIS, THE SCREEN BANK SURVIVED FAR ACCESS.", 14);

    for (;;) { }
    return 0;
}

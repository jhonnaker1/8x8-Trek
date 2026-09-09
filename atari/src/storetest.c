/* Does the disk seam agree with DOS?
 *
 * tools/atr.py can round-trip a file through the image on the host, but that
 * only proves the tool is self-consistent -- it writes the sector chains and
 * then reads back its own. THE DISCRIMINATOR IS DOS READING THEM, which is
 * what this does: the files below were planted by atr.py and are opened here
 * through the D: handler, and the file this writes is extracted by atr.py
 * afterwards and compared on the host. Each half checks the other's work.
 *
 * It runs as AUTORUN.SYS off a booted DOS 2.5 disk, which is both the only way
 * to have D: exist at all and what a release would do anyway.
 */
#include <stdint.h>

#include "../../core/storage.h"
#include "vbxevid.h"

extern unsigned char plat_dbg_status;

#define BIG_BYTES 3000U         /* TREKDATA.BIN, planted by atr.py */
#define OUT_BYTES 700U          /* what this writes back */

static unsigned char buf[128];
static unsigned char row = 2;

static unsigned char want(uint16_t j) {
    return (unsigned char)((j * 7U + 11U) & 0xFFU);
}

static void num(unsigned char x, uint16_t n) {
    char t[6];
    unsigned char i = 0;
    if (!n) { scr_puts(x, row, "0", 15); return; }
    while (n && i < 5) { t[i++] = (char)('0' + n % 10); n /= 10; }
    while (i--) { char c[2]; c[0] = t[i]; c[1] = 0; scr_puts(x++, row, c, 15); }
}

static void result(const char *tag, unsigned char ok, uint16_t a, uint16_t b) {
    scr_puts(2, row, tag, ok ? 10 : 12);
    num(34, a);
    num(42, b);
    num(50, plat_dbg_status);
    scr_puts(58, row, ok ? "PASS" : "FAIL", ok ? 10 : 12);
    row++;
}

int main(void) {
    uint16_t got, total, i, bad;
    uint8_t st;

    vdc_init();
    scr_puts(2, 0, "DISK SEAM -- CIO, IOCB 1, THE D: HANDLER", 15);
    scr_puts(34, 1, "A       B       CIO", 7);

    /* 1. A WHOLE-FILE READ of something small. */
    st = plat_read_all("SHORT.TXT", buf, (uint16_t)sizeof buf, &got);
    result("read_all SHORT.TXT", st == STOR_OK && got == 19, st, got);

    /* 2. A MISSING FILE must be NOTFOUND and not merely ERROR -- it is the one
          distinction core/storage.h requires this seam to make, because the
          save/restore UI branches on it. */
    st = plat_read_all("NOSUCH.DAT", buf, (uint16_t)sizeof buf, &got);
    result("read_all missing = NOTFOUND", st == STOR_NOTFOUND, st, got);

    /* 3. THE STREAMING PATH, which is what the briefing and far_load use, over
          a file far longer than any buffer here -- so the short final block
          and the zero-length call after it both happen. */
    total = 0;
    bad = 0;
    if (plat_open("TREKDATA.BIN") != STOR_OK) {
        result("open TREKDATA.BIN", 0, plat_dbg_status, 0);
    } else {
        for (;;) {
            got = plat_read(buf, (uint16_t)sizeof buf);
            if (!got) break;
            for (i = 0; i < got; i++)
                if (buf[i] != want((uint16_t)(total + i))) bad++;
            total = (uint16_t)(total + got);
        }
        plat_close();
        result("stream TREKDATA.BIN", total == BIG_BYTES && bad == 0, total, bad);
    }

    /* 4. A WRITE, which atr.py checks on the host afterwards. Written from the
          same generator, so the host needs no copy of the bytes to compare
          against -- only the rule. */
    for (i = 0; i < (uint16_t)sizeof buf; i++) buf[i] = want(i);
    {
        /* Longer than the buffer, so the write crosses sector boundaries the
           way a save record does. Built by repeating the generator. */
        static unsigned char out[OUT_BYTES];
        for (i = 0; i < OUT_BYTES; i++) out[i] = want(i);
        st = plat_write_all("WROTE.DAT", out, OUT_BYTES);
        result("write_all WROTE.DAT", st == STOR_OK, st, OUT_BYTES);

        /* 5. AND READ IT BACK through the same seam, before the host looks --
              so a failure here separates "wrote nothing" from "wrote
              something DOS cannot read". */
        bad = 0;
        for (i = 0; i < OUT_BYTES; i++) out[i] = 0;
        st = plat_read_all("WROTE.DAT", out, OUT_BYTES, &got);
        for (i = 0; i < got; i++) if (out[i] != want(i)) bad++;
        result("read back: st, got", st == STOR_OK && got == OUT_BYTES, st, got);
        result("read back: bad bytes", bad == 0 && got == OUT_BYTES, bad, got);
    }

    scr_puts(2, (unsigned char)(row + 1),
             "HOST CHECKS WROTE.DAT OUT OF THE IMAGE NEXT.", 14);
    for (;;) { }
    return 0;
}

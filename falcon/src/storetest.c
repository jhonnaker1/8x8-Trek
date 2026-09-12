/* Does the storage seam actually do what core/storage.h promises?
 *
 * Run on the machine, not reasoned about: the X16's version of this caught a
 * channel-ordering bug that would have made every load but the first fail,
 * and the MEGA65's caught a hypervisor that hands back the trap number as a
 * file descriptor. Each check prints PASS or FAIL and the whole thing ends
 * with a count, so one screenshot says whether the seam is sound.
 *
 * IT EARNED ITS KEEP HERE TOO, ON THE FIRST RUN. GEMDOS made this port's
 * storage seam so easy -- Fopen, Fread, Fclose, no channels, no device
 * numbers -- that the seam was written straight through and looked obviously
 * right. It was not: `Fread(h, max, buf)` reads UP TO max bytes and reports
 * how many, so a file LONGER than the buffer came back STOR_OK with a silent
 * truncation, which is the one thing storage.h names as forbidden ("silently
 * reading half a save is worse than refusing"). The easiest seam on the
 * project was the one that skipped its own contract.
 */
#include "../../c128/src/vdc.h"
#include "../../c128/src/input.h"
#include "../../core/storage.h"

static unsigned char row = 1;
static int failures;

static void say(const char *what, int ok) {
    scr_puts(2, row, what, 7);
    scr_puts(60, row, ok ? "PASS" : "FAIL", ok ? 10 : 12);
    if (!ok) failures++;
    row++;
}

static void num(const char *label, uint16_t v) {
    char b[6];
    b[0] = (char)('0' + (v / 10000) % 10);
    b[1] = (char)('0' + (v / 1000) % 10);
    b[2] = (char)('0' + (v / 100) % 10);
    b[3] = (char)('0' + (v / 10) % 10);
    b[4] = (char)('0' + v % 10);
    b[5] = '\0';
    scr_puts(2, row, label, 8);
    scr_puts(40, row, b, 14);
    row++;
}

#define N 600
static unsigned char out[N], in[N + 4];

int main(void) {
    uint16_t got = 0, i, n;
    uint8_t rc;

    vdc_init();
    kb_init();
    scr_puts(2, 0, "FALCON STORAGE SEAM", 15);

    for (i = 0; i < N; i++) out[i] = (unsigned char)((i * 7 + i / 251) & 0xFF);

    rc = plat_write_all("STORTEST.DAT", out, N);
    say("PLAT WRITE ALL returns STOR_OK", rc == STOR_OK);

    got = 0;
    rc = plat_read_all("STORTEST.DAT", in, sizeof in, &got);
    say("PLAT READ ALL returns STOR_OK", rc == STOR_OK);
    say("the byte count comes back", got == N);
    num("bytes read", got);

    for (i = 0; i < N && in[i] == out[i]; i++) { }
    say("every byte round-trips", i == N);

    /* THE CONTRACT'S ONE SHARP EDGE: a file longer than max must be an ERROR
       and not a short read. Reading the same 600-byte file into 100 bytes. */
    got = 0xFFFF;
    rc = plat_read_all("STORTEST.DAT", in, 100, &got);
    say("a file longer than max is an error", rc == STOR_ERROR);

    /* A missing file has to be NOTFOUND, not ERROR: the setup screen offers to
       restore a save and must tell "there is none" from "the disk went wrong". */
    rc = plat_read_all("NOSUCHFILE.DAT", in, sizeof in, &got);
    say("a missing file is STOR_NOTFOUND", rc == STOR_NOTFOUND);
    say("a missing file reports zero bytes", got == 0);

    rc = plat_open("NOSUCHFILE.DAT");
    say("PLAT OPEN of a missing file is NOTFOUND", rc == STOR_NOTFOUND);

    /* The streaming path, which the briefing uses. Read the same file back in
       small bites and count. */
    rc = plat_open("STORTEST.DAT");
    say("PLAT OPEN returns STOR_OK", rc == STOR_OK);
    n = 0;
    for (;;) {
        uint16_t k = plat_read(in, 64);
        if (!k) break;
        for (i = 0; i < k; i++) if (in[i] != out[n + i]) { n = 0xFFFF; break; }
        if (n == 0xFFFF) break;
        n = (uint16_t)(n + k);
    }
    say("streaming read returns the same bytes", n == N);
    say("PLAT READ returns 0 at end of file", plat_read(in, 64) == 0);
    plat_close();

    /* plat_open must close whatever was open before it -- storage.h says one
       at a time, and the X16 got this wrong in a way that broke every load
       after the first. */
    say("PLAT OPEN twice in a row is fine",
        plat_open("STORTEST.DAT") == STOR_OK &&
        plat_open("STORTEST.DAT") == STOR_OK);
    plat_close();

    row++;
    num("failures", (uint16_t)failures);
    scr_puts(2, row, failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED",
             failures ? 12 : 10);
    row++;
    scr_puts(2, (unsigned char)(row + 1), "PRESS RETURN", 14);

    while (kb_waitkey() != KB_RETURN) { }
    vdc_shutdown();
    plat_exit();
    return 0;
}

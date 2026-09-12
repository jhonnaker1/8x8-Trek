/* Does the Disk BASIC filesystem layer do what core/storage.h promises?
 *
 * Results land at $7000 for the host to read: this port has no video yet, so
 * the screen cannot be the report the way it is on every other port. Build it
 * against coco3/src/coco3storage.c and drive it with MAME's DEBUGGER (Lua's
 * frame notifier dies -- see README.md):
 *
 *   cmoc --coco --org=3800 -fomit-frame-pointer -o fstest.bin \
 *        fstest.c ../../coco3/src/coco3storage.c
 *   ... strip the 5-byte DECB header, load the payload at $3800, pc=3800
 *   ... gtime 9000   -- REAL FLOPPY TIMING, it needs seconds, not frames
 *   ... dump res.txt,7000,10,1,0
 *
 * ALLOW ENOUGH TIME. At 2000 (hex, 8s) the run stopped part way through and
 * the half-filled struct read exactly like a hang in the missing-file case.
 * It was nine directory sectors at floppy speed and nothing else.
 */
#include <stdint.h>
#include "../../core/storage.h"

static unsigned char buf[8192];

struct result {
    unsigned char ran;
    unsigned char rc_all;      unsigned int got_all;   unsigned int sum_all;
    unsigned char rc_toolong;
    unsigned char rc_missing;
    unsigned char rc_open;     unsigned int got_stream; unsigned int sum_stream;
    unsigned char rc_reopen;
    unsigned char done;
};

int main(void)
{
    struct result *r = (struct result *) 0x7000;
    uint16_t got = 0, i, n;
    unsigned int sum;

    r->ran = 0xA5;

    r->rc_all = plat_read_all("STRINGS.DAT", buf, sizeof buf, &got);
    r->got_all = got;
    sum = 0;
    for (i = 0; i < got; i++) sum += buf[i];
    r->sum_all = sum;

    /* a file longer than max must be an ERROR, not a truncation */
    r->rc_toolong = plat_read_all("STRINGS.DAT", buf, 100, &got);

    /* a missing file must be NOTFOUND, so the setup screen can tell "no save"
       from "the disk went wrong" */
    r->rc_missing = plat_read_all("NOSUCH.DAT", buf, sizeof buf, &got);

    /* the streaming path, which the briefing uses */
    r->rc_open = plat_open("STRINGS.DAT");
    sum = 0; n = 0;
    for (;;) {
        uint16_t k = plat_read(buf, 64);
        if (!k) break;
        for (i = 0; i < k; i++) sum += buf[i];
        n = (uint16_t)(n + k);
    }
    r->got_stream = n;
    r->sum_stream = sum;
    plat_close();

    /* plat_open twice in a row must be fine -- the X16 got this wrong in a
       way that broke every load after the first */
    r->rc_reopen = (uint8_t)(plat_open("STRINGS.DAT") == STOR_OK &&
                             plat_open("STRINGS.DAT") == STOR_OK);
    plat_close();

    r->done = 0x5A;
    for (;;) ;
}

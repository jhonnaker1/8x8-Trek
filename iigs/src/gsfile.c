/* PROBE 6: does the storage seam read a named file off our own disk?
 *
 * The boot block already reads raw blocks -- that is how the game gets loaded.
 * What is new here is the DIRECTORY: a name, an extent, and the five plat_*
 * of core/storage.h over the top. So this asks for a file by name, reports
 * what came back, and then writes one to a slot and reads it back, because a
 * save that cannot be re-read is the failure mode that matters.
 */
#include <string.h>
#include "../../core/storage.h"

#define ASMVAR __attribute__((used, retain))

ASMVAR unsigned char rep[64];
ASMVAR unsigned char gs_done;

__attribute__((naked, used, retain, section(".init.040")))
void gs_emul(void) { __asm__ volatile("sec\n\txce"); }
__attribute__((naked, used, retain, section(".init.050")))
void gs_setdb(void) { __asm__ volatile("phk\n\tplb"); }

extern volatile unsigned char blk_dbg_status, blk_dbg_booted;

static unsigned char buf[64];

int main(void)
{
    unsigned int got = 0;
    unsigned char st;
    unsigned char i;

    for (i = 0; i < 64; i++) rep[i] = 0;

    /* 1. A file that IS there. */
    st = plat_read_all("TEST.DAT", buf, sizeof buf, &got);
    rep[0] = st;
    rep[1] = (unsigned char)got;
    rep[2] = blk_dbg_booted;
    rep[3] = blk_dbg_status;
    for (i = 0; i < 16; i++) rep[8 + i] = buf[i];

    /* 2. A file that is NOT, because "found" means nothing without it. */
    rep[4] = plat_read_all("NOSUCH.DAT", buf, sizeof buf, &got);

    /* 3. A write to a name with no slot bit must be REFUSED BY THE FORMAT. */
    rep[5] = plat_write_all("TEST.DAT", "XXXX", 4);

    /* 4. A write to a new name takes a slot; then read it back. A save that
       cannot be re-read is the failure mode that matters, and only the second
       half of this pair can see it. */
    rep[6] = plat_write_all("SAVE.DAT", "SAVED-OK", 8);
    memset(buf, 0, sizeof buf);
    rep[7] = plat_read_all("SAVE.DAT", buf, sizeof buf, &got);
    rep[24] = (unsigned char)got;
    for (i = 0; i < 8; i++) rep[32 + i] = buf[i];

    /* 5. And the streaming path, which the briefing uses. */
    if (plat_open("TEST.DAT") == STOR_OK) {
        rep[25] = (unsigned char)plat_read(buf, 5);
        rep[26] = buf[0];
        rep[27] = (unsigned char)plat_read(buf, 5);
        rep[28] = buf[0];
        plat_close();
    }

    gs_done = 0x5A;
    for (;;) ;
    return 0;
}

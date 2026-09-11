/* The no-DOS storage seam, against a disk with no DOS on it.
 *
 * Exercises all five plat_* of core/storage.h through src/atarisio.c: a whole
 * file read checked against a known length and known bytes, a STREAMED read
 * (which is what the briefing needs and what a whole-file read cannot stand in
 * for), and a write followed by a read-back.
 *
 * Results in volatile globals, no screen: a failure here must not be able to
 * hide behind the video seam, and a probe the program never reads is invisible
 * to the optimiser without volatile.
 */
#include <stdint.h>
#include <string.h>

#include "../../core/storage.h"

volatile unsigned char t_read_st, t_open_st, t_write_st, t_back_st, t_done;
volatile unsigned int  t_read_len, t_stream_len, t_back_len;
volatile unsigned char t_read_head[4], t_back_head[8];

static unsigned char big[8000];
static unsigned char chunk[64];

int main(void) {
    uint16_t got = 0, n;
    unsigned char i;

    /* 1. WHOLE FILE. STRINGS.DAT is 7,284 bytes and starts with its own
          count word -- a length that is wrong is as bad as bytes that are. */
    t_read_st = plat_read_all("STRINGS.DAT", big, sizeof big, &got);
    t_read_len = got;
    for (i = 0; i < 4; i++) t_read_head[i] = big[i];

    /* 2. STREAMED. The briefing is read a piece at a time and never held
          whole, so this is a different path, not a smaller version of (1). */
    t_open_st = plat_open("BRIEF.TXT");
    t_stream_len = 0;
    if (t_open_st == STOR_OK) {
        while ((n = plat_read(chunk, sizeof chunk)) != 0)
            t_stream_len = (unsigned int)(t_stream_len + n);
        plat_close();
    }

    /* 3. WRITE, then READ BACK. A status of OK on the write proves nothing;
          this project has been caught by exactly that twice. */
    for (i = 0; i < 64; i++) chunk[i] = (unsigned char)(0x5A ^ i);
    t_write_st = plat_write_all("EGATREK.SAV", chunk, 64);
    memset(big, 0, 64);
    t_back_st = plat_read_all("EGATREK.SAV", big, sizeof big, &got);
    t_back_len = got;
    for (i = 0; i < 8; i++) t_back_head[i] = big[i];

    t_done = 1;
    for (;;) { }
}

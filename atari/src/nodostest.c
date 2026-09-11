/* The no-DOS storage seam, against a disk with no DOS on it.
 *
 * Exercises all five plat_* of core/storage.h through src/atarisio.c: a whole
 * file read, a STREAMED read (which is what the briefing needs and what a
 * whole-file read cannot stand in for), and a write followed by a read-back.
 *
 * EVERY PATH IS CHECKED BY ITS CONTENT, NOT ITS LENGTH, and that is a repair.
 * The first version of this file summed nothing and compared only the byte
 * count -- which plat_read maintained from the DIRECTORY, independently of
 * what it actually copied. So when plat_read was dropping the back half of
 * every sector, this probe reported the briefing's exact length, 10,557, and
 * was believed. The bug surfaced two changes later as a build-stamp mismatch
 * on OVERLAYS.BIN: a true statement about a file that was never read.
 *
 * AND THE CHUNK IS FIFTY BYTES, NOT SIXTY-FOUR. Both are smaller than a
 * sector, but 64 divides 128 -- so a reader that mishandles the middle of a
 * sector still lands on a boundary every other chunk. 50 does not divide 128
 * and never lines up.
 *
 * Results in volatile globals, no screen: a failure here must not be able to
 * hide behind the video seam, and a probe the program never reads is
 * invisible to the optimiser without volatile.
 */
#include <stdint.h>
#include <string.h>

#include "../../core/storage.h"

volatile unsigned char t_read_st, t_open_st, t_write_st, t_back_st, t_done;
volatile unsigned int  t_read_len, t_stream_len, t_back_len;
volatile unsigned int  t_read_sum, t_stream_sum, t_back_sum;

/* 1,600 BYTES, NOT 8,000, AND THE SIZE IS AN ARGUMENT ABOUT THE SEAM.
   plat_read_all is only ever asked for a save or the hall of fame -- 625 and
   384 bytes, both inside a slot's 1,024. The big files (STRINGS.DAT,
   MUSIC.DAT, OVERLAYS.BIN) reach the game through far_load(), which STREAMS,
   and that path is (2) below. An 8,000-byte buffer here modelled a call the
   game does not make, and after the writable half moved to $0A00 it did not
   fit either. */
static unsigned char big[1600];
static unsigned char chunk[50];

/* Position-sensitive on purpose: a plain byte sum cannot tell a reordered
   file from a correct one, and losing half of every sector reorders. */
static unsigned int tally(const unsigned char *p, unsigned int n,
                          unsigned int sum) {
    while (n--) sum = (unsigned int)((sum << 1 | sum >> 15) + *p++);
    return sum;
}

int main(void) {
    uint16_t got = 0, n;

    /* 1. WHOLE FILE. 412 bytes is three sectors and a bit, so the last one
          is partial -- the case a whole-sector count would get away with. */
    t_read_st = plat_read_all("MUSIC.DAT", big, sizeof big, &got);
    t_read_len = got;
    t_read_sum = tally(big, got, 0);

    /* 2. STREAMED. The briefing is read a piece at a time and never held
          whole, so this is a different path, not a smaller version of (1). */
    t_open_st = plat_open("BRIEF.TXT");
    t_stream_len = 0;
    t_stream_sum = 0;
    if (t_open_st == STOR_OK) {
        while ((n = plat_read(chunk, sizeof chunk)) != 0) {
            t_stream_len = (unsigned int)(t_stream_len + n);
            t_stream_sum = tally(chunk, n, t_stream_sum);
        }
        plat_close();
    }

    /* 3. WRITE, then READ BACK. A status of OK on the write proves nothing;
          this project has been caught by exactly that twice. The name is one
          no disk is built with, so this also claims a SLOT -- see
          tools/nodos.py on why the save cannot be a build-time file. */
    {
        unsigned int i;
        for (i = 0; i < sizeof big && i < 300; i++)
            big[i] = (unsigned char)(0x5A ^ (i * 7));
        t_write_st = plat_write_all("PROBE.DAT", big, 300);
        memset(big, 0, 300);
        t_back_st = plat_read_all("PROBE.DAT", big, sizeof big, &got);
        t_back_len = got;
        t_back_sum = tally(big, got, 0);
    }

    t_done = 1;
    for (;;) { }
}

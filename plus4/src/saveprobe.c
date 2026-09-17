/* DOES A SAVE ACTUALLY PUT BYTES ON THE DISK?
 *
 * Jamie saved a game with the default filename and restoring it said the file
 * was not found. The cause: storage.c writes every byte through cbm_k_bsout,
 * which is a SEPARATE SYMBOL from cbm_k_chrout at the same $FFD2, and
 * p4bank.c wrapped only chrout. The unwrapped libc bsout is a bare
 * `jsr $FFD2` -- which p4_ram_in() has filled with an RTS. So OPEN succeeded,
 * CKOUT succeeded, every byte went to the RTS, the file closed clean and the
 * drive reported no error.
 *
 * EVERY STATUS SAID SUCCESS, which is why this probe checks the CONTENT and
 * not the return codes: write a known pattern, read it back, and compare byte
 * for byte. [[hof-write-witnessed]] -- witnessed, not inferred.
 *
 * report[0] write status   report[2..3] bytes read back
 * report[1] read status    report[4]    1 if every byte matches
 * report[5] first mismatching byte's index low, report[6] high
 * report[15] completion marker
 */
#include "../../core/storage.h"

volatile unsigned char report[16];

#define N 300
static unsigned char out[N], back[N];

int main(void)
{
    unsigned int i, got = 0;
    unsigned char ok = 1;

    for (i = 0; i < N; i++) out[i] = (unsigned char)(i * 7 + 3);

    report[0] = plat_write_all("TESTSAVE", out, N);
    report[1] = plat_read_all("TESTSAVE", back, N, &got);
    report[2] = (unsigned char)got;
    report[3] = (unsigned char)(got >> 8);

    for (i = 0; i < N; i++) {
        if (back[i] != out[i]) {
            ok = 0;
            report[5] = (unsigned char)i;
            report[6] = (unsigned char)(i >> 8);
            break;
        }
    }
    report[4] = (unsigned char)(ok && got == N);
    report[15] = 0x5A;
    for (;;) { }
}

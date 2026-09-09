/* The disk seam for the Atari 800XL: CIO, IOCB 1, and the D: handler.
 *
 * core/storage.h names this port's answer in its own header -- "`D:TREK.SCR`
 * through an Atari IOCB" -- and that is what this is. Names arrive as opaque
 * tokens and a device prefix is added here; the core and the UI never see a
 * path, a device number or a channel.
 *
 * WHY CIO AND NOT SIO. Going straight to the OS's SIO vector would read
 * sectors without DOS resident, which would hand this port back the 2,282
 * bytes of writable data it wants below the window. But saves are named by the
 * PLAYER -- ui.c lets them type a filename -- and sector ranges have no names,
 * so that route needs a filesystem of its own. CIO has one already. The cost
 * is measured and recorded in README.md rather than assumed: DOS 2.5 puts
 * MEMLO at $1CFC, read off a booted machine, so with DOS resident there are
 * 772 bytes below the VBXE window rather than 2,282.
 *
 * THE INLINE ASM DECLARES "p" AND THAT IS NOT OPTIONAL. llvm-mos will
 * otherwise emit a compare BEFORE a `jsr` and branch on the flags AFTER it,
 * because nothing told it the call touches the status register. That cost this
 * project a day on the MEGA65 and was latent in the released C128 and X16
 * builds. Every asm block here that reaches CIOV carries it.
 */
#include <stdint.h>

#include "../../core/storage.h"

#define CIOV 0xE456

/* IOCB 1. IOCB 0 is the OS's own screen editor and is left alone. */
#define IOCB  ((volatile unsigned char *)0x0350)
#define IOCB_X 0x10          /* CIO wants the IOCB's byte offset in X */

#define ICCOM 2
#define ICSTA 3
#define ICBAL 4
#define ICBAH 5
#define ICBLL 8
#define ICBLH 9
#define ICAX1 10
#define ICAX2 11

#define CMD_OPEN   0x03
#define CMD_GET    0x07      /* get BYTES -- block, not GETREC, which stops
                                at an EOL and would corrupt every binary. */
#define CMD_PUT    0x0B
#define CMD_CLOSE  0x0C

#define AUX_READ   0x04
#define AUX_WRITE  0x08      /* create, truncating anything already there */

/* CIO STATUS, AND THERE ARE THREE SUCCESSFUL ONES FOR A READ, NOT TWO.
 *
 *   $01  the operation succeeded and there is more file after it
 *   $03  the LAST BYTE of the file was read, successfully -- returned when a
 *        read ends exactly at the end of the file
 *   $88  end of file: the read asked for more than was left
 *   $AA  file not found, the one error this seam is contractually required to
 *        tell apart; see core/storage.h on why the rest are lumped together
 *
 * $03 COST A ROUND OF TESTING AND WOULD HAVE COST A SAVED GAME. This driver
 * accepted $01 and $88 and called $03 an error, which is invisible until a
 * file's length happens to equal the buffer it is read into -- reading 19
 * bytes into 128 gives $88 and passes, reading 700 into 700 gives $03 and
 * fails. The save record is a fixed size read into a fixed-size buffer, so
 * that is not an unlikely case: it is the normal one. tools/storetest.py
 * writes a file and reads it back at exactly its own length for this reason.
 */
#define CIO_OK        0x01
#define CIO_EOF_LAST  0x03
#define CIO_EOF       0x88
#define CIO_NOTFOUND  0xAA

#define CIO_READ_OK(s) ((s) == CIO_OK || (s) == CIO_EOF_LAST || (s) == CIO_EOF)

static unsigned char open_live;

/* THE LAST RAW CIO STATUS, kept because STOR_* is deliberately coarse and a
   failing seam then reports "error" and nothing else. The MEGA65 port carries
   the same hook for the same reason: the first run of tools/storetest.py had
   three failures and no way to tell an unopenable file from an unreadable one
   without it. `used` so LTO cannot drop it when only a test reads it. */
__attribute__((used)) unsigned char plat_dbg_status;

/* ONE CALL SITE FOR THE ONLY `jsr` IN THIS FILE. Status comes back through
   ICSTA rather than out of Y, which keeps the asm block to two instructions
   with no output constraint to get wrong -- CIO writes the same value to both.
   "p" is the status register; see the header. */
static unsigned char cio(void) {
    __asm__ volatile("ldx #%0\n\t"
                     "jsr %1"
                     :
                     : "i"(IOCB_X), "i"(CIOV)
                     : "a", "x", "y", "memory", "p");
    plat_dbg_status = IOCB[ICSTA];
    return plat_dbg_status;
}

static void put_ptr(unsigned char lo, const void *p) {
    IOCB[lo]     = (unsigned char)((uintptr_t)p & 0xFF);
    IOCB[lo + 1] = (unsigned char)((uintptr_t)p >> 8);
}

/* "D:" + the caller's token + an EOL, which is how CIO knows the name has
   ended. ATASCII and ASCII agree over A-Z, 0-9 and '.', so the token copies
   straight through; anything outside that range is not a filename this game
   ever produces. */
static char path[16];

static void make_path(const char *name) {
    unsigned char i = 2;
    path[0] = 'D';
    path[1] = ':';
    while (*name && i < (unsigned char)(sizeof path - 1)) path[i++] = *name++;
    path[i] = (char)0x9B;                     /* ATASCII EOL terminates it */
}

static unsigned char open_file(const char *name, unsigned char aux) {
    if (open_live) plat_close();
    make_path(name);
    IOCB[ICCOM] = CMD_OPEN;
    IOCB[ICAX1] = aux;
    IOCB[ICAX2] = 0;
    put_ptr(ICBAL, path);
    IOCB[ICBLL] = 0;
    IOCB[ICBLH] = 0;
    if (cio() != CIO_OK) {
        unsigned char err = plat_dbg_status;

        /* A FAILED OPEN STILL HOLDS THE CHANNEL, and nothing says so until the
           NEXT open returns 129. Measured exactly that way: the missing-file
           check passed, and then every later operation failed with $81 because
           IOCB 1 was still allocated to an open that had never succeeded. So a
           close is issued here and the open's own status is put back
           afterwards -- the close overwrites it, and the interesting error is
           the one that started it. */
        IOCB[ICCOM] = CMD_CLOSE;
        (void)cio();
        plat_dbg_status = err;
        return (err == CIO_NOTFOUND) ? STOR_NOTFOUND : STOR_ERROR;
    }
    open_live = 1;
    return STOR_OK;
}

/* Transferred count comes back in ICBLL/ICBLH -- CIO overwrites the length
   with what it actually moved, which is the only way to see a short final
   block. */
static uint16_t transfer(unsigned char cmd, void *buf, uint16_t len) {
    IOCB[ICCOM] = cmd;
    put_ptr(ICBAL, buf);
    IOCB[ICBLL] = (unsigned char)(len & 0xFF);
    IOCB[ICBLH] = (unsigned char)(len >> 8);
    (void)cio();
    return (uint16_t)(IOCB[ICBLL] | ((uint16_t)IOCB[ICBLH] << 8));
}

void plat_close(void) {
    if (!open_live) return;
    IOCB[ICCOM] = CMD_CLOSE;
    (void)cio();
    open_live = 0;
}

uint8_t plat_open(const char *name) {
    return open_file(name, AUX_READ);
}

/* Returns bytes read, 0 at end of file. A short read is not an error: it is
   how the last block of a file arrives, and the NEXT call returns zero. */
uint16_t plat_read(void *buf, uint16_t len) {
    if (!open_live) return 0;
    return transfer(CMD_GET, buf, len);
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got) {
    uint8_t st = open_file(name, AUX_READ);

    *got = 0;
    if (st != STOR_OK) return st;
    *got = transfer(CMD_GET, buf, max);
    st = plat_dbg_status;
    plat_close();
    plat_dbg_status = st;          /* the close's status is never the
                                      interesting one -- see open_file */
    if (!CIO_READ_OK(st)) { *got = 0; return STOR_ERROR; }
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len) {
    uint16_t put;
    uint8_t st = open_file(name, AUX_WRITE);

    if (st != STOR_OK) return STOR_ERROR;     /* a create that fails is not
                                                 "not found" */
    put = transfer(CMD_PUT, (void *)buf, len);
    st = plat_dbg_status;
    plat_close();
    plat_dbg_status = st;
    return (st == CIO_OK && put == len) ? STOR_OK : STOR_ERROR;
}

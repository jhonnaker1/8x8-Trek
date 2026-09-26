/* Storage for the MSX2: MSX-DOS 2's file HANDLES, called direct.
 *
 * DIRECT, because the game is the shell: installed as the boot disk's
 * COMMAND2.COM there is no COMMAND2 in memory to route calls through, and
 * ($0005) jumps straight to the BDOS. Bare names ("STRINGS.DAT") resolve
 * against the boot drive's current directory, which is where the disk puts
 * them.
 *
 * A 64-BYTE READ-AHEAD, AND IT IS NOT OPTIONAL. ui_briefing() streams
 * BRIEF.TXT through plat_read(&c, 1), one byte a call. DOSTIME.COM timed
 * the same 10,557-byte file: 166 calls of 64 bytes took 75 frames, 10,558
 * calls of one byte took 2,735 -- FIVE MILLISECONDS a DOS2 call, and 55
 * seconds of briefing. So plat_read serves from this buffer and DOS sees 64.
 *
 * KEEP THE HANDLE YOURSELF: _READ does NOT preserve B. DOSTIME's first
 * version wrote B back after every call, and its second read used handle 0,
 * standard input -- it sat in CHGET waiting for a key.
 *
 * NEVER A DOS CONSOLE CALL: msx2input.c reads the BIOS key ring itself. */
#include <stdint.h>

#include "storage.h"

#define F_CLOSE   0x45
#define F_OPEN    0x43
#define F_CREATE  0x44
#define F_READ    0x48
#define F_WRITE   0x49

#define E_NOFIL   0xD7            /* .NOFIL, file not found */
#define E_EOF     0xC7            /* .EOF, read at end of file */

static unsigned char rA, rB;
static unsigned int  rDE, rHL;

/* One BDOS call through these globals, so no argument convention can be in
   doubt: A, B, DE, HL in; DOS's error code back in A; B and HL out. */
static unsigned char dos(unsigned char fn) __naked
{
    (void)fn;
    __asm
        push ix
        ld   c, a
        ld   a, (_rB)
        ld   b, a
        ld   de, (_rDE)
        ld   hl, (_rHL)
        ld   a, (_rA)
        call 5
        ld   (_rHL), hl
        ld   c, a
        ld   a, b
        ld   (_rB), a
        ld   a, c
        pop  ix
        ret
    __endasm;
}

static uint8_t map_err(unsigned char e)
{
    if (!e) return STOR_OK;
    return e == E_NOFIL ? STOR_NOTFOUND : STOR_ERROR;
}

/* Opens `name` read-only; the handle, on success, is left in rB. */
static unsigned char open_ro(const char *name)
{
    rDE = (unsigned int)name;
    rA = 1;                       /* no write */
    return dos(F_OPEN);
}

static void close_h(unsigned char h)
{
    rB = h;
    dos(F_CLOSE);
}

/* Reads up to `len` into `buf` from handle `h`; the count, 0 at the end. */
static uint16_t read_h(unsigned char h, void *buf, uint16_t len)
{
    rB = h;
    rDE = (unsigned int)buf;
    rHL = len;
    if (dos(F_READ)) return 0;    /* .EOF, or a real error: both end the read */
    return rHL;
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got)
{
    unsigned char h, e;
    uint16_t n;
    unsigned char extra;

    if ((e = open_ro(name)) != 0) return map_err(e);
    h = rB;
    n = read_h(h, buf, max);
    *got = n;
    /* LONGER THAN max IS AN ERROR, not a truncation: one more byte means
       the file did not fit. */
    e = (unsigned char)(n == max && read_h(h, &extra, 1) ? STOR_ERROR : STOR_OK);
    close_h(h);
    return e;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len)
{
    unsigned char h, e;

    rDE = (unsigned int)name;
    rA = 0;                       /* read and write */
    rB = 0;                       /* attributes; bit 7 clear REPLACES a file */
    if ((e = dos(F_CREATE)) != 0) return STOR_ERROR;
    h = rB;                       /* ...and rB still holds it for _WRITE */
    rDE = (unsigned int)buf;
    rHL = len;
    e = dos(F_WRITE);
    if (!e && rHL != len) e = 1;  /* a short write is a full disk */
    close_h(h);                   /* the close is what flushes the buffers */
    return e ? STOR_ERROR : STOR_OK;
}

/* The stream: one file at a time, served from the read-ahead. */
static unsigned char fh = 0xFF;
static unsigned char sbuf[64];
static unsigned char spos, slen;

uint8_t plat_open(const char *name)
{
    unsigned char e;

    plat_close();
    if ((e = open_ro(name)) != 0) return map_err(e);
    fh = rB;
    spos = slen = 0;
    return STOR_OK;
}

uint16_t plat_read(void *buf, uint16_t len)
{
    unsigned char *d = (unsigned char *)buf;
    uint16_t n = 0;

    if (fh == 0xFF) return 0;
    while (n < len) {
        if (spos == slen) {
            slen = (unsigned char)read_h(fh, sbuf, sizeof sbuf);
            spos = 0;
            if (!slen) break;
        }
        d[n++] = sbuf[spos++];
    }
    return n;
}

void plat_close(void)
{
    if (fh != 0xFF) close_h(fh);
    fh = 0xFF;
}

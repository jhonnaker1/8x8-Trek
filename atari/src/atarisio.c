/* Files for the Atari with NO DOS: SIO, and this port's own directory.
 *
 * The same five plat_* of core/storage.h that ataristorage.c implements over
 * CIO's `D:` handler -- but `D:` IS Atari DOS, and DOS costs this port two
 * things at once: DOS.SYS on the disk is Atari's code, so a bootable image is
 * not ours to redistribute, and DOS occupies $0700..$1FFF, which is where the
 * writable data wants to live. See NOTES.md, "The two Atari DOS questions are
 * one lever".
 *
 * SIO is the layer underneath: fill the Device Control Block at $0300 and JSR
 * $E459. No handler, no filesystem, no DOS. `src/siotest.c` proved reads and
 * writes both work with none of it resident, against a known answer, before
 * any of this was written.
 *
 * THE FORMAT IS tools/nodos.py's, and it is deliberately the smallest thing
 * that honours a NAME-KEYED contract -- the save filename is typed by the
 * player, so names cannot be dropped:
 *
 *     sector 4        directory, 8 entries of 16 bytes
 *     sector 5..      file data, each file CONTIGUOUS
 *
 * Contiguous, so there is no link byte in every sector and no free map to
 * allocate from. The only file that changes size is the save, and it gets a
 * fixed reservation big enough for the largest it can be.
 */
#include <stdint.h>
#include <string.h>

#include "../../core/storage.h"

#define DDEVIC (*(volatile unsigned char *)0x0300)
#define DUNIT  (*(volatile unsigned char *)0x0301)
#define DCOMND (*(volatile unsigned char *)0x0302)
#define DSTATS (*(volatile unsigned char *)0x0303)
#define DBUFLO (*(volatile unsigned char *)0x0304)
#define DBUFHI (*(volatile unsigned char *)0x0305)
#define DTIMLO (*(volatile unsigned char *)0x0306)
#define DBYTLO (*(volatile unsigned char *)0x0308)
#define DBYTHI (*(volatile unsigned char *)0x0309)
#define DAUX1  (*(volatile unsigned char *)0x030A)
#define DAUX2  (*(volatile unsigned char *)0x030B)

#define SECTOR      128
#define DIR_SECTOR  4
#define ENTRIES     8
#define SIO_OK      1

/* Visible to the probe, the way ataristorage.c exposes its CIO status: a
   failure has to be readable from outside without a screen. */
unsigned char sio_dbg_status;

static unsigned char secbuf[SECTOR];
static unsigned char dirbuf[SECTOR];
static unsigned int  open_sec, open_left;
static unsigned char open_live;

static void siov(void) {
    /* "p" because the compiler otherwise emits a compare before the jsr and
       branches on it after -- latent in two released ports until 2026-09-08.
       See the llvm-mos note in NOTES.md. */
    __asm__ volatile("jsr $E459" : : : "a", "x", "y", "memory", "p");
}

static unsigned char sector_io(unsigned char cmd, unsigned int n,
                               unsigned char stat, unsigned char *buf) {
    DDEVIC = 0x31;
    DUNIT  = 1;
    DCOMND = cmd;
    DSTATS = stat;
    DBUFLO = (unsigned char)((unsigned int)buf & 0xFF);
    DBUFHI = (unsigned char)((unsigned int)buf >> 8);
    DTIMLO = 15;
    DBYTLO = SECTOR;
    DBYTHI = 0;
    DAUX1  = (unsigned char)(n & 0xFF);
    DAUX2  = (unsigned char)(n >> 8);
    siov();
    sio_dbg_status = DSTATS;
    return DSTATS;
}

/* "EGATREK.SAV" -> "EGATREK SAV". The dot carries nothing once the field is
   fixed width, so the compare below is one memcmp with no parsing. */
static void to_entry(const char *name, unsigned char *out) {
    unsigned char i = 0, j = 0;
    memset(out, ' ', 11);
    while (name[i] && name[i] != '.' && j < 8) out[j++] = (unsigned char)name[i++];
    while (name[i] && name[i] != '.') i++;
    if (name[i] == '.') {
        i++;
        j = 8;
        while (name[i] && j < 11) out[j++] = (unsigned char)name[i++];
    }
}

/* The entry index for `name`, or -1. Leaves the directory in dirbuf so a
   write can update it without reading the sector twice. */
static int dir_find(const char *name) {
    unsigned char want[11];
    unsigned char i;

    if (sector_io('R', DIR_SECTOR, 0x40, dirbuf) != SIO_OK) return -1;
    to_entry(name, want);
    for (i = 0; i < ENTRIES; i++) {
        unsigned char *e = dirbuf + i * 16;
        if (e[15] && memcmp(e, want, 11) == 0) return (int)i;
    }
    return -1;
}

static unsigned int ent_start(unsigned char i) {
    return (unsigned int)(dirbuf[i * 16 + 11] | (dirbuf[i * 16 + 12] << 8));
}

static unsigned int ent_len(unsigned char i) {
    return (unsigned int)(dirbuf[i * 16 + 13] | (dirbuf[i * 16 + 14] << 8));
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got) {
    int i = dir_find(name);
    unsigned int sec, left;
    unsigned char *p = (unsigned char *)buf;

    if (i < 0) return STOR_NOTFOUND;
    left = ent_len((unsigned char)i);
    if (left > max) return STOR_ERROR;        /* the caller offers more room
                                                 than the file needs -- see
                                                 core/storage.h on why */
    sec = ent_start((unsigned char)i);
    if (got) *got = (uint16_t)left;
    while (left) {
        unsigned int n = left < SECTOR ? left : SECTOR;
        if (sector_io('R', sec, 0x40, secbuf) != SIO_OK) return STOR_ERROR;
        memcpy(p, secbuf, n);
        p += n;
        left -= n;
        sec++;
    }
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len) {
    int i = dir_find(name);
    unsigned int sec, left;
    const unsigned char *p = (const unsigned char *)buf;

    if (i < 0) return STOR_NOTFOUND;          /* the slot is reserved by
                                                 nodos.py; there is no
                                                 allocator here on purpose */
    sec = ent_start((unsigned char)i);
    left = len;
    while (left) {
        unsigned int n = left < SECTOR ? left : SECTOR;
        memset(secbuf, 0, SECTOR);
        memcpy(secbuf, p, n);
        if (sector_io('W', sec, 0x80, secbuf) != SIO_OK) return STOR_ERROR;
        p += n;
        left -= n;
        sec++;
    }
    /* The length is what makes a short save readable afterwards, so the
       directory is rewritten even though the extent did not move. */
    dirbuf[i * 16 + 13] = (unsigned char)(len & 0xFF);
    dirbuf[i * 16 + 14] = (unsigned char)(len >> 8);
    if (sector_io('W', DIR_SECTOR, 0x80, dirbuf) != SIO_OK) return STOR_ERROR;
    return STOR_OK;
}

uint8_t plat_open(const char *name) {
    int i = dir_find(name);
    if (i < 0) return STOR_NOTFOUND;
    open_sec  = ent_start((unsigned char)i);
    open_left = ent_len((unsigned char)i);
    open_live = 1;
    return STOR_OK;
}

uint16_t plat_read(void *buf, uint16_t len) {
    unsigned char *p = (unsigned char *)buf;
    uint16_t done = 0;

    if (!open_live) return 0;
    while (len && open_left) {
        unsigned int n = open_left < SECTOR ? open_left : SECTOR;
        if (n > len) n = len;
        if (sector_io('R', open_sec, 0x40, secbuf) != SIO_OK) return done;
        memcpy(p, secbuf, n);
        p += n;
        done = (uint16_t)(done + n);
        len = (uint16_t)(len - n);
        open_left -= n;
        open_sec++;
    }
    return done;
}

void plat_close(void) {
    open_live = 0;
}

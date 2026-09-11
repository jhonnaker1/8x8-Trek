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
 * any of this was written; `src/boot.s` starts the machine the same way.
 *
 * THE FORMAT IS tools/nodos.py's, and it is deliberately the smallest thing
 * that honours a NAME-KEYED contract -- the save filename is typed by the
 * player, so names cannot be dropped:
 *
 *     sector 4..5     directory, 16 entries of 16 bytes
 *     sector 6..      file data, each file CONTIGUOUS
 *
 * Contiguous, so there is no link byte in every sector and no free map to
 * allocate from.
 *
 * WHICH LEAVES THE SAVE. Its name is typed by the player, so it cannot be
 * known when the disk is built; tools/nodos.py instead lays down SLOTS --
 * entries with an extent already assigned, no name, and the writable bit set
 * -- and dir_claim() below takes one on the first write to a name that is not
 * there. That is the entire allocator. It cannot fragment, because nothing is
 * ever freed and every slot is the same size.
 *
 * The writable bit does a second job: a write to STRINGS.DAT is refused by the
 * FORMAT, rather than by nobody having tried it.
 *
 * TWO SECTORS OF DIRECTORY, ONE BUFFER. dirbuf holds whichever sector was read
 * last and dir_sec says which -- 128 bytes rather than 256, on the port with
 * the tightest budget in the project.
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
#define DIR_SECTORS 2
#define PER_SECTOR  (SECTOR / 16)
#define ENTRIES     (DIR_SECTORS * PER_SECTOR)
#define SIO_OK      1

/* Flags, byte 15 of an entry. */
#define ENT_USED    1
#define ENT_SLOT    2

/* HOW MUCH A CLAIMED SLOT HOLDS, and tools/nodos.py carries the same number
   because both halves have to agree. 625 bytes of save and 384 of hall of
   fame fit with room for a save record that grows again -- it has. */
#define SLOT_SECTORS 8
#define SLOT_BYTES   ((unsigned int)SLOT_SECTORS * SECTOR)

/* Visible to the probe: a failure has to be readable from outside without a
   screen. `used` AND `volatile` because neither alone is enough -- LTO drops
   a global the program never reads, and the MEGA65's sound probe lost three
   of six that way and had two more folded into zero page. The symbol not
   being in the map is how this was noticed here too. */
__attribute__((used)) volatile unsigned char sio_dbg_status;

static unsigned char secbuf[SECTOR];
static unsigned char dirbuf[SECTOR];
static unsigned char dir_sec;          /* which directory sector dirbuf holds */
static unsigned int  open_sec, open_left;
static unsigned char open_off;         /* how far into open_sec the stream is */
static unsigned char open_have;        /* secbuf still holds open_sec */
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

/* Bring the directory sector holding entry `i` into dirbuf, and answer with a
   pointer to the entry itself. Every accessor below goes through here, so no
   caller can read one sector's buffer with another sector's index. */
static unsigned char *dir_at(unsigned char i) {
    unsigned char want = (unsigned char)(DIR_SECTOR + i / PER_SECTOR);

    if (want != dir_sec) {
        if (sector_io('R', want, 0x40, dirbuf) != SIO_OK) return 0;
        dir_sec = want;
    }
    return dirbuf + (i % PER_SECTOR) * 16;
}

/* Write back whichever directory sector dirbuf is holding. */
static unsigned char dir_flush(void) {
    return sector_io('W', dir_sec, 0x80, dirbuf);
}

/* The entry index for `name`, or -1. */
static int dir_find(const char *name) {
    unsigned char want[11];
    unsigned char i;

    to_entry(name, want);
    for (i = 0; i < ENTRIES; i++) {
        unsigned char *e = dir_at(i);
        if (!e) return -1;
        if ((e[15] & ENT_USED) && memcmp(e, want, 11) == 0) return (int)i;
    }
    return -1;
}

/* Take an unclaimed slot for `name`: write the name in, mark it used, and
   leave the length at zero for the caller to set. -1 if the disk has no slot
   left, which is the only "disk full" this format can produce. */
static int dir_claim(const char *name) {
    unsigned char i;

    for (i = 0; i < ENTRIES; i++) {
        unsigned char *e = dir_at(i);
        if (!e) return -1;
        if (e[15] == ENT_SLOT) {
            to_entry(name, e);
            e[15] = ENT_SLOT | ENT_USED;
            return (int)i;
        }
    }
    return -1;
}

static unsigned int ent_start(unsigned char i) {
    unsigned char *e = dir_at(i);
    return (unsigned int)(e[11] | (e[12] << 8));
}

static unsigned int ent_len(unsigned char i) {
    unsigned char *e = dir_at(i);
    return (unsigned int)(e[13] | (e[14] << 8));
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
    open_have = 0;                            /* secbuf is about to be reused */
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

/* A write that gives up after dir_claim() has already written a name into
   dirbuf leaves the buffer holding an entry the DISK does not have -- and
   dir_at() would hand that phantom out on the next call, because it only
   re-reads when the sector it wants is not the one it is holding. Dropping
   the cache costs one sector read and cannot be got wrong later. */
static uint8_t write_failed(void) {
    dir_sec = 0;
    return STOR_ERROR;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len) {
    int i = dir_find(name);
    unsigned int sec, left;
    unsigned char *e;
    const unsigned char *p = (const unsigned char *)buf;

    if (i < 0) i = dir_claim(name);           /* first save under this name */
    if (i < 0) return write_failed();         /* every slot is taken */
    e = dir_at((unsigned char)i);
    if (!e) return write_failed();
    /* A DATA FILE IS NOT WRITABLE AND A SLOT IS NOT UNBOUNDED. Both are
       refused here rather than discovered by the sector after the file. */
    if (!(e[15] & ENT_SLOT) || len > SLOT_BYTES) return write_failed();
    sec = ent_start((unsigned char)i);
    left = len;
    open_have = 0;                            /* secbuf is about to be reused */
    while (left) {
        unsigned int n = left < SECTOR ? left : SECTOR;
        memset(secbuf, 0, SECTOR);
        memcpy(secbuf, p, n);
        if (sector_io('W', sec, 0x80, secbuf) != SIO_OK) return write_failed();
        p += n;
        left -= n;
        sec++;
    }
    /* The length is what makes a short save readable afterwards, and on a
       freshly claimed slot the name went in with it -- so the directory is
       written back even though the extent never moved. */
    e = dir_at((unsigned char)i);
    if (!e) return write_failed();
    e[13] = (unsigned char)(len & 0xFF);
    e[14] = (unsigned char)(len >> 8);
    if (dir_flush() != SIO_OK) return write_failed();
    return STOR_OK;
}

uint8_t plat_open(const char *name) {
    int i = dir_find(name);
    if (i < 0) return STOR_NOTFOUND;
    open_sec  = ent_start((unsigned char)i);
    open_left = ent_len((unsigned char)i);
    open_off  = 0;
    open_have = 0;
    open_live = 1;
    return STOR_OK;
}

/* THE CALLER'S CHUNK IS NOT THE SECTOR, and an earlier version here assumed
   it was: it read a whole sector, handed back the `len` bytes asked for, and
   stepped to the next sector -- so a reader using anything but 128 silently
   lost the tail of every sector. far_load() streams in SIXTY-FOUR, which made
   OVERLAYS.BIN exactly half the file, alternating chunks. It got as far as
   the build-stamp guard, which reported a mismatch: a true statement about a
   file that was never really read.

   So the stream keeps its place INSIDE the sector. open_off is how far in it
   has got and open_have says secbuf still holds open_sec -- which a whole-file
   read or a write in between would have taken for itself. */
uint16_t plat_read(void *buf, uint16_t len) {
    unsigned char *p = (unsigned char *)buf;
    uint16_t done = 0;

    if (!open_live) return 0;
    while (len && open_left) {
        unsigned int n;

        if (!open_have || open_off == 0) {
            if (sector_io('R', open_sec, 0x40, secbuf) != SIO_OK) return done;
            open_have = 1;
        }
        n = SECTOR - open_off;
        if (n > open_left) n = open_left;
        if (n > len) n = len;
        memcpy(p, secbuf + open_off, n);
        p += n;
        done = (uint16_t)(done + n);
        len = (uint16_t)(len - n);
        open_left -= n;
        open_off = (unsigned char)(open_off + n);
        if (open_off >= SECTOR) {
            open_off = 0;
            open_sec++;
        }
    }
    return done;
}

void plat_close(void) {
    open_live = 0;
    open_have = 0;
}

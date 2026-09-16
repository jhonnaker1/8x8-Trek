/* Files for an Apple IIgs with NO PRODOS: the slot's block driver, and this
 * port's own directory.
 *
 * The five plat_* of core/storage.h, over the interface every ProDOS block
 * device exposes in its slot firmware -- the byte at $CnFF is the driver's
 * offset within $Cn00, and the call takes command in $42, unit in $43, buffer
 * in $44/$45 and block in $46/$47, returning carry clear on success. That is
 * in ROM, it is what ProDOS itself calls, and src/boot.S already loads the
 * game with it.
 *
 * WHY NOT PRODOS. It costs this port two things at once, and they are the
 * Atari's two exactly (see atari/src/atarisio.c and NOTES.md, "The two Atari
 * DOS questions are one lever"): PRODOS on the disk is Apple's code, so a
 * bootable image would not be ours to give away; and the MLI uses zero page
 * during a call, which is a collision this port has no way to measure its way
 * out of -- llvm-mos puts its imaginary registers there.
 *
 * THE FORMAT IS tools/mkdisk.py's, and it is the Atari's format at 512 bytes:
 *
 *     block 0        the loader
 *     block 1        directory, 32 entries of 16 bytes
 *     block 2..      the game image, loaded by block 0
 *     then           file data, each file CONTIGUOUS
 *
 * Contiguous, so there is no link field in every block and no free map to
 * allocate from. The save's name is typed by the player and cannot be known
 * when the disk is built, so mkdisk.py lays down SLOTS -- entries with an
 * extent already assigned, no name, and the slot bit set -- and dir_claim()
 * takes one on the first write to a name that is not there. That is the whole
 * allocator: it cannot fragment, because nothing is ever freed and every slot
 * is the same size. The slot bit does a second job -- a write to STRINGS.DAT
 * is refused by the FORMAT rather than by nobody having tried it.
 */
#include <stdint.h>
#include <string.h>

#include "../../core/storage.h"

#define ASMVAR __attribute__((used, retain))

#define BLOCK       512
#define DIR_BLOCK   1
#define PER_BLOCK   (BLOCK / 16)
#define ENTRIES     PER_BLOCK

#define ENT_USED    1
#define ENT_SLOT    2

/* Where src/boot.S left the drive and its driver. Four bytes at $0280, in the
   monitor's input buffer, which nothing this port runs touches. */
#define HANDOFF     ((volatile unsigned char *)0x0280)
#define HANDOFF_SIG 0x5A

/* ProDOS block-driver call block, at the addresses the interface defines. */
#define BD_CMD   (*(volatile unsigned char *)0x42)
#define BD_UNIT  (*(volatile unsigned char *)0x43)
#define BD_BUFLO (*(volatile unsigned char *)0x44)
#define BD_BUFHI (*(volatile unsigned char *)0x45)
#define BD_BLKLO (*(volatile unsigned char *)0x46)
#define BD_BLKHI (*(volatile unsigned char *)0x47)

ASMVAR unsigned char gs_drv[2];
ASMVAR unsigned char gs_status;

/* Visible from outside, because a storage failure has to be readable without
   a screen. `used, retain` AND volatile: LTO drops a global the program never
   reads, and the MEGA65's sound probe lost three of six that way. */
ASMVAR volatile unsigned char blk_dbg_status;
ASMVAR volatile unsigned char blk_dbg_booted;

static unsigned char blkbuf[BLOCK];
static unsigned char dirbuf[BLOCK];
static unsigned char dir_have;          /* dirbuf holds the directory block */
static unsigned int  open_blk, open_left;
static unsigned int  open_off;          /* how far into open_blk the stream is */
static unsigned char open_have;         /* blkbuf still holds open_blk */
static unsigned char open_live;

/* `jsr` to an address held in memory. `jmp (abs)` is the only indirect jump a
   6502 has, so the call is a jsr to a stub that performs it and the driver's
   own rts comes back here.

   "p" IS NOT OPTIONAL. Without it the compiler may emit a compare before the
   jsr and branch on its flags after -- the driver has long since overwritten
   them. That was latent in two released ports until 2026-09-08. */
static void blk_call(void)
{
    __asm__ volatile(
        "jsr 1f\n\t"
        "bcs 2f\n\t"
        "stz gs_status\n\t"
        "bra 3f\n\t"
        "1:\n\t"
        "jmp (gs_drv)\n\t"
        "2:\n\t"
        "sta gs_status\n\t"
        "3:\n\t"
        : : : "a", "x", "y", "memory", "p");
}

static unsigned char blk_io(unsigned char cmd, unsigned int blk,
                            unsigned char *buf)
{
    /* THE SIGNATURE, CHECKED ON EVERY CALL RATHER THAN ONCE. tools/run.lua
       pokes an image in and sets the PC, and in that path boot.S never ran:
       gs_drv would be whatever the ROM left and the jmp would go anywhere.
       Refusing is a diagnosis; jumping is a hang. */
    if (HANDOFF[3] != HANDOFF_SIG) {
        blk_dbg_booted = 0;
        gs_status = 0xFF;
        return 0xFF;
    }
    blk_dbg_booted = 1;
    gs_drv[0] = HANDOFF[1];
    gs_drv[1] = HANDOFF[2];

    BD_CMD   = cmd;
    BD_UNIT  = HANDOFF[0];
    BD_BUFLO = (unsigned char)((unsigned int)buf & 0xFF);
    BD_BUFHI = (unsigned char)((unsigned int)buf >> 8);
    BD_BLKLO = (unsigned char)(blk & 0xFF);
    BD_BLKHI = (unsigned char)(blk >> 8);
    blk_call();
    blk_dbg_status = gs_status;
    return gs_status;
}

/* Exposed, because far memory and the overlay loader both want raw blocks and
   neither wants a filename. Reads into the CALLER'S buffer, never into
   blkbuf: the CoCo 3 port had one buffer with two users and a raw read
   silently destroyed whatever the open stream was holding. */
uint8_t blk_read_to(unsigned int blk, void *dst)
{
    return blk_io(1, blk, (unsigned char *)dst) ? STOR_ERROR : STOR_OK;
}

/* "EGATREK.SAV" -> "EGATREK SAV". The dot carries nothing once the field is
   fixed width, so the compare below is one memcmp with no parsing. */
static void to_entry(const char *name, unsigned char *out)
{
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

static unsigned char *dir_at(unsigned char i)
{
    if (!dir_have) {
        if (blk_io(1, DIR_BLOCK, dirbuf)) return 0;
        dir_have = 1;
    }
    return dirbuf + (unsigned int)i * 16;
}

static unsigned char dir_flush(void)
{
    return blk_io(2, DIR_BLOCK, dirbuf);
}

static int dir_find(const char *name)
{
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

static int dir_claim(const char *name)
{
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

static unsigned int ent_start(unsigned char i)
{
    unsigned char *e = dir_at(i);
    return (unsigned int)(e[11] | (e[12] << 8));
}

static unsigned int ent_len(unsigned char i)
{
    unsigned char *e = dir_at(i);
    return (unsigned int)(e[13] | (e[14] << 8));
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got)
{
    int i = dir_find(name);
    unsigned int blk, left;
    unsigned char *p = (unsigned char *)buf;

    if (i < 0) return STOR_NOTFOUND;
    left = ent_len((unsigned char)i);
    if (left > max) return STOR_ERROR;
    blk = ent_start((unsigned char)i);
    if (got) *got = (uint16_t)left;
    open_have = 0;                          /* blkbuf is about to be reused */
    while (left) {
        unsigned int n = left < BLOCK ? left : BLOCK;
        if (blk_io(1, blk, blkbuf)) return STOR_ERROR;
        memcpy(p, blkbuf, n);
        p += n;
        left -= n;
        blk++;
    }
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len)
{
    int i = dir_find(name);
    unsigned int blk, left = len;
    const unsigned char *p = (const unsigned char *)buf;
    unsigned char *e;

    if (i < 0) i = dir_claim(name);
    if (i < 0) return STOR_ERROR;

    e = dir_at((unsigned char)i);
    if (!e) return STOR_ERROR;
    /* A NAME THAT IS NOT A SLOT IS READ-ONLY BY THE FORMAT. STRINGS.DAT and
       the overlays are laid down by the build and have no slot bit, so a
       write to one is refused here rather than corrupting the disk. */
    if (!(e[15] & ENT_SLOT)) return STOR_ERROR;

    e[13] = (unsigned char)(len & 0xFF);
    e[14] = (unsigned char)(len >> 8);
    blk = (unsigned int)(e[11] | (e[12] << 8));

    if (dir_flush()) {
        /* A WRITE THAT GIVES UP AFTER dir_claim() HAS WRITTEN A NAME INTO
           dirbuf leaves the buffer holding an entry the DISK does not have,
           and dir_at() would hand that phantom out on the next call because it
           only re-reads when it is holding nothing. Dropping the cache costs
           one block read and cannot be got wrong later. */
        dir_have = 0;
        return STOR_ERROR;
    }

    open_have = 0;
    while (left) {
        unsigned int n = left < BLOCK ? left : BLOCK;
        memset(blkbuf, 0, BLOCK);           /* never leak the last file's tail */
        memcpy(blkbuf, p, n);
        if (blk_io(2, blk, blkbuf)) { dir_have = 0; return STOR_ERROR; }
        p += n;
        left -= n;
        blk++;
    }
    return STOR_OK;
}

uint8_t plat_open(const char *name)
{
    int i = dir_find(name);
    if (i < 0) { open_live = 0; return STOR_NOTFOUND; }
    open_blk  = ent_start((unsigned char)i);
    open_left = ent_len((unsigned char)i);
    open_off  = 0;
    open_have = 0;
    open_live = 1;
    return STOR_OK;
}

uint16_t plat_read(void *buf, uint16_t len)
{
    unsigned char *p = (unsigned char *)buf;
    uint16_t done = 0;

    if (!open_live) return 0;
    while (len && open_left) {
        unsigned int avail, n;
        if (!open_have) {
            if (blk_io(1, open_blk, blkbuf)) return done;
            open_have = 1;
        }
        avail = BLOCK - open_off;
        n = len < avail ? len : avail;
        if (n > open_left) n = open_left;
        memcpy(p, blkbuf + open_off, n);
        p += n; done += n; len -= n;
        open_off += n; open_left -= n;
        if (open_off == BLOCK) { open_off = 0; open_blk++; open_have = 0; }
    }
    return done;
}

void plat_close(void)
{
    open_live = 0;
    open_have = 0;
}

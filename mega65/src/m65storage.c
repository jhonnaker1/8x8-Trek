/* Files, through the MEGA65 Hypervisor.
 *
 * READS WORK AND WRITES DO NOT, and that is a limit of the platform library
 * rather than a shortcut: mega65-libc's fileio is open/read512/close, with no
 * write of any kind. The only writing path the SD card offers is
 * mega65_sdcard_writesector(), which would mean implementing a FAT32 writer --
 * and getting it wrong corrupts the player's card. Not worth it for a save
 * game, so plat_write_all() reports STOR_ERROR and the UI says so.
 *
 * What that costs: SAVE cannot store a game and the hall of fame cannot record
 * one. Everything that READS -- STRINGS.DAT, MUSIC.DAT, the briefing, an
 * existing hall of fame, an existing save -- works.
 */
#include <stdint.h>
#include <string.h>
#include <mega65/fileio.h>
#include <mega65/memory.h>
#include "../../core/storage.h"

/* read512() delivers a whole sector, so this buffer cannot be smaller -- the
   Hypervisor decides the size, not us. It is the single biggest thing this
   port keeps in bank 0 and it earns its place. */
#define SECTOR 512

static uint8_t  open_fd = 0xFF;
static uint8_t  buf[SECTOR];
static uint16_t buf_len, buf_pos;

/* mega65-libc wants a mutable char*, and the Hypervisor wants the name in
   upper case with no path. */
static char namebuf[20];

/* THE HYPERVISOR LEAVES THE MACHINE DIFFERENT FROM HOW IT FOUND IT.
 *
 * hyppo's file calls trap into the Hypervisor, which has its own memory and
 * I/O banking, and control comes back without the extended I/O context this
 * port needs -- the VIC-IV registers and the palette stop being reachable, so
 * everything drawn afterwards goes nowhere and the screen stays black. The
 * smoke build never noticed because it opens no files.
 *
 * Re-enabling I/O after every hyppo call is cheap and makes the seam
 * self-contained: nothing above this file has to know. */
static void after_hyppo(void) { mega65_io_enable(); }

/* AND THE CALLS THEMSELVES GO THROUGH A SHIM -- see m65hyppo.s. The library's
   file routines are assembly that clobbers llvm-mos's pseudo-registers without
   saying so, which miscompiled the loop below into one that ignored `len`
   entirely. Nothing in this file calls open/read512/close directly any more. */
uint16_t trek_read512(uint8_t *buf);
uint8_t  trek_open(const char *name);
void     trek_closeall(void);

/* CLOSING IS closeall(), NOT close(fd), AND THAT IS NOT A SHORTCUT.
 *
 * hyppo's openfile does not hand back a usable descriptor here: every open
 * returns $18 -- the trap number itself -- and passing that to closefile is
 * refused with error $89, so the file stays open. Hyppo has four descriptors,
 * so the FOURTH open in a session failed. In the game that was BRIEF.TXT: the
 * player asked for the briefing and it silently did nothing, because
 * STRINGS.DAT, MUSIC.DAT and OVERLAYS.BIN had used up the table at startup and
 * never given it back.
 *
 * MEASURED, not assumed: a probe opening five files in a row gets three and
 * then fails, and with closeall() gets all five and reads the fifth.
 *
 * THE PRECONDITION IS THAT ONLY ONE FILE IS EVER OPEN, which is true of this
 * port -- plat_open/plat_read/plat_close never nest, and plat_read_all is
 * self-contained. If a caller ever needs two files at once, this has to be
 * solved properly and not by widening closeall. */

static char *fixname(const char *name) {
    uint8_t i = 0;
    while (name[i] && i < sizeof namebuf - 1) {
        char c = name[i];
        namebuf[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
        i++;
    }
    namebuf[i] = 0;
    return namebuf;
}

uint8_t plat_read_all(const char *name, void *dst, uint16_t max, uint16_t *got) {
    uint8_t *out = (uint8_t *)dst;
    uint16_t total = 0;
    uint8_t fd = trek_open(fixname(name));
    after_hyppo();

    if (got) *got = 0;
    if (fd == 0xFF) return STOR_NOTFOUND;

    for (;;) {
        uint16_t n = trek_read512(buf);
        after_hyppo();
        if (n == 0) break;
        if (total + n > max) n = max - total;
        memcpy(out + total, buf, n);
        total = (uint16_t)(total + n);
        if (total >= max) break;
    }
    trek_closeall(); after_hyppo();
    if (got) *got = total;
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *src, uint16_t len) {
    (void)name; (void)src; (void)len;
    return STOR_ERROR;          /* see the note at the top */
}

uint8_t plat_open(const char *name) {
    open_fd = trek_open(fixname(name));
    after_hyppo();
    if (open_fd == 0xFF) return STOR_NOTFOUND;
    buf_len = buf_pos = 0;
    return STOR_OK;
}

uint16_t plat_read(void *dst, uint16_t len) {
    uint8_t *out = (uint8_t *)dst;
    uint16_t done = 0;
    if (open_fd == 0xFF) return 0;
    while (done < len) {
        if (buf_pos >= buf_len) {
            uint16_t n = trek_read512(buf);
            after_hyppo();
            if (n == 0) break;
            buf_len = (uint16_t)n; buf_pos = 0;
        }
        out[done++] = buf[buf_pos++];
    }
    return done;
}

void plat_close(void) {
    if (open_fd != 0xFF) { trek_closeall(); after_hyppo(); open_fd = 0xFF; }
}

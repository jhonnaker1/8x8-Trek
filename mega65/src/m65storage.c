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
#include "../../core/storage.h"

#define SECTOR 512

static uint8_t  open_fd = 0xFF;
static uint8_t  buf[SECTOR];
static uint16_t buf_len, buf_pos;

/* mega65-libc wants a mutable char*, and the Hypervisor wants the name in
   upper case with no path. */
static char namebuf[20];

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
    uint8_t fd = open(fixname(name));

    if (got) *got = 0;
    if (fd == 0xFF) return STOR_NOTFOUND;

    for (;;) {
        size_t n = read512(buf);
        if (n == 0) break;
        if (total + n > max) n = max - total;
        memcpy(out + total, buf, n);
        total = (uint16_t)(total + n);
        if (total >= max) break;
    }
    close(fd);
    if (got) *got = total;
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *src, uint16_t len) {
    (void)name; (void)src; (void)len;
    return STOR_ERROR;          /* see the note at the top */
}

uint8_t plat_open(const char *name) {
    open_fd = open(fixname(name));
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
            size_t n = read512(buf);
            if (n == 0) break;
            buf_len = (uint16_t)n; buf_pos = 0;
        }
        out[done++] = buf[buf_pos++];
    }
    return done;
}

void plat_close(void) {
    if (open_fd != 0xFF) { close(open_fd); open_fd = 0xFF; }
}

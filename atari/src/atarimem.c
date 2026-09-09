/* Far memory for the Atari 800XL + VBXE: the store is VBXE's VRAM.
 *
 * `core/farmem.h` lists the banking model of every target it was designed
 * around and this one is not in the list, because when it was written this
 * machine was going to be a 130XE with 16K banks at $4000. It is not: VBXE
 * brings 512K of its own VRAM, reached through the MEMAC window the video
 * driver already opens at $2000, so far memory and the screen are the same
 * mechanism seen through the same 4K hole.
 *
 * THE STORE STARTS AT VRAM $04000, above the screen, the font and the message
 * log. The seam's offsets are sixteen bits, so it can address 64K of the 512K
 * -- banks 4..19 -- which is four times what any port has ever put in it.
 *
 * READ IN CHUNKS, NOT BYTES, says the header, and here the reason is sharper
 * than usual: a byte at a time costs a bank compare, and a chunk that stays
 * inside one 4K bank costs one for the whole run.
 */
#include <stdint.h>

#include "../../core/farmem.h"
#include "../../core/storage.h"
#include "vbxevid.h"
#include "atarimem.h"

/* Banks 0..3 belong to the video driver: screen and XDL, font, and the
   message log's two. See src/vbxevid.c for that map. */
#define FAR_BANK0 4

static uint16_t far_used;

/* THE ONE PLACE THAT TURNS AN OFFSET INTO A BANK AND A POINTER. Everything
   else here is a loop around it, and a run is cut at the 4K boundary so the
   bank is selected once per bank rather than once per byte. */
static void far_copy(uint16_t off, unsigned char *dst, uint16_t len,
                     unsigned char writing) {
    while (len) {
        uint16_t win = (uint16_t)(off & 0x0FFF);
        uint16_t room = (uint16_t)(0x1000 - win);
        uint16_t n = (len < room) ? len : room;
        unsigned char *p = VBXE_WIN + win;
        uint16_t i;

        vbxe_bank((unsigned char)(FAR_BANK0 + (off >> 12)));
        if (writing) { for (i = 0; i < n; i++) *p++ = *dst++; }
        else         { for (i = 0; i < n; i++) *dst++ = *p++; }
        off = (uint16_t)(off + n);
        len = (uint16_t)(len - n);
    }
}

void far_read(uint16_t off, void *dst, uint8_t len) {
    far_copy(off, (unsigned char *)dst, len, 0);
}

void far_bulk(uint16_t off, void *dst, uint16_t len) {
    far_copy(off, (unsigned char *)dst, len, 0);
}

void far_write(uint16_t off, const void *src, uint16_t len) {
    far_copy(off, (unsigned char *)(void *)src, len, 1);
}

uint16_t far_size(void) {
    return far_used;
}

/* APPENDS, and returns where it put the file -- the store has more than one
   tenant (the string pool, the music and the overlay images) and an earlier
   version of this seam elsewhere loaded at offset 0 every time and let the
   music silently overwrite the prose.
 *
 * STREAMED THROUGH A SMALL RAM BUFFER, because there is no way to hand the
 * Atari's file system a VRAM address the way the C128 hands the KERNAL bank 1
 * -- VRAM is not in the address space except through the window, and the
 * window is 4K wide and moves. So: read a chunk into RAM, push it through the
 * window, repeat. The chunk is deliberately small; making it large would buy
 * nothing, since the cost here is the file read and not the copy. */
uint16_t far_load(const char *name) {
    unsigned char buf[64];
    uint16_t base = far_used;
    uint16_t got;

    if (plat_open(name) != STOR_OK) return FAR_NONE;
    for (;;) {
        got = plat_read(buf, (uint16_t)sizeof buf);
        if (!got) break;
        far_write(far_used, buf, got);
        far_used = (uint16_t)(far_used + got);
        if (got < sizeof buf) break;
    }
    plat_close();
    if (far_used == base) return FAR_NONE;      /* opened, but empty */
    return base;
}

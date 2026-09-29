#include <stdint.h>
#include <string.h>

#include "vdc.h"
#include "egavdc.h"
#include "../../core/ega.h"
#include "../../core/farmem.h"
#include "../../core/storage.h"
#include "input.h"
#include "c64os.h"

/* WHERE THE GAME'S FILES ARE, AND WHERE ITS REU IS -- the two things C64 OS
 * decides and the C64 port could take for granted.
 *
 * THE FILES are wherever the player put the app's folder. C64 OS's record of
 * the running app -- the page `appfileref` ($0338) points at -- holds its
 * device at +0, its partition at +1 and its path at +$16, "//os/..." with a
 * trailing slash (read off a running system, 2026-09-27: File Manager's said
 * device 10, partition 1, "//os/services/File Manager/"). Every file the game
 * opens is named "<partition><path>:<name>", CMD's syntax for a file in a
 * directory, so nothing depends on the drive's current directory -- which
 * C64 OS moves about for its own purposes.
 *
 * THE REU is C64 OS's too: bank 0 is its work bank and one bank per fast-app-
 * switching slot follows. An app OWNS banks only through bkalloc_ in
 * memory.lib, which marks them against the app and frees them when it quits
 * (Programmer's Guide, and //os/h/memory.t in 1.09); reuconf_ would hand out
 * the same banks to every app that asked. The game needs two:
 *
 *     bank A        overlay images 0-15, 4K each (src/osovl.c)
 *     bank A+1      images 16-22 to $6FFF, the message log at $7000,
 *                   the far store at $8000
 *
 * "The user will need to leave some slots open" -- Jamie. When bkalloc_
 * cannot find two free banks together, the game says so and stops. */

#define APPFILEREF (*(unsigned char *volatile *)0x0338)
#define FREF_DEV   0x00
#define FREF_PART  0x01
#define FREF_PATH  0x16

#define REU_CMD   (*(volatile unsigned char *)0xDF01)
#define REU_C64L  (*(volatile unsigned char *)0xDF02)
#define REU_C64H  (*(volatile unsigned char *)0xDF03)
#define REU_REUL  (*(volatile unsigned char *)0xDF04)
#define REU_REUH  (*(volatile unsigned char *)0xDF05)
#define REU_BANK  (*(volatile unsigned char *)0xDF06)
#define REU_LENL  (*(volatile unsigned char *)0xDF07)
#define REU_LENH  (*(volatile unsigned char *)0xDF08)
#define REU_IMR   (*(volatile unsigned char *)0xDF09)
#define REU_ACR   (*(volatile unsigned char *)0xDF0A)
#define REU_STASH 0x90
#define REU_FETCH 0x91

/* C64 OS's first app REU bank; bkalloc_ counts from it (io_rec.t). */
#define APPREUBK  (*(volatile unsigned char *)0x0282)
#define FAR_BASE  0x8000u         /* in bank A+1 */
#define FAR_ROOM  0x8000u
#define LOG_BASE  0x7000u         /* in bank A+1, above image 22 */


unsigned char c64os_dev;
unsigned char c64os_bank;
volatile unsigned char c64os_libpage;
/* "1//os/applications/" + a folder name of up to 16 + "/:" is 38, and 40
   leaves the terminator room. Every byte of this file counted: the first link
   with the startup trail in it was over by one. */
static char prefix[40];
static uint16_t far_len;

static void halt(const char *what, const char *detail, const char *detail2) {
    scr_clear();
    scr_puts(1, 2, what, EGA_TO_VDC(EGA_LTRED));
    scr_puts(1, 4, detail, EGA_TO_VDC(EGA_WHITE));
    scr_puts(1, 5, detail2, EGA_TO_VDC(EGA_WHITE));
    scr_puts(1, 7, "PRESS A KEY TO RETURN TO C64 OS.", EGA_TO_VDC(EGA_LTCYAN));
    kb_waitkey();
    plat_exit();
}

/* Read once, on first use: the device and "<partition>//path/:". */
const char *c64os_prefix(void) {
    if (!prefix[0]) {
        const unsigned char *ref = APPFILEREF;
        const char *path = (const char *)ref + FREF_PATH;
        unsigned char part = ref[FREF_PART];
        char *p = prefix;
        c64os_dev = ref[FREF_DEV];
        if (part >= 100) *p++ = (char)('0' + part / 100);
        if (part >= 10)  *p++ = (char)('0' + part / 10 % 10);
        *p++ = (char)('0' + part % 10);
        while (*path && p < prefix + sizeof prefix - 2) *p++ = *path++;
        *p++ = ':';
        *p = '\0';
    }
    return prefix;
}

/* A DMA between main memory and bank c64os_bank + `hi`. The CPU stops until
   it is done. The game runs with $01 = $36, so the REU's registers are
   there; interrupts are off so nothing on C64 OS's IRQ touches them midway. */
void reu_dma(unsigned char cmd, void *c64, uint16_t reu, unsigned char hi, uint16_t len) {
    uint16_t a = (uint16_t)(uintptr_t)c64;
    __asm__ volatile("sei" ::: "memory");
    REU_C64L = (unsigned char)a;
    REU_C64H = (unsigned char)(a >> 8);
    REU_REUL = (unsigned char)reu;
    REU_REUH = (unsigned char)(reu >> 8);
    REU_BANK = (unsigned char)(c64os_bank + hi);
    REU_LENL = (unsigned char)len;
    REU_LENH = (unsigned char)(len >> 8);
    REU_IMR = 0;
    REU_ACR = 0;
    REU_CMD = cmd;
    __asm__ volatile("cli" ::: "memory");
}

/* The two banks, allocated once. memory.lib is loaded by name -- its first
   two letters in C64 OS's PETSCII, 4 pages -- and bkalloc_ is at +9. */
void c64os_banks(void) {
    unsigned char page, rel;
    if (c64os_bank) return;
    page = os_loadlib(0x4D, 0x45, 4);           /* "me", memory.lib */
    c64os_libpage = page;
    c64os_stage = 3;
    if (page) {
        rel = os_bkalloc(page, 2);
        if (rel != 0xFF) c64os_bank = (unsigned char)(rel + APPREUBK);
    }
    c64os_stage = 4;
    if (!c64os_bank)
        halt("EGA TREK NEEDS TWO FREE REU BANKS",
             "C64 OS COULD NOT FIND TWO TOGETHER.",
             "CONFIGURE FEWER FAST APP SWITCH SLOTS.");
}

/* THE FAR STORE, in bank A+1 from $8000. far_load streams the file through a
   page buffer rather than loading it whole: there is nowhere in main memory
   to put 7.5K of prose, which is the reason it is in the REU at all. */
/* THE PAGE BUFFER IS app.s's `bounce`, the page os_present copies colours
   through. It was the overlay window, while the window was empty at startup
   -- until main() became an overlay and ran FROM the window: far_load then
   read STRINGS.DAT over main's own first page, main returned into prose, and
   the BRK that found cascaded through C64 OS's exception handler into a JAM
   (NOTES.md, "THE FILE WAS TOO BIG"). os_present never runs during a read. */
extern unsigned char bounce[256];

uint16_t far_load(const char *name) {
    unsigned char *page = bounce;
    uint16_t base = far_len, n;

    c64os_banks();
    /* SEQ, as storage.c opens everything -- so the installer writes
       STRINGS.DAT and MUSIC.DAT as SEQ, where the C64's disk has PRG. */
    c64os_stage = 5;
    if (plat_open(name) != STOR_OK) return FAR_NONE;
    while ((n = plat_read(page, 256)) != 0) {
        if (far_len + n > FAR_ROOM) { plat_close(); far_len = base; return FAR_NONE; }
        reu_dma(REU_STASH, page, (uint16_t)(FAR_BASE + far_len), 1, n);
        far_len = (uint16_t)(far_len + n);
    }
    plat_close();
    c64os_stage = 6;
    return far_len > base ? base : FAR_NONE;
}

uint16_t far_size(void) { return far_len; }

void far_read(uint16_t off, void *dst, uint8_t len) {
    if (len) reu_dma(REU_FETCH, dst, (uint16_t)(FAR_BASE + off), 1, len);
}

/* THE MESSAGE LOG, in the REU. ui.c keeps a 32-entry log outside the
 * program through three functions whose names are the C128's (see
 * ../c64/src/c64log.c, whose shape this is). On the C64 it is a 2K array;
 * here that 2K was exactly what the file could not hold -- the first link
 * was 2,049 bytes over $7A00 -- so each byte is a one-byte DMA to bank A+1.
 * ui.c streams a whole record through one address, so the cursor advances. */
#define LOG_ORIGIN  0x1000
#define LOG_BYTES   (32 * 64)
static unsigned int log_cursor;

void vdc_set_address(unsigned int addr) {
    log_cursor = (addr >= LOG_ORIGIN) ? (addr - LOG_ORIGIN) : 0;
}

void vdc_data_write(unsigned char value) {
    if (log_cursor < LOG_BYTES)
        reu_dma(REU_STASH, &value, (uint16_t)(LOG_BASE + log_cursor), 1, 1);
    log_cursor++;
}

unsigned char vdc_data_read(void) {
    unsigned char v = 0;
    if (log_cursor < LOG_BYTES)
        reu_dma(REU_FETCH, &v, (uint16_t)(LOG_BASE + log_cursor), 1, 1);
    log_cursor++;
    return v;
}

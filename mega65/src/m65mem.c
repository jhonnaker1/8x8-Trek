/* The three C128 subsystems this machine does not need, answered in one file.
 *
 * OVERLAYS. The C128 port has ten 4K code windows because its resident pool is
 * 37,823 bytes and the game does not fit. `ovl_load()` here is a no-op that
 * returns immediately: OVL_CODE expands to nothing off-target, so every
 * function the C128 pages in and out is simply linked resident.
 *
 * FAR MEMORY. `far_load`/`far_read` exist because the C128's string pool and
 * music live in bank 1, reachable only through the MMU a byte at a time. Here
 * they are a plain array and a memcpy.
 *
 * The point is not that these are easier. It is that they are ANSWERS TO
 * QUESTIONS THIS MACHINE DOES NOT ASK, and keeping the same interfaces means
 * `ui.c` and `main.c` never find out which machine they are on.
 */
#include <string.h>
#include <stdint.h>
#include "../../core/overlay.h"
#include "../../core/farmem.h"
#include "../../core/storage.h"

/* OVERLAYS, AND WHY THIS PORT STILL HAS THEM.
 *
 * Measured, not assumed: llvm-mos's mega65 region is $2001..$CFFF -- 45,055
 * bytes -- and the whole game is about 66,500 bytes of code. Unmapping the
 * KERNAL too would buy 8K and still leave it short. So the window stays, and
 * the first version of this file claiming "nothing needs overlaying" was
 * wrong.
 *
 * WHAT DOES CHANGE IS THE COST. The C128 reads each 4K image off a 1541 --
 * hundreds of milliseconds, which is why that port works so hard to call this
 * rarely and why `check_overlay_calls` exists at all. Here the ten images sit
 * in banked RAM at $50000 and come in by DMAgic: one lcopy, microseconds. The
 * mechanism is the same and the reason it shaped the C128's structure is
 * gone. */
#include <mega65/memory.h>

#define OVL_IMAGES   0x50000UL     /* banked RAM, loaded once at startup */
#define OVL_SIZE     0x1000UL

/* THE WINDOW'S ADDRESS COMES FROM THE LINKER, never from a number typed twice.
 * It was typed twice for about an hour: mega65.ld moved the window from $B000
 * to $C000 and this file still said $B000, so every ovl_load DMA'd four
 * kilobytes over .bss -- including mega65-libc's `dmalist` at $B357. The next
 * lcopy then read its job list from a corrupted descriptor and Xemu reported
 * an unhandled read at $166B357, whose low sixteen bits are that address.
 *
 * c128/src/overlay.c takes the same symbol for the same reason. */
extern unsigned char __ovl_start[];
#define OVL_WINDOW   ((uint32_t)(uint16_t)__ovl_start)

static uint8_t ovl_init(void);
static uint8_t ovl_live = 0xFE;   /* 0xFE: images not fetched yet */

/* ONE staging buffer for both loaders, and a small one: plat_read already
   holds a 512-byte sector internally, so this is only the hop from there into
   banked RAM, and 64 bytes costs nothing in time. Two private buffers of 256
   and 512 put .bss 1,204 bytes over a region that is 36,863 because the window
   had to move clear of the stack. */
static uint8_t stage[64];

void ovl_load(uint8_t which) {
    /* Lazily, so the SHARED main.c needs no MEGA65-specific startup call. */
    if (ovl_live == 0xFE) { if (!ovl_init()) return; }
    if (which >= OVL_COUNT || which == ovl_live) return;
    lcopy(OVL_IMAGES + (uint32_t)which * OVL_SIZE, OVL_WINDOW, OVL_SIZE);
    ovl_live = which;
}

/* Reads OVERLAYS.BIN into banked RAM. Called once, before anything calls
   ovl_load. Returns non-zero on success; a build with no overlay file cannot
   run at all, unlike the C128's optional MUSIC.DAT. */
static uint8_t ovl_init(void) {
    uint32_t dst = OVL_IMAGES;
    uint16_t n;

    if (plat_open("OVERLAYS.BIN") != STOR_OK) return 0;
    for (;;) {
        n = plat_read(stage, sizeof stage);
        if (n == 0) break;
        lcopy((uint32_t)(uint16_t)stage, dst, n);
        dst += n;
    }
    plat_close();
    ovl_live = OVL_NONE;
    return 1;
}

/* THE POOL LIVES IN BANKED RAM, not in bank 0 -- $40000, clear of the overlay
   images at $50000. Putting an 8K array in .bss would spend a fifth of the
   45,055 bytes bank 0 actually has, which is the same mistake the C128 port
   exists to avoid; the MEGA65's answer is simply a wider pointer rather than
   an MMU dance. far_read is a DMA burst. */
#define FAR_POOL     0x40000UL
#define FAR_CAPACITY 0x4000U      /* 16K: STRINGS.DAT is ~7.8K, MUSIC.DAT 412 */

static uint16_t far_len;

/* APPENDS, and returns where this file starts. Both ports load STRINGS.DAT and
   then MUSIC.DAT into the same pool and keep the two bases, so a far_load that
   overwrote would silently take the prose away the moment the music arrived. */
uint16_t far_load(const char *name) {
    uint16_t base = far_len;
    uint32_t dst = FAR_POOL + base;
    uint16_t n;

    if (base >= FAR_CAPACITY) return FAR_NONE;
    if (plat_open(name) != STOR_OK) return FAR_NONE;
    for (;;) {
        n = plat_read(stage, sizeof stage);
        if (n == 0) break;
        if ((uint16_t)(far_len + n) > FAR_CAPACITY) n = (uint16_t)(FAR_CAPACITY - far_len);
        if (n == 0) break;
        lcopy((uint32_t)(uint16_t)stage, dst, n);
        dst += n; far_len = (uint16_t)(far_len + n);
    }
    plat_close();
    return (far_len == base) ? FAR_NONE : base;
}

uint16_t far_size(void) { return far_len; }

void far_read(uint16_t off, void *dst, uint8_t len) {
    if (off >= far_len) { memset(dst, 0, len); return; }
    if ((uint16_t)(off + len) > far_len) len = (uint8_t)(far_len - off);
    lcopy(FAR_POOL + off, (uint32_t)(uint16_t)dst, len);
}

/* THE MESSAGE LOG, which on the C128 lives in spare VDC RAM.
 *
 * That port keeps 32 entries of 64 bytes at VDC address $1000 -- a region the
 * 80-column chip has and the 8502 cannot otherwise reach, so it is genuinely
 * free storage. `ui.c` reaches it through vdc_set_address / vdc_data_write /
 * vdc_data_read, which is a cursor-and-stream interface because that is what
 * the VDC's data port is.
 *
 * Here it is 2K of ordinary RAM behind the same three calls. Keeping the
 * interface rather than rewriting ui.c is the whole point: 2,408 lines of
 * console code do not need to know which machine they are on, and the one
 * place they touch VDC-shaped hardware is answered here instead of edited
 * out. */
#define LOG_BYTES 2048
#define LOG_MEM   0x44000UL        /* banked, like everything else bulky here */
static uint16_t log_cursor;

void vdc_set_address(unsigned int addr) {
    /* The C128 log is based at VDC $1000; fold that away so this array starts
       at zero and nothing above this line has to change. */
    log_cursor = (uint16_t)((addr - 0x1000u) % LOG_BYTES);
}

void vdc_data_write(unsigned char v) {
    lpoke(LOG_MEM + log_cursor, v);
    log_cursor = (uint16_t)((log_cursor + 1) % LOG_BYTES);
}

unsigned char vdc_data_read(void) {
    unsigned char v = lpeek(LOG_MEM + log_cursor);
    log_cursor = (uint16_t)((log_cursor + 1) % LOG_BYTES);
    return v;
}

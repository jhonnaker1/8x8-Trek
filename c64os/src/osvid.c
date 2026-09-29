#include <stdint.h>
#include <string.h>

#include "vdc.h"
#include "layout.h"
#include "c64os.h"

/* THE C64 OS SCREEN DRIVER: c128/src/vic.c's seam, drawn into C64 OS's
 * buffers instead of the VIC's memory.
 *
 * WHERE THE SCREEN IS. C64 OS keeps an application's characters in a BUFFER
 * at $0400 and its colours in a buffer in the RAM under I/O at $D800; the VIC
 * shows the RAM under I/O at $DC00, and C64 OS's event loop copies one to the
 * other. The game does not return to that loop while it plays (see
 * src/app.s), so this driver marks the buffers dirty and os_present() copies
 * them when the game is about to wait -- kb_waitkey() and wait_vsync().
 *
 * THE COLOUR BUFFER NEEDS THE I/O BANKED OUT. The game runs with $01 = $36,
 * where $D800 is colour RAM, not C64 OS's buffer -- the probe read $36 in
 * init. So every colour store is made at $34 with interrupts off, and put
 * back. Characters at $0400 are plain RAM and need nothing.
 *
 * THE FONT IS C64 OS'S, AND IT HAS NO BOX-DRAWING GLYPHS -- dumped from the
 * running system on 2026-09-27: lowercase at 0-31, ASCII punctuation, digits
 * and capitals at 32-95, the OS's icons at 96-127, and the reverse of all of
 * it above. The console needs fifteen shapes it lacks (layout.h's G_* set,
 * less G_BLOCK, which is its reverse space too). They are drawn here, with
 * the 2-pixel strokes of C64 OS's own font, into fifteen of its NORMAL-VIDEO
 * icon slots -- the probe found its menu and status bars drawn entirely in
 * reverse video, so those slots are the ones its own bars do not use -- and
 * the originals are put back by vdc_shutdown(). The shapes are this project's,
 * not Commodore's ROM. */

#define SCREEN   ((volatile unsigned char *)0x0400)
#define COLBUF   ((volatile unsigned char *)0xD800)
#define CHARSET  ((volatile unsigned char *)0xD000)
#define PORT     (*(volatile unsigned char *)0x0001)
#define VIC_RASTER (*(volatile unsigned char *)0xD012)
#define VIC_BORDER (*(volatile unsigned char *)0xD020)
#define VIC_BGND   (*(volatile unsigned char *)0xD021)
#define COLS 40
#define ROWS 25

/* Fifteen icon slots, and what goes in each. The slots avoid 96-101 (the
   shortcut-key and arrow icons a menu draws) and 113 (its check mark). */
static const unsigned char slot_of[15] = {
    102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 114, 115, 116, 117
};
static const unsigned char glyph_of[15] = {
    G_HLINE, G_VLINE, G_TL, G_TR, G_BL, G_BR, G_TEE_L, G_TEE_R,
    G_TEE_D, G_TEE_U, G_CROSS, G_BAR, G_SHIP, G_HALF_LO, G_HALF_HI
};
#define H 0xFF          /* a 2-pixel horizontal stroke is two rows of this */
#define V 0x18          /* a 2-pixel vertical stroke: columns 3 and 4 */
static const unsigned char bitmap[15][8] = {
    {0, 0, 0, H, H, 0, 0, 0},                          /* G_HLINE */
    {V, V, V, V, V, V, V, V},                          /* G_VLINE */
    {0, 0, 0, 0x1F, 0x1F, V, V, V},                    /* G_TL */
    {0, 0, 0, 0xF8, 0xF8, V, V, V},                    /* G_TR */
    {V, V, V, 0x1F, 0x1F, 0, 0, 0},                    /* G_BL */
    {V, V, V, 0xF8, 0xF8, 0, 0, 0},                    /* G_BR */
    {V, V, V, 0x1F, 0x1F, V, V, V},                    /* G_TEE_L */
    {V, V, V, 0xF8, 0xF8, V, V, V},                    /* G_TEE_R */
    {0, 0, 0, H, H, V, V, V},                          /* G_TEE_D */
    {V, V, V, H, H, 0, 0, 0},                          /* G_TEE_U */
    {V, V, V, H, H, V, V, V},                          /* G_CROSS */
    {H, H, H, H, H, H, H, 0},                          /* G_BAR: 7 of 8 rows */
    {0, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0},        /* G_SHIP */
    {0, 0, 0, 0, H, H, H, H},                          /* G_HALF_LO */
    {H, H, H, H, 0, 0, 0, 0},                          /* G_HALF_HI */
};
static unsigned char saved_glyphs[15][8];
static unsigned char saved_border, saved_bgnd;

/* Game screen code (the C64 ROM's set, which the shared UI draws in) -> C64
   OS's. Built once: capitals 1-26 move to 65-90, the fifteen named glyphs
   move to their slots, everything else -- digits, punctuation, reverse space
   -- is already where C64 OS has it. Bit 7 (reverse) is carried across. */
static unsigned char xlat[256];
volatile unsigned char c64os_stage;
static unsigned char dirty;

/* The RAM under I/O, read or written with interrupts off and put back. */
static void charset_copy(volatile unsigned char *dst, const volatile unsigned char *src) {
    unsigned char save = PORT, i;
    __asm__ volatile("sei" ::: "memory");
    PORT = (unsigned char)((save & 0xF8) | 0x04);
    for (i = 0; i < 8; i++) dst[i] = src[i];
    PORT = save;
    __asm__ volatile("cli" ::: "memory");
}

static void colours(unsigned int off, unsigned char n, unsigned char color) {
    unsigned char save = PORT, i;
    color &= 0x0F;
    __asm__ volatile("sei" ::: "memory");
    PORT = (unsigned char)((save & 0xF8) | 0x04);
    for (i = 0; i < n; i++) COLBUF[off + i] = color;
    PORT = save;
    __asm__ volatile("cli" ::: "memory");
    dirty = 1;
}

void vdc_init(void) {
    static unsigned char done;
    unsigned int c;
    unsigned char i;

    /* TWICE: c64os_start() needs the screen for the REU load's count, and
       main() calls this again. A second pass would save the BORROWED shapes
       as the originals, and quitting would leave C64 OS's icons gone. */
    if (done) {
        scr_clear();
        return;
    }
    done = 1;

    for (c = 0; c < 256; c++) {
        unsigned char b = (unsigned char)(c & 0x7F);
        /* The game's raw codes are the C128's: @ at 0, A-Z at 1-26, [ £ ] at
           27-29. C64 OS's font (//os/charsets/charset.o) keeps @ at 0 and
           [ \ ] ^ _ at 27-31 but has LOWERCASE at 1-26 and capitals at
           65-90, so only the letters move. */
        if (b >= 1 && b <= 26) b = (unsigned char)(b + 64);
        xlat[c] = (unsigned char)(b | (c & 0x80));
    }
    for (i = 0; i < 15; i++) {
        xlat[glyph_of[i]] = slot_of[i];
        charset_copy(saved_glyphs[i], CHARSET + slot_of[i] * 8);
        charset_copy(CHARSET + slot_of[i] * 8, bitmap[i]);
    }
    saved_border = VIC_BORDER;
    saved_bgnd = VIC_BGND;
    VIC_BORDER = 0;
    VIC_BGND = 0;
    scr_clear();
    c64os_stage = 1;
}

/* Puts back what vdc_init() borrowed, and nothing else: the farewell stays
   on screen, as on every port. */
void vdc_shutdown(void) {
    unsigned char i;
    os_present();
    for (i = 0; i < 15; i++)
        charset_copy(CHARSET + slot_of[i] * 8, saved_glyphs[i]);
    VIC_BORDER = saved_border;
    VIC_BGND = saved_bgnd;
}

/* NOT A RESET, unlike the C64's: C64 OS is running underneath. Back to init's
   stack depth, and from there back to C64 OS -- src/app.s. */
void plat_exit(void) {
    app_exit();
}

/* A frame, and the screen brought up to date first: this is where the game
   waits, so this is where it should be seen. */
void wait_vsync(void) {
    osv_flush();
    while (VIC_RASTER != 0) {}
    while (VIC_RASTER == 0) {}
}

void osv_flush(void) {
    if (dirty) {
        dirty = 0;
        os_present();
    }
}

__attribute__((noinline))
void scr_clear(void) {
    unsigned int i;
    for (i = 0; i < COLS * ROWS; i++) SCREEN[i] = 32;
    for (i = 0; i < COLS * ROWS; i += COLS) colours(i, COLS, 0);
}

__attribute__((noinline))
void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color) {
    unsigned int off = (unsigned int)y * COLS + x;
    SCREEN[off] = xlat[ch];
    colours(off, 1, color);
}

/* Text: ASCII in, C64 OS codes out. Capitals stay capitals (65-90), and so
   do the lowercase letters and PETSCII's shifted ones, because the game is
   written in capitals; clipped at the right edge, as vic.c does. */
__attribute__((noinline))
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color) {
    unsigned int off = (unsigned int)y * COLS + x;
    unsigned char n = 0;
    for (; *s && x + n < COLS; s++, n++) {
        unsigned char u = (unsigned char)*s;
        if (u >= 97 && u <= 122) u = (unsigned char)(u - 32);
        /* ASCII @ [ \ ] ^ _ are 64 and 91-95; C64 OS's font has them at 0
           and 27-31, and a BACKQUOTE at 64 -- which is what "MONGOLS KILLED
           @ 10 EACH" showed until this line. */
        if (u == 64 || (u >= 91 && u <= 95)) u = (unsigned char)(u - 64);
        else if (u >= 193 && u <= 218) u = (unsigned char)(u - 128);
        else if (u < 32 || (u > 95 && u < 193) || u > 218) u = 32;
        SCREEN[off + n] = u;
    }
    if (n) colours(off, n, color);
}

__attribute__((noinline))
void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color) {
    unsigned int off = (unsigned int)y * COLS + x;
    unsigned char i, c = xlat[ch];
    if (x + w > COLS) w = (unsigned char)(COLS - x);
    for (i = 0; i < w; i++) SCREEN[off + i] = c;
    if (w) colours(off, w, color);
}

__attribute__((noinline))
void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color) {
    unsigned char i;
    for (i = 0; i < h; i++) scr_put(x, (unsigned char)(y + i), ch, color);
}

__attribute__((noinline))
void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color) {
    unsigned char row;
    for (row = 0; row < h; row++) scr_hline(x, (unsigned char)(y + row), w, ch, color);
}

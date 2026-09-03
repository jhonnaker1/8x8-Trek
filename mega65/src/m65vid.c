#include <mega65/conio.h>
#include <mega65/memory.h>
#include "m65vid.h"

/* THE EGA PALETTE, EXACTLY -- and this is the first thing the MEGA65 buys us.
 *
 * The C128's VDC is a separate RGBI chip with sixteen fixed colours, and the
 * port maps EGA onto it with a 4-bit rotate: fifteen land exactly and EGA's
 * BROWN comes out olive, which is recorded in egavdc.h as the one colour this
 * project cannot have. The VIC-IV's palette is programmable, so here we simply
 * write EGA's own values into entries 0..15 and the mapping becomes the
 * identity. Brown included.
 *
 * EGA's two-bits-per-gun levels are 0x00, 0x55, 0xAA and 0xFF -- every one of
 * them a duplicated nibble. That matters because it makes these bytes correct
 * whether the core takes the palette register as eight bits or as the low
 * nibble of a VIC-III-compatible four: 0xAA read either way is the same
 * colour. No revision check needed.
 */
static const unsigned char ega_r[16] = {
    0x00,0x00,0x00,0x00,0xAA,0xAA,0xAA,0xAA,
    0x55,0x55,0x55,0x55,0xFF,0xFF,0xFF,0xFF };
static const unsigned char ega_g[16] = {
    0x00,0x00,0xAA,0xAA,0x00,0x00,0x55,0xAA,
    0x55,0x55,0xFF,0xFF,0x55,0x55,0xFF,0xFF };
static const unsigned char ega_b[16] = {
    0x00,0xAA,0x00,0xAA,0x00,0xAA,0x00,0xAA,
    0x55,0xFF,0x55,0xFF,0x55,0xFF,0x55,0xFF };

#define PAL_R ((volatile unsigned char *)0xD100)
#define PAL_G ((volatile unsigned char *)0xD200)
#define PAL_B ((volatile unsigned char *)0xD300)

static void load_ega_palette(void) {
    unsigned char i;
    for (i = 0; i < 16; i++) {
        PAL_R[i] = ega_r[i];
        PAL_G[i] = ega_g[i];
        PAL_B[i] = ega_b[i];
    }
}

void vdc_init(void) {
    conioinit();
    setscreensize(VDC_COLS, VDC_ROWS);   /* the real H640, unlike C64 mode */
    setextendedattrib(0);                /* no reverse bit: we want 16 plain
                                            foreground colours, not 8 + an
                                            attribute. See scr_put. */
    setuppercase();
    load_ega_palette();
    bordercolor(0);
    bgcolor(0);
    clrscr();
}

void vdc_shutdown(void) {
    bordercolor(6);
    bgcolor(6);
    clrscr();
}

/* Frame pace off the raster, as the C128 port does. $D012 is the VIC-II
   compatible low byte and the VIC-IV still maintains it in native mode. */
void wait_vsync(void) {
    while (PEEK(0xD012u) != 0xF8) { }
}

void scr_clear(void) { clrscr(); }

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color) {
    if (x >= VDC_COLS || y >= VDC_ROWS) return;
    /* cputcxy writes a raw SCREEN CODE, which is what layout.h's glyph
       constants already are -- they were converted once for the C128 and the
       MEGA65 uses the same C64-family character set, so they carry over
       unchanged. */
    textcolor(color);
    cputcxy(x, y, ch);
}

/* ASCII -> screen code, character for character the same table as the C128
   port's vdc.c. The split it enforces matters just as much here: STRINGS are
   converted, GLYPHS are not. The box-drawing codes the console is built from
   live at 64..127, which is exactly the range this rewrites, so routing a
   panel border through it would turn the frame into letters. */
static unsigned char ascii_to_screencode(char c) {
    unsigned char u = (unsigned char)c;
    if (u >= 32 && u <= 63) return u;           /* space, digits, punctuation */
    if (u >= 64 && u <= 95) return (unsigned char)(u - 64);
    if (u >= 97 && u <= 122) return (unsigned char)(u - 96);
    if (u >= 193 && u <= 218) return (unsigned char)(u - 192);
    return 32;
}

void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color) {
    while (*s) {
        if (x >= VDC_COLS) return;
        scr_put(x++, y, ascii_to_screencode(*s++), color);
    }
}

void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color) {
    unsigned char i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            scr_put((unsigned char)(x + i), (unsigned char)(y + j), ch, color);
}

void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color) {
    while (w--) scr_put(x++, y, ch, color);
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color) {
    while (h--) scr_put(x, y++, ch, color);
}

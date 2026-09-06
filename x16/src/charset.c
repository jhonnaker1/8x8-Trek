/* Is the VRAM font read working AT ALL? Previous probe found one horizontal
   candidate and no verticals -- the signature of reading zeros rather than of
   a charset without box glyphs. So dump the RAW BYTES of three tiles:
       $41  'A'  -- the CONTROL. Its shape is known; if this is not a letter
                    bitmap then the read is wrong and nothing else here means
                    anything.
       $C0       -- where a Commodore charset keeps the horizontal line
       $5F       -- what the search actually matched
   Hex is written with a RAW cell write, not scr_put: scr_put applies the
   screen-code translation, which mangled A-F into accented letters last time
   and made the numbers hard to read. */
#include <stdint.h>
#include "x16vera.h"

#define VERA_ADDR_L  (*(volatile unsigned char *)0x9F20)
#define VERA_ADDR_M  (*(volatile unsigned char *)0x9F21)
#define VERA_ADDR_H  (*(volatile unsigned char *)0x9F22)
#define VERA_DATA0   (*(volatile unsigned char *)0x9F23)
#define VERA_CTRL    (*(volatile unsigned char *)0x9F25)
#define L1_TILEBASE  (*(volatile unsigned char *)0x9F36)

static void seek(unsigned long a, unsigned char inc) {
    VERA_CTRL = 0;
    VERA_ADDR_L = (unsigned char)(a & 0xFF);
    VERA_ADDR_M = (unsigned char)((a >> 8) & 0xFF);
    VERA_ADDR_H = (unsigned char)(((a >> 16) & 1) | (inc << 4));
}

/* Raw cell write -- no translation, so hex digits stay hex digits. */
static void raw(unsigned char x, unsigned char y, unsigned char ch, unsigned char col) {
    unsigned long a = 0x1B000UL + ((unsigned long)y * 128 + x) * 2UL;
    seek(a, 1);
    VERA_DATA0 = ch;
    VERA_DATA0 = col;
}
static void raws(unsigned char x, unsigned char y, const char *s, unsigned char col) {
    while (*s) raw(x++, y, (unsigned char)*s++, col);
}
static void hex2(unsigned char x, unsigned char y, unsigned char v, unsigned char col) {
    const char *h = "0123456789ABCDEF";
    raw(x, y, (unsigned char)h[(v >> 4) & 15], col);
    raw((unsigned char)(x + 1), y, (unsigned char)h[v & 15], col);
}

static void show_tile(unsigned char row, unsigned long base, unsigned char code) {
    unsigned char i, b;
    unsigned char y = (unsigned char)(6 + row * 10);
    raws(2, y, "TILE $", 11); hex2(8, y, code, 14);
    for (i = 0; i < 8; i++) {
        seek(base + (unsigned long)code * 8 + i, 1);
        b = VERA_DATA0;
        hex2((unsigned char)(12 + i * 3), y, b, 15);
        /* draw the bitmap so the shape is visible, not just the numbers */
        { unsigned char bit;
          for (bit = 0; bit < 8; bit++)
              raw((unsigned char)(40 + bit), (unsigned char)(y + i),
                  (b & (unsigned char)(0x80 >> bit)) ? 'X' : '.', 15); }
    }
}

int main(void) {
    unsigned long base;
    vdc_init();
    raws(2, 0, "X16 FONT: RAW TILE BYTES (CONTROL = 'A')", 15);
    base = ((unsigned long)(L1_TILEBASE & 0xFC)) << 9;
    raws(2, 2, "L1 TILEBASE $", 11);
    hex2(15, 2, (unsigned char)((base >> 16) & 0xFF), 14);
    hex2(17, 2, (unsigned char)((base >> 8) & 0xFF), 14);
    hex2(19, 2, (unsigned char)(base & 0xFF), 14);

    show_tile(0, base, 0x41);   /* control: must look like a letter */
    show_tile(1, base, 0xC0);
    show_tile(2, base, 0x5F);

    for (;;) { }
    return 0;
}

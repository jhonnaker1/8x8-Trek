#include "vdc.h"

#define VDC_CTRL (*(volatile unsigned char *)0xD600)
#define VDC_DATA (*(volatile unsigned char *)0xD601)
#define VIC_RASTER (*(unsigned char *)0xD012)
#define C128_CLKRATE (*(unsigned char *)0xD030)

static void wait_ready(void) {
    while (!(VDC_CTRL & 0x80)) {}
}

unsigned char vdc_reg_read(unsigned char reg) {
    wait_ready();
    VDC_CTRL = reg;
    wait_ready();
    return VDC_DATA;
}

void vdc_reg_write(unsigned char reg, unsigned char value) {
    wait_ready();
    VDC_CTRL = reg;
    wait_ready();
    VDC_DATA = value;
}

void vdc_set_address(unsigned int addr) {
    vdc_reg_write(18, (unsigned char)(addr >> 8));
    vdc_reg_write(19, (unsigned char)(addr & 0xFF));
}

void vdc_data_write(unsigned char value) {
    vdc_reg_write(31, value);
}

unsigned char vdc_data_read(void) {
    return vdc_reg_read(31);
}

/* The VIC-IIe keeps rastering at the normal rate even while the VDC drives
   the visible display, so its raster register is a free frame-rate timer.
   Inherited from the uno driver, minus that driver's per-frame rewrite of
   register 28 -- see the header for why this one has nothing to defend. */
void wait_vsync(void) {
    while (VIC_RASTER != 0) {}
    while (VIC_RASTER == 0) {}
}

/* String literal -> C64/C128 screen code, used by scr_puts only.
 *
 * What arrives here is PETSCII, not ASCII. cc65 translates string literals to
 * the target character set for Commodore targets, so an uppercase 'A' written
 * in the source reaches this function as PETSCII $C1 (193), not ASCII $41.
 * Confirmed in the linked binary -- "6=BROWN" assembles to
 * 36 3d c2 d2 cf d7 ce. Digits and punctuation survive translation at their
 * ASCII values; letters do not.
 *
 * Missing the 193-218 range failed silently rather than loudly: those bytes
 * matched no branch and fell through to `return 32`, so every panel title
 * rendered as spaces while digits came through untouched. The screen read
 * "0-15" where "EGA PALETTE 0-15" was written, and "U.S.S. LEXINGTON" as three
 * periods. Nothing looked broken -- it looked empty.
 *
 * Everything else in this driver takes a RAW screen code. That split is
 * deliberate: the box-drawing glyphs the console is built from live at screen
 * codes 64-127, which overlaps the ranges rewritten below. Routing panel
 * glyphs through here would silently turn borders into letters. Strings are
 * text and get converted; glyphs are glyphs and do not. */
static unsigned char ascii_to_screencode(char c) {
    unsigned char u = (unsigned char)c;
    if (u >= 32 && u <= 63) return u;           /* space, digits, punctuation */
    if (u >= 64 && u <= 95) return u - 64;      /* @A-Z[\]^_ -- lowercase in source */
    if (u >= 97 && u <= 122) return u - 96;     /* untranslated ASCII lowercase */
    if (u >= 193 && u <= 218) return u - 192;   /* PETSCII A-Z -- uppercase in source */
    return 32;
}

void vdc_init(void) {
    vdc_reg_write(12, (unsigned char)(VDC_SCREEN_BASE >> 8));
    vdc_reg_write(13, (unsigned char)(VDC_SCREEN_BASE & 0xFF));
    vdc_reg_write(20, (unsigned char)(VDC_ATTR_BASE >> 8));
    vdc_reg_write(21, (unsigned char)(VDC_ATTR_BASE & 0xFF));

    /* Enable attribute (per-character colour) mode, keep other mode bits. */
    vdc_reg_write(25, (unsigned char)(vdc_reg_read(25) | 0x40));

    /* Register 28 (character base) is left exactly as the KERNAL set it.
       This port uses the stock character ROM the KERNAL already uploaded to
       VDC RAM, so there is no custom charset to point at -- and nothing for
       the KERNAL's 80-column cursor IRQ to undo. When EGA Trek's panel
       glyphs need a custom set, this is where the uno driver's upload (and
       its 16-byte-per-glyph slot padding) comes back. */

    /* Screen background, low nibble. The console is drawn on black like the
       original. */
    vdc_reg_write(26, VDC_BLACK);

    /* 2MHz. The VIC-IIe's own picture becomes unwatchable at 2x -- it cannot
       fetch coherently -- but this build is entirely VDC-driven, and the uno
       work confirmed empirically that the raster register wait_vsync() reads
       keeps counting normally either way. Bit 0 only; the rest are left alone. */
    C128_CLKRATE |= 0x01;

    scr_clear();
}

void vdc_shutdown(void) {
    /* Back to 1MHz. Leaving the machine at 2MHz would hand BASIC a blanked
       VIC-IIe screen, which reads as a hung computer. The VDC registers
       vdc_init() touched are all at their KERNAL defaults already -- screen
       and attribute bases were never moved, and attribute mode is how the
       C128 comes up -- so there is nothing else to undo. */
    C128_CLKRATE &= (unsigned char)~0x01;

    /* Deliberately does NOT clear the screen. BASIC resumes on whichever
       display the KERNAL owns, and the C128 boots in 40 columns -- so it
       comes back on the VIC-IIe, in x128's *other* window, not here.
       Clearing the VDC therefore left the window the player was watching
       completely black, which reads as a crash rather than as an exit.
       Leaving the final console up is both more useful and more honest. */
}

void scr_clear(void) {
    unsigned int i;
    vdc_set_address(VDC_SCREEN_BASE);
    for (i = 0; i < (unsigned int)VDC_COLS * VDC_ROWS; i++) vdc_data_write(32);
    vdc_set_address(VDC_ATTR_BASE);
    for (i = 0; i < (unsigned int)VDC_COLS * VDC_ROWS; i++) vdc_data_write(VDC_WHITE);
}

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color) {
    unsigned int off = (unsigned int)y * VDC_COLS + x;
    vdc_set_address(VDC_SCREEN_BASE + off);
    vdc_data_write(ch);
    vdc_set_address(VDC_ATTR_BASE + off);
    vdc_data_write(color & 0x0F);
}

void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color) {
    unsigned int off = (unsigned int)y * VDC_COLS + x;
    const char *p;

    vdc_set_address(VDC_SCREEN_BASE + off);
    for (p = s; *p; p++) vdc_data_write(ascii_to_screencode(*p));

    vdc_set_address(VDC_ATTR_BASE + off);
    for (p = s; *p; p++) vdc_data_write(color & 0x0F);
}

/* One address seek per run rather than per cell -- the VDC auto-increments.
   The nine-panel console is drawn almost entirely out of runs, so this is
   the hot path for a full redraw. */
void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color) {
    unsigned int off = (unsigned int)y * VDC_COLS + x;
    unsigned char i;

    vdc_set_address(VDC_SCREEN_BASE + off);
    for (i = 0; i < w; i++) vdc_data_write(ch);
    vdc_set_address(VDC_ATTR_BASE + off);
    for (i = 0; i < w; i++) vdc_data_write(color & 0x0F);
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color) {
    unsigned char i;
    for (i = 0; i < h; i++) scr_put(x, (unsigned char)(y + i), ch, color);
}

void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color) {
    unsigned char row;
    for (row = 0; row < h; row++) scr_hline(x, (unsigned char)(y + row), w, ch, color);
}

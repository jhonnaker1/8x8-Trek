/* Video for the Atari ST and STE: the Shifter in 320x200, sixteen colours.
 *
 * THIS IS THE ONLY FILE THIS PORT NEEDED, and that is the finding rather than
 * a boast. The Falcon port already supplies the input, storage, far-memory,
 * sound and overlay seams for this machine: `vc +tos` IS the ST target, the
 * Falcon's sound is the YM2149 through XBIOS Giaccess at the same measured
 * 2 MHz clock, storage is GEMDOS, far memory is a plain array. See
 * ../README.md, "What a sub-80 port actually entails".
 *
 * WHAT THIS FILE OWES TO TWO OTHERS, stated because neither is obvious:
 *
 *   falcon/src/falconvid.c   the plane arithmetic, the glyph_rows rule and
 *                            the missing-glyph marker. THE PLANES ARE
 *                            WORD-INTERLEAVED THE SAME WAY -- four planes of
 *                            the same sixteen pixels in four consecutive
 *                            words -- so cell_byte() is that file's, unchanged
 *                            but for the stride.
 *   amiga/src/amigagfx.c     the 8x8 box glyphs. The Falcon's are 8x16 and
 *                            do not scale; the Amiga draws at 8x8 for the
 *                            same reason this port does, so its artwork is
 *                            the right size already and is this project's own.
 *
 * WHY 320x200 AND NOT 640x200. The ST has three modes and only two are
 * colour: 320x200 in sixteen, and 640x200 in FOUR. The console needs eight
 * information-bearing colours -- four Mongol ship types, the chart's Mongol
 * and base markers, the departments, the message line -- so 640x200 loses the
 * game's rules, not its looks. That is why the whole ST line was ruled out of
 * this project on 2026-08-22, back when the console also had to be eighty
 * columns wide. It is forty columns now, 320/8 = 40 and 200/8 = 25, and the
 * mode that was never wide enough is exactly the right size.
 *
 * ST MONOCHROME IS REFUSED rather than painted on. One plane and no colour is
 * a different driver, not a different mode -- the same decision falconvid.c
 * makes for a Falcon reporting an ST-mono display.
 */
#include <tos.h>
#include <string.h>

#include "../../c128/src/vdc.h"
#include "../../core/ega.h"

#define SCR_W    320
#define SCR_H    200
#define STRIDE   (SCR_W / 2)      /* 4 planes, 1 bit each = 160 bytes a line */
#define CELL_W   8
#define CELL_H   8
#define ST_COLS  40
#define ST_ROWS  25
#define REZ_LOW  0                /* Getrez()/Setscreen(): 0 low, 1 med, 2 mono */

/* Line-A init: d0/a0 = variable table, a1 = the three font headers, a2 = the
   routine table. Only a1 is wanted. d2 and a2 are saved because Line-A is
   documented to use them and vbcc must not assume otherwise. Verbatim from
   falconvid.c -- the call is TOS's, not the Falcon's. */
__regsused("d0/d1/a0/a1/a2") void *linea_fonts(void) =
  "\tmovem.l\td2/a2,-(sp)\n"
  "\tdc.w\t$a000\n"
  "\tmove.l\ta1,d0\n"
  "\tmovem.l\t(sp)+,d2/a2\n";

/* HIDE THE MOUSE, AND IT IS NOT COSMETIC TIDYING.
 *
 * The VBL mouse handler is still running while this program owns the screen,
 * and the way it draws is to SAVE THE BACKGROUND under the cursor and restore
 * it when the cursor moves. Whatever was under the pointer when the desktop
 * handed over is what it restores -- so a block of stale desktop pixels
 * appears over the console the first time the handler fires, in the middle of
 * whatever the game had drawn there. On the title screen it landed across the
 * word MONGOL as a solid green rectangle.
 *
 * Nothing in this port writes that block and no redraw prevents it: the game
 * paints the cell, and then an interrupt paints over it again. $A00A is
 * Line-A's HIDE_MOUSE, which stops the handler drawing at all.
 *
 * Cursconf() is NOT this -- that is the TEXT cursor, a different thing that
 * this driver also turns off.
 */
__regsused("d0/d1/a0/a1/a2") void linea_hide_mouse(void) =
  "\tmovem.l\td2/a2,-(sp)\n"
  "\tdc.w\t$a000\n"
  "\tdc.w\t$a00a\n"
  "\tmovem.l\t(sp)+,d2/a2\n";

struct fnthdr {
    short id, point;
    char  name[32];
    unsigned short first_ade, last_ade;
    short top, ascent, half, descent, bottom;
    short max_char_width, max_cell_width;
    short left_offset, right_offset;
    short thicken, ul_size, lighten, skew;
    unsigned short flags;
    unsigned char  *hor_table;
    unsigned short *off_table;
    unsigned char  *dat_table;
    unsigned short form_width, form_height;
    struct fnthdr *next_font;
};

static unsigned char *scr;                /* Physbase(), once */
static const struct fnthdr *rom_font;     /* the 8x8 system font */
static short old_rez = -1;
static short old_pal[16];
static short pal[16];

/* EGA's palette in the STE's four bits a gun. EGA's own levels are
 * 0x00/0x55/0xAA/0xFF, which at four bits are 0, 5, 10 and 15 -- close to
 * exact quarters and no rounding worth arguing about. Index order is EGA's,
 * so core/ega.h's names ARE the palette registers and EGA_TO_VDC stays the
 * identity: the same trick the MEGA65, the X16, the Amiga and the Falcon use.
 */
static const unsigned char ega_rgb[16][3] = {
    { 0, 0, 0}, { 0, 0,10}, { 0,10, 0}, { 0,10,10},
    {10, 0, 0}, {10, 0,10}, {10, 5, 0}, {10,10,10},
    { 5, 5, 5}, { 5, 5,15}, { 5,15, 5}, { 5,15,15},
    {15, 5, 5}, {15, 5,15}, {15,15, 5}, {15,15,15}
};

/* ONE ENCODING, CORRECT ON BOTH MACHINES, and it is not the obvious one.
 *
 * An STE gun is four bits, but the extra bit lives in bit 3 of the nibble as
 * the LEAST significant one. So a plain ST -- which reads only bits 2..0 --
 * sees the top three bits and shows the nearest of its 512 colours, while an
 * STE reads all four and shows the nearest of 4096. Writing the ST's own
 * three-bit encoding instead would work on both and throw the STE's extra bit
 * away for nothing.
 *
 * Read off commodore-uno's `ste/src/stevid.c`, which solved this first and
 * which this project treats as the house reference for these machines.
 */
static short ste_nibble(unsigned char v)
{
    return (short)(((v >> 1) & 7) | ((v & 1) << 3));
}

/* THIS PORT'S GLYPHS ARE THE AMIGA'S, at 8x8 -- every code the shared UI can
 * pass that the ROM font cannot supply. layout.h's box-drawing set lives at
 * 64..127, which in a ROM font is '@'..'_', so those cells MUST come from here
 * or the console draws as a wall of letters.
 *
 * Copied rather than shared: the two ports are never linked together and a
 * header holding one table is worse than the copy. If a third 8x8 target
 * appears, that is the moment to lift it out.
 */
struct box_glyph { unsigned char code; unsigned char row[CELL_H]; };

static const struct box_glyph box[] = {
  /* EVERY ENTRY STARTS ITS OWN LINE, and that is a parser contract rather
     than a style choice. tools/verify_st.py reads this table out of the file
     with `^\s*\{\s*(\d+),\s*\{` -- the same regex the Falcon's checker
     uses on its own driver -- so an entry hiding behind a comment on the same
     line is INVISIBLE TO THE GATE. The first draft of this file put the
     comment first and the checker saw 6 of 17, then reported ten glyphs
     missing that were sitting right there. It failed safe, loudly, in the
     right direction; a parser that reports a subset is still a parser that
     was wrong. */

  /* 32  space, stated rather than left to the font, so that 160 -- the
         cursor and the badge's body -- is solid even if the font is missing */
  { 32, {0,0,0,0,0,0,0,0}},
  /* 64  G_HLINE  ---- */
  { 64, {0,0,0,0xFF,0xFF,0,0,0}},
  /* 93  G_VLINE  |    */
  { 93, {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},
  /* 112 G_TL     ,-   */
  {112, {0,0,0,0x1F,0x1F,0x18,0x18,0x18}},
  /* 110 G_TR     -.   */
  {110, {0,0,0,0xF8,0xF8,0x18,0x18,0x18}},
  /* 109 G_BL     `-   */
  {109, {0x18,0x18,0x18,0x1F,0x1F,0,0,0}},
  /* 125 G_BR     -'   */
  {125, {0x18,0x18,0x18,0xF8,0xF8,0,0,0}},
  /* 107 G_TEE_L  |-   */
  {107, {0x18,0x18,0x18,0x1F,0x1F,0x18,0x18,0x18}},
  /* 115 G_TEE_R  -|   */
  {115, {0x18,0x18,0x18,0xF8,0xF8,0x18,0x18,0x18}},
  /* 114 G_TEE_D  T    */
  {114, {0,0,0,0xFF,0xFF,0x18,0x18,0x18}},
  /* 113 G_TEE_U  _|_  */
  {113, {0x18,0x18,0x18,0xFF,0xFF,0,0,0}},
  /* 91  G_CROSS  +    */
  { 91, {0x18,0x18,0x18,0xFF,0xFF,0x18,0x18,0x18}},
  /* 98  BADGE_DISC_TOP -- its reverse, 226, is the bottom half and needs no
         entry of its own. Geometry: a half block has one shape. */
  { 98, {0,0,0,0,0xFF,0xFF,0xFF,0xFF}},
  /* 100 the bottom row alone, and the ONLY reason it is here is that its
         reverse, 228, is the systems-status bar: seven rows filled on an
         eight-pixel pitch, which leaves the hairline between bars that ui.c
         measured off the original. */
  {100, {0,0,0,0,0,0,0,0xFF}},
  /* 30, 31 the up and left arrows -- the two of 27..31 with no ASCII to
         borrow. Unused by the game as it stands; here because a screen code
         with a glyph is one that cannot come out as a hollow box later. */
  { 30, {0x18,0x3C,0x7E,0x18,0x18,0x18,0x18,0}},
  { 31, {0,0x10,0x30,0x7E,0x30,0x10,0,0}},
  /* 81  the ship's saucer. The badge and the info panel both put this beside
         four cells of G_HLINE and a solid block, so it has to read as a hull
         at 8x8: wider than tall, clear of the top and bottom rows so it does
         not merge with the cell above or below. */
  { 81, {0,0x3C,0x7E,0xFF,0xFF,0x7E,0x3C,0}}
};
#define NBOX ((int)(sizeof box / sizeof box[0]))

/* SCREEN CODE -> ASCII, for everything the ROM font can supply. The C64
   unshifted set puts '@' at 0 and A..Z at 1..26; 32..63 are ASCII already. */
static int code_to_ascii(unsigned char c)
{
    if (c == 0) return '@';
    if (c <= 26) return 'A' + c - 1;
    /* 27 and 29 are real: the play-again box is drawn as "[YES]" and "[NO]",
       which scr_puts turns into screen codes 27 and 29. Without these that
       prompt reads as a hollow box either side of the word. */
    if (c == 27) return '[';
    if (c == 29) return ']';
    if (c >= 32 && c <= 63) return c;
    return -1;
}

/* Code -> eight rows of bits. REVERSE VIDEO IS A RULE, NOT A TABLE: codes
   128..255 are their base glyph inverted, so 160 falls out of 32, 226 out of
   98 and 228 out of 100 without three more hand-written entries to disagree
   with the rule. */
static void glyph_rows(unsigned char ch, unsigned char *out)
{
    unsigned char base = ch & 0x7F;
    int rev = (ch & 0x80) != 0;
    int i, j, a;

    for (i = 0; i < NBOX; i++)
        if (box[i].code == base) {
            for (j = 0; j < CELL_H; j++)
                out[j] = rev ? (unsigned char)~box[i].row[j] : box[i].row[j];
            return;
        }

    a = code_to_ascii(base);
    if (a >= 0 && rom_font) {
        const unsigned char *d = rom_font->dat_table + a;
        unsigned short w = rom_font->form_width;
        for (j = 0; j < CELL_H; j++)
            out[j] = rev ? (unsigned char)~d[j * w] : d[j * w];
        return;
    }

    /* THE MISSING-GLYPH MARKER, deliberately loud. A code with no box entry
       and no ASCII is a bug in this file, not in the caller, and a blank cell
       would hide it -- on the Amiga this marker is what caught two missing
       bracket glyphs. A hollow box is visible and is not a letter. */
    for (j = 0; j < CELL_H; j++)
        out[j] = (j == 0 || j == CELL_H - 1) ? 0xFF : 0x81;
    if (rev)
        for (j = 0; j < CELL_H; j++)
            out[j] = (unsigned char)~out[j];
}

/* One cell's byte, in one plane, on one scanline. Eight pixels are a single
   byte in each of four words that sit eight bytes apart; an even column is
   the high byte of each word and an odd column the low one. Identical to the
   Falcon's -- only STRIDE differs. */
static unsigned char *cell_byte(unsigned char x, int py, int plane)
{
    return scr + (long)py * STRIDE + ((long)(x >> 1) * 8) + (plane * 2) + (x & 1);
}

void vdc_init(void)
{
    void **fonts = (void **)linea_fonts();
    const struct fnthdr *f;
    int i;

    /* MONO IS REFUSED, NOT SQUEEZED. One plane and no colour is a different
       driver. Said through GEMDOS, and the machine handed back untouched. */
    if (Getrez() == 2) {
        Cconws("\r\nEGA Trek needs a colour monitor.\r\n"
               "This machine is in ST high resolution.\r\n");
        Pterm(1);
    }

    /* Take the 8x8 font by MEASURING the headers rather than by taking an
       index. TOS ships 6x6, 8x8 and 8x16, commonly in that order -- but the
       cell size is the thing this driver actually depends on, and a ROM is
       free to disagree about the order. Same reasoning as falconvid.c. */
    rom_font = 0;
    for (i = 0; i < 3; i++) {
        f = (const struct fnthdr *)fonts[i];
        if (f && f->form_height == CELL_H && f->max_cell_width == CELL_W) {
            rom_font = f;
            break;
        }
    }

    /* Before the mode change, so the handler is already quiet when the screen
       is cleared -- otherwise it has one more chance to stamp the old
       background onto the new picture. */
    linea_hide_mouse();

    old_rez = Getrez();
    for (i = 0; i < 16; i++) old_pal[i] = 0;
    Setscreen((void *)-1L, (void *)-1L, REZ_LOW);
    Cursconf(0, 0);                       /* the blinking text cursor, gone */

    scr = (unsigned char *)Physbase();

    for (i = 0; i < 16; i++)
        pal[i] = (short)((ste_nibble(ega_rgb[i][0]) << 8) |
                         (ste_nibble(ega_rgb[i][1]) << 4) |
                          ste_nibble(ega_rgb[i][2]));
    Setpalette(pal);

    scr_clear();
}

/* Deliberately does NOT clear: the farewell has to survive it. That is the
   C128's own rule and the bug the X16 and the Amiga both shipped in v0.13.0.
   The resolution goes back so the desktop is usable again. */
/* $A009 is SHOW_MOUSE. The desktop needs its pointer back, and a program that
   takes something from the machine gives it back -- the same rule that puts
   the resolution and the palette back below. */
__regsused("d0/d1/a0/a1/a2") void linea_show_mouse(void) =
  "\tmovem.l\td2/a2,-(sp)\n"
  "\tdc.w\t$a000\n"
  "\tsub.l\ta2,a2\n"
  "\tdc.w\t$a009\n"
  "\tmovem.l\t(sp)+,d2/a2\n";

void vdc_shutdown(void)
{
    Cursconf(1, 0);
    linea_show_mouse();
    if (old_rez >= 0)
        Setscreen((void *)-1L, (void *)-1L, old_rez);
    Setpalette(old_pal);
}

/* Nothing to do: TOS gets its process back and the desktop redraws itself.
   Same as the Falcon, and unlike every 6502 port, which has taken over the
   whole machine and has to say how it gives it back. */
void plat_exit(void) { }

void wait_vsync(void) { Vsync(); }

void scr_clear(void)
{
    if (scr)
        memset(scr, 0, (long)STRIDE * SCR_H);
}

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color)
{
    unsigned char rows[CELL_H];
    int py, r, p;

    if (!scr || x >= ST_COLS || y >= ST_ROWS)
        return;

    glyph_rows(ch, rows);
    py = y * CELL_H;

    for (r = 0; r < CELL_H; r++) {
        unsigned char bits = rows[r];
        for (p = 0; p < 4; p++)
            *cell_byte(x, py + r, p) = (color >> p) & 1 ? bits : 0;
    }
}

/* ASCII IN, SCREEN CODES OUT -- the same conversion every port does. scr_put
   takes a SCREEN code, so a caller holding a C string comes through here.
   CLIPS AT ST_COLS: screen memory is linear and an overflowing string would
   otherwise land on the next row, which is how the C128's 40-column build
   learned to clip. */
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color)
{
    const char *p;
    unsigned char u;

    for (p = s; *p && x < ST_COLS; p++, x++) {
        u = (unsigned char)*p;
        if (u >= 64 && u <= 95)       u -= 64;
        else if (u >= 97 && u <= 122) u -= 96;
        else if (u >= 193 && u <= 218) u -= 192;
        else if (!(u >= 32 && u <= 63)) u = 32;
        scr_put(x, y, u, color);
    }
}

void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color)
{
    unsigned char i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            scr_put((unsigned char)(x + i), (unsigned char)(y + j), ch, color);
}

void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color)
{
    unsigned char i;
    for (i = 0; i < w && x + i < ST_COLS; i++)
        scr_put((unsigned char)(x + i), y, ch, color);
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color)
{
    unsigned char i;
    for (i = 0; i < h && y + i < ST_ROWS; i++)
        scr_put(x, (unsigned char)(y + i), ch, color);
}

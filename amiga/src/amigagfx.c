/* EGA Trek video for the Amiga (OCS/ECS, KS2.0+).
 *
 * The seam is c128/src/vdc.h, exactly as on the MEGA65 and the X16: an 80x25
 * grid of cells, each one a glyph plus a foreground colour on black. What
 * changes here is that there is no character generator to talk to -- this
 * machine has a bitmap, so a "cell" is eight bytes written across four
 * bitplanes and the port has to say what a glyph looks like.
 *
 * 640x200 IN FOUR BITPLANES IS EXACTLY THE CONSOLE. 80 cells of 8 pixels by
 * 25 rows of 8 makes 640x200 with nothing left over, and four planes at 640
 * wide is the OCS/ECS hires maximum -- sixteen colours against the fifteen
 * the console uses. One to spare, and no compromise to negotiate.
 *
 * WHY THE GLYPHS ARE WRITTEN INTO THE PLANES BY HAND rather than drawn with
 * graphics.library's Text(). Text() draws in one pen through the RastPort,
 * and every cell here carries its own colour; setting a pen per cell would
 * mean a SetAPen and a Move and a Text for each of two thousand cells. A cell
 * is 32 bytes of plane data -- eight rows, four planes -- and writing them
 * directly is both simpler and exact. The RastPort is still opened, because
 * the ROM font is read through it.
 *
 * THE ASCII GLYPHS COME OUT OF THE ROM, THE BOX GLYPHS DO NOT. topaz.font 8
 * is definitionally 8x8, always present, and its letters and digits are read
 * straight out of tf_CharData below. The box-drawing characters cannot come
 * from anywhere: the shared layout.h names them by C64 SCREEN CODE (G_HLINE
 * is 64, G_VLINE 93) and topaz has '@' and ']' at those code points. They are
 * this port's own artwork, thirteen glyphs, written out in `box[]` -- the same
 * rule that made tools/make_music.py compose the music rather than extract it.
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include "../../c128/src/vdc.h"

#define SCR_W  640
#define SCR_H  200
#define DEPTH    4
#define BPR     (SCR_W / 8)          /* 80 bytes per plane row */

static struct NewScreen ns = {
    0, 0, SCR_W, SCR_H, DEPTH,
    0, 1,
    HIRES,
    CUSTOMSCREEN,
    NULL, (UBYTE *)"EGA TREK", NULL, NULL
};

/* A BACKDROP WINDOW OVER THE WHOLE SCREEN, and it exists for the KEYBOARD.
   Nothing is drawn through it -- scr_put writes the bitplanes directly -- but
   Intuition delivers keys to a WINDOW's message port and there is no other
   way to be given them. BORDERLESS|BACKDROP so it covers the console without
   a frame, ACTIVATE so it has the focus from the first keystroke, RMBTRAP so
   the right button does not drop a menu over the game. */
static struct NewWindow nw = {
    0, 0, SCR_W, SCR_H,
    0, 1,
    IDCMP_VANILLAKEY | IDCMP_RAWKEY,
    ACTIVATE | BORDERLESS | BACKDROP | RMBTRAP | NOCAREREFRESH,
    NULL, NULL, NULL, NULL, NULL,
    0, 0, 0, 0,
    CUSTOMSCREEN
};

static struct Screen *scr;
static struct Window *win;
static struct TextFont *font;
static UBYTE *plane[DEPTH];

/* For amigainput.c, which needs the window's message port and nothing else. */
struct Window *amiga_window(void) { return win; }

/* EGA's own palette, in the two bits per channel the standard actually uses:
   0x00, 0x55, 0xAA, 0xFF become 0, 5, 10, 15 in the Amiga's four-bit guns
   with no rounding at all. Index order is EGA's, so core/ega.h's names are
   already the palette registers and EGA_TO_VDC stays the identity. */
static const UBYTE ega[16][3] = {
    { 0,  0,  0}, { 0,  0, 10}, { 0, 10,  0}, { 0, 10, 10},
    {10,  0,  0}, {10,  0, 10}, {10,  5,  0}, {10, 10, 10},
    { 5,  5,  5}, { 5,  5, 15}, { 5, 15,  5}, { 5, 15, 15},
    {15,  5,  5}, {15,  5, 15}, {15, 15,  5}, {15, 15, 15}
};

/* THIS PORT'S OWN GLYPHS -- every code the shared UI can pass that topaz
 * does not have. Re-derived from the sources rather than guessed at: a sweep
 * of every scr_put/hline/vline/fill_rect argument in ui.c, layout.c and
 * main.c found FIFTEEN, not the eleven box-drawing ones an eyeball count
 * gives. The two that were missed both live in panels nothing had drawn yet
 * -- the badge's disc and the systems-status bars.
 *
 * WHAT WAS COPIED AND WHAT WAS NOT. The C128 chargen ROM was read to find out
 * what SHAPE each code is meant to be, which is the same thing
 * c128/test/test_panels.c does to check our screen codes. Three of these are
 * pure geometry and can only look one way -- a half block is a half block --
 * so knowing the shape IS knowing the bytes. The rest are drawn here: a line
 * sits on rows 3 and 4 and in columns 3 and 4, which is what makes a vertical
 * meet a horizontal in the middle of a cell and two adjacent cells join, and
 * the disc is this port's own circle rather than Commodore's.
 *
 * REVERSE VIDEO IS A RULE, NOT ENTRIES. Codes 128..255 are their base glyph
 * inverted, so 160 (the block cursor and the badge's solid body) falls out of
 * 32, 226 out of 98, and 228 -- the systems bar -- out of 100. Writing those
 * three out by hand would be three more chances to disagree with the rule. */
struct box_glyph { unsigned char code; unsigned char row[8]; };

static const struct box_glyph box[] = {
  /* 32  space, stated rather than left to the font, so that 160 -- the
         cursor and the badge's body -- is solid even if topaz is missing */
                         { 32, {0,0,0,0,0,0,0,0}},
  /* 64  G_HLINE  ---- */ { 64, {0,0,0,0xFF,0xFF,0,0,0}},
  /* 93  G_VLINE  |    */ { 93, {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},
  /* 112 G_TL     ,-   */ {112, {0,0,0,0x1F,0x1F,0x18,0x18,0x18}},
  /* 110 G_TR     -.   */ {110, {0,0,0,0xF8,0xF8,0x18,0x18,0x18}},
  /* 109 G_BL     `-   */ {109, {0x18,0x18,0x18,0x1F,0x1F,0,0,0}},
  /* 125 G_BR     -'   */ {125, {0x18,0x18,0x18,0xF8,0xF8,0,0,0}},
  /* 107 G_TEE_L  |-   */ {107, {0x18,0x18,0x18,0x1F,0x1F,0x18,0x18,0x18}},
  /* 115 G_TEE_R  -|   */ {115, {0x18,0x18,0x18,0xF8,0xF8,0x18,0x18,0x18}},
  /* 114 G_TEE_D  T    */ {114, {0,0,0,0xFF,0xFF,0x18,0x18,0x18}},
  /* 113 G_TEE_U  _|_  */ {113, {0x18,0x18,0x18,0xFF,0xFF,0,0,0}},
  /* 91  G_CROSS  +    */ { 91, {0x18,0x18,0x18,0xFF,0xFF,0x18,0x18,0x18}},

  /* 98  BADGE_DISC_TOP -- the lower half filled, which rounds the top edge of
         the badge's disc. Geometry: a half block has one shape. Its reverse,
         226, is BADGE_DISC_BOTTOM and needs no entry. */
                         { 98, {0,0,0,0,0xFF,0xFF,0xFF,0xFF}},

  /* 100 the bottom row alone -- and the ONLY reason it is here is that its
         REVERSE is 228, SYS_BAR_GLYPH: seven rows filled on an eight-pixel
         pitch, which is what leaves a hairline between the systems-status
         bars instead of one continuous block. ui.c measured that at "7px bars
         on an 8px pitch" off the original. Geometry again. */
                         {100, {0,0,0,0,0,0,0,0xFF}},

  /* 30, 31 the up and left arrows -- the only two of 27..31 with no ASCII to
         borrow, so they are drawn. Not used by the game as it stands; they
         are here because a screen code with a glyph is one that cannot come
         out as a hollow box later. POUND at 28 is deliberately NOT here: it
         is unused, and the marker is a better answer than an invented glyph
         for a character nobody asks for. */
                         { 30, {0x18,0x3C,0x7E,0x18,0x18,0x18,0x18,0}},
                         { 31, {0,0x10,0x30,0x7E,0x30,0x10,0,0}},

  /* 81  the ship's saucer, drawn HERE and not copied: the badge and the info
         panel both put this next to four cells of G_HLINE and a solid block,
         so what it has to be is a round body that reads as a hull at 8x8.
         Wider than it is tall, clear of the top and bottom rows so it does
         not merge with the cell above or below. */
                         { 81, {0,0x3C,0x7E,0xFF,0xFF,0x7E,0x3C,0}}
};
#define BOX_COUNT ((int)(sizeof box / sizeof box[0]))

/* Screen code -> ASCII, for everything the ROM font can supply. The C64
   unshifted set puts '@' at 0 and A..Z at 1..26, then 32..63 are ASCII
   already. Anything else is a graphic and comes from box[]. */
static int code_to_ascii(unsigned char c) {
    if (c == 0) return '@';
    if (c <= 26) return 'A' + c - 1;
    /* 27..31 ARE [ POUND ] UP-ARROW LEFT-ARROW, and two of them are real: the
       play-again box is drawn as "[YES]" and "[NO]", which scr_puts turns into
       screen codes 27 and 29. Without these that prompt reads as a hollow box
       either side of the word -- caught by the missing-glyph marker firing on
       this port's own test labels, which is the whole reason the marker is
       there. topaz has both brackets, so they come from the ROM exactly. */
    if (c == 27) return '[';
    if (c == 29) return ']';
    if (c >= 32 && c <= 63) return c;
    return -1;
}

/* Eight rows of one topaz glyph. tf_CharData is one wide strip of every
   glyph side by side, tf_Modulo bytes per row; tf_CharLoc gives each glyph's
   bit offset and width in a long. topaz 8 is byte aligned and eight wide, so
   the offset divides cleanly -- checked rather than assumed, and a glyph that
   is not eight wide is skipped rather than drawn at the wrong stride. */
static int rom_glyph(int ch, unsigned char *out) {
    ULONG *loc;
    UWORD offset, width;
    UBYTE *data;
    int r;

    if (!font || ch < font->tf_LoChar || ch > font->tf_HiChar) return 0;
    loc = (ULONG *)font->tf_CharLoc;
    offset = (UWORD)(loc[ch - font->tf_LoChar] >> 16);
    width  = (UWORD)(loc[ch - font->tf_LoChar] & 0xFFFF);
    if (width != 8 || (offset & 7)) return 0;

    data = (UBYTE *)font->tf_CharData + (offset >> 3);
    for (r = 0; r < 8; r++)
        out[r] = (r < font->tf_YSize) ? data[r * font->tf_Modulo] : 0;
    return 1;
}

static void glyph_for(unsigned char code, unsigned char *out) {
    int i, ch, rev = 0;
    unsigned char base = code;

    /* Reverse video is the top bit. Fold it here and every entry below is a
       plain glyph -- 160, 226 and 228 are 32, 98 and 100 inverted. */
    if (base >= 128) { base -= 128; rev = 1; }

    for (i = 0; i < 8; i++) out[i] = 0;

    for (i = 0; i < BOX_COUNT; i++) {
        if (box[i].code == base) {
            for (ch = 0; ch < 8; ch++) out[ch] = box[i].row[ch];
            goto done;
        }
    }
    ch = code_to_ascii(base);
    if (ch >= 0) {
        rom_glyph(ch, out);
        goto done;
    }

    /* A GRAPHICS CODE NOBODY DREW. Every other port hands these to a charset
       that has something at every code; here the set is finite and authored,
       so a code that was missed renders as NOTHING -- invisible, and exactly
       the bug that leaves a panel looking merely empty. Two of the fifteen
       were missed on the first pass, so this is not hypothetical. Draw a
       hollow box instead: it is obvious on screen, it is impossible to
       mistake for game content, and it names the failure the moment a panel
       that has never been drawn before is drawn for the first time. */
    out[0] = 0xFF; out[7] = 0xFF;
    for (i = 1; i < 7; i++) out[i] = 0x81;

done:
    if (rev) for (i = 0; i < 8; i++) out[i] = (unsigned char)~out[i];
}

void vdc_init(void) {
    int i;

    scr = OpenScreen(&ns);
    if (!scr) return;

    for (i = 0; i < 16; i++)
        SetRGB4(&scr->ViewPort, i, ega[i][0], ega[i][1], ega[i][2]);

    /* The title bar would sit over the top row of the console. */
    ShowTitle(scr, FALSE);

    nw.Screen = scr;
    win = OpenWindow(&nw);

    {
        struct TextAttr topaz8;
        topaz8.ta_Name  = (STRPTR)"topaz.font";
        topaz8.ta_YSize = 8;
        topaz8.ta_Style = FS_NORMAL;
        topaz8.ta_Flags = 0;
        font = OpenFont(&topaz8);
    }

    for (i = 0; i < DEPTH; i++)
        plane[i] = (UBYTE *)scr->RastPort.BitMap->Planes[i];

    scr_clear();
}

void vdc_shutdown(void) {
    if (win)  { CloseWindow(win); win = NULL; }
    if (font) { CloseFont(font); font = NULL; }
    if (scr)  { CloseScreen(scr); scr = NULL; }
}

/* Nothing to hand back. AmigaDOS gets the process returned to it the ordinary
   way -- no banking to undo, and no reset needed as on the C128. */
void plat_exit(void) { }

void wait_vsync(void) {
    if (scr) WaitTOF();
}

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color) {
    unsigned char g[8];
    UBYTE *p;
    int r, b;
    unsigned long off;

    if (!scr || x >= VDC_COLS || y >= VDC_ROWS) return;

    glyph_for(ch, g);
    off = (unsigned long)y * 8 * BPR + x;

    for (b = 0; b < DEPTH; b++) {
        p = plane[b] + off;
        if ((color >> b) & 1)
            for (r = 0; r < 8; r++) p[r * BPR] = g[r];
        else
            for (r = 0; r < 8; r++) p[r * BPR] = 0;
    }
}

/* ASCII in, screen codes out -- the same conversion every port does, and the
   same 64..95 and 97..122 branches. scr_put takes a SCREEN code, so a caller
   with a C string has to come through here. */
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color) {
    while (*s && x < VDC_COLS) {
        unsigned char u = (unsigned char)*s++;
        unsigned char c;
        if (u >= 32 && u <= 63)       c = u;
        else if (u >= 64 && u <= 95)  c = (unsigned char)(u - 64);
        else if (u >= 97 && u <= 122) c = (unsigned char)(u - 96);
        else                          c = 32;
        scr_put(x++, y, c, color);
    }
}

void scr_clear(void) {
    unsigned char x, y;
    for (y = 0; y < VDC_ROWS; y++)
        for (x = 0; x < VDC_COLS; x++) scr_put(x, y, 32, 0);
}

void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color) {
    unsigned char r, c;
    for (r = 0; r < h; r++)
        for (c = 0; c < w; c++) scr_put((unsigned char)(x + c), (unsigned char)(y + r), ch, color);
}

void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color) {
    while (w--) scr_put(x++, y, ch, color);
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color) {
    while (h--) scr_put(x, y++, ch, color);
}

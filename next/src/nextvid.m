/* EGA Trek video for the NeXT: a NeXTSTEP application whose window holds
 * the console, drawn into a bitmap of our own and handed to Display
 * PostScript.
 *
 * The seam is c128/src/vdc.h, exactly as on the Mac, the Amiga and the
 * Falcon: an 80x25 grid of cells, each one a glyph plus a foreground colour
 * on black. Like those machines this one has no character generator we can
 * use, so a cell is pixels this file writes. And like the Mac THE SCREEN IS
 * NOT OURS -- the Workspace, the dock and every other application share
 * it -- so the console is a WINDOW.
 *
 * THE SHAPE IS mac/src/macvid.c's, and so are the glyph rules: the box
 * glyphs are the Falcon's 8x16 set, the letters are the machine's own font
 * read once at start-up into 8x16 cells, codes 128-255 are their base glyph
 * reversed, and a code nothing can draw is a loud hollow box. What differs is
 * how pixels reach the screen.
 *
 * THE BITMAP IS 12 BITS A PIXEL, FOUR BITS EACH OF RED, GREEN AND BLUE --
 * the NeXTstation Color's own depth. EGA's levels, $00/$55/$AA/$FF, are
 * exactly 0, 5, 10 and 15 in four bits, so nothing is rounded: EGA's
 * sixteen colours are among the 4096 that machine shows. A cell row is eight
 * pixels, twelve bytes, at a byte offset of x * 12, so a glyph row is one
 * table lookup and a twelve-byte copy. Drawing is NXDrawBitmap of the rows
 * of cells that changed. A NeXT with no colour screen is refused in words
 * (vdc_init), as the Mac refuses a Mac without Color QuickDraw.
 *
 * THE EVENT LOOP LIVES IN wait_vsync(), the trick commodore-uno/mac and
 * commodore-uno/next both use: every port's waiting already goes through it
 * (kb_waitkey calls it between polls), so here it takes the AppKit's events
 * until the next sixtieth of a second and then draws whatever changed. The
 * window is buffered, so a half-drawn console is never seen, and the Window
 * Server repaints it by itself when something that covered it moves away.
 *
 * THE GAME RUNS INSIDE THE APPLICATION. main() below makes the application,
 * its menu and a delegate; when the application has started, the
 * delegate's appDidInit: calls the shared game's main() -- compiled as
 * trek_main() (Makefile.next) -- which returns only by ending the program.
 */
#import <appkit/appkit.h>
#import <objc/NXBundle.h>
#import <sys/time.h>
#import <stdlib.h>
#import <string.h>
#import <stdio.h>

#include "../../c128/src/vdc.h"
#include "../../c128/src/sid.h"
#include "nextvid.h"

int trek_main(void);   /* c128/src/main.c's main(), renamed by Makefile.next */

#define CELL_W   8
#define CELL_H   16
#define CON_W    (VDC_COLS * CELL_W)    /* 640 */
#define CON_H    (VDC_ROWS * CELL_H)    /* 400 */
#define CELL_BYTES 12                   /* eight 12-bit pixels */
#define ROW_BYTES  (VDC_COLS * CELL_BYTES)   /* 960 */

/* ---------------------------------------------------------------- colours */

/* EGA's palette in four bits a gun: $55 is 5, $AA is 10, $FF is 15. */
static const unsigned char ega4[16][3] = {
    { 0, 0, 0}, { 0, 0,10}, { 0,10, 0}, { 0,10,10},
    {10, 0, 0}, {10, 0,10}, {10, 5, 0}, {10,10,10},
    { 5, 5, 5}, { 5, 5,15}, { 5,15, 5}, { 5,15,15},
    {15, 5, 5}, {15, 5,15}, {15,15, 5}, {15,15,15}
};

/* ---------------------------------------------------------------- glyphs */

/* THIS PORT'S OWN GLYPHS, at 8x16: every code the shared UI can pass that
   a font does not have. They are the Falcon's set, restated here as the
   Mac restates it (mac/src/macvid.c says why: a line sits on rows 7 and 8
   and in columns 3 and 4, which is what makes a vertical meet a
   horizontal). */
#define HL 0xFF
#define VL 0x18

struct box_glyph { unsigned char code; unsigned char row[CELL_H]; };

static const struct box_glyph box[] = {
  { 32, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
  { 64, {0,0,0,0,0,0,0,HL,HL,0,0,0,0,0,0,0}},                   /* G_HLINE */
  { 93, {VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL}},     /* G_VLINE */
  {112, {0,0,0,0,0,0,0,0x1F,0x1F,VL,VL,VL,VL,VL,VL,VL}},       /* G_TL    */
  {110, {0,0,0,0,0,0,0,0xF8,0xF8,VL,VL,VL,VL,VL,VL,VL}},       /* G_TR    */
  {109, {VL,VL,VL,VL,VL,VL,VL,0x1F,0x1F,0,0,0,0,0,0,0}},       /* G_BL    */
  {125, {VL,VL,VL,VL,VL,VL,VL,0xF8,0xF8,0,0,0,0,0,0,0}},       /* G_BR    */
  {107, {VL,VL,VL,VL,VL,VL,VL,0x1F,0x1F,VL,VL,VL,VL,VL,VL,VL}},/* G_TEE_L */
  {115, {VL,VL,VL,VL,VL,VL,VL,0xF8,0xF8,VL,VL,VL,VL,VL,VL,VL}},/* G_TEE_R */
  {114, {0,0,0,0,0,0,0,HL,HL,VL,VL,VL,VL,VL,VL,VL}},           /* G_TEE_D */
  {113, {VL,VL,VL,VL,VL,VL,VL,HL,HL,0,0,0,0,0,0,0}},           /* G_TEE_U */
  { 91, {VL,VL,VL,VL,VL,VL,VL,HL,HL,VL,VL,VL,VL,VL,VL,VL}},    /* G_CROSS */
  { 98, {0,0,0,0,0,0,0,0,HL,HL,HL,HL,HL,HL,HL,HL}},            /* disc top */
  {100, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,HL}},       /* reversed: systems bar */
  { 30, {0,0,0x18,0x3C,0x7E,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0,0,0}},
  { 31, {0,0,0,0x10,0x30,0x7E,0xFF,0xFF,0x7E,0x30,0x10,0,0,0,0,0}},
  { 81, {0,0,0,0,0x3C,0x7E,0xFF,0xFF,0xFF,0x7E,0x3C,0,0,0,0,0}}, /* saucer */
};
#define NBOX ((int)(sizeof box / sizeof box[0]))

/* The font's printable ASCII, 32..126, captured at start-up. */
static unsigned char font[95][CELL_H];
static unsigned char have_font;

/* THE FONT. Courier, the one fixed-pitch face every NeXT has, at the size
   whose advance is eight pixels or just under. EGATREK_FONT="Name size" in
   the environment overrides it, for trying another face without a rebuild. */
#define FONT_NAME "Courier-Bold"
#define FONT_SIZE 13.0

/* One sample of a bitmap row, any depth: `bps` bits at pixel x. */
static unsigned sample(const unsigned char *row, int x, int bpp, int bps)
{
    long bit = (long)x * bpp;
    unsigned v = 0;
    int i;
    for (i = 0; i < bps; i++, bit++)
        v = (v << 1) | ((row[bit >> 3] >> (7 - (bit & 7))) & 1);
    return v;
}

/* Draws each character white on black into a small image, reads the image
   back, and keeps one bit a pixel -- the Mac's capture_font(), in Display
   PostScript. The font is the machine's; none ships here. */
static void capture_font(void)
{
    char name[64];
    float size = FONT_SIZE;
    const char *env = getenv("EGATREK_FONT");
    NXSize sz;
    NXRect r;
    NXCoord asc, desc, lh;
    id img, fnt;
    NXBitmapImageRep *rep;
    int c, y, x, top, base;

    strcpy(name, FONT_NAME);
    if (env && strlen(env) < sizeof name) {
        char *sp;
        strcpy(name, env);
        sp = strchr(name, ' ');
        if (sp) { *sp = 0; size = (float)atof(sp + 1); }
    }
    fnt = [Font newFont:name size:size];
    if (!fnt) return;
    NXTextFontInfo(fnt, &asc, &desc, &lh);
    if (desc < 0) desc = -desc;
    /* Centre the font's own height in the 16-line cell; the baseline is
       measured from the TOP of the cell, as the bitmap reads. */
    top = (int)((CELL_H - (asc + desc)) / 2.0 + 0.5);
    if (top < 0) top = 0;
    base = top + (int)(asc + 0.5);
    if (base > CELL_H - 1) base = CELL_H - 1;

    sz.width = CELL_W;
    sz.height = CELL_H;
    img = [[NXImage alloc] initSize:&sz];
    NXSetRect(&r, 0.0, 0.0, CELL_W, CELL_H);
    for (c = 32; c <= 126; c++) {
        char s[2];
        int bps, bpp, bpr, max, invert;
        unsigned char *data;

        s[0] = (char)c;
        s[1] = 0;
        if (![img lockFocus]) return;
        PSsetgray(NX_BLACK);
        NXRectFill(&r);
        PSsetgray(NX_WHITE);
        [fnt set];
        PSmoveto(0.0, (float)(CELL_H - base));   /* PostScript's y is up */
        PSshow(s);
        rep = [[NXBitmapImageRep alloc] initData:NULL fromRect:&r];
        [img unlockFocus];
        if (!rep) return;
        bps = [rep bitsPerSample];
        bpp = [rep bitsPerPixel];
        bpr = [rep bytesPerRow];
        data = [rep data];
        max = (1 << bps) - 1;
        invert = [rep colorSpace] == NX_OneIsBlackColorSpace;
        for (y = 0; y < CELL_H; y++) {
            unsigned char bits = 0;
            for (x = 0; x < CELL_W; x++) {
                unsigned v = sample(data + (long)y * bpr, x, bpp, bps);
                int on = invert ? (v * 2 < (unsigned)max) : (v * 2 > (unsigned)max);
                if (on) bits |= (unsigned char)(0x80 >> x);
            }
            /* THE IMAGE READS BACK BOTTOM ROW FIRST -- measured: the first
               build kept the rows as they came and every letter stood on
               its head -- so the first row read is the cell's last. */
            font[c - 32][CELL_H - 1 - y] = bits;
        }
        [rep free];
    }
    [img free];
    have_font = 1;
}

/* SCREEN CODE -> ASCII, for everything the font can supply. scr_put takes a
   C128 SCREEN CODE on every port, not a character. */
static int code_to_ascii(unsigned char c)
{
    if (c == 0) return '@';
    if (c <= 26) return 'A' + c - 1;
    if (c == 27) return '[';
    if (c == 29) return ']';
    if (c >= 32 && c <= 63) return c;
    return -1;
}

/* Code -> 16 rows of bits. Codes 128..255 are their base glyph REVERSED. */
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
    if (a >= 32 && have_font) {
        for (j = 0; j < CELL_H; j++)
            out[j] = rev ? (unsigned char)~font[a - 32][j] : font[a - 32][j];
        return;
    }

    /* THE MISSING-GLYPH MARKER, loud on purpose: a hollow box. */
    for (j = 0; j < CELL_H; j++)
        out[j] = (j == 0 || j == CELL_H - 1) ? 0xFF : 0x81;
    if (rev)
        for (j = 0; j < CELL_H; j++)
            out[j] = (unsigned char)~out[j];
}

/* One glyph row's eight bits in one colour -> twelve bytes of pixels, every
   colour and every byte, built once (48K). Two pixels are three bytes:
   R0G0 B0R1 G1B1. */
static unsigned char expand[16][256][CELL_BYTES];

static void build_expand(void)
{
    int c, b, i;
    for (c = 0; c < 16; c++)
        for (b = 0; b < 256; b++) {
            unsigned char nib[24];
            for (i = 0; i < 8; i++) {
                int on = (b & (0x80 >> i)) != 0;
                nib[i * 3 + 0] = on ? ega4[c][0] : 0;
                nib[i * 3 + 1] = on ? ega4[c][1] : 0;
                nib[i * 3 + 2] = on ? ega4[c][2] : 0;
            }
            for (i = 0; i < CELL_BYTES; i++)
                expand[c][b][i] = (unsigned char)((nib[2 * i] << 4) | nib[2 * i + 1]);
        }
}

/* ---------------------------------------------------------------- the window */

static unsigned char pixels[CON_H * ROW_BYTES];      /* 384,000 bytes */
static short dirty_top = VDC_ROWS, dirty_bot = 0;     /* cell rows */
static id win, view;
static char app_dir[1024], save_dir[1024];

#define KQ 32
static int keyq[KQ];
static unsigned char kq_head, kq_tail;

static void push_key(int k)
{
    unsigned char next = (unsigned char)((kq_tail + 1) % KQ);
    if (next == kq_head) return;                    /* full: drop */
    keyq[kq_tail] = k;
    kq_tail = next;
}

int next_next_key(void)
{
    int k;
    if (kq_head == kq_tail) return 0;
    k = keyq[kq_head];
    kq_head = (unsigned char)((kq_head + 1) % KQ);
    return k;
}

void next_flush_keys(void) { kq_head = kq_tail; }

const char *next_app_dir(void) { return app_dir; }
const char *next_save_dir(void) { return save_dir; }

/* Cell rows top..bottom-1 of the bitmap onto the view, which is NOT
   flipped: PostScript's y runs up, so row 0 is at the top of the view,
   CON_H - 16 up from its origin. The bitmap's first row is its top. */
static void draw_rows(short top, short bottom)
{
    NXRect r;
    const unsigned char *planes[5];

    if (top >= bottom) return;
    planes[0] = pixels + (long)top * CELL_H * ROW_BYTES;
    planes[1] = planes[2] = planes[3] = planes[4] = NULL;
    NXSetRect(&r, 0.0, (NXCoord)(CON_H - bottom * CELL_H),
              (NXCoord)CON_W, (NXCoord)((bottom - top) * CELL_H));
    NXDrawBitmap(&r, CON_W, (bottom - top) * CELL_H, 4, 3, 12, ROW_BYTES,
                 NO, NO, NX_RGBColorSpace, planes);
}

static void present(void)
{
    if (!view || dirty_top >= dirty_bot) return;
    [view lockFocus];
    draw_rows(dirty_top, dirty_bot);
    [view unlockFocus];
    [win flushWindow];
    NXPing();
    dirty_top = VDC_ROWS;
    dirty_bot = 0;
}

@interface TrekView : View
@end

@implementation TrekView

- (BOOL)acceptsFirstResponder { return YES; }

/* The whole console, whenever the AppKit asks for it. */
- drawSelf:(const NXRect *)rects :(int)count
{
    draw_rows(0, VDC_ROWS);
    return self;
}

- keyDown:(NXEvent *)e
{
    int c = e->data.key.charCode;

    /* EGATREK_KEYLOG in the environment lists every key-down on stderr:
       the instrument for a keyboard this port has not met. */
    if (getenv("EGATREK_KEYLOG"))
        fprintf(stderr, "key set %d code %d flags %x\n",
                e->data.key.charSet, c, (unsigned)e->flags);
    if (e->flags & NX_COMMANDMASK) return [super keyDown:e];
    if (e->data.key.charSet == NX_SYMBOLSET) {
        /* The arrow keys come in the symbol set, not as ASCII. */
        switch (c) {
        case 0xAC: push_key(NEXT_KEY_LEFT); break;
        case 0xAD: push_key(NEXT_KEY_UP); break;
        case 0xAE: push_key(NEXT_KEY_RIGHT); break;
        case 0xAF: push_key(NEXT_KEY_DOWN); break;
        default: break;
        }
        return self;
    }
    if (c) push_key(c);
    return self;
}

@end

@interface TrekDelegate : Object
@end

@implementation TrekDelegate

- appDidInit:sender
{
    trek_main();
    return self;            /* never reached: the game ends the program */
}

/* Info > Info Panel... THE CREDIT COMES FIRST, as on the title screen of
   every port: the game is Nels Anderson's, and this is a port of it. */
- showInfo:sender
{
    NXRunAlertPanel("EGA Trek",
        "EGA Trek was written by Nels Anderson and released as shareware "
        "between 1988 and 1992.\n\n"
        "This is an independent port of his game to the NeXT. The original "
        "is at archive.org/details/EGATrek",
        "OK", NULL, NULL);
    return self;
}

/* QUIT FROM THE MENU, or closing the window, leaves an unsaved game, as
   quitting does on the Mac; the game's own way out is at the command line.
   The sound stops first. */
- quit:sender
{
    snd_off();
    exit(0);
    return self;
}

- windowWillClose:sender
{
    return [self quit:sender];
}

@end

int main(int argc, char *argv[])
{
    id menu, info, delegate;

    NXApp = [Application new];
    delegate = [[TrekDelegate alloc] init];
    [NXApp setDelegate:delegate];

    /* The NeXT's own order: Info first, Hide and Quit last. */
    menu = [[Menu alloc] initTitle:"EGA Trek"];
    info = [[Menu alloc] initTitle:"Info"];
    [[info addItem:"Info Panel..." action:@selector(showInfo:) keyEquivalent:'\0']
        setTarget:delegate];
    [info sizeToFit];
    [menu setSubmenu:info
             forItem:[menu addItem:"Info" action:@selector(submenuAction:)
                       keyEquivalent:'\0']];
    [menu addItem:"Hide" action:@selector(hide:) keyEquivalent:'h'];
    [[menu addItem:"Quit" action:@selector(quit:) keyEquivalent:'q']
        setTarget:delegate];
    [menu sizeToFit];
    [NXApp setMainMenu:menu];

    [NXApp run];
    [NXApp free];
    return 0;
}

/* ---------------------------------------------------------------- the clock */

static struct timeval t0;

/* Seconds since vdc_init(), to the microsecond. */
static double secs(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)(tv.tv_sec - t0.tv_sec) +
           (double)(tv.tv_usec - t0.tv_usec) / 1000000.0;
}

unsigned long next_ticks(void) { return (unsigned long)(secs() * 60.0); }

/* Takes events until the next sixtieth of a second, then draws whatever
   changed. Key-downs go to the view, which queues them; menu commands, the
   Info panel, Hide and window moves all happen in sendEvent:. */
void wait_vsync(void)
{
    static double next;
    double t = secs();
    NXEvent *e;

    if (next < t - 0.1) next = t;               /* fell behind: catch up */
    next += 1.0 / 60.0;
    for (;;) {
        double wait = next - secs();
        if (wait < 0.0) wait = 0.0;
        snd_poll();                 /* the sound stream must never run dry */
        if (wait > 0.01) wait = 0.01;
        e = [NXApp getNextEvent:NX_ALLEVENTS waitFor:wait
                      threshold:NX_BASETHRESHOLD];
        if (e) [NXApp sendEvent:e];
        else if (secs() >= next) break;
    }
    present();
}

/* ---------------------------------------------------------------- the seam */

void vdc_init(void)
{
    NXRect r;
    const char *dir;
    char *slash;
    /* THE COLOUR SCREEN, not the main one: on a NeXTcube with a
       NeXTdimension board the main screen is the mono MegaPixel and the
       colour one is the second, so that is where the console goes. */
    const NXScreen *scr = [NXApp colorScreen];

    gettimeofday(&t0, NULL);

    /* COLOUR NEXTS ONLY, by Jamie's decision (2026-11-27), as on every EGA
       Trek port: the console's departments speak in EGA's sixteen colours,
       and in four greys they would not be told apart. A NeXT without a
       colour screen -- a NeXT Computer, a NeXTcube, a mono NeXTstation -- is
       told so in words, and the program ends, the Mac's rule. */
    if (!scr ||
        NXNumberOfColorComponents(NXColorSpaceFromDepth(scr->depth)) < 3) {
        NXRunAlertPanel("EGA Trek",
            "EGA Trek needs a colour NeXT: a NeXTstation Color or Turbo "
            "Color, or a NeXTdimension board. This NeXT has no colour screen.",
            "Quit", NULL, NULL);
        exit(0);
    }

    dir = [[NXBundle mainBundle] directory];
    strncpy(app_dir, dir ? dir : ".", sizeof app_dir - 1);
    strcpy(save_dir, app_dir);
    slash = strrchr(save_dir, '/');
    if (slash && slash != save_dir) *slash = 0;
    else if (slash) slash[1] = 0;
    else strcpy(save_dir, ".");

    build_expand();
    capture_font();

    /* Centred on the colour screen, a little above the middle. */
    NXSetRect(&r, 0.0, 0.0, CON_W, CON_H);
    r.origin.x = (NXCoord)(int)(scr->screenBounds.origin.x +
                                (scr->screenBounds.size.width - CON_W) / 2);
    r.origin.y = (NXCoord)(int)(scr->screenBounds.origin.y +
                                (scr->screenBounds.size.height - CON_H) * 0.6);
    win = [[Window alloc] initContent:&r
                                style:NX_TITLEDSTYLE
                              backing:NX_BUFFERED
                           buttonMask:NX_MINIATURIZEBUTTONMASK | NX_CLOSEBUTTONMASK
                                defer:NO];
    [win setTitle:"EGA Trek"];
    [win setDelegate:[NXApp delegate]];
    NXSetRect(&r, 0.0, 0.0, CON_W, CON_H);
    view = [[TrekView alloc] initFrame:&r];
    [win setContentView:view];
    [win makeFirstResponder:view];
    [win display];                      /* the title bar, and a black console */
    [win makeKeyAndOrderFront:nil];
    [NXApp activateSelf:YES];
    wait_vsync();
}

/* Deliberately does NOT clear: the farewell has to survive it. */
void vdc_shutdown(void) { present(); }

/* An application ends; the Workspace takes over. */
void plat_exit(void)
{
    snd_off();
    exit(0);
}

void scr_clear(void)
{
    memset(pixels, 0, sizeof pixels);
    dirty_top = 0;
    dirty_bot = VDC_ROWS;
}

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color)
{
    unsigned char rows[CELL_H];
    unsigned char *p;
    int r;

    if (x >= VDC_COLS || y >= VDC_ROWS)
        return;

    glyph_rows(ch, rows);
    color &= 15;
    p = pixels + (long)y * CELL_H * ROW_BYTES + (long)x * CELL_BYTES;
    for (r = 0; r < CELL_H; r++, p += ROW_BYTES)
        memcpy(p, expand[color][rows[r]], CELL_BYTES);

    if (y < dirty_top) dirty_top = y;
    if (y + 1 > dirty_bot) dirty_bot = (short)(y + 1);
}

/* ASCII IN, SCREEN CODES OUT -- the same conversion every port does. */
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color)
{
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
    for (i = 0; i < w; i++)
        scr_put((unsigned char)(x + i), y, ch, color);
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color)
{
    unsigned char i;
    for (i = 0; i < h; i++)
        scr_put(x, (unsigned char)(y + i), ch, color);
}

/* THE MESSAGE LOG'S BACKING STORE: a plain array, as on the Falcon and the
   Mac. Sized from ui.c's own constants: LOG_BASE $1000 and 32 slots of 64
   bytes. */
#define LOG_ORIGIN  0x1000
#define LOG_BYTES   (32 * 64)

static unsigned char logstore[LOG_BYTES];
static unsigned int  log_cursor;

void vdc_set_address(unsigned int addr)
{
    log_cursor = (addr >= LOG_ORIGIN) ? (addr - LOG_ORIGIN) : 0;
}

void vdc_data_write(unsigned char value)
{
    if (log_cursor < LOG_BYTES) logstore[log_cursor] = value;
    log_cursor++;
}

unsigned char vdc_data_read(void)
{
    unsigned char v = (log_cursor < LOG_BYTES) ? logstore[log_cursor] : 0;
    log_cursor++;
    return v;
}

/* C128-only: that machine's CRTC registers. */
unsigned char vdc_reg_read(unsigned char reg) { (void)reg; return 0; }
void vdc_reg_write(unsigned char reg, unsigned char value) { (void)reg; (void)value; }

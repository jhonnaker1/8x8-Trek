/* EGA Trek video for the colour Macintosh: a Toolbox window holding the
 * console, drawn offscreen and copied to the screen by QuickDraw.
 *
 * The seam is c128/src/vdc.h, exactly as on the Amiga and the Falcon: an
 * 80x25 grid of cells, each one a glyph plus a foreground colour on black.
 * Like those two machines this one has no character generator, so a cell is
 * pixels this file writes. Unlike them THE SCREEN IS NOT OURS -- this is an
 * application under the Finder, sharing the display with the menu bar, desk
 * accessories and whatever else is open -- so the console is a WINDOW, and
 * what this file draws into is an offscreen GWorld that QuickDraw copies
 * onto it.
 *
 * 80x25 OF 8x16 IS THE FALCON'S GEOMETRY, 640x400, and a 13-inch colour Mac
 * is 640x480: 20 lines of menu bar and 19 of title bar leave 441, so the
 * console fits with a margin. 8x8 cells (the Amiga's) would make a picture
 * half the height of the screen. The box-drawing glyphs are therefore the
 * Falcon's 8x16 set, restated below.
 *
 * THE OFFSCREEN IS FOUR BITS DEEP AND ITS COLOUR TABLE IS EGA'S. A pixel
 * value IS an EGA colour index (TREK_COLOUR_IS_EGA), and CopyBits translates
 * to whatever the screen is -- 16 or 256 colours through the CLUT, thousands
 * or millions directly. A Palette Manager palette of the same sixteen
 * colours, tolerance zero, asks for EGA's exact colours in the screen's CLUT
 * while the window is in front, so brown stays brown on an 8-bit screen
 * rather than becoming the nearest of Apple's 256.
 *
 * A cell row is FOUR BYTES -- eight 4-bit pixels -- at a byte offset of
 * x * 4, so every write is aligned for the 68000 and there is no plane
 * arithmetic at all. The Falcon's driver spends its header on word
 * interleave; this one has none.
 *
 * THE EVENT LOOP LIVES IN wait_vsync(), the trick commodore-uno/mac found:
 * every port's waiting already goes through it (kb_waitkey calls it between
 * polls), so here it runs the Toolbox event loop until the 1/60 s tick
 * changes -- keys, menus, window drags, desk accessories, update events --
 * and then copies whatever cells changed. The shared game never knows it is
 * running under a cooperative GUI.
 *
 * THE TEXT IS THE MAC'S OWN MONACO, read once at start-up by drawing each
 * character into a 1-bit cell and keeping the bits -- the same principle as
 * the Falcon reading its 8x16 font out of ROM and the Atari 8-bit copying
 * the OS font: the machine's glyphs stay the machine's, and none ship here.
 */
#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>
#include <OSUtils.h>
#include <ToolUtils.h>
#include <Memory.h>
#include <SegLoad.h>
#include <Gestalt.h>
#include <string.h>

#include "../../c128/src/vdc.h"
#include "../../c128/src/sid.h"
#include "macvid.h"

#define GW_KEEP_LOCAL 8   /* GWorldFlags keepLocal */

/* Low memory MMU32Bit: nonzero when the Mac runs in 32-bit mode. Read through
   a volatile POINTER because gcc -O2 refuses a constant low-memory address
   (-Werror=array-bounds, the LMGetCurApRefNum trap); GetMMUMode has no glue. */
static volatile unsigned char *volatile mmu32bit = (volatile unsigned char *)0x0CB2;
#define CELL_W   8
#define CELL_H   16
#define CON_W    (VDC_COLS * CELL_W)   /* 640 */
#define CON_H    (VDC_ROWS * CELL_H)   /* 400 */
#define MENU_H   20
#define TITLE_H  19

static struct mac_facts facts;

const struct mac_facts *mac_get_facts(void) { return &facts; }

/* ---------------------------------------------------------------- colours */

/* EGA's palette, sixteen bits a gun. EGA's own levels are $00/$55/$AA/$FF,
   and x * 257 puts each exactly at the top of its QuickDraw range -- the
   same values the Falcon writes as eight bits, with no rounding. */
static const unsigned char ega_rgb[16][3] = {
    {0x00,0x00,0x00}, {0x00,0x00,0xAA}, {0x00,0xAA,0x00}, {0x00,0xAA,0xAA},
    {0xAA,0x00,0x00}, {0xAA,0x00,0xAA}, {0xAA,0x55,0x00}, {0xAA,0xAA,0xAA},
    {0x55,0x55,0x55}, {0x55,0x55,0xFF}, {0x55,0xFF,0x55}, {0x55,0xFF,0xFF},
    {0xFF,0x55,0x55}, {0xFF,0x55,0xFF}, {0xFF,0xFF,0x55}, {0xFF,0xFF,0xFF}
};

static CTabHandle ega_ctab(void)
{
    CTabHandle ct = (CTabHandle)NewHandleClear(
        (long)sizeof(ColorTable) + 15L * (long)sizeof(ColorSpec));
    int i;

    if (!ct) return NULL;
    (**ct).ctSeed = GetCTSeed();
    (**ct).ctFlags = 0;
    (**ct).ctSize = 15;                     /* entries minus one */
    for (i = 0; i < 16; i++) {
        (**ct).ctTable[i].value = (short)i;
        (**ct).ctTable[i].rgb.red   = (unsigned short)(ega_rgb[i][0] * 257);
        (**ct).ctTable[i].rgb.green = (unsigned short)(ega_rgb[i][1] * 257);
        (**ct).ctTable[i].rgb.blue  = (unsigned short)(ega_rgb[i][2] * 257);
    }
    return ct;
}

/* ---------------------------------------------------------------- glyphs */

/* THIS PORT'S OWN GLYPHS, at 8x16 -- every code the shared UI can pass that
 * the Mac's font does not have. They are the Falcon's (falcon/src/
 * falconvid.c), whose header says why they are drawn for 8x16 rather than
 * doubled from 8x8: a line sits on rows 7 and 8 and in columns 3 and 4, which
 * is what makes a vertical meet a horizontal and two adjacent cells join.
 * The set itself was swept off the shared sources on the Amiga -- fifteen
 * codes where an eyeball count gives eleven. Restated rather than shared
 * because that file is vbcc's and this one gcc's, and the table is data. */
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

/* Monaco's printable ASCII, 32..126, captured at start-up. */
static unsigned char font[95][CELL_H];
static unsigned char have_font;

static void capture_font(void)
{
    GrafPort port;
    BitMap bm;
    FontInfo fi;
    GrafPtr old;
    short top, c, r;

    bm.rowBytes = 2;
    bm.baseAddr = NewPtrClear(2L * CELL_H);
    if (!bm.baseAddr) return;
    SetRect(&bm.bounds, 0, 0, 16, CELL_H);

    GetPort(&old);
    OpenPort(&port);
    SetPortBits(&bm);
    PortSize(16, CELL_H);
    RectRgn(port.visRgn, &bm.bounds);
    ClipRect(&bm.bounds);
    TextFont(kFontIDMonaco);
    TextSize(12);
    TextFace(0);
    TextMode(srcOr);
    GetFontInfo(&fi);
    facts.font_ascent  = fi.ascent;
    facts.font_descent = fi.descent;
    facts.font_widmax  = fi.widMax;
    facts.font_leading = fi.leading;

    /* Centre the font's own height in the 16-line cell. */
    top = (short)((CELL_H - (fi.ascent + fi.descent)) / 2);
    if (top < 0) top = 0;

    for (c = 32; c <= 126; c++) {
        EraseRect(&bm.bounds);
        MoveTo(0, (short)(top + fi.ascent));
        DrawChar((char)c);
        for (r = 0; r < CELL_H; r++)
            font[c - 32][r] = ((unsigned char *)bm.baseAddr)[r * 2];
    }
    ClosePort(&port);
    SetPort(old);
    DisposePtr(bm.baseAddr);
    have_font = 1;
}

/* SCREEN CODE -> ASCII, for everything the font can supply. scr_put takes a
   C128 SCREEN CODE, not a character, on every port; the Falcon's first build
   drew the Q of BRIEFING as the ship's saucer by forgetting it. */
static int code_to_ascii(unsigned char c)
{
    if (c == 0) return '@';
    if (c <= 26) return 'A' + c - 1;
    if (c == 27) return '[';
    if (c == 29) return ']';
    if (c >= 32 && c <= 63) return c;
    return -1;
}

/* Code -> 16 rows of bits. Codes 128..255 are their base glyph REVERSED, a
   rule rather than a table, exactly as on the Falcon. */
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

    /* THE MISSING-GLYPH MARKER, loud on purpose: a hollow box, which is
       visible and is not a letter. On the Amiga it caught two brackets. */
    for (j = 0; j < CELL_H; j++)
        out[j] = (j == 0 || j == CELL_H - 1) ? 0xFF : 0x81;
    if (rev)
        for (j = 0; j < CELL_H; j++)
            out[j] = (unsigned char)~out[j];
}

/* One glyph row's eight bits -> eight 4-bit pixels in one long: a set bit
   becomes a nibble of $F, then the colour is ANDed in. Built once. */
static unsigned long expand[256];

static void build_expand(void)
{
    int b, i;
    for (b = 0; b < 256; b++) {
        unsigned long m = 0;
        for (i = 0; i < 8; i++)
            if (b & (0x80 >> i))
                m |= 0xFUL << (28 - 4 * i);
        expand[b] = m;
    }
}

/* ---------------------------------------------------------------- window */

enum { MENU_APPLE = 128, MENU_FILE = 129, MENU_EDIT = 130 };

static WindowPtr win;
static GWorldPtr off;
static PixMapHandle off_pm;
static unsigned char *off_base;
static long off_rowbytes;
static PaletteHandle pal;
static short dirty_top = VDC_ROWS, dirty_bot = 0;   /* cell rows; empty if top >= bot */

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

int mac_next_key(void)
{
    int k;
    if (kq_head == kq_tail) return 0;
    k = keyq[kq_head];
    kq_head = (unsigned char)((kq_head + 1) % KQ);
    return k;
}

void mac_flush_keys(void) { kq_head = kq_tail; }

static void copy_rows(short top, short bottom)
{
    Rect r;
    GrafPtr old;

    if (!win || !off || top >= bottom) return;
    SetRect(&r, 0, (short)(top * CELL_H), CON_W, (short)(bottom * CELL_H));
    GetPort(&old);
    SetPort(win);
    ForeColor(blackColor);                  /* so CopyBits does not colourise */
    BackColor(whiteColor);
    CopyBits((BitMap *)*off_pm, &win->portBits, &r, &r, srcCopy, NULL);
    SetPort(old);
}

#ifdef MAC_MEMPROBE
/* Built with -DMAC_MEMPROBE (by hand, into scratch): the window title shows the heap's LOWEST free
   space seen so far and its largest block, so the SIZE resource's partition
   is set from a measurement rather than a guess. Never in a release. */
static long mem_low = 0x7FFFFFFFL;
static void mem_title(void)
{
    static unsigned long last;
    unsigned char t[64];
    long f = FreeMem(), m = MaxBlock(), v;
    int i, n;
    char num[12];

    if (f < mem_low) mem_low = f;
    if (TickCount() - last < 60) return;
    last = TickCount();
    t[0] = 0;
    {
        static const char *lab[2] = { "low free ", "  max block " };
        long val[2];
        int k;
        val[0] = mem_low; val[1] = m;
        for (k = 0; k < 2; k++) {
            for (i = 0; lab[k][i]; i++) t[++t[0]] = (unsigned char)lab[k][i];
            v = val[k]; n = 0;
            do { num[n++] = (char)('0' + v % 10); v /= 10; } while (v);
            while (n) t[++t[0]] = (unsigned char)num[--n];
        }
    }
    SetWTitle(win, t);
}
#endif

static void present(void)
{
    copy_rows(dirty_top, dirty_bot);
    dirty_top = VDC_ROWS;
    dirty_bot = 0;
#ifdef MAC_MEMPROBE
    mem_title();
#endif
}

static void setup_menus(void)
{
    MenuHandle m;

    m = NewMenu(MENU_APPLE, (ConstStringPtr)"\p\024");   /* the Apple glyph */
    AppendMenu(m, (ConstStringPtr)"\pAbout EGA Trek...;(-");
    AppendResMenu(m, 'DRVR');                            /* desk accessories */
    InsertMenu(m, 0);
    m = NewMenu(MENU_FILE, (ConstStringPtr)"\pFile");
    AppendMenu(m, (ConstStringPtr)"\pQuit/Q");
    InsertMenu(m, 0);
    /* For desk accessories, which get their commands through SystemEdit. */
    m = NewMenu(MENU_EDIT, (ConstStringPtr)"\pEdit");
    AppendMenu(m, (ConstStringPtr)"\pUndo/Z;(-;Cut/X;Copy/C;Paste/V;Clear");
    InsertMenu(m, 0);
    DrawMenuBar();
}

/* ---- About box: a modal window drawn in code (no DLOG/DITL resources),
   dismissed by a click or a key, as commodore-uno/mac does it. Its own
   small event loop keeps answering the console's update events. THE CREDIT
   COMES FIRST, as on the title screen of every port: the game is Nels
   Anderson's, and this is a port of it. */
#define ABOUT_W 400
#define ABOUT_H 196

static void about_line(short y, short size, short face, const char *s)
{
    short n = (short)strlen(s);
    TextFont(systemFont);
    TextSize(size);
    TextFace(face);
    MoveTo((short)((ABOUT_W - TextWidth((Ptr)s, 0, n)) / 2), y);
    DrawText((Ptr)s, 0, n);
}

static void about_draw(void)
{
    about_line(36, 24, bold, "EGA Trek");
    about_line(64, 12, 0, "Written by Nels Anderson and released");
    about_line(80, 12, 0, "as shareware between 1988 and 1992.");
    about_line(106, 12, 0, "This is an independent port of his game");
    about_line(122, 12, 0, "to the Macintosh. The original is at");
    about_line(138, 12, 0, "archive.org/details/EGATrek");
    about_line(178, 12, bold, "Click to continue");
}

static void about_box(void)
{
    Rect r, sb = qd.screenBits.bounds;
    WindowPtr box;
    GrafPtr old;
    EventRecord e;
    short left = (short)((sb.right - ABOUT_W) / 2);
    short top = (short)((sb.bottom - ABOUT_H) / 2 + 10);

    GetPort(&old);
    SetRect(&r, left, top, (short)(left + ABOUT_W), (short)(top + ABOUT_H));
    box = NewWindow(NULL, &r, (ConstStringPtr)"\p", true, dBoxProc,
                    (WindowPtr)-1, false, 0);
    SetPort(box);
    about_draw();
    for (;;) {
        SystemTask();
        if (!GetNextEvent(everyEvent, &e)) continue;
        if (e.what == mouseDown || e.what == keyDown) break;
        if (e.what == updateEvt) {
            WindowPtr w = (WindowPtr)e.message;
            BeginUpdate(w);
            if (w == box) { SetPort(box); about_draw(); }
            else if (w == win) copy_rows(0, VDC_ROWS);
            EndUpdate(w);
        }
    }
    DisposeWindow(box);         /* the console gets an update for the hole */
    SetPort(old);
}

static void do_menu(long choice)
{
    short menu = (short)(choice >> 16), item = (short)(choice & 0xFFFF);

    HiliteMenu(0);
    if (menu == MENU_APPLE) {
        if (item > 2) {
            Str255 name;
            GrafPtr old;
            GetPort(&old);
            GetMenuItemText(GetMenuHandle(MENU_APPLE), item, name);
            OpenDeskAcc(name);
            SetPort(old);
        }
        else if (item == 1)
            about_box();
    } else if (menu == MENU_FILE && item == 1) {
        /* QUIT FROM THE MENU leaves an unsaved game, exactly as closing the
           window on the Amiga or resetting an 8-bit machine does; the game's
           own way out is at the command line. The sound channels go back to
           the Toolbox first -- see macsnd.c. */
        snd_off();
        ExitToShell();
    } else if (menu == MENU_EDIT) {
        SystemEdit((short)(item - 1));
    }
}

static void handle_event(EventRecord *e)
{
    WindowPtr w;
    short part;

    switch (e->what) {
    case mouseDown:
        part = FindWindow(e->where, &w);
        if (part == inMenuBar) {
            do_menu(MenuSelect(e->where));
        } else if (part == inSysWindow) {
            SystemClick(e, w);
        } else if (part == inDrag) {
            Rect limit = qd.screenBits.bounds;
            InsetRect(&limit, 4, 4);
            DragWindow(w, e->where, &limit);
        } else if (part == inContent && w != FrontWindow()) {
            SelectWindow(w);
        }
        break;
    case keyDown:
    case autoKey: {
        char ch = (char)(e->message & charCodeMask);
        if (e->modifiers & cmdKey) {
            if (e->what == keyDown) do_menu(MenuKey(ch));
        } else {
            push_key((unsigned char)ch);
        }
        break;
    }
    case updateEvt:
        if ((WindowPtr)e->message == win) {
            BeginUpdate(win);
            copy_rows(0, VDC_ROWS);          /* clipped to what was uncovered */
            EndUpdate(win);
        }
        break;
    case activateEvt:
        if ((WindowPtr)e->message == win && (e->modifiers & activeFlag) && pal)
            ActivatePalette(win);
        break;
    default:
        break;
    }
}

void wait_vsync(void)
{
    EventRecord e;
    unsigned long t = TickCount();

    do {
        SystemTask();
        if (GetNextEvent(everyEvent, &e)) handle_event(&e);
    } while (TickCount() == t);
    if (dirty_top < dirty_bot) present();
}

/* ---------------------------------------------------------------- refusing */

/* A MAC THAT CANNOT SHOW THE CONSOLE IS TOLD SO IN WORDS, in a plain window
   in black and white -- which any Mac can draw -- and the program goes back
   to the Finder. The same rule as the Falcon refusing an ST mono monitor:
   never paint something the player cannot read. */
static void refuse(const char *line1, const char *line2)
{
    Rect r;
    WindowPtr w;
    EventRecord e;
    short left = (short)((qd.screenBits.bounds.right - 420) / 2);

    SetRect(&r, left, 80, (short)(left + 420), 190);
    w = NewWindow(NULL, &r, (ConstStringPtr)"\pEGA Trek", true, noGrowDocProc,
                  (WindowPtr)-1, false, 0);
    SetPort(w);
    TextFont(systemFont);
    TextSize(12);
    MoveTo(16, 30); DrawText((Ptr)line1, 0, (short)strlen(line1));
    MoveTo(16, 50); DrawText((Ptr)line2, 0, (short)strlen(line2));
    MoveTo(16, 90); DrawText((Ptr)"Click or press a key to quit.", 0, 29);
    for (;;) {
        SystemTask();
        if (!GetNextEvent(everyEvent, &e)) continue;
        if (e.what == mouseDown || e.what == keyDown) break;
    }
    ExitToShell();
}

/* ---------------------------------------------------------------- the seam */

void vdc_init(void)
{
    SysEnvRec env;
    GDHandle dev;
    Rect wr, b;
    short sw, sh, left, top;
    CTabHandle ct;

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();
    FlushEvents(everyEvent, 0);
    setup_menus();

    if (SysEnvirons(1, &env) == noErr) {
        facts.sys_version = env.systemVersion;
        facts.machine = env.machineType;
        facts.processor = env.processor;
        facts.has_cqd = env.hasColorQD;
    }
    sw = (short)(qd.screenBits.bounds.right - qd.screenBits.bounds.left);
    sh = (short)(qd.screenBits.bounds.bottom - qd.screenBits.bounds.top);
    facts.scr_w = sw;
    facts.scr_h = sh;

    if (!facts.has_cqd)
        refuse("EGA Trek needs a colour Macintosh:",
               "this one has no Color QuickDraw.");
    /* Gestalt only exists from System 6.0.4, so its trap is checked first. */
    if (GetOSTrapAddress(_Gestalt) != GetToolTrapAddress(_Unimplemented))
        Gestalt(gestaltQuickdrawVersion, &facts.qd_version);
    if (facts.qd_version < gestalt32BitQD)
        refuse("EGA Trek needs 32-Bit QuickDraw:",
               "System 7, or the 32-Bit QuickDraw INIT on System 6.");
    dev = GetMainDevice();
    facts.depth = (*(*dev)->gdPMap)->pixelSize;
    if (sw < CON_W || sh < MENU_H + TITLE_H + CON_H)
        refuse("EGA Trek needs a 640x480 screen or larger:",
               "its console is 80 columns by 25 rows of 8x16.");
    if (facts.depth < 4)
        refuse("EGA Trek needs 16 colours or more:",
               "set the screen's depth in the Monitors control panel.");

    build_expand();
    capture_font();

    /* The offscreen: 640x400 at four bits, with EGA's colour table. */
    ct = ega_ctab();
    SetRect(&b, 0, 0, CON_W, CON_H);
    /* keepLocal (Inside Macintosh's bit 3; Multiversal names none of the
       GWorld flags) keeps the pixels in main memory, never on a NuBus
       card: scr_put writes them directly, which Inside Macintosh allows
       only for pixels the CPU can address as they are. */
    facts.gworld_err = ct ? NewGWorld(&off, 4, &b, ct, NULL, GW_KEEP_LOCAL)
                          : memFullErr;
    if (facts.gworld_err != noErr || !off)
        refuse("Not enough memory for the console.",
               "Give EGA Trek more memory in its Get Info window.");
    /* THE PIXMAP COMES FROM THE PORT, NOT FROM GetGWorldPixMap: that is a
       System 7 call, and under System 6's 32-Bit QuickDraw (1.2) its
       selector returned garbage -- MEASURED on MAME's Mac II, 6.0.8: a
       "PixMap" with pixelSize $39 and baseAddr $3E7C2000, every console
       write went there, and the screen scrambled. The Quadra under System
       7 never showed it. A GWorld IS a CGrafPort, and its portPixMap is
       the same handle on every version. The base address and the 32-bit
       flag are read out of the PixMap for the same reason. */
    off_pm = ((CGrafPtr)off)->portPixMap;
    LockPixels(off_pm);
    facts.pixels_32bit = (**off_pm).pmVersion == 4;    /* baseAddr32 */
    if (facts.pixels_32bit && !*mmu32bit)
        refuse("EGA Trek cannot reach its console's memory",
               "in 24-bit mode: turn on 32-bit addressing in Memory.");
    off_base = (unsigned char *)(**off_pm).baseAddr;
    off_rowbytes = (long)((**off_pm).rowBytes & 0x3FFF);
    memset(off_base, 0, off_rowbytes * CON_H);

    /* Centre the window below the menu bar. */
    left = (short)((sw - CON_W) / 2);
    top  = (short)(MENU_H + TITLE_H + (sh - MENU_H - TITLE_H - CON_H) / 2);
    SetRect(&wr, left, top, (short)(left + CON_W), (short)(top + CON_H));
    win = NewCWindow(NULL, &wr, (ConstStringPtr)"\pEGA Trek", true,
                     noGrowDocProc, (WindowPtr)-1, false, 0);
    pal = NewPalette(16, ct, pmTolerant, 0);
    if (pal) {
        NSetPalette(win, pal, pmAllUpdates);   /* SetPalette has no glue in Retro68 */
        ActivatePalette(win);
    }
    SetPort(win);
    copy_rows(0, VDC_ROWS);
}

/* Deliberately does NOT clear: the farewell has to survive it (the C128's
   rule, and the reason the teardown could move after the keypress). */
void vdc_shutdown(void) { if (dirty_top < dirty_bot) present(); }

/* An application returns to the Finder; there is no machine to reset. */
void plat_exit(void) { ExitToShell(); }

void scr_clear(void)
{
    if (!off_base) return;
    memset(off_base, 0, off_rowbytes * CON_H);
    dirty_top = 0;
    dirty_bot = VDC_ROWS;
}

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color)
{
    unsigned char rows[CELL_H];
    unsigned long fill;
    unsigned char *p;
    int r;

    if (!off_base || x >= VDC_COLS || y >= VDC_ROWS)
        return;

    glyph_rows(ch, rows);
    fill = (unsigned long)(color & 15) * 0x11111111UL;
    p = off_base + (long)y * CELL_H * off_rowbytes + (long)x * 4;
    for (r = 0; r < CELL_H; r++, p += off_rowbytes)
        *(unsigned long *)p = expand[rows[r]] & fill;

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

/* THE MESSAGE LOG'S BACKING STORE: a plain array, as on the Falcon, whose
   comment tells how these three were stubs until Jamie played that port and
   every message read back as zeros. ui.c calls all three. Sized from ui.c's
   own constants: LOG_BASE $1000 and 32 slots of 64 bytes. */
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

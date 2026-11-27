/* Macintosh first light. One screenshot, one recording and some typing
 * answer the questions the scope left open (NOTES.md, "SCOPE: the
 * colour Macintosh", which has what each one measured):
 *
 *   1. does the toolchain's output run here at all (this binary is small;
 *      the 72K `Main` segment the whole game makes was settled by booting
 *      the game itself);
 *   2. does an 80x25 console of 8x16 cells fit the window, and do all
 *      sixteen EGA colours come out of a 4-bit GWorld through CopyBits;
 *   3. does Monaco 12, captured at start-up, make readable 8x16 text, and do
 *      the Falcon's box glyphs join on this machine;
 *   4. do the three data RESOURCES read back through core/storage.h by their
 *      file names, and does a FILE written beside the app read back equal;
 *   5. will the Sound Manager give TWO square-wave channels at once (music
 *      and effects), and what pitch does a MIDI note number really make --
 *      measured from a recording, at three points, never at one;
 *   6. what does kb_waitkey() actually return for each key.
 *
 * The console frame is the SHARED c128/src/layout.c, as in amiga/src/
 * smoke.c, whose shape this file follows.
 */
#include <Sound.h>
#include <string.h>

#include "../../c128/src/input.h"
#include "../../c128/src/vdc.h"
#include "../../c128/src/layout.h"
#include "../../core/strpool.h"
#include "../../core/storage.h"
#include "../../c128/src/strdata.h"
#include "macvid.h"

/* The smoke test probes the Sound Manager itself, below; the game's driver
   is not linked, and these are the two calls the shared files make. */
void snd_poll(void) { }
void snd_off(void) { }

static const char *names[16] = {
    "BLACK","BLUE","GREEN","CYAN","RED","MAGENTA","BROWN","LTGRAY",
    "DKGRAY","LTBLUE","LTGREEN","LTCYAN","LTRED","LTMAGENTA","YELLOW","WHITE" };

/* A signed decimal, written at (x, y); returns the column after it. */
static unsigned char putnum(unsigned char x, unsigned char y, long v, unsigned char col)
{
    char buf[12];
    int i = 11;
    int neg = v < 0;
    unsigned long u = neg ? (unsigned long)(-v) : (unsigned long)v;

    buf[i] = '\0';
    do { buf[--i] = (char)('0' + u % 10); u /= 10; } while (u && i > 1);
    if (neg) buf[--i] = '-';
    scr_puts(x, y, buf + i, col);
    return (unsigned char)(x + (11 - i));
}

static unsigned char puthex(unsigned char x, unsigned char y, unsigned long v,
                            int digits, unsigned char col)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[10];
    int i;
    buf[0] = '$';
    for (i = 0; i < digits; i++)
        buf[1 + i] = hex[(v >> (4 * (digits - 1 - i))) & 15];
    buf[1 + digits] = '\0';
    scr_puts(x, y, buf, col);
    return (unsigned char)(x + 1 + digits);
}

/* ---------------------------------------------------------------- sound */

/* TWO CHANNELS, OPENED AND PLAYED TOGETHER. The game wants music on one and
   effects on the other, as on every port with two voices. If the second
   channel is refused, that is the answer, and it is printed. The notes are
   chosen for a recording: channel 1 plays A3, A4, A5 (MIDI 57, 69, 81 --
   220, 440, 880 Hz if note 69 is A440) for 0.6 s each, while channel 2
   holds E4 (64, 329.6 Hz) under all three. A frequency in the first 0.6 s
   that is neither 220 nor 329.6 is a wrong scale; only 220 there means the
   channels do not mix. */
static SndChannelPtr ch1, ch2;
static OSErr ch1_err = 1, ch2_err = 1;

static void note(SndChannelPtr c, unsigned char midi, unsigned short ms)
{
    SndCommand sc;
    sc.cmd = noteCmd;
    sc.param1 = (short)(ms * 2);                 /* half-milliseconds */
    sc.param2 = (0xC0L << 24) | midi;            /* amplitude, note */
    SndDoCommand(c, &sc, false);
}

static void sound_probe(void)
{
    ch1_err = SndNewChannel(&ch1, noteSynth, 0, NULL);
    ch2_err = SndNewChannel(&ch2, noteSynth, 0, NULL);
    if (ch1_err == noErr) {
        note(ch1, 57, 600);
        note(ch1, 69, 600);
        note(ch1, 81, 600);
    }
    if (ch2_err == noErr)
        note(ch2, 64, 1800);
}

/* ---------------------------------------------------------------- files */

static void storage_probe(unsigned char y)
{
    static unsigned char buf[12000];
    static const char msg[] = "EGA TREK MAC SMOKE 0123456789";
    uint16_t got;
    uint8_t r;
    unsigned char x;
    long total;

    /* The three data resources, by the names the other ports' files have. */
    r = plat_read_all("STRINGS.DAT", buf, sizeof buf, &got);
    x = (unsigned char)(scr_puts(42, y, "STRINGS.DAT RES", 7), 58);
    x = putnum(x, y, r == STOR_OK ? (long)got : -(long)r, r == STOR_OK ? 10 : 12);
    r = plat_read_all("MUSIC.DAT", buf, sizeof buf, &got);
    scr_puts(42, (unsigned char)(y + 1), "MUSIC.DAT RES", 7);
    putnum(58, (unsigned char)(y + 1), r == STOR_OK ? (long)got : -(long)r,
           r == STOR_OK ? 10 : 12);
    /* BRIEF.TXT through the STREAM, a page at a time, as the game reads it. */
    total = -1;
    if (plat_open("BRIEF.TXT") == STOR_OK) {
        uint16_t n;
        total = 0;
        while ((n = plat_read(buf, 512)) > 0) total += n;
        plat_close();
    }
    scr_puts(42, (unsigned char)(y + 2), "BRIEF.TXT STREAM", 7);
    putnum(59, (unsigned char)(y + 2), total, total > 0 ? 10 : 12);

    /* A FILE beside the app: written, read back, compared. */
    r = plat_write_all("SMOKE.TMP", msg, (uint16_t)(sizeof msg - 1));
    scr_puts(42, (unsigned char)(y + 3), "FILE WRITE", 7);
    putnum(53, (unsigned char)(y + 3), r, r == STOR_OK ? 10 : 12);
    memset(buf, 0, 64);
    r = plat_read_all("SMOKE.TMP", buf, 64, &got);
    scr_puts(57, (unsigned char)(y + 3), "READ", 7);
    putnum(62, (unsigned char)(y + 3), r, r == STOR_OK ? 10 : 12);
    scr_puts(66, (unsigned char)(y + 3),
             (r == STOR_OK && got == sizeof msg - 1 && !memcmp(buf, msg, got))
                 ? "SAME" : "DIFFERS", r == STOR_OK ? 10 : 12);
    /* And the contract: a file longer than the buffer is an ERROR. */
    r = plat_read_all("SMOKE.TMP", buf, 8, &got);
    scr_puts(42, (unsigned char)(y + 4), "OVERSIZE READ IS ERROR:", 7);
    scr_puts(66, (unsigned char)(y + 4), r == STOR_ERROR ? "YES" : "NO",
             r == STOR_ERROR ? 10 : 12);
    r = plat_read_all("NOSUCH.SAV", buf, 64, &got);
    scr_puts(42, (unsigned char)(y + 5), "MISSING FILE IS NOTFOUND:", 7);
    scr_puts(68, (unsigned char)(y + 5), r == STOR_NOTFOUND ? "YES" : "NO",
             r == STOR_NOTFOUND ? 10 : 12);
}

/* ---------------------------------------------------------------- facts */

static void facts_panel(void)
{
    const struct mac_facts *f = mac_get_facts();
    unsigned char x;

    x = (unsigned char)(scr_puts(42, 1, "SYS", 7), 46);
    x = puthex(x, 1, (unsigned long)f->sys_version, 4, 15);
    x = (unsigned char)(scr_puts((unsigned char)(x + 1), 1, "MACH", 7), x + 6);
    x = putnum(x, 1, f->machine, 15);
    x = (unsigned char)(scr_puts((unsigned char)(x + 1), 1, "CPU", 7), x + 5);
    putnum(x, 1, f->processor, 15);

    x = (unsigned char)(scr_puts(42, 2, "QD", 7), 45);
    x = puthex(x, 2, (unsigned long)f->qd_version, 4, 15);
    x = (unsigned char)(scr_puts((unsigned char)(x + 1), 2, "DEPTH", 7), x + 7);
    x = putnum(x, 2, f->depth, 15);
    x = (unsigned char)(scr_puts((unsigned char)(x + 1), 2, "SCR", 7), x + 5);
    x = putnum(x, 2, f->scr_w, 15);
    scr_puts(x, 2, "X", 7);
    putnum((unsigned char)(x + 1), 2, f->scr_h, 15);

    x = (unsigned char)(scr_puts(42, 3, "MONACO12 A", 7), 53);
    x = putnum(x, 3, f->font_ascent, 15);
    x = (unsigned char)(scr_puts((unsigned char)(x + 1), 3, "D", 7), x + 3);
    x = putnum(x, 3, f->font_descent, 15);
    x = (unsigned char)(scr_puts((unsigned char)(x + 1), 3, "W", 7), x + 3);
    x = putnum(x, 3, f->font_widmax, 15);
    x = (unsigned char)(scr_puts((unsigned char)(x + 1), 3, "L", 7), x + 3);
    putnum(x, 3, f->font_leading, 15);

    x = (unsigned char)(scr_puts(42, 4, "PIX32", 7), 48);
    x = putnum(x, 4, f->pixels_32bit, 15);
    x = (unsigned char)(scr_puts((unsigned char)(x + 1), 4, "SND CH1", 7), x + 9);
    x = putnum(x, 4, ch1_err, ch1_err == 0 ? 10 : 12);
    x = (unsigned char)(scr_puts((unsigned char)(x + 1), 4, "CH2", 7), x + 5);
    putnum(x, 4, ch2_err, ch2_err == 0 ? 10 : 12);
}

int main(void)
{
    int i;
    uint8_t pool_ok;

    vdc_init();
    sound_probe();                    /* first, so the recording starts clean */

    pool_ok = str_load();             /* the pool, out of the RESOURCE */

    for (i = 0; i < 16; i++)
        scr_puts((unsigned char)(3 + (i / 8) * 19),
                 (unsigned char)(12 + (i % 8)), names[i], (unsigned char)i);

    draw_console();

    {
        /* EVERY GLYPH THIS PORT HAD TO DRAW -- the Amiga's swept list. */
        static const unsigned char codes[] = {
            G_HLINE, G_VLINE, G_TL, G_TR, G_BL, G_BR,
            G_TEE_L, G_TEE_R, G_TEE_D, G_TEE_U, G_CROSS,
            98, 226, 160, 228, 81
        };
        unsigned char k;
        for (k = 0; k < sizeof codes; k++)
            scr_put((unsigned char)(3 + k * 2), 21, codes[k], 11);
        scr_puts(3, 20, "EVERY DRAWN GLYPH", 14);

        scr_puts(3, 22, "SHIP", 14);
        scr_put(9, 22, 81, 15);
        scr_hline(10, 22, 4, G_HLINE, 15);
        scr_put(14, 22, 160, 15);

        scr_puts(18, 22, "DISC", 14);
        scr_put(24, 21, 98, 9); scr_put(24, 22, 160, 9); scr_put(24, 23, 226, 9);

        scr_puts(28, 22, "BARS", 14);
        scr_hline(34, 22, 6, 228, 10);

        /* ONE CODE ON PURPOSE THAT NOBODY DREW: it must be the hollow box. */
        scr_puts(3, 23, "CODE 97 -> BOX:", 8);
        scr_put(19, 23, 97, 12);
        scr_puts(3, 11, "PLAY AGAIN?  [YES]  [NO]", 13);
        scr_puts(3, 10, "the quick brown fox 0123456789", 15);
    }

    facts_panel();
    storage_probe(14);

    scr_puts(42, 20, pool_ok ? "STRINGS.DAT POOLED, TITLES ARE REAL"
                             : "NO POOL -- PANEL TITLES WILL BE BLANK",
             pool_ok ? 10 : 12);
    scr_puts(42, 23, "PRESS RETURN TO QUIT", 12);

    /* THE INPUT SEAM, ECHOED: char and code. Arrows must read 1 and 2,
       letters arrive UPPER CASE, delete reads 20. RETURN ends it. */
    kb_init();
    scr_puts(42, 6, "KEYS ECHO: CHAR CODE", 14);
    {
        unsigned char col = 42, row = 7;
        for (;;) {
            char k = kb_waitkey();
            unsigned char u = (unsigned char)k;
            char one[2];

            if (k == KB_RETURN) break;
            scr_fill_rect(col, row, 6, 1, 32, 15);
            one[0] = (u >= 32 && u < 127) ? k : '?';
            one[1] = '\0';
            scr_puts(col, row, one, 15);
            putnum((unsigned char)(col + 2), row, u, 10);
            if (++row > 12) { row = 7; col = (unsigned char)(col + 6); }
            if (col > 72) col = 42;
        }
    }

    vdc_shutdown();
    plat_exit();
    return 0;
}

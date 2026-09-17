/* THE LIVE MATRIX AND ITS DECODE, ON SCREEN. Hold a key; both lines move.
 *
 * WHY THIS EXISTS: src/keyprobe.c scanned with NO KEY HELD, got $FF from
 * every row, and I reported that as "the registers work". $FF is also what a
 * dead register reads -- a check that cannot fail. And nothing here can press
 * a key: VICE's binary monitor feeds the KERNAL's buffer, not the matrix, and
 * this port never reads that buffer. A finger is the only instrument.
 *
 * The access sequence is the PLUS/4 KERNAL'S OWN, disassembled from
 * kernal-318004-05.bin at $DB70, which SCNKEY ($FF9F -> $DB11) calls:
 *
 *     8D 30 FD    STA $FD30      row select
 *     8D 08 FF    STA $FF08      the WRITE is the sample trigger
 *     AD 08 FF    LDA $FF08      read the columns
 *     60          RTS
 *
 * Line 2 is those eight bytes, one per row select, active low.
 * Line 4 is what src/p4key.c's table makes of them -- the actual character.
 * Line 0 counts, so "nothing changed" cannot be confused with "stopped".
 *
 * LINES 6 AND 8 LATCH. A keypress lasts milliseconds and this loop runs about
 * sixty times a second, so a live-only display can miss one entirely and read
 * as "nothing works". Line 6 keeps the last character decoded and line 8 the
 * eight row bytes AT THAT MOMENT, and neither is ever cleared. That makes a
 * single keystroke sufficient evidence instead of requiring a held key.
 */
#define SCR ((volatile unsigned char *)0x0C00)
#define COL ((volatile unsigned char *)0x0800)
#define KEY_LATCH (*(volatile unsigned char *)0xFD30)
#define KEY_PORT  (*(volatile unsigned char *)0xFF08)

typedef struct { unsigned char row, col; char ch; } Key;
static const Key keys[] = {
    { 1, 0, 13 }, { 0, 0, 20 }, { 4, 6, 27 }, { 3, 5, 1 }, { 0, 5, 2 },
    { 4, 7, 32 },
    { 2, 1, 'A' }, { 4, 3, 'B' }, { 4, 2, 'C' }, { 2, 2, 'D' },
    { 6, 1, 'E' }, { 5, 2, 'F' }, { 2, 3, 'G' }, { 5, 3, 'H' },
    { 1, 4, 'I' }, { 2, 4, 'J' }, { 5, 4, 'K' }, { 2, 5, 'L' },
    { 4, 4, 'M' }, { 7, 4, 'N' }, { 6, 4, 'O' }, { 1, 5, 'P' },
    { 6, 7, 'Q' }, { 1, 2, 'R' }, { 5, 1, 'S' }, { 6, 2, 'T' },
    { 6, 3, 'U' }, { 7, 3, 'V' }, { 1, 1, 'W' }, { 7, 2, 'X' },
    { 1, 3, 'Y' }, { 4, 1, 'Z' },
    { 3, 4, '0' }, { 0, 7, '1' }, { 3, 7, '2' }, { 0, 1, '3' },
    { 3, 1, '4' }, { 0, 2, '5' }, { 3, 2, '6' }, { 0, 3, '7' },
    { 3, 3, '8' }, { 0, 4, '9' },
};
#define KEY_COUNT (sizeof keys / sizeof keys[0])

static void put(unsigned char x, unsigned char y, unsigned char c)
{ SCR[y * 40 + x] = c; COL[y * 40 + x] = 0x71; }

static void hex(unsigned char x, unsigned char y, unsigned char v)
{
    unsigned char h = v >> 4, l = v & 15;
    put(x,   y, (unsigned char)(h < 10 ? 0x30 + h : h - 9));
    put(x+1, y, (unsigned char)(l < 10 ? 0x30 + l : l - 9));
}

static unsigned char rd(unsigned char r)
{
    unsigned char sel = (unsigned char)~(1 << r);
    KEY_LATCH = sel; KEY_PORT = sel; return KEY_PORT;   /* the KERNAL's own */
}

int main(void)
{
    unsigned char r, i, hit, seen[8]; unsigned int t, spin = 0;

    __asm__ volatile ("sei");
    *(volatile unsigned char *)0xFF0A = 0;
    for (t = 0; t < 1000; t++) { SCR[t] = 32; COL[t] = 0x71; }

    for (;;) {
        hex(0, 0, (unsigned char)(spin >> 8)); hex(2, 0, (unsigned char)spin); spin++;

        for (r = 0; r < 8; r++) hex(r * 3, 2, rd(r));

        hit = 0;
        for (i = 0; i < KEY_COUNT; i++) {
            if ((rd(keys[i].row) & (1 << keys[i].col)) == 0) {
                char c = keys[i].ch;
                /* screen codes: letters/digits map straight, specials named */
                put(0, 4, (unsigned char)(c >= 'A' ? c - 64 :
                                          c >= '0' ? c :
                                          c == 32  ? 'S' - 64 :
                                          c == 13  ? 'R' - 64 :
                                          c == 27  ? 'E' - 64 :
                                          c == 20  ? 'D' - 64 : 'U' - 64));
                hit = 1;
                break;
            }
        }
        if (!hit) put(0, 4, 0x2D);          /* '-' : nothing held */

        /* THE LATCH. Any row reading other than $FF means a key is down --
           record it and the decode, and never clear either. */
        for (r = 0; r < 8; r++) seen[r] = rd(r);
        for (r = 0; r < 8; r++) {
            if (seen[r] != 0xFF) {
                unsigned char k;
                for (k = 0; k < 8; k++) hex(k * 3, 8, seen[k]);
                if (hit) { SCR[6 * 40] = SCR[4 * 40]; COL[6 * 40] = 0x71; }
                break;
            }
        }
    }
}

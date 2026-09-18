/* WHAT IS IN THIS MACHINE'S FONT, AND HOW BIG IS THE SCREEN?
 *
 * Two questions the video driver cannot be written without, and both are
 * cheaper to look at than to read about:
 *
 *   * THE BOX-DRAWING SET. Every port draws the nine-panel console out of
 *     twelve border glyphs. Most borrow the machine's font -- topaz, Line-A,
 *     the Atari OS ROM, three Commodore chargens -- and the CoCo 3 had to
 *     author a whole 6x8 face because the GIME's generator is silicon. Which
 *     of those two this port is depends entirely on what is at codes $80-$FF,
 *     and a CP437-shaped font would hand us the set for nothing.
 *
 *   * THE GRID. 80x60 is what the docs say; corner markers are what proves it.
 *     A driver that computes offsets from the wrong COLS writes every row one
 *     place further off than the last.
 *
 * 256 cells and four corners answer both in one run. Hex labels down the side
 * and across the top so a glyph can be NAMED from the snapshot rather than
 * counted to.
 */
#define IOCTRL (*(volatile unsigned char *)0x0001)
#define MATRIX ((volatile unsigned char *)0xC000)
#define COLS 80
#define ROWS 60

__attribute__((used, retain)) volatile unsigned char ran;

/* One cell, code and colour, page left at 0. Deliberately the slow way: this
   probe is measuring the font, not the driver, and a batched write would be a
   second thing that could be wrong. */
static void cell(unsigned char x, unsigned char y, unsigned char ch, unsigned char col)
{
    unsigned int off = (unsigned int)y * COLS + x;
    __asm__ volatile ("sei");
    IOCTRL = 2; MATRIX[off] = ch;
    IOCTRL = 3; MATRIX[off] = col;
    IOCTRL = 0;
    __asm__ volatile ("cli");
}

static void text(unsigned char x, unsigned char y, const char *s, unsigned char col)
{
    while (*s) cell(x++, y, (unsigned char)*s++, col);
}

static void clear(void)
{
    unsigned int i;
    __asm__ volatile ("sei");
    IOCTRL = 2;
    for (i = 0; i < (unsigned int)COLS * ROWS; i++) MATRIX[i] = ' ';
    IOCTRL = 3;
    for (i = 0; i < (unsigned int)COLS * ROWS; i++) MATRIX[i] = 0xF0;
    IOCTRL = 0;
    __asm__ volatile ("cli");
}

int main(void)
{
    static const char hex[] = "0123456789ABCDEF";
    unsigned char hi, lo;

    ran = 0x11;
    clear();
    text(2, 1, "F256K CHARMAP -- 256 CODES, HIGH NIBBLE DOWN, LOW ACROSS", 0xE0);

    for (lo = 0; lo < 16; lo++)
        cell((unsigned char)(8 + lo * 3), 3, (unsigned char)hex[lo], 0xB0);

    for (hi = 0; hi < 16; hi++) {
        unsigned char y = (unsigned char)(5 + hi);
        cell(4, y, (unsigned char)hex[hi], 0xB0);
        cell(5, y, 'X', 0xB0);
        for (lo = 0; lo < 16; lo++)
            cell((unsigned char)(8 + lo * 3), y,
                 (unsigned char)((hi << 4) | lo), 0xF0);
    }

    /* THE CORNERS, on the assumption under test. If the grid is not 80x60
       these land somewhere other than the four corners, and the snapshot says
       which way it is wrong. */
    cell(0, 0, '1', 0xC0);
    cell(COLS - 1, 0, '2', 0xC0);
    cell(0, ROWS - 1, '3', 0xC0);
    cell(COLS - 1, ROWS - 1, '4', 0xC0);
    text(2, ROWS - 3, "CORNERS 1 2 3 4 AT 0,0 / 79,0 / 0,59 / 79,59", 0xA0);

    ran = 0x5A;
    for (;;) { }
}

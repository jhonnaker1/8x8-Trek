/* IS THE BOX-DRAWING SET ALREADY IN THIS MACHINE'S FONT?
 *
 * The charmap says yes by eye. By eye is not good enough: this project has
 * read "overlapping text" off a screenshot three times in one session and been
 * wrong all three, and #41 in the list is a whole session spent debugging a
 * display that decoded perfectly at every read because SCREEN CODES ARE NOT
 * PIXELS. An 8x8 glyph in a 640x480 snapshot is four pixels of evidence.
 *
 * So dump the BYTES. The F256's text font is RAM at $C000 on I/O PAGE 1 --
 * page 0 is registers, 2 the character matrix, 3 the colour matrix -- and this
 * probe prints each candidate's eight bytes as a bitmap beside the glyph the
 * hardware actually draws for that code.
 *
 * THAT PAIRING IS THE CONTROL, and it is the reason the probe is built this
 * way. If page 1 is not the font, the bitmap and the glyph disagree and the
 * probe says so. A bitmap on its own would be a number I could believe.
 *
 * The candidates are CP437's box set, because that is the shape the charmap
 * suggests. If they are there the console's twelve border glyphs cost nothing;
 * if not, this port authors a face the way the CoCo 3 had to.
 */
#define IOCTRL (*(volatile unsigned char *)0x0001)
#define MATRIX ((volatile unsigned char *)0xC000)
#define FONT   ((volatile unsigned char *)0xC000)   /* same window, page 1 */
#define COLS 80
#define ROWS 60

__attribute__((used, retain)) volatile unsigned char ran;

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

/* Eight bytes out of font RAM. The page switch is the whole trick, and it is
   bracketed like every other one on this machine because the kernel's IRQ
   reads its registers through the same window. */
static void glyph_bytes(unsigned char code, unsigned char *out)
{
    unsigned int base = (unsigned int)code * 8;
    unsigned char i;
    __asm__ volatile ("sei");
    IOCTRL = 1;
    for (i = 0; i < 8; i++) out[i] = FONT[base + i];
    IOCTRL = 0;
    __asm__ volatile ("cli");
}

static void show(unsigned char slot, unsigned char code, const char *name)
{
    static const char hex[] = "0123456789ABCDEF";
    unsigned char b[8];
    unsigned char x = (unsigned char)(2 + (slot & 3) * 19);
    unsigned char y = (unsigned char)(4 + (slot >> 2) * 11);
    unsigned char r, c;

    glyph_bytes(code, b);

    cell(x, y, '$', 0xB0);
    cell((unsigned char)(x + 1), y, (unsigned char)hex[code >> 4], 0xB0);
    cell((unsigned char)(x + 2), y, (unsigned char)hex[code & 15], 0xB0);
    /* THE GLYPH THE HARDWARE DRAWS, right next to the bytes I claim are its
       definition. Disagreement here means the page is wrong. */
    cell((unsigned char)(x + 4), y, code, 0xE0);
    text((unsigned char)(x + 6), y, name, 0xA0);

    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++)
            cell((unsigned char)(x + c), (unsigned char)(y + 1 + r),
                 (b[r] & (unsigned char)(0x80 >> c)) ? 'X' : '.',
                 (b[r] & (unsigned char)(0x80 >> c)) ? 0xF0 : 0x80);
        cell((unsigned char)(x + 9), (unsigned char)(y + 1 + r), (unsigned char)hex[b[r] >> 4], 0xD0);
        cell((unsigned char)(x + 10), (unsigned char)(y + 1 + r), (unsigned char)hex[b[r] & 15], 0xD0);
    }
}

int main(void)
{
    ran = 0x11;
    clear();
    text(2, 1, "F256K FONT PROBE -- BYTES FROM I/O PAGE 1 vs THE GLYPH DRAWN", 0xE0);
    text(2, 2, "IF THE BITMAP AND THE CHARACTER DISAGREE, PAGE 1 IS NOT THE FONT.", 0x90);

    show(0,  0xC4, "HLINE");
    show(1,  0xB3, "VLINE");
    show(2,  0xDA, "TL");
    show(3,  0xBF, "TR");
    show(4,  0xC0, "BL");
    show(5,  0xD9, "BR");
    show(6,  0xC3, "TEE-L");
    show(7,  0xB4, "TEE-R");
    show(8,  0xC2, "TEE-D");
    show(9,  0xC1, "TEE-U");
    show(10, 0xC5, "CROSS");
    show(11, 0xDB, "BLOCK");
    show(12, 0xDC, "HALF-LO");
    show(13, 0xDF, "HALF-HI");
    show(14, 0x41, "'A' CTRL");
    show(15, 0xFE, "SQUARE");

    ran = 0x5A;
    for (;;) { }
}

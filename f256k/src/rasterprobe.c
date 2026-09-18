/* WHERE IS VICKY'S LINE COUNTER?
 *
 * uno's F256 driver waits for vsync with `for (i = 0; i < 2500; i++) {}` -- a
 * delay loop calibrated to nothing, which is a frame timer only by luck and
 * stops being one the moment the compiler or the CPU speed changes. This port
 * paces music off the frame (every other port does) and #42 in the list is a
 * whole session where every burst frequency was right while the tune ran at
 * double speed, because the instrument measured PITCH and the bug was TEMPO.
 * A real raster register is worth finding before anything depends on it.
 *
 * FIND IT BY ITS BEHAVIOUR, not by its name in a manual I would then have to
 * trust: a line counter is the register that SWEEPS. So sample $D000..$D03F
 * many times across several frames and print each address's min and max. A
 * control register sits still; a counter's spread is most of its range.
 *
 * The high byte is the interesting one -- 480 lines needs nine bits, and the
 * Plus/4 shipped double-speed music for exactly this reason: its raster is
 * nine bits and the driver read eight.
 */
#define IOCTRL (*(volatile unsigned char *)0x0001)
#define MATRIX ((volatile unsigned char *)0xC000)
#define VKY    ((volatile unsigned char *)0xD000)
#define COLS 80
#define ROWS 60
#define NREG 64

__attribute__((used, retain)) volatile unsigned char ran;

static unsigned char lo[NREG], hi[NREG];

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

static void hex2(unsigned char x, unsigned char y, unsigned char v, unsigned char col)
{
    static const char h[] = "0123456789ABCDEF";
    cell(x, y, (unsigned char)h[v >> 4], col);
    cell((unsigned char)(x + 1), y, (unsigned char)h[v & 15], col);
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
    unsigned char r;
    unsigned int pass;
    unsigned char v;

    ran = 0x11;

    for (r = 0; r < NREG; r++) { lo[r] = 0xFF; hi[r] = 0x00; }

    /* Several frames' worth of samples. The registers are on I/O page 0, so
       the page is set ONCE around the whole sweep rather than per read -- the
       PSG tone probe was silent for a run because a page was left wrong, and
       a sample taken on the wrong page is somebody else's memory. */
    __asm__ volatile ("sei");
    IOCTRL = 0;
    for (pass = 0; pass < 6000; pass++) {
        for (r = 0; r < NREG; r++) {
            v = VKY[r];
            if (v < lo[r]) lo[r] = v;
            if (v > hi[r]) hi[r] = v;
        }
    }
    IOCTRL = 0;
    __asm__ volatile ("cli");

    clear();
    text(2, 1, "VICKY $D000-$D03F SAMPLED 6000x -- A COUNTER SWEEPS, A REGISTER SITS", 0xE0);
    text(2, 2, "ADDR LO HI           (SPREAD IS THE SIGNAL)", 0x90);

    for (r = 0; r < NREG; r++) {
        unsigned char x = (unsigned char)(2 + (r >> 4) * 20);
        unsigned char y = (unsigned char)(4 + (r & 15));
        unsigned char col = (hi[r] > lo[r]) ? 0xE0 : 0x80;
        cell(x, y, '$', col);
        cell((unsigned char)(x + 1), y, 'D', col);
        cell((unsigned char)(x + 2), y, '0', col);
        hex2((unsigned char)(x + 3), y, r, col);
        hex2((unsigned char)(x + 7), y, lo[r], col);
        hex2((unsigned char)(x + 10), y, hi[r], col);
        if (hi[r] > lo[r]) text((unsigned char)(x + 13), y, "SWEEP", 0xC0);
    }

    ran = 0x5A;
    for (;;) { }
}

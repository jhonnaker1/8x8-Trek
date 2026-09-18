/* IS $D01A/$D01B A LINE COUNTER, AND HOW FAST DOES IT WRAP?
 *
 * The sweep probe found the only two registers in $D000-$D03F that move. That
 * makes them a counter; it does not make them a FRAME counter, and the whole
 * music tempo will hang off the difference. #42 in the list is a session where
 * every burst frequency measured correct while the tune played at double
 * speed, because the instrument could see pitch and not tempo.
 *
 * So this probe does not ask "does it sweep" again. It asks the two questions
 * a vsync actually depends on:
 *
 *   1. WHAT IS THE TOP OF THE COUNT? $D01B reaching 3 means ten bits, so the
 *      wrap is somewhere up to 1023 -- and the Plus/4 ran its music at double
 *      speed for exactly this reason: nine-bit raster, eight-bit read, two
 *      false wraps per frame.
 *   2. HOW MANY WRAPS PER SECOND? Counted here, read by the host against real
 *      elapsed time. 60 is the answer that makes this a frame timer. Anything
 *      else and it is a line timer I would have mistaken for one.
 *
 * The count is published in `vsyncs` for the host to read -- the screen shows
 * it too, but a number on a screenshot is a number I have to transcribe.
 */
#define IOCTRL (*(volatile unsigned char *)0x0001)
#define MATRIX ((volatile unsigned char *)0xC000)
#define VKY_LINE_L (*(volatile unsigned char *)0xD01A)
#define VKY_LINE_H (*(volatile unsigned char *)0xD01B)
#define COLS 80
#define ROWS 60

__attribute__((used, retain)) volatile unsigned char ran;
/* Read by the host, so they must survive -Oz and --gc-sections. */
__attribute__((used, retain)) volatile unsigned int vsyncs;
__attribute__((used, retain)) volatile unsigned int line_max;
/* Loop iterations, so the SAMPLE RATE is measured rather than assumed. The
   first version of this probe reported -5944 Hz and the reason was that it
   sampled at ~21kHz a counter moving at ~45kHz. A probe that cannot tell
   aliasing from a reading is one more for the list. */
__attribute__((used, retain)) volatile unsigned long spins_total;

/* TEN BITS READ ATOMICALLY. lo and hi are two reads of a counter that does not
   stop between them, so a wrap landing in the middle gives a value that was
   never on the chip. Re-read the high byte and retry -- the classic guard, and
   the reason this is a function rather than two macros at every call site. */
static unsigned int line(void)
{
    unsigned char h1, l, h2;
    for (;;) {
        h1 = VKY_LINE_H;
        l  = VKY_LINE_L;
        h2 = VKY_LINE_H;
        if (h1 == h2) return ((unsigned int)h1 << 8) | l;
    }
}

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
{ while (*s) cell(x++, y, (unsigned char)*s++, col); }
static void hex4(unsigned char x, unsigned char y, unsigned int v, unsigned char col)
{
    static const char h[] = "0123456789ABCDEF";
    cell(x, y, (unsigned char)h[(v >> 12) & 15], col);
    cell((unsigned char)(x+1), y, (unsigned char)h[(v >> 8) & 15], col);
    cell((unsigned char)(x+2), y, (unsigned char)h[(v >> 4) & 15], col);
    cell((unsigned char)(x+3), y, (unsigned char)h[v & 15], col);
}
static void clear(void)
{
    unsigned int i;
    __asm__ volatile ("sei");
    IOCTRL = 2; for (i = 0; i < (unsigned int)COLS*ROWS; i++) MATRIX[i] = ' ';
    IOCTRL = 3; for (i = 0; i < (unsigned int)COLS*ROWS; i++) MATRIX[i] = 0xF0;
    IOCTRL = 0;
    __asm__ volatile ("cli");
}

int main(void)
{
    unsigned int prev, cur;
    unsigned long spins;

    ran = 0x11;
    vsyncs = 0;
    line_max = 0;

    clear();
    text(2, 1, "F256K VSYNC PROBE -- $D01A/$D01B", 0xE0);
    text(2, 3, "TOP OF COUNT $", 0x90);
    text(2, 4, "CROSSINGS    $", 0x90);
    text(2, 6, "60 CROSSINGS PER SECOND MAKES THIS A FRAME TIMER. MAME SAYS 60.0 HZ.", 0x80);

    /* THE TOP OF THE COUNT, found by watching for the fall. A maximum taken by
       sampling is a maximum of what sampling HAPPENED to catch -- but a wrap
       is an event, and the value just before one is exact. */
    prev = line();
    for (spins = 0; spins < 60000UL; spins++) {
        cur = line();
        if (cur < prev && prev > line_max) line_max = prev;
        prev = cur;
    }
    hex4(17, 3, line_max, 0xF0);

    /* THRESHOLD CROSSINGS, NOT `cur < prev`.
     *
     * Comparing each sample with the last one only works if no wrap can hide
     * between two samples, and this loop cannot read faster than the counter
     * moves -- which is what made the first run report a negative rate. A
     * threshold does not care about sample rate, only about whether the top
     * region lasts longer than one sample period. Above 600 is 150 counts of
     * 751, so if this is a frame counter the region lasts a fifth of a frame
     * and is impossible to miss; if it is something faster, the number will
     * not come out at 60 and that is the answer too. */
    spins_total = 0;
    for (;;) {
        while (line() >= 600) { spins_total++; }
        while (line() <  600) { spins_total++; }
        vsyncs = vsyncs + 1;
        hex4(18, 4, vsyncs, 0xF0);
        ran = 0x5A;
    }
}

/* DOES THE KERNEL DELIVER A FRAME TICK, AND IS IT 60 HZ?
 *
 * THE RASTER REGISTER IS A DEAD END IN THIS EMULATOR, which is worth writing
 * down because it is not a fact about the machine. MAME's f256k returns
 * `m_screen->hpos()` -- the HORIZONTAL dot position -- from $D01A/$D01B, the
 * addresses that hold the scan LINE, and returns a hard-coded 0 from the
 * column registers at $D018/$D019. The variable is even named `line` in the
 * source. So the counter sweeps 0..751 at the dot clock, which is why two
 * successive attempts to pace off it came back at -5944 Hz and 5457 Hz: both
 * were sampling noise, not a frame rate. On real hardware that register is
 * the scan line; here it cannot be used at all.
 *
 * The SOF interrupt IS implemented -- the video device sets pending bit 0 at
 * $D660 once a frame -- and FoenixMCP owns the IRQ and republishes it as a
 * clock.TICK event. So the frame timer this port can actually have comes
 * through the SAME QUEUE as the keyboard and every file read, and that is a
 * fact about the port's architecture rather than a detail of its timing.
 *
 * Counted here, calibrated by the host against MAME's own clock: 60 makes it
 * a frame timer. The keystroke count is beside it because the queue is
 * shared, and a tick pump that silently swallows keys is the bug this port
 * would otherwise ship.
 */
#include "f256kern.h"

#define IOCTRL (*(volatile unsigned char *)0x0001)
#define MATRIX ((volatile unsigned char *)0xC000)
#define COLS 80
#define ROWS 60

__attribute__((used, retain)) volatile unsigned char ran;
__attribute__((used, retain)) volatile unsigned int ticks;
__attribute__((used, retain)) volatile unsigned int keys;
__attribute__((used, retain)) volatile unsigned int others;
__attribute__((used, retain)) volatile unsigned char last_other;
__attribute__((used, retain)) volatile unsigned char tick_type;

static struct f256_event ev;
/* Written by the asm below, so it cannot be a local. Nonzero == the queue was
   empty and `ev` holds whatever it held before -- reading it anyway is how a
   pump reports an event that never arrived. */
static volatile unsigned char ev_empty;

/* ONE ASM BLOCK, because the carry is the return value. Splitting the call
   from the test lets the compiler put anything it likes in between, and
   anything it likes includes instructions that touch the flags -- which is
   also why "p" is in the clobber list. */
static void pump(void)
{
    __asm__ volatile(
        "        jsr $ff00\n"       /* NextEvent; carry set == queue empty */
        "        lda #0\n"
        "        rol a\n"
        "        sta ev_empty\n"
        ::: "a", "x", "y", "memory", "p");
}

/* The kernel's frame counter, low byte. SetTimer with the QUERY bit queues
   nothing and hands back kernel.ticks -- see f256kern.h for why this and not
   an event. The args block is at $F0 and `units` is its first byte. */
static volatile unsigned char frame_lo;
static unsigned char frames(void)
{
    __asm__ volatile(
        "        lda #$80\n"        /* TIMER_FRAMES | TIMER_QUERY */
        "        sta $f3\n"         /* timer.units -- THE UNION STARTS AT $F3 */
        "        jsr $fff0\n"       /* SetTimer */
        "        sta frame_lo\n"
        ::: "a", "x", "y", "memory", "p");
    return frame_lo;
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
    unsigned int shown = 0xFFFF;
    unsigned char prev;

    ran = 0x11;
    ticks = 0; keys = 0; others = 0; last_other = 0;
    tick_type = EV(clock.TICK);

    clear();
    text(2, 1, "F256K KERNEL TICK PROBE -- EVENTS, NOT THE RASTER", 0xE0);
    text(2, 2, "MAME'S $D01A/$D01B RETURN hpos(), NOT THE SCAN LINE. SEE THE SOURCE.", 0x80);
    text(2, 4, "TIMER QUERY $80 $", 0x90);
    text(2, 5, "FRAMES          $", 0x90);
    text(2, 6, "KEYS            $", 0x90);
    text(2, 7, "OTHER EVENTS    $", 0x90);
    text(2, 8, "LAST OTHER TYPE $", 0x90);
    hex4(19, 4, frames(), 0xF0);

    K_ARGS_EVENT = &ev;

    prev = frames();
    for (;;) {
        unsigned char now = frames();
        while (now != prev) { ticks = ticks + 1; prev = prev + 1; }
        ran = 0x5A;

        /* Pump the queue too, so the two can be counted side by side: the
           whole point of using the timer is that a frame wait must not be
           taking events off the queue that the keyboard needs. */
        pump();
        if (!ev_empty) {
            if (ev.type == EV(key.PRESSED) ||
                ev.type == EV(key.RELEASED))        keys = keys + 1;
            else { others = others + 1; last_other = ev.type; }
        }
        /* Repaint only on change: a screen write is ~10 cells of sei/cli and
           doing it every pump would make the pump itself the bottleneck. */
        if (ticks != shown) {
            shown = ticks;
            hex4(19, 5, ticks, 0xF0);
            hex4(19, 6, keys, 0xF0);
            hex4(19, 7, others, 0xF0);
            hex4(19, 8, last_other, 0xF0);
        }
    }
}

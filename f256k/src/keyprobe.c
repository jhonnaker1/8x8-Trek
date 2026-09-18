/* WHAT DOES A KEYPRESS ACTUALLY LOOK LIKE ON THIS MACHINE?
 *
 * Three things the input layer cannot be written without, and none of them is
 * guessable:
 *
 *   1. WHAT IS IN THE QUEUE WHEN THE GAME STARTS. The game is launched by
 *      typing `/- egatrek` and RETURN at the SuperBASIC prompt, and on the
 *      Amiga that RETURN was still sitting in Intuition's message port when
 *      the title screen asked for a key -- so the title dismissed itself.
 *      input.h documents kb_init() existing for exactly this and being DEAD
 *      CODE on two ports. This probe logs whatever is queued before it touches
 *      anything, so the answer is read rather than assumed.
 *   2. WHAT THE CURSOR KEYS SEND. event_key_t carries `ascii` and a `flags`
 *      byte that is negative when there is no ASCII -- which is what an arrow
 *      key must be. The shared header needs KB_UP and KB_DOWN, so the RAW
 *      code is the only thing that can identify them.
 *   3. WHETHER RELEASES COME THROUGH TOO. key.PRESSED and key.RELEASED are
 *      separate types, and a reader that counts both gets every keystroke
 *      twice.
 *
 * IT LOGS RATHER THAN SUMMARISES, and that is deliberate. #39 on the list is a
 * probe that could not fail, written the same hour I wrote up the last one:
 * it reported a value that a dead register produces just as readily as a live
 * one. A log of raw event bytes with a count beside it cannot do that -- an
 * empty log is visibly empty.
 */
#include "f256kern.h"

#define IOCTRL (*(volatile unsigned char *)0x0001)
#define MATRIX ((volatile unsigned char *)0xC000)
#define COLS 80
#define ROWS 60
#define LOGN 48

TREK_SIGNATURE;
__attribute__((used, retain)) volatile unsigned char ran;
__attribute__((used, retain)) volatile unsigned int  ev_total;
__attribute__((used, retain)) volatile unsigned char pre_queued;  /* before any key */
/* A ring the host can read: type, raw, ascii, flags for each logged event. */
__attribute__((used, retain)) volatile unsigned char log_type[LOGN];
__attribute__((used, retain)) volatile unsigned char log_raw[LOGN];
__attribute__((used, retain)) volatile unsigned char log_ascii[LOGN];
__attribute__((used, retain)) volatile unsigned char log_flags[LOGN];
__attribute__((used, retain)) volatile unsigned char log_n;

static struct f256_event ev;
static volatile unsigned char ev_empty;

static void pump(void)
{
    __asm__ volatile(
        "        jsr $ff00\n"        /* NextEvent; carry set == queue empty */
        "        lda #0\n"
        "        rol a\n"
        "        sta ev_empty\n"
        ::: "a", "x", "y", "memory", "p");
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
static void hex2(unsigned char x, unsigned char y, unsigned char v, unsigned char col)
{
    static const char h[] = "0123456789ABCDEF";
    cell(x, y, (unsigned char)h[v >> 4], col);
    cell((unsigned char)(x+1), y, (unsigned char)h[v & 15], col);
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

static void show(unsigned char i)
{
    unsigned char y = (unsigned char)(8 + (i & 23)) ;
    unsigned char a = log_ascii[i];
    hex2(4 + ((i >= 24) ? 48 : 0), y, i, 0x80);
    hex2(10, y, log_type[i], 0xE0);
    /* Name the two types that matter, so the log reads without a struct in
       the other hand. */
    text(14, y, log_type[i] == EV(key.PRESSED)  ? "PRESSED " :
                log_type[i] == EV(key.RELEASED) ? "RELEASED" : "--------",
         log_type[i] == EV(key.PRESSED) ? 0xA0 : 0x80);
    hex2(25, y, log_raw[i], 0xB0);
    hex2(31, y, a, 0xF0);
    /* The character itself, when it is printable. A code with no glyph would
       otherwise look identical to one with a blank glyph. */
    cell(35, y, (a >= 32 && a < 127) ? a : '.', 0xD0);
    hex2(40, y, log_flags[i], 0xC0);
    text(45, y, (log_flags[i] & 0x80) ? "NO ASCII" : "        ", 0xC0);
}

int main(void)
{
    unsigned char i;

    ran = 0x11;
    clear();
    text(2, 1, "F256K KEY PROBE -- RAW EVENT BYTES, LOGGED NOT SUMMARISED", 0xE0);
    text(2, 3, "PRE-QUEUED AT STARTUP $", 0x90);
    text(2, 4, "TOTAL EVENTS          $", 0x90);
    text(4, 6, "#    TYPE            RAW    ASCII     FLAGS", 0x90);

    K_ARGS_EVENT = &ev;

    /* WHAT WAS ALREADY WAITING, counted before anything else touches the
       queue. This is the number kb_init exists for. */
    pre_queued = 0;
    for (;;) {
        pump();
        if (ev_empty) break;
        if (pre_queued < 255) pre_queued++;
        if (log_n < LOGN) {
            log_type[log_n] = ev.type;
            log_raw[log_n] = ev.data[1];
            log_ascii[log_n] = EV_KEY_ASCII(ev);
            log_flags[log_n] = EV_KEY_FLAGS(ev);
            show(log_n);
            log_n++;
        }
    }
    hex2(26, 3, pre_queued, 0xF0);
    ran = 0x5A;

    /* Then log everything that arrives, forever. */
    for (;;) {
        pump();
        if (ev_empty) continue;
        ev_total = ev_total + 1;
        hex2(26, 4, (unsigned char)ev_total, 0xF0);
        if (log_n < LOGN) {
            log_type[log_n] = ev.type;
            log_raw[log_n] = ev.data[1];
            log_ascii[log_n] = EV_KEY_ASCII(ev);
            log_flags[log_n] = EV_KEY_FLAGS(ev);
            show(log_n);
            log_n++;
        }
        (void)i;
    }
}

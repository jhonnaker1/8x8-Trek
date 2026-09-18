#include <stdint.h>
#include "input.h"
#include "f256evt.h"

static struct f256_event ev;
static volatile unsigned char ev_empty;

struct f256_event f256_file_ev;
unsigned char f256_file_ready;
unsigned char f256_file_lost;

#define KEYRING 8
static unsigned char ring[KEYRING];
static unsigned char ring_in, ring_out;

/* ONE ASM BLOCK, because the carry IS the return value. Splitting the call
   from the test lets the compiler put flag-touching instructions in between,
   which is also why "p" is in the clobber list. */
static void next_event(void)
{
    __asm__ volatile(
        "        jsr $ff00\n"       /* NextEvent; carry set == queue empty */
        "        lda #0\n"
        "        rol a\n"
        "        sta ev_empty\n"
        ::: "a", "x", "y", "memory", "p");
}

/* The kernel's frame counter. SetTimer with the QUERY bit queues nothing and
   returns kernel.ticks in A. Measured at 60.00 Hz. */
static volatile unsigned char frame_lo;

unsigned char f256_frames(void)
{
    __asm__ volatile(
        "        lda #$80\n"      /* TIMER_FRAMES | TIMER_QUERY */
        "        sta $f3\n"       /* timer.units -- THE UNION STARTS AT $F3 */
        "        jsr $fff0\n"     /* SetTimer */
        "        sta frame_lo\n"
        ::: "a", "x", "y", "memory", "p");
    return frame_lo;
}

/* The C128's UPPERCASE ASCII is what the rest of the port is written for, so
   the fold happens here rather than at every comparison -- the same choice
   input.c and x16input.c made, and it keeps main.c's dispatcher identical on
   every machine.

   THE ARROWS ARE MATCHED ON RAW, NOT ASCII. They do carry ASCII ($10 and
   $0E), but those are control codes that other paths could plausibly produce;
   the raw code is the key itself and cannot be anything else. Measured in
   src/keyprobe.c. */
static unsigned char translate(unsigned char raw, unsigned char ascii)
{
    if (raw == 0xB6) return KB_UP;
    if (raw == 0xB7) return KB_DOWN;
    if (ascii == 0x03) return KB_ESC;       /* RUN/STOP -- there is no ESC */
    if (ascii == 0x08) return KB_DELETE;
    if (ascii >= 'a' && ascii <= 'z') return (unsigned char)(ascii - 32);
    return ascii;
}

/* IS THIS A FILE OR DIRECTORY EVENT? They are a contiguous block of types --
   file.NOT_FOUND through directory.DELETED -- so one range test covers both,
   and it stays correct if the kernel adds a member inside either namespace.
   It would NOT stay correct if a member were added between them, which is
   exactly the kind of change that moved clock.TICK by four and made uno's
   vendored header wrong. */
#define IS_FILE_EVENT(t) ((t) >= EV(file.NOT_FOUND) && (t) <= EV(directory.DELETED))

unsigned char f256_pump(void)
{
    unsigned char c;

    /* Re-stated every time: cheap, and the arg block is zero page that any
       kernel call could have walked on -- see f256kern.h on the union. */
    K_ARGS_EVENT = &ev;
    next_event();
    if (ev_empty) return 0;

    if (ev.type == EV(key.PRESSED)) {
        /* flags bit 7 means the key has no ASCII at all -- a modifier.
           Dropping those here is what keeps SHIFT from registering as a
           keystroke of its own, which the probe logged it doing. */
        if (!(EV_KEY_FLAGS(ev) & 0x80)) {
            c = translate(ev.data[1], EV_KEY_ASCII(ev));
            if (c != KB_NONE) {
                unsigned char n = (unsigned char)((ring_in + 1) % KEYRING);
                /* A full ring drops the NEWEST key, not the oldest: the game
                   is never more than a few keys behind, and losing the one
                   the player is waiting on is worse than losing type-ahead. */
                if (n != ring_out) { ring[ring_in] = c; ring_in = n; }
            }
        }
    } else if (IS_FILE_EVENT(ev.type)) {
        if (f256_file_ready) f256_file_lost++;
        f256_file_ev = ev;
        f256_file_ready = 1;
    }
    /* Everything else -- key.RELEASED, timers, the mouse -- is discarded on
       purpose. Nothing in this port waits on one. */
    return 1;
}

unsigned char f256_getkey(void)
{
    unsigned char c;
    if (ring_out == ring_in) return KB_NONE;
    c = ring[ring_out];
    ring_out = (unsigned char)((ring_out + 1) % KEYRING);
    return c;
}

void f256_drain(void)
{
    while (f256_pump()) { }
    f256_file_ready = 0;
    ring_in = ring_out = 0;
}

unsigned char f256_wait_file(unsigned char frames)
{
    unsigned char start = f256_frames();
    unsigned char elapsed = 0;

    for (;;) {
        /* PUMP UNTIL EMPTY BEFORE CHECKING THE CLOCK. The queue can hold the
           answer already, and a version that checked the deadline first would
           make the timeout a race against however much else was queued. */
        while (f256_pump()) {
            if (f256_file_ready) {
                f256_file_ready = 0;
                return f256_file_ev.type;
            }
        }
        /* The counter is a byte and wraps every 256 frames, so elapsed time is
           accumulated from DIFFERENCES rather than compared against a
           deadline -- a deadline computed as start+frames is wrong the moment
           it wraps past zero. */
        {
            unsigned char now = f256_frames();
            unsigned char d = (unsigned char)(now - start);
            start = now;
            if ((unsigned int)elapsed + d >= frames) return F256_WAIT_TIMEOUT;
            elapsed = (unsigned char)(elapsed + d);
        }
    }
}

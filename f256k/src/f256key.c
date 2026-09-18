/* Keyboard for the F256K, through FoenixMCP's event queue.
 *
 * THE QUEUE IS SHARED, AND THAT IS THE WHOLE DESIGN PROBLEM. One NextEvent
 * queue carries keystrokes, every byte of file I/O, and the kernel's timers.
 * A key wait that drains it while a load is in flight eats the file's data
 * events; a file read that drains it while the player is typing eats the
 * keys. So there is ONE pump in this port, here, and it sorts: key events go
 * into the ring below, everything else is held for the storage layer.
 *
 * WHAT IT WILL NOT DO IS DROP SOMETHING QUIETLY. If a second non-key event
 * arrives before the first is claimed, `f256_other_lost` counts it. That
 * counter existing is the difference between a storage bug that shows up as a
 * failed load and one that shows up as a file that is subtly short.
 *
 * MEASURED, not assumed -- src/keyprobe.c logged the raw event bytes:
 *
 *     type 8  = key.PRESSED     type 10 = key.RELEASED   (BOTH arrive;
 *                                          counting both doubles every key)
 *     letters   raw = unshifted ASCII, ascii = the shifted character
 *     ENTER     raw $94  ascii $0D
 *     DEL/BKSP  raw $92  ascii $08
 *     RUN/STOP  raw $BC  ascii $03   -- this keyboard is a C64 layout and has
 *                                       no ESC key at all
 *     CRSR UP   raw $B6  ascii $10
 *     CRSR DOWN raw $B7  ascii $0E
 *
 * And TWO EVENTS ARE ALREADY QUEUED when the game starts -- a file.CLOSED
 * from pexec closing the PGZ it just loaded, and a timer. Neither is a key,
 * so the Amiga's fault (the RETURN that launched the game dismissing the
 * title screen) cannot happen here -- but kb_init drains anyway, because
 * "cannot happen" is a claim about a queue nobody has looked in lately.
 */
#include <stdint.h>
#include "input.h"
#include "f256kern.h"

/* The sound driver does not exist yet. Every other port calls snd_poll()
   INSIDE the key wait, because that loop is where the program spends its idle
   time and is the driver's only chance to run -- so the call site is written
   now and the stub goes away when tedsnd's opposite number lands. Writing it
   later means finding this loop again, and the Plus/4's music played at
   double speed because a timing detail was not settled at its call site. */
#ifndef TREK_F256_SOUND
static void snd_poll(void) { }
#else
void snd_poll(void);
#endif

uint16_t kb_entropy = 0;

/* ------------------------------------------------------------- the pump */

static struct f256_event ev;
static volatile unsigned char ev_empty;

/* Held for the storage layer: the most recent event that was not a key. */
struct f256_event f256_other;
unsigned char f256_other_ready;
unsigned char f256_other_lost;

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

/* The C128's UPPERCASE ASCII is what the rest of the port is written for, so
   the fold happens here rather than at every comparison -- the same choice
   input.c and x16input.c made, and it keeps main.c's dispatcher identical on
   every machine.

   THE ARROWS ARE MATCHED ON RAW, NOT ASCII. They do carry ASCII ($10 and
   $0E), but those are control codes that other paths could plausibly produce;
   the raw code is the key itself and cannot be anything else. */
static unsigned char translate(unsigned char raw, unsigned char ascii)
{
    if (raw == 0xB6) return KB_UP;
    if (raw == 0xB7) return KB_DOWN;
    if (ascii == 0x03) return KB_ESC;       /* RUN/STOP -- there is no ESC */
    if (ascii == 0x08) return KB_DELETE;
    if (ascii >= 'a' && ascii <= 'z') return (unsigned char)(ascii - 32);
    return ascii;
}

/* Take one event off the queue and sort it. Returns 1 if anything was taken.
   THE ONLY PLACE IN THIS PORT THAT CALLS NextEvent. */
unsigned char f256_pump(void)
{
    unsigned char c;

    K_ARGS_EVENT = &ev;        /* re-stated every time: cheap, and the arg
                                  block is zero page that anything could have
                                  walked on -- see f256kern.h on the union */
    next_event();
    if (ev_empty) return 0;

    if (ev.type == EV(key.PRESSED)) {
        /* flags negative means the key has no ASCII at all -- a modifier.
           Dropping those here is what keeps SHIFT from registering as a
           keystroke of its own, which the probe logged it doing. */
        if (!(EV_KEY_FLAGS(ev) & 0x80)) {
            c = translate(ev.data[1], EV_KEY_ASCII(ev));
            if (c != KB_NONE) {
                unsigned char n = (unsigned char)((ring_in + 1) % KEYRING);
                /* A full ring drops the NEWEST key, not the oldest: the game
                   is never more than a few keys behind, and losing the one
                   the player is waiting on is worse than losing the one they
                   typed ahead. */
                if (n != ring_out) { ring[ring_in] = c; ring_in = n; }
            }
        }
    } else if (ev.type != EV(key.RELEASED)) {
        if (f256_other_ready) f256_other_lost++;
        f256_other = ev;
        f256_other_ready = 1;
    }
    return 1;
}

/* ------------------------------------------------------------- the API */

void kb_init(void)
{
    /* Drain whatever the launch left behind, keys and all. Measured: two
       events, a file.CLOSED and a timer. */
    while (f256_pump()) { }
    f256_other_ready = 0;
    f256_other_lost = 0;
    ring_in = ring_out = 0;
}

char kb_waitkey(void)
{
    unsigned char c;
    for (;;) {
        /* kb_entropy is bumped once per pass and sampled when the player
           answers. How long a human takes to reach a key is this port's only
           entropy source -- before the C128 had this, every game was the same
           galaxy. */
        kb_entropy++;
        snd_poll();
        f256_pump();
        if (ring_out != ring_in) {
            c = ring[ring_out];
            ring_out = (unsigned char)((ring_out + 1) % KEYRING);
            return (char)c;
        }
    }
}

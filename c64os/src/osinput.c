#include <stdint.h>

#include "input.h"
#include "sid.h"
#include "vdc.h"
#include "c64os.h"

/* Keyboard for C64 OS. The seam is one blocking call, kb_waitkey(), as on
 * every port.
 *
 * C64 OS SCANS THE KEYBOARD ITSELF, from its IRQ, into a queue; the probe
 * showed a key written into that queue (`$0277`, count `$C6`) reaching the
 * app, and showed that the key is what readkprnt_ returns -- NOT the value a
 * Kprnt callback is handed, which read $09 for an injected $41. The game does
 * not return to C64 OS's event loop while it plays, so it polls the queue
 * here instead of waiting for events. See src/app.s, os_readkey.
 *
 * THE POLL LOOP IS WHERE THREE THINGS HAPPEN, as on every other port: the
 * screen is brought up to date (the game draws into C64 OS's buffers, see
 * osvid.c), snd_poll() runs the music, and kb_entropy counts -- it is what
 * picks the galaxy when the player answers the setup screen. */

uint16_t kb_entropy;
volatile unsigned char kb_inject;

/* Anything already queued -- the double-click that launched the game leaves
   nothing, but a key held while C64 OS was loading would -- belongs to
   before the game. */
void kb_init(void) {
    while (os_readkey() != 0xFFFFu) { }
    c64os_stage = 2;
}

/* C64 OS delivers PETSCII in its lowercase set: a-z at $41-$5A, A-Z at
   $C1-$DA. The game compares against capitals ($41-$5A), so both fold there.
   0 means "no key the game has a use for". */
static char translate(unsigned char k) {
    if (k == 0x0D) return KB_RETURN;
    if (k == 0x14) return KB_DELETE;
    if (k == 0x91) return KB_UP;
    if (k == 0x11) return KB_DOWN;
    if (k == 0x03 || k == 0x1B || k == 0x5F) return KB_ESC;   /* STOP, ESC, <- */
    if (k >= 0xC1 && k <= 0xDA) return (char)(k - 0x80);
    if (k >= 0x20 && k <= 0x5A) return (char)k;
    return 0;
}

char kb_waitkey(void) {
    c64os_stage = 50;
    osv_flush();
    for (;;) {
        unsigned int k;
        char c;
#ifdef TREK_DEBUG_INPUT
        if (kb_inject) {
            c = (char)kb_inject;
            kb_inject = 0;
            return c;
        }
#endif
        kb_entropy++;
        snd_poll();
        k = os_readkey();
        if (k != 0xFFFFu && (c = translate((unsigned char)k)) != 0)
            return c;
    }
}

/* Keyboard for the NeXT.
 *
 * The seam is one blocking call, kb_waitkey(), and on this machine a key is
 * an EVENT: nextvid.m's wait_vsync() hands the AppKit its events, the
 * console's view takes each key-down, and this file takes them off the
 * queue it fills -- the Mac's arrangement (mac/src/macinput.c), with the
 * NeXT's codes.
 *
 * THE POLL LOOP IS NOT AN IDLE SPIN: kb_entropy is the only source of
 * randomness, sampled when the player answers the setup screen, and
 * snd_poll() is the sound driver's only chance to run. Here the loop also
 * keeps the application alive: a NeXT program that stops taking events
 * freezes its menu and its window with it.
 */
#include <stdint.h>

#include "../../c128/src/input.h"
#include "../../c128/src/sid.h"
#include "../../c128/src/vdc.h"
#include "nextvid.h"

uint16_t kb_entropy;

/* Throw away anything typed before the title screen asked for a key -- the
   RETURN that started the game from a Terminal must not dismiss it. */
void kb_init(void) { next_flush_keys(); }

/* THE NeXT's "delete" KEY SENDS 127, and the shared code deletes on 20 --
   PETSCII's DEL. Backspace (8) is folded too, for a keyboard that sends
   it. Letters fold to upper case: main.c dispatches on upper case. */
static char translate(int c)
{
    if (c == NEXT_KEY_UP)   return KB_UP;
    if (c == NEXT_KEY_DOWN) return KB_DOWN;
    if (c == 3 || c == 10)  return KB_RETURN;   /* keypad Enter; a newline */
    if (c == 8 || c == 127) return KB_DELETE;
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return (char)c;
}

char kb_waitkey(void)
{
    for (;;) {
        int k = next_next_key();
        if (k) {
            if (k == NEXT_KEY_LEFT || k == NEXT_KEY_RIGHT) continue;
            return translate(k);
        }
        snd_poll();
        kb_entropy++;
        wait_vsync();
    }
}

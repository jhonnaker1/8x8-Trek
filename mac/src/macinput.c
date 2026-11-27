/* Keyboard for the Macintosh.
 *
 * The seam is one blocking call, kb_waitkey(), and on this machine a key is
 * an EVENT: src/macvid.c's wait_vsync() runs the Toolbox event loop and puts
 * each key-down's character code on a queue, and this file takes them off.
 *
 * THE POLL LOOP IS NOT AN IDLE SPIN, and the two things every port does in
 * it are here for the reasons they are everywhere (see falcon/src/
 * falconinput.c): kb_entropy is the only source of randomness, sampled when
 * the player answers the setup screen, and snd_poll() is the sound driver's
 * only chance to run -- the Atari 8-bit port shipped silent for want of it.
 * Here the loop also keeps the MAC alive: a program that stops calling the
 * event loop freezes the menu bar and every desk accessory with it.
 */
#include <stdint.h>

#include "../../c128/src/input.h"
#include "../../c128/src/sid.h"
#include "../../c128/src/vdc.h"
#include "macvid.h"

/* The Mac's arrow keys arrive as these character codes (Inside Macintosh
   V, the Apple Extended and Standard keyboards). Left and right have no job
   in the game. */
#define MAC_LEFT   0x1C
#define MAC_RIGHT  0x1D
#define MAC_UP     0x1E
#define MAC_DOWN   0x1F
#define MAC_ENTER  0x03   /* the keypad's Enter, which a player means as Return */
#define MAC_BS     0x08   /* the key marked "delete" on a Mac */
#define MAC_DEL    0x7F   /* forward delete, on extended keyboards */

uint16_t kb_entropy;

/* Throw away anything typed before the title screen asked for a key -- on
   the Amiga the RETURN that launched the game dismissed the title. */
void kb_init(void) { mac_flush_keys(); }

/* BACKSPACE IS 8 HERE AND THE SHARED CODE DELETES ON 20 -- PETSCII's DEL,
   tested by read_field() and ui_read_command(). The Amiga measured the same
   trap. Letters fold to upper case: main.c dispatches on upper case, and on
   the X16 not folding made every typed order answer NO SUCH ORDER. */
static char translate(int c)
{
    if (c == MAC_UP)    return KB_UP;
    if (c == MAC_DOWN)  return KB_DOWN;
    if (c == MAC_ENTER) return KB_RETURN;
    if (c == MAC_BS || c == MAC_DEL) return KB_DELETE;
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return (char)c;
}

char kb_waitkey(void)
{
    for (;;) {
        int k = mac_next_key();
        if (k) {
            if (k == MAC_LEFT || k == MAC_RIGHT) continue;
            return translate(k);
        }
        snd_poll();
        kb_entropy++;
        wait_vsync();
    }
}

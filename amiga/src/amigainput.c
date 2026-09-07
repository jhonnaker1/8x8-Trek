/* Keyboard for the Amiga.
 *
 * THE DULLEST INPUT SEAM ON THE PROJECT, and that is the point. The C128
 * scans CIA1's matrix behind the KERNAL's back through a hand-transcribed
 * table of fifty row/column pairs -- a table that shipped for weeks holding
 * only the letters some command needed, so the self-destruct password JAMIE
 * could not be typed, and that needs two `make verify` checks of its own. The
 * X16 needed a probe to find out that GETIN returns lower-case ASCII and not
 * the PETSCII its own comment claimed. Here Intuition hands over a character.
 * No table, no encoding to measure, nothing to verify.
 *
 * VANILLAKEY FOR CHARACTERS, RAWKEY FOR THE ARROWS. Intuition converts a
 * keystroke to ASCII through the user's own keymap and delivers it as
 * IDCMP_VANILLAKEY -- so a non-US keyboard works without this file knowing
 * anything about it. The cursor keys have no ASCII, so they arrive only as
 * IDCMP_RAWKEY, and this takes exactly two raw codes from that stream and
 * discards the rest. Discarding is what keeps a letter from arriving twice
 * when both message classes are enabled.
 *
 * MEASURED 2026-09-06 rather than assumed -- see amiga/README.md.
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include "../../c128/src/input.h"

/* Raw keycodes. Only two, and they are the ONLY hand-written keyboard
   constants in this port -- the arrows are the original's primary binding for
   shields ("Shields Up (use up arrow)" in EGATREK.REF), so they have to work. */
#define RAW_UP    0x4C
#define RAW_DOWN  0x4D

uint16_t kb_entropy;

extern struct Window *amiga_window(void);

/* Drain whatever Intuition queued while the screen was being set up, so the
   first thing kb_waitkey() returns is a key the player actually pressed. */
void kb_init(void) {
    struct Window *w = amiga_window();
    struct IntuiMessage *msg;

    if (!w) return;
    while ((msg = (struct IntuiMessage *)GetMsg(w->UserPort)) != NULL)
        ReplyMsg((struct Message *)msg);
}

/* The rest of the port is written for the C128's scanner, which hands over
   UPPERCASE ASCII, so fold case here exactly as x16input.c does. main.c's
   dispatcher then reads the same on every machine -- and on the X16 not
   folding it made every typed order answer NO SUCH ORDER. */
static char fold(unsigned char c) {
    /* BACKSPACE IS 8 HERE AND THE SHARED CODE DELETES ON 20. MEASURED, and it
       would have shipped silently: read_field() and ui_read_command() both
       test KB_DELETE, which is PETSCII's 20, so on this machine backspace did
       nothing at all and the commander's name could be typed but not
       corrected. The Del key sends 127 and means the same thing to a player,
       so both land on KB_DELETE. ESC measured as 27 and needs no mapping. */
    if (c == 8 || c == 127) return KB_DELETE;
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return (char)c;
}

char kb_waitkey(void) {
    struct Window *w = amiga_window();
    struct IntuiMessage *msg;

    if (!w) return KB_RETURN;   /* no window: never block the game forever */

    for (;;) {
        while ((msg = (struct IntuiMessage *)GetMsg(w->UserPort)) != NULL) {
            ULONG  class = msg->Class;
            UWORD  code  = msg->Code;
            ReplyMsg((struct Message *)msg);

            if (class == IDCMP_VANILLAKEY)
                return fold((unsigned char)code);

            if (class == IDCMP_RAWKEY) {
                /* Key-up sets the top bit; take presses only, and take only
                   the two codes that have no character of their own. */
                if (code == RAW_UP)   return KB_UP;
                if (code == RAW_DOWN) return KB_DOWN;
            }
        }

        /* WAIT ON THE DISPLAY, NOT ON A SPIN. WaitTOF() blocks on the vertical
           blank interrupt, so this costs no CPU on a machine that is running
           other things -- but it still passes here fifty or sixty times a
           second, which is what kb_entropy needs.

           kb_entropy is the port's ONLY entropy source: it counts passes and
           is sampled when the player answers the setup screen, so how long a
           human takes to reach a key is what picks the galaxy. Before the
           C128 had this, every game was the same one. */
        kb_entropy++;
        WaitTOF();
    }
}

/* Keyboard for the Atari 800XL. The input seam, fifth port.
 *
 * THE DRIVER CARRIES NO TRANSLATION TABLE, and that is the whole trick.
 *
 * The OS keyboard IRQ writes a raw key code into CH ($02FC) -- $FF when
 * nothing is waiting, bit 6 for shift, bit 7 for ctrl. That code is a
 * KEYBOARD MATRIX POSITION, not a character. Every other port here decodes
 * such a thing with a table of its own: the C128 scans CIA1's matrix and
 * carries one, and the X16 shipped a header comment that had REASONED about
 * its encoding and was wrong.
 *
 * The Atari already has the table. KEYDEF ($0079/$007A) points at a 192-byte
 * ROM table -- 64 unshifted, 64 shifted, 64 control -- indexed by exactly the
 * byte CH holds, giving ATASCII. So this driver is one indexed load and a
 * small fold from ATASCII to the ASCII c128/src/input.h names.
 *
 * MEASURED, NOT ASSUMED, with tools/keytable.py and a read of the table
 * itself through the AltirraBridge (KEYDEF -> $FB51 on the XL OS this
 * profile runs). Nine injected keys cross-checked against their entries,
 * and the four the bridge has no name for came out of the table directly:
 *
 *     +  $06     *  $07     :  $42 (shift ;)    @  $75 (shift 8)
 *     up $8E (ctrl -)       down $8F (ctrl =)
 *
 * That last pair is why reading the table beat injecting keys: the Atari's
 * cursor keys ARE ctrl-minus and ctrl-equals, and no amount of asking the
 * emulator for a key called "UP" was going to say so.
 */
#include <stdint.h>

#include "input.h"

#define CH     (*(volatile unsigned char *)0x02FC)   /* OS key code, $FF = none */
#define KEYDEF (*(const unsigned char *const *)0x0079)
#define SKSTAT (*(volatile unsigned char *)0xD20F)   /* POKEY; bit2 = 0 while a key is down */

uint16_t kb_entropy;

/* Drain whatever the keyboard was holding when the game started. It matters
   most where the machine QUEUES keystrokes -- the Amiga's title screen
   dismissed itself on the RETURN that launched the program -- and CH is a
   one-byte queue that does exactly that. */
void kb_init(void) {
    CH = 0xFF;
}

/* ATASCII -> the ASCII values c128/src/input.h names.
 *
 * UPPERCASED, because the Atari returns lowercase for an unshifted letter
 * and every command and every free-text field in this game is uppercase.
 * The X16 found the same thing about its KERNAL and only a probe said so.
 *
 * Four codes are not ASCII on either side and are folded by hand: EOL is
 * $9B and not 13, the Atari's delete is $7E, and its cursor keys produce
 * $1C/$1D where input.h uses 1 and 2 from the unused control range. ESC is
 * $1B on both and needs nothing. */
static char to_ascii(unsigned char at) {
    if (at >= 0x61 && at <= 0x7A) return (char)(at - 0x20);   /* a-z -> A-Z */
    if (at >= 0x20 && at <= 0x5F) return (char)at;            /* space..Z, @ [ ] */
    switch (at) {
        case 0x9B: return KB_RETURN;
        case 0x7E: return KB_DELETE;
        case 0x1B: return KB_ESC;
        case 0x1C: return KB_UP;
        case 0x1D: return KB_DOWN;
        default:   return KB_NONE;
    }
}

/* Blocks until one key is pressed and released, and bumps kb_entropy once per
   pass -- the port's only source of entropy, exactly as on the C128, because
   how long a human takes to answer a prompt is unpredictable at this
   resolution and a constant seed makes every game the same game.
 *
 * THE RELEASE WAIT IS REASONED, NOT MEASURED. SKSTAT bit 2 reads 0 while a
 * key is held, so waiting for it to come back stops the OS's auto-repeat
 * turning one press into a burst. That is what the POKEY documentation says
 * and it is not what a probe here proved: `KEY` on the bridge queues a
 * press-and-release and cannot hold a key down, so the rig cannot reach this
 * case. First real play is what will settle it. */
char kb_waitkey(void) {
    unsigned char code;
    char c;

    for (;;) {
        kb_entropy++;
        code = CH;
        if (code == 0xFF) continue;
        CH = 0xFF;
        if (code >= 192) continue;          /* shift+ctrl: not in the table */
        c = to_ascii(KEYDEF[code]);
        if (c == KB_NONE) continue;         /* a key this game has no use for */
        while ((SKSTAT & 0x04) == 0) {}     /* ...and released */
        return c;
    }
}

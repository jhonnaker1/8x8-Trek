/* Keyboard. $D610 is the MEGA65's ASCII key register: reading it gives the
 * ASCII code of the key waiting, zero when nothing is, and writing any value
 * pops that entry.
 *
 * THIS IS THE OTHER THING THE MACHINE BUYS. The C128 port scans CIA1's matrix
 * directly, because cgetc() goes dead once the port takes over the machine --
 * which means a hand-transcribed table of fifty row/column pairs, a `make
 * verify` check that the table is ASCII rather than PETSCII in the linked
 * binary, and a second check that it can spell every command word. That table
 * shipped for weeks with only the seventeen letters some command needed, so
 * the self-destruct password JAMIE could not be typed.
 *
 * None of that exists here. The hardware hands over ASCII, so every key on the
 * keyboard works by construction and the whole class of bug is gone.
 */
#include <stdint.h>
#include "m65input.h"

#define ASCIIKEY (*(volatile unsigned char *)0xD610)

/* $D610's cursor codes are PETSCII's, which the MEGA65 inherited. Map the two
   the game uses onto the port's own KB_UP/KB_DOWN and pass the rest through. */
#define RAW_CRSR_DOWN 0x11
#define RAW_CRSR_UP   0x91

uint16_t kb_entropy;

void kb_init(void) { ASCIIKEY = 0; }

static char translate(unsigned char c) {
    if (c == RAW_CRSR_UP)   return KB_UP;
    if (c == RAW_CRSR_DOWN) return KB_DOWN;
    if (c >= 'a' && c <= 'z') return (char)(c - 32);   /* the game is uppercase */
    if (c == 0x14 || c == 0x08) return KB_DELETE;      /* DEL and BACKSPACE */
    if (c == 0x1B) return KB_ESC;
    if (c >= 32 && c < 127) return (char)c;
    if (c == 0x0D) return KB_RETURN;
    return KB_NONE;
}

char kb_poll(void) {
    unsigned char c = ASCIIKEY;
    if (!c) return KB_NONE;
    ASCIIKEY = 0;                 /* pop it */
    return translate(c);
}

char kb_waitkey(void) {
    char k;
    /* The same entropy trick the C128 port uses: count the polls spent waiting
       for the player, and seed the galaxy from it. */
    for (;;) {
        kb_entropy++;
        k = kb_poll();
        if (k != KB_NONE) return k;
    }
}

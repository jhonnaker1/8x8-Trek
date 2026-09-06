/* Keyboard for the Commander X16.
 *
 * The X16 has a CBM-compatible KERNAL, so GETIN ($FFE4) hands over a key and
 * returns zero when none is waiting -- the same shape as the MEGA65's $D610
 * and NOT the C128's situation, where cgetc() goes dead once the port takes
 * over the machine and input.c has to scan CIA1's matrix directly.
 *
 * WHAT THAT AVOIDS, and it is the same win the MEGA65 records: the C128 needs
 * a hand-transcribed table of fifty row/column pairs, a `make verify` check
 * that the table is ASCII rather than PETSCII in the linked binary, and a
 * second check that it can spell every command word. That table shipped for
 * weeks holding only the letters some command needed, so the self-destruct
 * password JAMIE could not be typed. None of that exists here.
 *
 * ENCODING: GETIN returns PETSCII, and in the unshifted set the letters are
 * $41..$5A -- identical to the ASCII the KB_* constants in input.h are written
 * as. So letters and digits pass straight through; only the cursor keys need
 * mapping, exactly as on the MEGA65.
 */
#include <stdint.h>
#include "../../c128/src/input.h"
#include "../../c128/src/sid.h"

/* PETSCII cursor codes, which the X16 inherits from the CBM line. */
#define RAW_CRSR_DOWN 0x11
#define RAW_CRSR_UP   0x91

uint16_t kb_entropy;

#ifdef TREK_AUTOPLAY
/* SCRIPTED INPUT, COMPILED IN -- because x16emu offers no way to poke memory
   or fake a keypress from outside, so the MEGA65's kb_inject trick (a driver
   writes a byte, the game zeroes it) has no transport here. The keys are a
   table instead, and kb_waitkey serves them in order then blocks.
   Debug builds only; the game build has none of this. */
static const char autoplay[] = {
    13,                     /* title -> setup            */
    'N', 13,                /* no briefing               */
    'N', 13,                /* no restore                */
    'J','A','M','I','E', 13,/* commander name            */
    '3', 13,                /* skill level               */
    'X', 13,                /* accept and start          */
    'W', '5', 13            /* WARP 5 -- the exact keys that froze the
                               machine with a voice sounding. If the build
                               gets past this, the dead-clock fix holds. */
};
static unsigned char ap_at = 0;
#endif

static unsigned char getin(void) {
    unsigned char c;
    __asm__ volatile("jsr $FFE4\n sta %0\n" : "=r"(c) :: "a", "x", "y");
    return c;
}

void kb_init(void) { while (getin()) { } }   /* drain anything already queued */

static char translate(unsigned char c) {
    if (c == RAW_CRSR_UP)   return KB_UP;
    if (c == RAW_CRSR_DOWN) return KB_DOWN;
    return (char)c;
}

char kb_waitkey(void) {
    unsigned char c;

#ifdef TREK_AUTOPLAY
    if (ap_at < sizeof autoplay) return autoplay[ap_at++];
#endif

    /* kb_entropy is bumped once per pass and sampled when the player answers.
       How long a human takes to reach a key is the port's only entropy source
       -- before the C128 had this, every game was the same galaxy.

       snd_poll() lives inside the wait here for the same reason it does on the
       other two ports: this loop is where the program spends its idle time,
       and it is the driver's only chance to run. */
    for (;;) {
        kb_entropy++;
        snd_poll();
        c = getin();
        if (c) return translate(c);
    }
}

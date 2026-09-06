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
 * ENCODING, MEASURED 2026-09-06 and NOT what this comment used to claim.
 *
 * It said GETIN returns PETSCII, whose unshifted letters are $41..$5A and so
 * identical to the ASCII the KB_* constants in input.h are written as. That
 * was reasoned, not read, and it was wrong: a probe (src/keyprobe.c) that
 * prints the raw byte for every key gives
 *
 *     m w q  ->  6D 77 71        unshifted letters are LOWERCASE ASCII
 *     M W Q  ->  4D 57 51        shifted ones are uppercase
 *     RETURN ->  0D    BACKSPACE ->  14    UP ->  91    DOWN ->  11
 *
 * so everything except the letters already lines up, and the letters were
 * one bit out. main.c dispatches commands by comparing cmd[0] against
 * uppercase ASCII, which meant EVERY typed order answered NO SUCH ORDER --
 * while the setup screen looked fine, because ask_yes() takes 'Y' or 'y',
 * read_field() stores whatever it is handed, and the screen-code converter in
 * x16vera.c draws 97..122 with the same glyphs as 65..90. Three separate
 * things quietly hid it.
 *
 * WHAT SETTLED IT: an autoplay build feeds 'W','5' past the keyboard
 * altogether and sets warp 5 correctly, so the dispatcher was never the
 * fault; and reading $0372 in the emulator said charset 2 with ISO off,
 * which PREDICTED PETSCII. The prediction lost to the probe.
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
/* LOWER CASE ON PURPOSE, and served through translate() below rather than
   returned raw: that is what a human's unshifted keys measure as, so this
   table exercises the case fold instead of stepping around it. Delete the
   fold and the console comes up saying NO SUCH ORDER -- which is how the
   fold was checked, since a test that cannot fail proves nothing. */
static const char autoplay[] = {
    13,                     /* title -> setup            */
    'n', 13,                /* no briefing               */
    'n', 13,                /* no restore                */
    'j','a','m','i','e', 13,/* commander name            */
    '3', 13,                /* skill level               */
    'x', 13,                /* accept and start          */
    'w', '5', 13,           /* WARP 5 -- the exact keys that froze the
                               machine with a voice sounding, and the exact
                               keys that then answered NO SUCH ORDER. The
                               console reads WARP 5.0 when both are fixed. */
    's', 13, 'x', 13,       /* SELF DESTRUCT. The password is what the setup
                               screen took above -- 'x' answered the PASSWORD
                               field, not an "accept" that does not exist. */
    13, 13, 13, 13, 13      /* through the loss memo, the evaluation and the
                               hall of fame, to the PLAY AGAIN prompt, which
                               is where the machine dropped into the monitor
                               at PC=$9840 -- inside the overlay window. */
};
static unsigned char ap_at = 0;
#endif

static unsigned char getin(void) {
    unsigned char c;
    __asm__ volatile("jsr $FFE4\n sta %0\n" : "=r"(c) :: "a", "x", "y");
    return c;
}

void kb_init(void) { while (getin()) { } }   /* drain anything already queued */

/* The rest of the port is written for the C128's scanner, which hands over
   UPPERCASE ASCII. Folding case here rather than at each comparison is the
   same choice input.c made, and it keeps main.c's dispatcher identical on
   all three machines.

   The $C1..$DA arm is defensive and is NOT what this machine was measured
   doing: it is where PETSCII keeps its shifted letters, so a machine that
   ever comes up in PETSCII mode -- another ROM revision, a keymap, a future
   KERNAL -- lands on the same uppercase rather than on NO SUCH ORDER. */
static char translate(unsigned char c) {
    if (c == RAW_CRSR_UP)   return KB_UP;
    if (c == RAW_CRSR_DOWN) return KB_DOWN;
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    if (c >= 0xC1 && c <= 0xDA) return (char)(c - 0x80);
    return (char)c;
}

char kb_waitkey(void) {
    unsigned char c;

#ifdef TREK_AUTOPLAY
    if (ap_at < sizeof autoplay) return translate((unsigned char)autoplay[ap_at++]);
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

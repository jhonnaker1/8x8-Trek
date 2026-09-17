#include <stdint.h>
#include "input.h"
#include "sid.h"

/* THE KEYBOARD, SCANNED OFF TED. This port cannot use the shared input.c and
 * cannot use the KERNAL, and the two reasons are different.
 *
 * NOT THE SHARED input.c: it reads the CIA1 matrix at $DC00/$DC01, which is
 * a C64 and C128 chip. A Plus/4 has no CIA -- $DC00 is plain RAM inside this
 * program's own image -- so key_down() was reading the game's data and
 * kb_waitkey() could never return. Worse, input.c's cursor-key handling is
 * `#ifdef __C128__ ... #else __C64__`, and mos-commodore-clang defines
 * NEITHER macro, so this port silently compiled the C64 branch: a shift-
 * decoded CRSR key on a machine that has four dedicated ones.
 *
 * NOT THE KERNAL EITHER, and this is the cost p4bank.c wrote down and never
 * paid: GETIN reads the buffer at KEYD ($0527), which the KERNAL's 60Hz IRQ
 * fills, and p4_ram_in() does `sei`, clears TED's interrupt mask at $FF0A and
 * points $FFFE at an RTI. No IRQ ever runs, so KEYD never fills, so GETIN
 * would return 0 forever. Scanning the hardware removes the dependency
 * instead of trying to keep the KERNAL's IRQ alive across a bank switch.
 *
 * HOW TED'S KEYBOARD PORT WORKS, from the Plus/4 Encyclopedia's matrix page:
 *
 *   1. write a row selector to $FD30 -- ACTIVE LOW, so ~(1 << row)
 *   2. WRITE to $FF08, which is what makes TED sample the keyboard lines
 *   3. READ $FF08 for the columns -- ALSO ACTIVE LOW, a 0 bit is pressed
 *
 * Step 2 is the one that is easy to miss: $FF08 is not a port that simply
 * reflects the lines, it is a latch, and the write is the trigger. Both
 * registers sit in the I/O window at $FD00..$FF3F, which is I/O whatever is
 * banked, so none of this needs the ROM in and none of it needs a bank shim.
 *
 * NO SEI/CLI AROUND THE STROBE, and that is a real difference from input.c
 * rather than an omission. On a C64 the KERNAL's IRQ strobes CIA1 for its own
 * key repeat and can land between the column write and the row read, so the
 * pair has to be atomic. Here interrupts are off permanently and nothing else
 * in the machine touches $FD30, so the pair cannot be interrupted.
 */

#define KEY_LATCH (*(volatile unsigned char *)0xFD30)   /* row select, write */
#define KEY_PORT  (*(volatile unsigned char *)0xFF08)   /* write to sample,
                                                           then read columns */

typedef struct {
    unsigned char row;   /* 0-7, the bit pulled low in $FD30 */
    unsigned char col;   /* 0-7, the bit tested in $FF08     */
    char          ch;    /* the KB_* this produces           */
} Key;

/* Transcribed from the Plus/4 Encyclopedia's keyboard/joystick matrix table,
   whose header row gives the column read-back values $FE $FD $FB $F7 $EF $DF
   $BF $7F -- so column N is bit N -- and whose selector column gives the same
   for the rows. Values are the KB_* constants and never character literals,
   for the reason input.h states. */
static const Key keys[] = {
    /* The three the game blocks on most. This machine has a REAL Escape key
       and four DEDICATED cursor keys, so unlike the C64 there is no shift to
       decode and no one physical key serving two directions. */
    { 1, 0, KB_RETURN }, { 0, 0, KB_DELETE }, { 4, 6, KB_ESC },
    { 3, 5, KB_UP },     { 0, 5, KB_DOWN },   { 4, 7, KB_SPACE },

    { 2, 1, KB_A }, { 4, 3, KB_B }, { 4, 2, KB_C }, { 2, 2, KB_D },
    { 6, 1, KB_E }, { 5, 2, KB_F }, { 2, 3, KB_G }, { 5, 3, KB_H },
    { 1, 4, KB_I }, { 2, 4, KB_J }, { 5, 4, KB_K }, { 2, 5, KB_L },
    { 4, 4, KB_M }, { 7, 4, KB_N }, { 6, 4, KB_O }, { 1, 5, KB_P },
    { 6, 7, KB_Q }, { 1, 2, KB_R }, { 5, 1, KB_S }, { 6, 2, KB_T },
    { 6, 3, KB_U }, { 7, 3, KB_V }, { 1, 1, KB_W }, { 7, 2, KB_X },
    { 1, 3, KB_Y }, { 4, 1, KB_Z },

    { 3, 4, KB_DIGIT0 + 0 }, { 0, 7, KB_DIGIT0 + 1 }, { 3, 7, KB_DIGIT0 + 2 },
    { 0, 1, KB_DIGIT0 + 3 }, { 3, 1, KB_DIGIT0 + 4 }, { 0, 2, KB_DIGIT0 + 5 },
    { 3, 2, KB_DIGIT0 + 6 }, { 0, 3, KB_DIGIT0 + 7 }, { 3, 3, KB_DIGIT0 + 8 },
    { 0, 4, KB_DIGIT0 + 9 },

    /* The unshifted punctuation, for the same reason input.h gives: a
       keyboard that can only spell the words the program already knows is not
       a keyboard. All eight are plain keys on this machine. */
    { 7, 0, KB_AT },    { 1, 6, KB_STAR },  { 2, 6, KB_SEMI },
    { 4, 5, KB_PERIOD },{ 5, 5, KB_COLON }, { 5, 6, KB_EQUALS },
    { 6, 5, KB_MINUS }, { 6, 6, KB_PLUS },  { 7, 5, KB_COMMA },
    { 7, 6, KB_SLASH },
};

#define KEY_COUNT (sizeof keys / sizeof keys[0])

static unsigned char key_down(unsigned char i)
{
    KEY_LATCH = (unsigned char)~(1 << keys[i].row);
    KEY_PORT  = 0xFF;                 /* THE WRITE IS THE SAMPLE TRIGGER */
    return (unsigned char)((KEY_PORT & (1 << keys[i].col)) == 0);
}

/* Index of the first key held, or 0xFF.

   snd_poll() IS INSIDE THE LOOP, not around it, and the C128's input.c
   records why: polling once per pass gave 82 samples a second against 50 or
   60 frames, frame detection needs at least two per frame, and the music ran
   45% slow. The same shape is kept here rather than rediscovered. */
static unsigned char scan(void)
{
    unsigned char i;
    for (i = 0; i < KEY_COUNT; i++) {
        snd_poll();
        if (key_down(i)) return i;
    }
    return 0xFF;
}

static void settle(void)
{
    unsigned int n;
    for (n = 0; n < 900; n++) { }
}

uint16_t kb_entropy = 0;

/* Nothing to drain: this port scans the matrix directly, so no buffer holds
   old keystrokes, and kb_waitkey() waits out anything still held. */
void kb_init(void) { }

char kb_waitkey(void)
{
    unsigned char i;

    /* One physical press must yield exactly one character rather than
       repeating for as long as a finger rests on the key. */
    while (scan() != 0xFF) { snd_poll(); }
    settle();

    for (;;) {
        kb_entropy++;
        i = scan();
        if (i != 0xFF) {
            settle();                 /* debounce the contact bounce */
            if (key_down(i)) return keys[i].ch;
        }
    }
}

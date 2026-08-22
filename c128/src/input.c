#include <6502.h>
#include "input.h"

#define CIA1_PRA (*(volatile unsigned char *)0xDC00)
#define CIA1_PRB (*(volatile unsigned char *)0xDC01)

/* Why not the KERNAL.
 *
 * cgetc() blocks forever in this port: the console draws, the cursor
 * appears, and no key ever arrives. commodore-uno's own C128 port hit the
 * same wall from a different direction and settled on this same answer --
 * its input.c records that "kbhit()/cgetc() go completely dead" there "for
 * reasons that resisted several fix attempts". Reading the matrix directly
 * removes the dependency entirely rather than guessing at which piece of
 * KERNAL state this program disturbed, and it is indifferent to the 2MHz
 * mode vdc_init() selects.
 *
 * Matrix positions come from VICE's own C128 keymap,
 *   .../share/vice/C128/gtk3_sym.vkm
 * whose lines read "keysym row column shiftflag". That file is the
 * authority here, not a published matrix table: commodore-uno's input.c
 * warns that a "standard" reference table "was already wrong once this
 * session for several other keys", so these were transcribed from the
 * keymap and nowhere else.
 *
 * The keymap's ROW is the bit pulled low on PRA; its COLUMN is the bit
 * tested in PRB. (commodore-uno stores the same pair under struct fields
 * named the other way round, which is easy to misread -- named plainly
 * here.) */
typedef struct {
    unsigned char pra;   /* keymap row -- the column strobe */
    unsigned char prb;   /* keymap column -- the line read back */
    char          ch;    /* ASCII produced */
} Key;

/* Values are the KB_* constants from input.h, never character literals --
   see the warning there. The digits are written as KB_DIGIT0 + n for the
   same reason. */
static const Key keys[] = {
    { 0, 1, KB_RETURN },
    { 0, 0, KB_DELETE },
    { 4, 4, KB_M }, { 1, 1, KB_W }, { 7, 6, KB_Q }, { 5, 2, KB_L }, { 2, 6, KB_T },
    { 2, 2, KB_D }, { 1, 6, KB_E }, { 1, 5, KB_S }, { 2, 7, KB_X },
    { 2, 1, KB_R },
    { 7, 0, KB_DIGIT0 + 1 }, { 7, 3, KB_DIGIT0 + 2 },
    { 1, 0, KB_DIGIT0 + 3 }, { 1, 3, KB_DIGIT0 + 4 },
    { 2, 0, KB_DIGIT0 + 5 }, { 2, 3, KB_DIGIT0 + 6 },
    { 3, 0, KB_DIGIT0 + 7 }, { 3, 3, KB_DIGIT0 + 8 },
    { 4, 0, KB_DIGIT0 + 9 }, { 4, 3, KB_DIGIT0 },
    { 5, 4, KB_PERIOD }, { 5, 7, KB_COMMA }, { 7, 4, KB_SPACE },
};

#define KEY_COUNT (sizeof keys / sizeof keys[0])

/* The KERNAL's 60Hz IRQ is still running -- we never disabled it -- and does
   its own strobe of CIA1 for the cursor and key repeat. If it lands between
   our column write and our row read it clobbers the column we just selected,
   so the pair has to be atomic. This is the detail commodore-uno's port
   documents having had to find. */
static unsigned char key_down(unsigned char i) {
    unsigned char save, pressed;

    SEI();
    save = CIA1_PRA;
    CIA1_PRA = (unsigned char)~(1 << keys[i].pra);
    pressed = (unsigned char)((CIA1_PRB & (1 << keys[i].prb)) == 0);
    CIA1_PRA = save;
    CLI();

    return pressed;
}

/* Index of the first key held, or 0xFF. */
static unsigned char scan(void) {
    unsigned char i;
    for (i = 0; i < KEY_COUNT; i++)
        if (key_down(i)) return i;
    return 0xFF;
}

static void settle(void) {
    unsigned int n;
    for (n = 0; n < 900; n++) { }
}

#ifdef TREK_DEBUG_INPUT
volatile unsigned char kb_inject = 0;
#endif

uint16_t kb_entropy = 0;

char kb_waitkey(void) {
    unsigned char i;

    /* Wait out whatever is still held, so one physical press yields exactly
       one character rather than repeating for as long as a finger rests on
       the key. */
    while (scan() != 0xFF) { }
    settle();

    for (;;) {
#ifdef TREK_DEBUG_INPUT
        /* Scripted input, debug builds only -- see input.h. One check, here
           in the poll loop, is all that is needed: this is where the port
           blocks, and a key poked between calls simply waits in the byte
           until the next entry.
           An earlier version had three copies of this and a
           `#pragma optimize (push, off)` around the function, added to defeat
           a supposed cc65 optimisation that was keeping the global in a
           register. That diagnosis was wrong. The real fault was outside the
           port entirely: VICE's binary monitor had stopped the machine, so
           nothing was executing at all. Removed rather than left in place --
           code kept for a refuted reason is worse than no comment. */
        if (kb_inject) {
            char c = (char)kb_inject;
            kb_inject = 0;
            return c;
        }
#endif
        kb_entropy++;
        i = scan();
        if (i != 0xFF) {
            settle();                /* debounce the contact bounce */
            if (key_down(i)) return keys[i].ch;
        }
    }
}

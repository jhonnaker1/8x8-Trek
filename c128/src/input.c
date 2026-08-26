#include <6502.h>
#include "input.h"
#include "sid.h"

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
    /* Y and N. Added 2026-08-22 for the play-again prompt, and they were
       overdue: ask_yes() has been on the setup screen since it was built and
       neither of its two questions could ever be answered YES, because the
       only key that reached it was RETURN, which ask_yes counts as no. The
       briefing the original offers has therefore been unreachable in this
       port for its whole life. */
    { 3, 1, KB_Y }, { 4, 7, KB_N },
    /* ESC, because EGA Trek's self-destruct prompt says "Hit ESC to abort"
       and a destruct sequence with no way out is not the same command. */
    { 7, 7, KB_ESC },
    /* C)hart and F)ix, both from the reference card. */
    { 2, 4, KB_C }, { 2, 5, KB_F },
    /* A, for A# -- acknowledge a message. */
    { 1, 2, KB_A },
    /* O)rbit, and U for USE. Added 2026-08-26 with the planet chain. LAND
       needs no new key -- L, A, N and D were all already here. */
    { 4, 6, KB_O }, { 3, 6, KB_U },
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

/* The C128's DEDICATED cursor keys, which the C64-style matrix above cannot
 * reach.
 *
 * The C64 has one shared CRSR key per axis: down unshifted, up shifted. The
 * C128 adds four separate keys, and they hang off three extra column lines
 * K0-K2 in the VIC-IIe's register $D02F rather than off CIA1's PRA. VICE's
 * keymap (the same authority as the table above) lists them as rows 8, 9 and
 * 10, and its "duplicate the cursor keys (c128 mode)" block gives
 *
 *     Up 10 3      Down 10 4      Left 10 5      Right 10 6
 *
 * so row 10 is K2 -- line index 2 here -- and the column is read back on PRB
 * exactly as before.
 *
 * Taking this route rather than the shared key is a deliberate choice: the
 * shared one would need the shift state decoded to tell up from down, and
 * every shift-reading bug this port might have had would land on a scrolling
 * viewer where holding a key is normal.
 *
 * PRA must be driven high while this runs, or a normal column is still
 * selected and the read is the union of two keys. */
#define VIC_KBD (*(volatile unsigned char *)0xD02F)

typedef struct {
    unsigned char line;  /* 0, 1, 2 for keymap rows 8, 9, 10 */
    unsigned char prb;   /* keymap column, as above */
    char          ch;
} ExtKey;

static const ExtKey extkeys[] = {
    { 2, 3, KB_UP }, { 2, 4, KB_DOWN },
};

#define EXT_COUNT (sizeof extkeys / sizeof extkeys[0])

/* Same SEI/CLI reasoning as key_down(): the KERNAL's 60Hz IRQ strobes the
   keyboard too, and it touches $D02F as well as CIA1, so BOTH have to be
   saved, driven and put back inside one atomic block. */
static unsigned char key_down_ext(unsigned char i) {
    unsigned char save_pra, save_k, pressed;

    SEI();
    save_pra = CIA1_PRA;
    save_k   = VIC_KBD;
    CIA1_PRA = 0xFF;
    VIC_KBD  = (unsigned char)~(1 << extkeys[i].line);
    pressed  = (unsigned char)((CIA1_PRB & (1 << extkeys[i].prb)) == 0);
    VIC_KBD  = save_k;
    CIA1_PRA = save_pra;
    CLI();

    return pressed;
}

/* One index space covers both tables: below KEY_COUNT is the CIA1 matrix,
   at or above it is `extkeys[i - KEY_COUNT]`. Callers never split them. */
static unsigned char key_down_any(unsigned char i) {
    if (i < KEY_COUNT) return key_down(i);
    return key_down_ext((unsigned char)(i - KEY_COUNT));
}

static char key_char(unsigned char i) {
    if (i < KEY_COUNT) return keys[i].ch;
    return extkeys[i - KEY_COUNT].ch;
}

/* Index of the first key held, or 0xFF. */
static unsigned char scan(void) {
    unsigned char i;
    for (i = 0; i < KEY_COUNT + EXT_COUNT; i++) {
        /* Sound advances HERE, inside the matrix scan, not once per pass
           around it. MEASURED: a full pass of this loop takes about 12ms --
           key_down() does a runtime variable shift per key, which cc65
           compiles to a subroutine loop -- so polling once per pass gives 82
           samples a second against 50 or 60 frames. Frame detection needs at
           least two samples per frame and had 1.4, so it missed frames: PAL
           music ran 45% slow, and NTSC was right by luck. From in here it is
           roughly 2000 samples a second.

           Outside key_down()'s SEI/CLI on purpose -- the atomic pair is the
           CIA strobe and read, and nothing here touches CIA1. */
        snd_poll();
        if (key_down_any(i)) return i;
    }
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
    while (scan() != 0xFF) { snd_poll(); }
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
            if (key_down_any(i)) return key_char(i);
        }
    }
}

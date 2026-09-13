/* Keyboard for the CoCo 3. The input seam, seventh port.
 *
 * THERE IS NO ROM TABLE TO BORROW. The Atari's driver is one indexed load
 * because the OS hands it a 192-byte ATASCII table at KEYDEF; this port pages
 * the ROM away before main() runs, so Color BASIC's POLCAT at $A000 and every
 * table behind it are simply not there. What is left is the PIA key matrix,
 * the same shape of problem the C128 solves against CIA1.
 *
 * THE MATRIX WAS DERIVED FROM THE MACHINE, NOT RECALLED. tools/keymatrix.py
 * holds each of MAME's 56 keyboard fields in turn, strobes all eight columns
 * at $FF02 and reads the rows at $FF00, and prints where each key answers.
 * Every key landed on the row its own port tag predicted, which is the
 * check that the method worked rather than the table being copied:
 *
 *        col0   col1   col2   col3   col4   col5   col6   col7
 *  row0   @      A      B      C      D      E      F      G
 *  row1   H      I      J      K      L      M      N      O
 *  row2   P      Q      R      S      T      U      V      W
 *  row3   X      Y      Z      UP     DOWN   LEFT   RIGHT  SPACE
 *  row4   0      1      2      3      4      5      6      7
 *  row5   8      9      :      ;      ,      -      .      /
 *  row6  ENTER  CLEAR  BREAK   ALT    CTRL   F1     F2     SHIFT
 *
 * $FF02 is PIA0 port B, the column strobe, active low. $FF00 is PIA0 port A,
 * the rows, active low in bits 0-6; bit 7 is the joystick comparator and is
 * not ours. Both are open-bus-ish enough that a stale strobe reads as a held
 * key, which is why every scan writes the strobe immediately before its read.
 *
 * THREE KEYS NEED SHIFT and the rest do not, which is the opposite of the
 * C128's situation and was read off MAME's own field names -- ":  *",
 * ";  +" and "-  =  _" are one key each on this keyboard. The apostrophe is
 * SHIFT+7 here as it is there, and is deliberately absent for the same
 * reason: c128/src/input.h does not name it.
 *
 * snd_poll() IS CALLED FROM THE WAIT LOOP, as it is on every other port: this
 * is where the game spends every second it is not drawing, and the music can
 * only advance while something calls it. The stub version of this file said
 * "when the sound seam is real this loop is where it goes" -- and when the
 * seam WAS built, the driver wrote its registers, the chip held them, and the
 * game stayed silent, because nothing ever ticked it. A note to your future
 * self is not a mechanism.
 */
#include <stdint.h>

#include "../../c128/src/input.h"
#include "../../c128/src/sid.h"

/* ABSOLUTE ADDRESSES, NOT POINTERS THROUGH A LOCAL. cmoc has no `volatile`,
   and README.md's "cmoc traps" already carries the scar: a run of stores
   through a local
   pointer that nothing reads back is free to vanish, and did. */
#define KB_STROBE  (*(unsigned char *)0xFF02)   /* PIA0 port B: columns, low */
#define KB_ROWS    (*(unsigned char *)0xFF00)   /* PIA0 port A: rows, low    */

uint16_t kb_entropy;

/* KB_NONE (0) for a key this game has no name for -- RIGHT, CLEAR and the
   modifiers. Values are the numeric constants from c128/src/input.h; no C
   character literals anywhere, which is that header's standing rule. */
static const unsigned char keymap[7][8] = {
    { KB_AT,     KB_A,     KB_B,     KB_C,
      KB_D,      KB_E,     KB_F,     KB_G     },
    { KB_H,      KB_I,     KB_J,     KB_K,
      KB_L,      KB_M,     KB_N,     KB_O     },
    { KB_P,      KB_Q,     KB_R,     KB_S,
      KB_T,      KB_U,     KB_V,     KB_W     },
    /* LEFT is this machine's backspace, so it carries KB_DELETE. */
    { KB_X,      KB_Y,     KB_Z,     KB_UP,
      KB_DOWN,   KB_DELETE, KB_NONE, KB_SPACE },
    { 48,        49,       50,       51,
      52,        53,       54,       55       },   /* '0'..'7' */
    { 56,        57,       KB_COLON, KB_SEMI,
      KB_COMMA,  KB_MINUS, KB_PERIOD, KB_SLASH },  /* '8','9', then punctuation */
    { KB_RETURN, KB_NONE,  KB_ESC,   KB_NONE,
      KB_NONE,   KB_NONE,  KB_NONE,  KB_NONE  },
};

/* Row 5 is the only row shift changes, and only in three places. */
static unsigned char shifted(unsigned char c)
{
    if (c == KB_COLON) return KB_STAR;      /* : -> * */
    if (c == KB_SEMI)  return KB_PLUS;      /* ; -> + */
    if (c == KB_MINUS) return KB_EQUALS;    /* - -> = */
    return c;
}

/* One pass over the matrix. Returns the key's ASCII, or KB_NONE.
   SHIFT is read first and separately: it is a modifier, never a result. */
static unsigned char scan(void)
{
    unsigned char col, r, row, held = 0;

    KB_STROBE = (unsigned char)~0x80;            /* column 7 alone */
    if ((KB_ROWS & 0x40) == 0) held = 1;         /* row 6: SHIFT */

    for (col = 0; col < 8; col++) {
        KB_STROBE = (unsigned char)~(1 << col);
        r = (unsigned char)(KB_ROWS | 0x80);     /* bit 7 is the joystick */
        if (r == 0xFF) continue;                 /* nothing in this column */
        for (row = 0; row < 7; row++) {
            if ((r & (1 << row)) == 0) {
                unsigned char c = keymap[row][col];
                if (c == KB_NONE) continue;      /* a modifier, keep looking */
                return held ? shifted(c) : c;
            }
        }
    }
    return KB_NONE;
}

/* Drain whatever was held when the game started. On this port that is not
   theoretical: the machine is started by typing EXEC at the BASIC prompt, so
   the ENTER that launches it can still be down when main() reaches the title
   screen -- the Amiga's RETURN, arriving by a different road. */
void kb_init(void)
{
    while (scan() != KB_NONE) { }
}

char kb_waitkey(void)
{
    unsigned char c;

    while (scan() != KB_NONE) { kb_entropy++; snd_poll(); }   /* release it */
    for (;;) {
        kb_entropy++;
        snd_poll();                             /* the music lives here */
        c = scan();
        if (c != KB_NONE) break;
    }
    while (scan() != KB_NONE) { }                /* and wait for release */
    return (char)c;
}

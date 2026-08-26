#ifndef INPUT_H
#define INPUT_H

/* Keyboard, read straight off the CIA1 matrix rather than through the
   KERNAL. See input.c for why the KERNAL route is not usable here. Returns
   plain ASCII values, so callers compare against ordinary numbers with no
   PETSCII ambiguity. */

/* Every key value is a NUMERIC ASCII constant, and both the table in
   input.c and the command dispatch in main.c use these same symbols.
 *
 * Never write a C character literal for one of these. cc65 translates
 * literals to the target character set, so 'Q' compiles to PETSCII 209, not
 * ASCII 81 -- verified in the linked binary, where a table written with
 * literals assembled as CD D7 D1 rather than 4D 57 51. That is invisible at
 * the call site: the letters still echo correctly, because the screen-code
 * converter accepts both encodings, so the only symptom is that commands
 * silently never match.
 *
 * This bit the project three times -- panel titles, then the input path,
 * then the dispatch -- which is why the values now live in one place that
 * both sides include rather than being spelled out twice. */
#include <stdint.h>

#define KB_NONE   0

/* The two arrow keys, and the one place in this header where a value is NOT
   ASCII: there is no ASCII code for a cursor key. These are taken from the
   unused control range, which this port never produces otherwise, rather than
   from PETSCII's 145/17 -- a PETSCII value here would read as though the
   translation trap this header warns about had been let back in.

   They come off the C128's DEDICATED cursor keys, not the C64-style shared
   CRSR key, so no shift decoding is involved. See input.c: those live on the
   extended matrix and need a different strobe. */
#define KB_UP     1
#define KB_DOWN   2

#define KB_RETURN 13
#define KB_ESC    27
#define KB_DELETE 20
#define KB_SPACE  32
#define KB_COMMA  44
#define KB_PERIOD 46
#define KB_DIGIT0 48
#define KB_DIGIT9 57
#define KB_A      65
#define KB_C      67
#define KB_D      68
#define KB_E      69
#define KB_F      70
#define KB_S      83
#define KB_X      88
#define KB_L      76
#define KB_M      77
#define KB_N      78
#define KB_O      79
#define KB_Q      81
#define KB_R      82
#define KB_T      84
#define KB_U      85
#define KB_W      87
#define KB_Y      89

char kb_waitkey(void);   /* blocks until one key is pressed and released */

/* Free-running counter, bumped once per pass of the key poll loop. Sampling it
   when the player presses a key is the port's only source of entropy: the C128
   has no clock the game reads, and how long a human takes to answer a prompt
   is unpredictable at this resolution. Used to seed the galaxy -- before this
   every game was the same one, because GAME_SEED was a constant. */
extern uint16_t kb_entropy;

#ifdef TREK_DEBUG_INPUT
/* Scripted input, for `make monitor` builds only.
 *
 * VICE's own KEYBOARD_FEED cannot drive this port: it fills the KERNAL's
 * keyboard buffer, and input.c scans the CIA1 matrix directly behind the
 * KERNAL's back, so fed keys never arrive. Verified, not assumed. This byte
 * is the way round it -- tools/vice_mon.py writes it through VICE's binary
 * monitor and kb_waitkey consumes it as though the key had been pressed.
 *
 * volatile because nothing inside the program ever stores a nonzero value
 * here. (It was once suspected of being insufficient against cc65's -O; that
 * suspicion was wrong, and the pragma added to work around it has been
 * removed -- see input.c.)
 *
 * Never in a release build: `make` does not define TREK_DEBUG_INPUT, so this
 * whole path compiles out and the shipping binary has no scripted-input
 * affordance at all. */
extern volatile unsigned char kb_inject;
#endif

#endif

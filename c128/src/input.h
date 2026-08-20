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
#define KB_NONE   0
#define KB_RETURN 13
#define KB_DELETE 20
#define KB_SPACE  32
#define KB_COMMA  44
#define KB_PERIOD 46
#define KB_DIGIT0 48
#define KB_DIGIT9 57
#define KB_D      68
#define KB_L      76
#define KB_M      77
#define KB_Q      81
#define KB_T      84
#define KB_W      87

char kb_waitkey(void);   /* blocks until one key is pressed and released */

#ifdef TREK_DEBUG_INPUT
/* Scripted input, for `make monitor` builds only.
 *
 * VICE's own KEYBOARD_FEED cannot drive this port: it fills the KERNAL's
 * keyboard buffer, and input.c scans the CIA1 matrix directly behind the
 * KERNAL's back, so fed keys never arrive. Verified, not assumed. This byte
 * is the way round it -- tools/vice_mon.py writes it through VICE's binary
 * monitor and kb_waitkey consumes it as though the key had been pressed.
 *
 * volatile is load-bearing. Nothing inside the program ever stores a nonzero
 * value here, so without it the compiler is entitled to decide the test can
 * never fire and fold it away.
 *
 * Never in a release build: `make` does not define TREK_DEBUG_INPUT, so this
 * whole path compiles out and the shipping binary has no scripted-input
 * affordance at all. */
extern volatile unsigned char kb_inject;
#endif

#endif

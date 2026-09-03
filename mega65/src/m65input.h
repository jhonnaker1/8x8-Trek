#ifndef M65INPUT_H
#define M65INPUT_H

/* THE KEY CONTRACT IS SHARED, and this file no longer restates it.
 *
 * It used to be a 135-line copy of c128/src/input.h -- the same fifty KB_
 * constants, the same kb_waitkey/kb_entropy declarations -- and NOTHING BUT
 * m65input.c EVER READ IT. The shared main.c and ui.c say `#include "input.h"`,
 * which on this port's include path resolves to the C128's header, so the two
 * copies were one edit away from disagreeing about a key value with only one of
 * them in force. The duplicate is gone; the contract has one home.
 *
 * It lives under c128/src/ for historical reasons rather than good ones -- it
 * is a UI contract, not a C128 one, and belongs beside core/strpool.h. Moving
 * it touches both ports and is not worth doing in the middle of a bug hunt. */
#include "../../c128/src/input.h"

/* The two calls that are this port's own. The C128 scans a key matrix and has
   no use for either: $D610 hands over ASCII directly, so a non-blocking poll is
   one register read, and kb_init only has to empty the queue of whatever was
   typed before the game started. */
char kb_poll(void);      /* KB_NONE if nothing is waiting */
void kb_init(void);

#endif

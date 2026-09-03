#ifndef M65SND_H
#define M65SND_H

/* THE SOUND CONTRACT IS SHARED, and this file no longer restates it.
 *
 * It was a byte-identical copy of c128/src/sid.h apart from the include guard,
 * and -- exactly like m65input.h before it -- NOTHING BUT m65snd.c EVER READ
 * IT: the shared main.c and ui.c say `#include "sid.h"`, which on this port's
 * include path resolves to the C128's header. Two copies, one in force.
 *
 * The duplicate had already cost something. Its `extern uint8_t snd_region`
 * promised region detection that m65snd.c did not implement, and because
 * nothing included both, nothing complained -- until the .c finally defined
 * the variable and the compiler said the declarations disagreed. */
#include "../../c128/src/sid.h"

#endif

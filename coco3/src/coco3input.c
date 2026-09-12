/* Keyboard for the CoCo 3 -- STUB. The real one polls the PIA key matrix at
 * $FF00/$FF02, or goes through Color BASIC's POLCAT at $A000.
 */
#include <stdint.h>

#include "../../c128/src/input.h"

/* The port's only entropy source -- see falcon/src/falconinput.c. As a stub it
   never advances, so a stubbed build plays the same game every time. */
uint16_t kb_entropy;

void kb_init(void) { }
char kb_waitkey(void) { return KB_RETURN; }

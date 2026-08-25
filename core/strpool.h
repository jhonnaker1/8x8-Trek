#ifndef STRPOOL_H
#define STRPOOL_H

#include <stdint.h>

/* The string pool: prose out of the address space and into far memory.
 *
 * The C128 has ~42K for code and read-only data together, and 4,088 bytes of
 * it were string literals -- a tenth of the machine spent on words. Every
 * feature still to build (RAY's four outcomes and its loss report, five more
 * loss endings, the planet dialogs, the boarding-party and event messages) is
 * string-heavy, so without this each of them would cost code AND data out of
 * the same shrinking budget.
 *
 * Pooled strings live in STRINGS.DAT on the game disk, are streamed into far
 * memory at startup (core/farmem.h), and are fetched one at a time into a
 * small rotating set of RAM buffers.
 *
 * WHY IT ROTATES. `ui_message(S(S_HELM), S(S_AWAITING))` needs two pooled
 * strings alive at once, and a single buffer would hand the same pointer to
 * both arguments. STR_SLOTS is the number that can be live in one expression;
 * exceed it and the oldest is quietly reused, which is why it is generous
 * rather than exact.
 *
 * The IDs are generated -- see tools/gen_strings.py. Do not renumber by hand.
 */

#define STR_SLOTS  4
#define STR_MAX   64      /* longest pooled string, plus a terminator */

/* Fetches string `id` into the next buffer and returns it. The pointer stays
   valid until STR_SLOTS further calls have been made. */
const char *S(uint8_t id);

/* Loads the pool. Returns non-zero on success; if it fails every S() returns
   an empty string and the game runs wordless rather than crashing. */
uint8_t str_load(void);

#endif

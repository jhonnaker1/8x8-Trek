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
 * THE INDEX IS IN THE FILE TOO, ahead of the text it points into, so a pooled
 * string costs the binary NOTHING but its id -- which is a #define. It used to
 * cost two bytes of offset table as well, 600 of them on the C128, and that
 * was the last of the prose still sitting in the image. A platform
 * implementing this seam should read its index from the same place; see
 * tools/gen_strings.py for the layout and c128/src/strpool.c for a reader.
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
/* A NAMED TYPE, so the ceiling can be asserted against it.
   The pool passed 256 strings on 2026-08-27 and a uint8_t id silently
   WRAPPED -- S_262 fetched string 6. The only complaint was a compiler
   warning in a wall of them, and `make verify` had nothing to say: it checks
   the key table and the panel widths, not this. c128/src/strpool.c now fails
   to COMPILE if STR_COUNT outgrows this type. */
typedef uint16_t StrId;
const char *S(StrId id);

/* Loads the pool. Returns non-zero on success; if it fails every S() returns
   an empty string and the game runs wordless rather than crashing. */
uint8_t str_load(void);

#endif

#include <string.h>

#include "../../core/strpool.h"
#include "../../core/farmem.h"
#include "strdata.h"

/* The C128's string pool. See core/strpool.h for why it exists.
 *
 * STRINGS.DAT carries BOTH halves -- a count, then the offset table, then the
 * text back to back with each string NUL terminated. Nothing about the pool
 * is left in the binary except the ids, which are #defines and cost nothing.
 *
 * THE INDEX USED TO BE IN THE BINARY, and that was the last 600 bytes of the
 * prose still sitting in the resident image: `const unsigned int
 * str_offset[STR_COUNT]` in a generated strdata.c. Lifting the text out and
 * leaving its index behind had only half-moved it.
 *
 * It could not be declared in far memory instead. Bank 1 is filled by LOADing
 * a file into it (see c128/src/farmem.c) and a static initialiser table has no
 * route there -- that is the standing limit of the far-memory seam, not an
 * oversight. So the table travels WITH the text it indexes, written by
 * tools/gen_strings.py into the same file, arriving in the same single KERNAL
 * LOAD, and costing one extra two-byte far read per lookup. */

static uint8_t  loaded = 0;
static uint16_t idx_base = 0;   /* the offset table, in the far store */
static uint16_t txt_base = 0;   /* the text it points into */

/* Rotating, because one expression can want several pooled strings at once --
   `ui_message(S(a), S(b))` is the common case and a single buffer would hand
   the same pointer to both. */
static char slot[STR_SLOTS][STR_MAX];
static uint8_t next_slot = 0;

uint8_t str_load(void) {
    uint16_t base = far_load("STRINGS.DAT");
    uint16_t count;

    if (base == FAR_NONE) return (loaded = 0);

    /* THE COUNT IS A GUARD, not a convenience -- STR_COUNT is a compile-time
       constant and the loader does not need to be told it. What it needs is
       to know that the FILE agrees. A disk built from an older tree has no
       header at all, so the first two bytes here would be the first two
       letters of the first string; every id would then index into noise and
       the screen would fill with garbage read from somewhere else in the
       store. Refusing the pool turns that into the failure the pool already
       plans for -- no words, and a game that still plays. */
    far_read(base, &count, sizeof count);
    if (count != STR_COUNT) return (loaded = 0);

    idx_base = (uint16_t)(base + sizeof count);
    txt_base = (uint16_t)(idx_base + STR_COUNT * 2);
    return (loaded = 1);
}

/* uint16_t, NOT uint8_t. The pool passed 256 strings on 2026-08-27 and every
   id from 256 up silently WRAPPED -- S_262 fetched string 6. The build said
   so, in a wall of -Wconstant-conversion warnings that scrolled past, and
   `make verify` had nothing to say about it because it checks the key table
   and the panel widths and not this. STR_COUNT is checked against the type
   below so the next ceiling is a build failure, not a wrong word on screen. */
/* THE NEXT CEILING IS A BUILD FAILURE, not a wrong word on screen. If StrId
   is ever narrowed, or the pool outgrows it, the round trip stops being the
   identity and this array gets a negative size. */
typedef char str_id_holds_the_pool[
    ((STR_COUNT - 1) == (int)(StrId)(STR_COUNT - 1)) ? 1 : -1];

const char *S(StrId id) {
    char *dst = slot[next_slot];
    uint16_t off;
    uint8_t i;

    next_slot = (uint8_t)((next_slot + 1) % STR_SLOTS);

    /* No pool means no words. The game still plays: panels draw, commands
       work, and every label is blank. That is a far better failure than a
       crash on a disk somebody built without reference/. */
    if (!loaded || id >= STR_COUNT) { dst[0] = '\0'; return dst; }

    /* Two far reads now, where there was one. The first is two bytes and
       costs two FETCH calls; the second was always going to be STR_MAX of
       them. Against 600 bytes of resident image that is not a trade worth
       thinking about. */
    far_read((uint16_t)(idx_base + id * 2), &off, sizeof off);

    /* One far read of the whole slot, then find the terminator -- rather than
       a far read per character. Every byte across the bank boundary costs a
       KERNAL call, so the fixed cost of reading a few bytes too many is far
       cheaper than the per-byte alternative. The last string in the pool is
       the only one that can read past the end, which is why the generator
       writes a terminator after it. */
    far_read((uint16_t)(txt_base + off), dst, STR_MAX - 1);
    dst[STR_MAX - 1] = '\0';
    for (i = 0; i < STR_MAX; i++)
        if (dst[i] == '\0') break;

    return dst;
}

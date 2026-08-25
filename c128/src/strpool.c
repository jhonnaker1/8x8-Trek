#include <string.h>

#include "../../core/strpool.h"
#include "../../core/farmem.h"
#include "strdata.h"

/* The C128's string pool. See core/strpool.h for why it exists.
 *
 * STRINGS.DAT is the text back to back, each NUL terminated; strdata.c holds
 * the offset of each, which is the only part left in the binary -- 194 bytes
 * of index against 2,203 bytes of prose. */

static uint8_t  loaded = 0;
static uint16_t base = 0;      /* where the pool landed in the far store */

/* Rotating, because one expression can want several pooled strings at once --
   `ui_message(S(a), S(b))` is the common case and a single buffer would hand
   the same pointer to both. */
static char slot[STR_SLOTS][STR_MAX];
static uint8_t next_slot = 0;

uint8_t str_load(void) {
    base = far_load("STRINGS.DAT");
    loaded = (uint8_t)(base != FAR_NONE);
    return loaded;
}

const char *S(uint8_t id) {
    char *dst = slot[next_slot];
    uint16_t off;
    uint8_t i;

    next_slot = (uint8_t)((next_slot + 1) % STR_SLOTS);

    /* No pool means no words. The game still plays: panels draw, commands
       work, and every label is blank. That is a far better failure than a
       crash on a disk somebody built without reference/. */
    if (!loaded || id >= STR_COUNT) { dst[0] = '\0'; return dst; }

    off = (uint16_t)(base + str_offset[id]);

    /* One far read of the whole slot, then find the terminator -- rather than
       a far read per character. Every byte across the bank boundary costs a
       KERNAL call, so the fixed cost of reading a few bytes too many is far
       cheaper than the per-byte alternative. The last string in the pool is
       the only one that can read past the end, which is why the generator
       writes a terminator after it. */
    far_read(off, dst, STR_MAX - 1);
    dst[STR_MAX - 1] = '\0';
    for (i = 0; i < STR_MAX; i++)
        if (dst[i] == '\0') break;

    return dst;
}

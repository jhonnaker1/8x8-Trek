#ifndef HOF_H
#define HOF_H

#include <stdint.h>

/* TREK.SCR, the hall of fame file, as data rather than as I/O.
 *
 * Same split as core/serial.c: this parses and formats, the platform layer
 * moves the bytes. No filename appears here.
 *
 * FORMAT -- MEASURED from reference/TREK.SCR, ten records of:
 *
 *     25 characters of name, padded with '.'   CRLF
 *     the score in decimal                     CRLF
 *
 * The name field is fixed at 25; the score line is variable length, so the
 * file is line-oriented text and not fixed records. The shipped sample is
 * exactly 300 bytes only because every score in it is 0.
 *
 * TEN RECORDS, FIVE RANKS -- MEASURED 2026-08-23 and NOT what it looks like.
 * The hall of fame shows two entries per rank, a bright first place and a dim
 * second, so ten records are five ranks by two places. A file crafted with
 * SLOT0..SLOT9 in order came back on screen as:
 *
 *     Lt. Commander  SLOT0 / SLOT5      Commodore  SLOT3 / SLOT8
 *     Commander      SLOT1 / SLOT6      Admiral    SLOT4 / SLOT9
 *     Captain        SLOT2 / SLOT7
 *
 * So the file is PLACE-MAJOR: records 0..4 are first place for ranks 1..5,
 * records 5..9 are second place. Rank-major -- records 0 and 1 both belonging
 * to rank 1 -- is the natural guess and it is wrong. hof_index() is the only
 * place that knows this.
 */

#define HOF_RANKS    5
#define HOF_PLACES   2
#define HOF_ENTRIES  (HOF_RANKS * HOF_PLACES)
#define HOF_NAME     25

/* Ten records, each at most 25 + 2 + 6 + 2 bytes. Rounded up: a buffer this
   size holds any legal file and leaves room to notice one that is too long. */
#define HOF_BUF      384

typedef struct {
    char    name[HOF_NAME + 1];
    int16_t score;
} HofEntry;

/* Where rank `level` (1..5) and `place` (0 = first, 1 = second) live in the
   file. The measured place-major mapping, in one function. */
uint8_t hof_index(uint8_t level, uint8_t place);

/* Non-zero on success. A file that is short, malformed or the wrong length
   leaves `out` filled with blank entries rather than half-parsed ones -- a
   corrupt score file should read as an empty hall of fame, not a random one. */
uint8_t hof_parse(const uint8_t *buf, uint16_t len, HofEntry *out);

/* Bytes written, or 0 if `max` was too small. Always writes all ten records,
   because the original's file has no notion of a partial table. */
uint16_t hof_format(const HofEntry *in, uint8_t *buf, uint16_t max);

/* Offers a finished game to the table. Non-zero if it earned a place, in
   which case the table has been rewritten and wants saving. Second place is
   pushed off by a new first; a score that beats neither changes nothing. */
uint8_t hof_offer(HofEntry *tbl, uint8_t level, const char *name, int16_t score);

/* Empties a table, for when there is no file to read. */
void hof_clear(HofEntry *tbl);

#endif

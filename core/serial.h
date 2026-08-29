#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

/* Turning the game state into bytes, and back.
 *
 * THIS FILE TOUCHES NO I/O AND KNOWS NO FILENAME. That is the seam: the core
 * turns state into a byte array, the platform layer puts byte arrays on a
 * disk, and neither knows how the other does it. See core/storage.h for the
 * other half, and NOTES.md "Disk I/O: the seam".
 *
 * It is a separate translation unit from trek.c on purpose. ld65 links whole
 * modules, so anything living in trek.c is in every binary whether it is
 * called or not -- and on the C128 the free space is measured in hundreds of
 * bytes. A port with no SAVE command simply does not list this file.
 *
 * TWO RULES, both of which exist because breaking either produces files that
 * are wrong in a way every test on the writing machine will miss.
 *
 * 1. NO memcpy OF STRUCTS. Struct padding differs between compilers, so a
 *    memcpy of `ship` would produce a file that only the compiler that wrote
 *    it can read.
 *
 * 2. LITTLE-ENDIAN, ALWAYS, EVERYWHERE. Every 16-bit value goes low byte
 *    first, whatever the host does natively, and is read back with shifts
 *    rather than a cast. That is the 6502's own order, so the 8-bit ports pay
 *    nothing and the 68000 pays two shifts per field -- in exchange for save
 *    files that interchange between them.
 *
 *    The failure this prevents is the quiet kind: a C128 save read on the
 *    Amiga would come back with every field byte-swapped while both machines
 *    pass their own round-trip tests, because each round-trips its own bytes
 *    happily. core/test/test_serial.c asserts the exact bytes against a fixed
 *    array so that neither host can be wrong in its own favour.
 */

/* Exact size of a saved state. Callers size their buffer with this rather
   than guessing; trek_state_save() also refuses to write past `max`.

   IT IS A FUNCTION OF PLANET_MAX, and forgetting that shipped a crash: the
   planet count went from ten to twenty-two on 2026-08-26 and this stayed at
   578, so trek_state_save() wrote 72 bytes past a buffer sized from it and
   test_serial aborted. Six bytes per planet slot -- if PLANET_MAX moves, this
   moves with it. See "when a constant changes, re-read everything derived
   from it". */
#define TREK_SAVE_SIZE  558

/* Bumped whenever the layout changes. A load of an older version is refused
   rather than misread -- there is no upgrade path and a half-read galaxy is
   worse than no galaxy. */
#define TREK_SAVE_VERSION 13

/* Bytes written, or 0 if `max` was too small. */
uint16_t trek_state_save(uint8_t *buf, uint16_t max);

/* Non-zero on success. Refuses a short buffer, a bad magic or a version it
   does not know, and touches no game state until all three pass. */
uint8_t trek_state_load(const uint8_t *buf, uint16_t len);

#endif

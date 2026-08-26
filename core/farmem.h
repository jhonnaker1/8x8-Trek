#ifndef FARMEM_H
#define FARMEM_H

#include <stdint.h>

/* Read-only bulk data that does not fit in the address space.
 *
 * THE SECOND PORTABLE SEAM, and the same shape as core/storage.h: the contract
 * lives beside the core, the mechanism is per platform, and `core/` itself
 * never calls any of it.
 *
 * WHY IT IS A SEAM AND NOT A C128 TRICK. Four of the six remaining targets
 * bank memory anyway:
 *
 *     C128        bank 1's 64K through the KERNAL's FETCH/STASH
 *     X16         8K pages at $A000 out of 512K, or VERA's 128K
 *     Atari 130XE 16K banks at $4000
 *     CoCo 3      8K pages through the MMU
 *     F256        8K pages through the MMU
 *     MEGA65      no banking needed -- a plain array
 *     Amiga       no banking needed -- a plain array
 *
 * So the majority case is banked, and the two roomy targets implement this in
 * a few lines with no window at all. Building it on the C128 is building it
 * where it is cheapest to learn.
 *
 * THE STORE IS LOADED FROM DISK. Initialised data has to come from somewhere,
 * and it cannot come from the binary -- the whole point is to get it out of
 * the binary. HOW it gets there is entirely the platform's business: the C128
 * hands the KERNAL bank 1 as a load target and the file lands there in one
 * call, which measured between eight and eighteen times faster than the
 * byte-at-a-time read it started with.
 *
 * READ IN CHUNKS, NOT BYTES. On a banked target every byte costs a KERNAL call
 * or a window switch. Copy a whole record into a small RAM buffer once and
 * work on that; do not walk far memory a byte at a time in a drawing loop.
 */

/* The store has MORE THAN ONE TENANT -- the string pool and the music both
   live in it -- so far_load APPENDS and returns where it put the file. An
   earlier version loaded at offset 0 every time and the music silently
   overwrote the prose. */
#define FAR_NONE  0xFFFFU

/* Streams `name` into the far store, appending to whatever is already there,
   and returns its base offset -- or FAR_NONE if it could not be read. Callers
   add this base to the offsets their own index gives them.

   A platform with no far memory may implement this as a read into a static
   array; the contract does not care where the bytes live. */
uint16_t far_load(const char *name);

/* Bytes loaded in total, across every tenant. */
uint16_t far_size(void);

/* Copies `len` bytes from far offset `off` into `dst`. Reading past the end
   is the caller's error; the store knows its size and callers can ask. */
void far_read(uint16_t off, void *dst, uint8_t len);

#endif

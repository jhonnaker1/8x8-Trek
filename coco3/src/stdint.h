/* stdint.h for cmoc, which does not ship one.
 *
 * FIVE TYPEDEFS AND NOTHING ELSE. The shared half of this project uses
 * exactly three fixed-width types -- uint8_t, uint16_t and int16_t, confirmed
 * by sweeping core/ and the shared UI rather than assumed -- and the other two
 * are here so that a file which reaches for int8_t or uint32_t gets a type
 * rather than a parse error twenty lines later.
 *
 * THE 6809 IS A 16-BIT-REGISTER MACHINE AND cmoc's `int` IS 16 BITS, which is
 * why int16_t is int and not long. cmoc's `long` is 32 bits, so uint32_t is
 * the one type here that costs anything to use; core/ uses none of it, and
 * that is a property of the game worth keeping -- "8-bit is not a mistake:
 * core/trek.c has zero floats and zero 32-bit ints".
 */
#ifndef STDINT_H
#define STDINT_H

typedef unsigned char  uint8_t;
typedef signed char    int8_t;
typedef unsigned int   uint16_t;
typedef int            int16_t;
typedef unsigned long  uint32_t;

#endif

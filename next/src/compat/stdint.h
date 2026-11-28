/* <stdint.h> for NeXTSTEP 3.3, which has none: its cc is gcc 2.5.8, from
 * 1993, six years before C99 named these types. The shared code includes
 * <stdint.h> everywhere, so this stands in for it -- on the NeXT only;
 * Makefile.next puts src/compat on the include path and nothing else does.
 *
 * The widths are the 68k's under NeXT's cc: char 8, short 16, int and long
 * 32. Only what the files this port compiles use is defined.
 */
#ifndef EGATREK_NEXT_STDINT_H
#define EGATREK_NEXT_STDINT_H

typedef signed char    int8_t;
typedef unsigned char  uint8_t;
typedef short          int16_t;
typedef unsigned short uint16_t;
typedef int            int32_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uintptr_t;

#define INT16_MAX   32767
#define UINT16_MAX  65535U
#define UINT32_C(v) v ## U

#endif

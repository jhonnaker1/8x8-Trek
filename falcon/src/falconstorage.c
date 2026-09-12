/* Files for the Falcon: GEMDOS Fopen/Fread/Fwrite/Fclose under the five
 * plat_* functions of core/storage.h.
 *
 * THE EASIEST STORAGE SEAM ON THE PROJECT, and it takes the Amiga's title.
 * There is no KERNAL channel to open and close in the right order, no
 * hypervisor trap, no device number, no secondary address, no 8K window to
 * negotiate, no 512-byte sector to buffer -- and unlike the Amiga, no
 * PROGDIR: to prefix either. GEMDOS gives a real hierarchical filesystem and
 * a current directory, and TOS starts a program with the current directory
 * set to the one the program was launched from.
 *
 * core/storage.h's contract is deliberately weaker than this machine can
 * manage -- it promises only NOTFOUND against ERROR, because that is the most
 * every target can tell apart. GEMDOS returns a signed error code that
 * distinguishes far more; the detail is thrown away here rather than smuggled
 * into a fourth return code the shared UI would not know what to do with.
 *
 * GEMDOS ERROR CODES ARE NEGATIVE, and a handle is non-negative. That is the
 * whole of the error checking, and it is why every result here is a LONG and
 * not the int a careless port would use: a handle is a 16-bit value but
 * Fopen's error returns need the sign, and truncating to int on a 16-bit
 * `int` compiler is how this goes wrong quietly.
 */
#include <tos.h>

#include "../../core/storage.h"

/* EFILNF (-33) and EPTHNF (-34) come from <tos.h> -- do NOT restate them
   here. The first draft did, and vbcc caught it as "macro redefined
   unidentically" because the header's are ints and these were longs. A
   constant worth defining twice is a constant worth importing once. */

static long fh = -1L;           /* the streaming handle, for plat_open */

static uint8_t map_err(long e)
{
    if (e == EFILNF || e == EPTHNF)
        return STOR_NOTFOUND;
    return STOR_ERROR;
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got)
{
    long h, n;

    if (got)
        *got = 0;
    h = Fopen(name, 0);                 /* 0 = read only */
    if (h < 0)
        return map_err(h);
    n = Fread((int)h, (long)max, buf);
    Fclose((int)h);
    if (n < 0)
        return map_err(n);
    if (got)
        *got = (uint16_t)n;
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len)
{
    long h, n;

    /* Fcreate with attribute 0 truncates an existing file, which is what
       every other port's save does. */
    h = Fcreate(name, 0);
    if (h < 0)
        return map_err(h);
    n = Fwrite((int)h, (long)len, (void *)buf);   /* GEMDOS takes void*, not const */
    Fclose((int)h);
    if (n < 0)
        return map_err(n);
    return (n == (long)len) ? STOR_OK : STOR_ERROR;
}

uint8_t plat_open(const char *name)
{
    if (fh >= 0)                        /* never two at once -- see below */
        plat_close();
    fh = Fopen(name, 0);
    if (fh < 0) {
        uint8_t e = map_err(fh);
        fh = -1L;
        return e;
    }
    return STOR_OK;
}

/* The briefing is STREAMED through this, a page at a time, because the C128
   has no room to buffer it. Here it would fit in memory ten times over and
   the seam still streams: the shape is the 8-bit machines' and stays that
   way, exactly as farmem does. */
uint16_t plat_read(void *buf, uint16_t len)
{
    long n;

    if (fh < 0)
        return 0;
    n = Fread((int)fh, (long)len, buf);
    return (n > 0) ? (uint16_t)n : 0;
}

void plat_close(void)
{
    if (fh >= 0)
        Fclose((int)fh);
    fh = -1L;
}

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

/* MEASURE THE FILE BEFORE READING IT. storage.h: "A file longer than `max`
   is an error, not a truncation -- silently reading half a save is worse than
   refusing."

   The first draft just called `Fread(h, max, buf)`, which reads UP TO max
   bytes and reports how many -- so an oversized file came back STOR_OK with a
   silent truncation, the one behaviour the contract names as forbidden.
   src/storetest.c caught it on its first run. GEMDOS makes this seam so easy
   that it was written straight through and looked obviously right; the easiest
   seam on the project turned out to be the one that skipped its own contract. */
uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got)
{
    long h, n, size;

    if (got)
        *got = 0;
    h = Fopen((char *)name, 0);         /* 0 = read only; GEMDOS takes char* */
    if (h < 0)
        return map_err(h);

    size = Fseek(0L, (int)h, 2);        /* 2 = from the end, so this IS the size */
    if (size < 0) {
        Fclose((int)h);
        return map_err(size);
    }
    if (size > (long)max) {
        Fclose((int)h);
        return STOR_ERROR;
    }
    if (Fseek(0L, (int)h, 0) < 0) {     /* 0 = from the start */
        Fclose((int)h);
        return STOR_ERROR;
    }

    n = Fread((int)h, size, buf);
    Fclose((int)h);
    if (n < 0)
        return map_err(n);
    if (got)
        *got = (uint16_t)n;
    return (n == size) ? STOR_OK : STOR_ERROR;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len)
{
    long h, n;

    /* Fcreate with attribute 0 truncates an existing file, which is what
       every other port's save does. */
    h = Fcreate((char *)name, 0);
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
    fh = Fopen((char *)name, 0);
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

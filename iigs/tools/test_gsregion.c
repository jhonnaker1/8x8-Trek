/* The 50Hz/60Hz classifier, exercised over the cases no emulator here can
   produce. Compiled natively -- it is pure uint16_t arithmetic and the header
   under test is INCLUDED, not copied. */
#include <stdio.h>
#include "../src/gsregion.h"

static int fails;

static void want(const char *what, uint16_t mn, uint16_t mx, uint8_t exp)
{
    uint8_t got = gs_classify_region(mn, mx);
    const char *gs = got == REGION_PAL ? "PAL" : "NTSC";
    const char *es = exp == REGION_PAL ? "PAL" : "NTSC";
    printf("  %-42s min=%3u max=%3u -> %-4s", what, mn, mx, gs);
    if (got != exp) { printf("  FAIL, wanted %s", es); fails++; }
    printf("\n");
}

int main(void)
{
    puts("gs_classify_region:");
    want("NTSC, the documented range",            0x0FA, 0x1FF, REGION_NTSC);
    want("50Hz, the documented range",            0x0C8, 0x1FF, REGION_PAL);
    want("NTSC measured under MAME",              250,   511,   REGION_NTSC);
    want("just inside NTSC",                      226,   511,   REGION_NTSC);
    want("just inside 50Hz",                      225,   511,   REGION_PAL);
    want("dead register, all zero",               0,     0,     REGION_NTSC);
    want("dead register, all ones",               511,   255,   REGION_NTSC);
    want("max wrong -- not the documented counter",200,  300,   REGION_NTSC);
    want("min implausibly low",                   10,    511,   REGION_NTSC);
    want("min above the frame",                   300,   511,   REGION_NTSC);
    printf("%s\n", fails ? "test_gsregion: FAIL" : "test_gsregion: all pass");
    return fails ? 1 : 0;
}

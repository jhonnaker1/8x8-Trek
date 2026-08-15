/* Host-side test for the EGA -> VDC colour index mapping in src/egavdc.h.
 *
 * Runs natively (cc, not cl65) -- the mapping is pure arithmetic, so it can
 * be proven on the build machine without an emulator. Worth doing precisely
 * because getting this wrong is silent: every colour still renders, just as
 * the wrong hue. commodore-uno/c128/src/vdc.h documents shipping exactly
 * that bug, where "only shades of blue and green, no red or yellow" turned
 * out to be VIC-II palette indices fed to a VDC.
 *
 *     cc -Wall -o /tmp/test_egavdc test/test_egavdc.c && /tmp/test_egavdc
 */

#include <stdio.h>
#include <string.h>
#include "../src/egavdc.h"

/* The VDC's RGBI table, transcribed from commodore-uno/c128/src/vdc.h.
   Index order is R G B I -- bit0 is intensity. */
static const char *vdc_name[16] = {
    "black",      "dark gray",    "dark blue",   "light blue",
    "dark green", "light green",  "dark cyan",   "light cyan",
    "dark red",   "light red",    "dark purple", "light purple",
    "dark yellow","light yellow", "light gray",  "white"
};

/* What each EGA index must land on, by name. EGA order is I R G B. */
static const struct {
    unsigned char ega;
    const char *want;
} expect[16] = {
    {  0, "black"        },
    {  1, "dark blue"    },
    {  2, "dark green"   },
    {  3, "dark cyan"    },
    {  4, "dark red"     },
    {  5, "dark purple"  },
    {  6, "dark yellow"  },   /* EGA calls this brown; VDC has no brown */
    {  7, "light gray"   },
    {  8, "dark gray"    },
    {  9, "light blue"   },
    { 10, "light green"  },
    { 11, "light cyan"   },
    { 12, "light red"    },
    { 13, "light purple" },
    { 14, "light yellow" },
    { 15, "white"        },
};

int main(void) {
    int i, fails = 0;
    unsigned char seen[16];

    memset(seen, 0, sizeof seen);

    for (i = 0; i < 16; i++) {
        unsigned char got = EGA_TO_VDC(expect[i].ega);
        const char *got_name = vdc_name[got];
        int ok = (strcmp(got_name, expect[i].want) == 0);

        printf("EGA %2u -> VDC %2u  %-13s %s\n",
               expect[i].ega, got, got_name, ok ? "ok" : "FAIL");
        if (!ok) {
            printf("        expected %s\n", expect[i].want);
            fails++;
        }
        seen[got]++;
    }

    /* The mapping must be a bijection: a rotate is, but a typo'd table or a
       shift-instead-of-rotate would collapse entries and lose colours. */
    for (i = 0; i < 16; i++) {
        if (seen[i] != 1) {
            printf("FAIL: VDC index %d produced %u times, expected once\n", i, seen[i]);
            fails++;
        }
    }

    /* Intensity must survive the move from bit3 to bit0. */
    for (i = 0; i < 8; i++) {
        if (EGA_TO_VDC(i) & 1) { printf("FAIL: EGA %d (dark) set VDC intensity\n", i); fails++; }
        if (!(EGA_TO_VDC(i + 8) & 1)) { printf("FAIL: EGA %d (light) lost VDC intensity\n", i + 8); fails++; }
    }

    printf("\n%s\n", fails ? "FAILED" : "all 16 map correctly, bijective, intensity preserved");
    return fails ? 1 : 0;
}

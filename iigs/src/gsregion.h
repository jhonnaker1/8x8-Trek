#ifndef GSREGION_H
#define GSREGION_H

#include <stdint.h>
#include "../../c128/src/sid.h"       /* REGION_NTSC / REGION_PAL */

/* THE 50Hz/60Hz DECISION, SEPARATED FROM THE SAMPLING SO IT CAN BE TESTED.
 *
 * MAME has no 50Hz IIgs -- every apple2gs clone is a ROM revision -- so the
 * PAL branch cannot be reached on any machine available here. Sampling the VGC
 * counter is hardware and must be measured; CLASSIFYING what was sampled is
 * arithmetic and can be checked exhaustively. Splitting them means the branch
 * that ships untested on hardware is at least tested as logic, and the thing
 * tested IS the thing that ships -- this header is included by the driver, not
 * copied into the test.
 *
 * uint16_t rather than unsigned int ON PURPOSE: `int` is 16 bits under
 * llvm-mos and 32 on the host that runs tools/test_gsregion.c, and a host test
 * of a different type is a test of different code. cc65 taught this project
 * that native tests are blind to what the cross compiler does.
 *
 * From the IIgs Hardware Reference: the vertical address runs $0FA..$1FF at
 * 60Hz (262 lines) and would run $0C8..$1FF at 50Hz (312). The MAXIMUM is
 * $1FF in both, so the MINIMUM is the discriminator: 250 against 200.
 *
 * EVERY UNCERTAIN READING RETURNS NTSC. An undetected 50Hz machine plays 17%
 * slow, which is what the port did before this existed; a misdetected 60Hz
 * machine would play 20% fast and would be a regression for everybody.
 */
static inline uint8_t gs_classify_region(uint16_t vmin, uint16_t vmax)
{
    if (vmax != 0x01FF) return REGION_NTSC;   /* not the documented counter */
    if (vmin > 225 && vmin < 261) return REGION_NTSC;   /* 250 */
    if (vmin > 150 && vmin <= 225) return REGION_PAL;   /* 200 */
    return REGION_NTSC;                        /* unrecognised */
}

#endif

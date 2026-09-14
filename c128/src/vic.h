#ifndef VIC_H
#define VIC_H

/* The C128's VIC-IIe at 40 columns -- the same ten-function seam vdc.h
 * declares, on the other chip in the same machine.
 *
 * WHY THIS EXISTS AND WHAT IT IS FOR. NOTES.md item 57: the 80-column rule
 * turned out to be wrong about the geometry, and the first sub-80 platform is
 * the C64. This driver is step one, and it is deliberately built on the C128
 * rather than on the C64 -- same disk, same KERNAL, same SID, same bank-1 far
 * memory, same overlays, same VICE rig -- so THE LAYOUT IS THE ONLY VARIABLE
 * and there are no new seams to debug at the same time.
 *
 * AND IT IS THE C64'S DRIVER. The VIC-IIe writes a video matrix at $0400 and
 * colour nibbles at $D800; so does the C64's VIC-II, with the same sixteen
 * colours in the same order (see egavic.h). What is C128-specific here is
 * confined to the two notes marked C128 ONLY below.
 *
 * THE INTERFACE IS DECLARED IN vdc.h AND THAT IS NOT A MISTAKE. Every caller
 * -- ui.c, layout.c, main.c -- includes vdc.h and calls scr_put and friends.
 * A second video driver must satisfy the same declarations or the shared UI
 * would have to know which machine it is on, which is exactly what the seam
 * exists to prevent. This header adds only what is VIC-shaped.
 */

#define VIC_COLS 40
#define VIC_ROWS 25

/* Where the VIC-IIe reads its picture. $0400 is where the KERNAL already has
   it, so nothing has to be moved and no bank has to be switched. */
#define VIC_SCREEN ((unsigned char *)0x0400)
#define VIC_COLRAM ((unsigned char *)0xD800)

#endif

/* Overlays for the card-less CoCo 3: NONE YET, and this is the empty function
 * core/overlay.h asks for.
 *
 * NOT A CLAIM THAT NONE ARE NEEDED -- that is what `make early` is for. This
 * port links the whole game RESIDENT first and reads the overflow, which is
 * the staging rule every 6502 port here followed and which coco3/Makefile
 * states in as many words. If 64K does not hold the program, the 4,000-byte
 * screen and the string pool, the answer is overlays and ../coco3/src has a
 * working design to copy -- a window in ordinary RAM with images read into it
 * from disk, because MMUEN breaks the disk on this machine.
 *
 * ../coco3/src/coco3ovl.c is NOT used here: it includes a generated ovlmap.h
 * that only exists once the overlay build has run, so borrowing it would make
 * the first measurement depend on machinery the measurement is meant to
 * decide the need for.
 */
#include "../../core/overlay.h"

void ovl_load(unsigned char which) { (void)which; }

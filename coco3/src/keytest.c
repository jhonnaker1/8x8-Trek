/* Does the keyboard driver return the right character for the right key?
 *
 * The driver reads a hardware matrix, so nothing short of pressing the keys
 * answers this -- and the one thing a table like coco3input.c's can be is
 * plausibly wrong, in one cell, in a way that only shows up when a player
 * tries to type a name. So the harness holds MAME's own keyboard fields and
 * this program records what kb_waitkey() made of each one.
 *
 * INTERRUPTS ARE MASKED, and that is not incidental. With Disk BASIC's 60Hz
 * handler live, POLCAT drives $FF02 for its own scan, and an interrupt landing
 * between this driver's strobe write and its row read makes it read one column
 * while another is selected. The game masks them; so does this.
 */
#include <stdint.h>

#include "../../c128/src/input.h"

#define RESULT ((unsigned char *)0x7F00)
#define NKEYS  26

int main(void)
{
    unsigned char i;

    asm { orcc #$50 }               /* BASIC's scan is not ours to race */
    RESULT[31] = 0;                 /* the completion stamp, cleared first */
    for (i = 0; i < NKEYS; i++) RESULT[i] = 0;

    kb_init();
    for (i = 0; i < NKEYS; i++)
        RESULT[i] = (unsigned char)kb_waitkey();

    RESULT[31] = 0x5A;
    for (;;) ;
    return 0;
}

/* Keyboard for the Apple IIgs.
 *
 * $C000 holds the last key with bit 7 set while one is waiting; $C010 clears
 * the strobe. That is the whole interface, it is the same on every Apple II,
 * and it needs no ROM, no interrupts and no firmware -- which matters on a
 * port that has taken the machine and switched the language card in.
 *
 * "BLOCKS UNTIL ONE KEY IS PRESSED AND RELEASED" is the contract, and on this
 * machine the second half is free: the strobe latches ONE keypress and clearing
 * it is what allows the next, so there is no auto-repeat to swallow and no key
 * held down to read twice.
 */
#include <stdint.h>

#include "../../c128/src/input.h"

#define KBD    (*(volatile unsigned char *)0xC000)
#define STROBE (*(volatile unsigned char *)0xC010)

uint16_t kb_entropy;

/* Called ONCE before the title screen, to throw away whatever the keyboard was
   holding when the game started. IT WAS DEAD CODE ON TWO PORTS -- m65input.c
   and x16input.c both defined one and neither was ever called -- and on the
   Amiga the RETURN that launched the game was still queued when the title
   asked for a key, so the title dismissed itself. This port is booted from a
   disk with no shell and nothing to type, so there is nothing to discard; it
   clears the strobe anyway, because the alternative is a comment claiming
   there cannot be one. */
void kb_init(void)
{
    STROBE;
}

char kb_waitkey(void)
{
    unsigned char c;

    /* The entropy counter is bumped once per pass, and sampling it when the
       player presses a key is this port's only source of randomness: there is
       no clock the game reads, and how long a human takes to answer a prompt
       is unpredictable at this resolution. Before this existed on any port
       every game was the same one. */
    while (!(KBD & 0x80))
        kb_entropy++;

    c = (unsigned char)(KBD & 0x7F);
    STROBE;

    /* A IIgs returns real ASCII including lowercase, where a C64 returns
       PETSCII -- the X16 port met the same difference and it cost a release.
       The game's prompts are all upper case, so fold here rather than at
       thirty call sites. */
    if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 32);
    return (char)c;
}

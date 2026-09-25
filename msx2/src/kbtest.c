/* THE KEYBOARD DRIVER, driven through the real path: tools/kbtest.tcl types
   into openMSX's emulated MATRIX, the BIOS interrupt handler queues the
   characters, and msx2input.c reads them off the ring. Nothing is poked.

   Three things are checked, and the first is the one a sloppy test passes:
     1. kb_init DISCARDS -- an "x" is typed before it runs, so if kb_init
        did nothing the first key read would be X;
     2. every key maps -- letters folded, digits, punctuation, RETURN, ESC,
        BS, and the two cursor keys that have no ASCII;
     3. the poll loop runs -- kb_entropy moved while it waited.

   Nothing is printed until every key is in, in case DOS's console output
   checks the ring for CTRL-C (not measured; not risked). Installed as the
   shell, like the game. */
#include "input.h"
#include "sid.h"
#include "vdc.h"

#define N 13
char got[N];                        /* global: tools/kbtest.tcl dumps it on a timeout */

void snd_poll(void) { }             /* the test does not need the PSG */

static void say(const char *s)
{
    (void)s;
    __asm
        ex   de, hl
        ld   c, #9
        push ix
        call 5
        pop  ix
    __endasm;
}

static void put_dec(unsigned int v)
{
    static char out[8];
    unsigned char i = 6;
    out[6] = ' '; out[7] = '$';
    do { out[--i] = (char)('0' + v % 10); v /= 10; } while (v && i);
    say(&out[i]);
}

void main(void)
{
    unsigned char i;
    unsigned int e0;

    for (i = 0; i < 150; i++) wait_vsync();   /* the "x" arrives in here */
    kb_init();
    e0 = kb_entropy;
    for (i = 0; i < N; i++)
        got[i] = kb_waitkey();

    say("KEYS $");
    for (i = 0; i < N; i++) put_dec((unsigned char)got[i]);
    say("\r\nENTROPY MOVED $");
    put_dec(kb_entropy - e0);
    say("\r\nDONE\r\n$");
    for (;;)
        ;
}

/* Keyboard for the F256K.
 *
 * ALMOST NOTHING IS HERE, and that is the point. The machine has one event
 * queue carrying keystrokes, file I/O and timers, so the pump and the
 * translation live in f256evt.c where storage can share them; this file is
 * the shared input.h contract on top of a ring somebody else fills.
 *
 * The measurements behind the translation are in f256evt.c and were taken by
 * src/keyprobe.c. The short version: this keyboard is a C64 layout, RUN/STOP
 * stands in for the ESC key it does not have, and SHIFT arrives as an event
 * of its own.
 */
#include <stdint.h>
#include "input.h"
#include "f256evt.h"

/* The sound driver does not exist yet. Every other port calls snd_poll()
   INSIDE the key wait, because that loop is where the program spends its idle
   time and is the driver's only chance to run -- so the call site is written
   now and the stub goes away when tedsnd's opposite number lands. Writing it
   later means finding this loop again, and the Plus/4's music played at
   double speed because a timing detail was not settled at its call site. */
#ifndef TREK_F256_SOUND
static void snd_poll(void) { }
#else
void snd_poll(void);
#endif

uint16_t kb_entropy = 0;

void kb_init(void)
{
    /* Drain whatever the launch left behind. Measured: two events, a
       file.CLOSED from pexec closing the PGZ it just loaded, and a timer.
       Neither is a key -- so the Amiga's fault, where the RETURN that started
       the game dismissed the title screen, cannot happen here. It drains
       anyway: "cannot happen" is a claim about a queue nobody has looked in
       lately, and input.h records kb_init being DEAD CODE on two ports. */
    f256_drain();
}

char kb_waitkey(void)
{
    unsigned char c;
    for (;;) {
        /* kb_entropy is bumped once per pass and sampled when the player
           answers. How long a human takes to reach a key is this port's only
           entropy source -- before the C128 had this, every game was the same
           galaxy. */
        kb_entropy++;
        snd_poll();
        f256_pump();
        c = f256_getkey();
        if (c != KB_NONE) return (char)c;
    }
}

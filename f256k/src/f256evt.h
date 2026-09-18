#ifndef F256EVT_H
#define F256EVT_H

/* THE EVENT QUEUE, WHICH BELONGS TO NEITHER THE KEYBOARD NOR STORAGE.
 *
 * FoenixMCP has ONE NextEvent queue and it carries keystrokes, every byte of
 * file I/O and the kernel's timers. That is not a detail of either driver; it
 * is the shape of the machine, so the pump lives here and both drivers sit on
 * top of it.
 *
 * THE REFERENCE IMPLEMENTATION GETS THIS WRONG IN A WAY WORTH NAMING. uno's
 * vendored kernel.c waits for a file event with `default: continue;` -- every
 * event that is not the one it wants is dropped on the floor, keystrokes
 * included. A player typing while a briefing page loads would lose the keys,
 * and nothing anywhere would report it. Here a file wait keeps filling the
 * key ring, and the ring is what kb_waitkey reads.
 *
 * ONE CALL SITE. f256_pump() is the only place in this port that calls
 * NextEvent. If a second one ever appears, the two will race for the same
 * queue and whichever loses will look like flaky hardware.
 */
#include "f256kern.h"

/* Take at most one event off the queue and sort it. Returns 1 if it took one.
   Keys go to the ring, file and directory events to f256_file_*, timers are
   discarded (nothing in this port waits on one). */
unsigned char f256_pump(void);

/* The oldest key still in the ring, already translated to the shared header's
   values, or KB_NONE. */
unsigned char f256_getkey(void);

/* Throw away everything queued, keys included. */
void f256_drain(void);

/* The most recent file/directory event, and whether one is waiting. The
   storage layer claims it by clearing `ready`. */
extern struct f256_event f256_file_ev;
extern unsigned char f256_file_ready;

/* A FILE EVENT THAT WAS NEVER CLAIMED. Counting these is the difference
   between a storage bug that shows up as a failed load and one that shows up
   as a file that is quietly short. */
extern unsigned char f256_file_lost;

/* Pump until a file event is ready, or until `frames` frames have passed.
   Returns the event type, or F256_WAIT_TIMEOUT.

   THE TIMEOUT IS NOT DEFENSIVE PADDING. Every one of these calls is a
   `for(;;)` in the reference implementation, so a kernel that never answers
   hangs the game with no message and no way out -- and this port cannot test
   against real hardware, where an absent SD card is the ordinary case. */
#define F256_WAIT_TIMEOUT 0xFF
unsigned char f256_wait_file(unsigned char frames);

/* The kernel's own frame counter, low byte, 60 Hz. It lives here rather than
   in the video driver because it is a KERNEL CALL, not a video register --
   see f256kern.h on why it is not the raster and not an event. */
unsigned char f256_frames(void);

#endif

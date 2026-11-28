#ifndef NEXTVID_H
#define NEXTVID_H

/* The NeXT side of the video seam: the parts of nextvid.m the other NeXT
   files need, and that c128/src/vdc.h, the shared seam, has no business
   knowing about. Plain C, so the .c files can include it. */

/* THE KEYBOARD ARRIVES THROUGH THE EVENT LOOP, in wait_vsync(). Key-downs
   land on a small queue as their character codes; the arrow keys, which the
   NeXT sends in its symbol set rather than as ASCII, are given these codes
   from the unused control range. 0 means the queue is empty. */
#define NEXT_KEY_LEFT  0x1C
#define NEXT_KEY_RIGHT 0x1D
#define NEXT_KEY_UP    0x1E
#define NEXT_KEY_DOWN  0x1F

int  next_next_key(void);
void next_flush_keys(void);

/* Sixtieths of a second since the program started, from gettimeofday: the
   clock the event loop paces itself by, and the sound driver's tick. */
unsigned long next_ticks(void);

/* EGATrek.app itself, where the data files are, and the folder holding it,
   where saves and the hall of fame go. No trailing slash. */
const char *next_app_dir(void);
const char *next_save_dir(void);

#endif

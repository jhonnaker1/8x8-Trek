#ifndef MACVID_H
#define MACVID_H

/* The Macintosh side of the video seam: the parts of src/macvid.c that the
   other Mac files need and that c128/src/vdc.h, the shared seam, has no
   business knowing about. */

/* THE KEYBOARD ARRIVES THROUGH THE EVENT LOOP, which lives in wait_vsync()
   -- see macvid.c. Key-down and auto-key events land in a small queue;
   src/macinput.c takes them off it. 0 means the queue is empty. The value
   is the event's character code (arrows are $1C..$1F), untranslated. */
int  mac_next_key(void);
void mac_flush_keys(void);

/* What the machine reported at start-up, kept for the smoke test to print.
   Nothing in the game reads it. */
struct mac_facts {
    short sys_version;      /* SysEnvirons: $0607 is System 6.0.7 */
    short machine;          /* SysEnvirons machineType */
    short processor;        /* SysEnvirons processor: 1 = 68000 ... */
    short has_cqd;          /* Color QuickDraw present */
    long  qd_version;       /* Gestalt 'qd  ', 0 if Gestalt is absent */
    short depth;            /* main screen's pixel depth */
    short scr_w, scr_h;     /* main screen's size */
    short font_ascent, font_descent, font_widmax, font_leading; /* Monaco 12 */
    short gworld_err;       /* NewGWorld's result */
    short pixels_32bit;     /* the GWorld's pixels need 32-bit addressing */
};
const struct mac_facts *mac_get_facts(void);

#endif

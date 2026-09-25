/* Keyboard for the MSX2: the BIOS's own key buffer, read DIRECTLY.
 *
 * THE BIOS IS NOT CALLED, and that is measured, not taste. The BIOS
 * interrupt handler already scans the matrix every frame, handles SHIFT,
 * CAPS and the key repeat, and queues characters in a 40-byte ring at
 * KEYBUF in page 3, which is RAM whatever page 0 holds. CHSNS/CHGET would
 * read the same ring -- but through CALSLT, which puts the BIOS ROM in page
 * 0, and an interrupt taken THEN runs the ROM's handler on OUR stack: 88
 * bytes forced by a waiting CHGET in STKTEST.COM, against 14 while DOS owns
 * page 0 and its handler switches stacks. This loop is entered 100 bytes
 * deep. So it reads the ring itself, which is what CHGET does inside:
 *
 *     GETPNT == PUTPNT      empty
 *     else                  take (GETPNT), advance, wrap at KEYBUF + 40
 *
 * GETPNT is ours alone -- only a reader moves it -- and PUTPNT is one
 * 16-bit load. The pair is still read with interrupts off: it costs two
 * bytes, and keeps the compare honest if SDCC ever splits a load.
 *
 * NO BDOS CONSOLE CALLS while this is the reader. A DOS console routine may
 * check the keyboard for CTRL-C or CTRL-S, and if it takes from this ring it
 * eats keys. NOT MEASURED -- assumed, because the game has no need to find
 * out: it never uses the DOS console.
 *
 * THE POLL LOOP IS NOT AN IDLE SPIN: kb_entropy is the only randomness
 * (how long a human takes to answer picks the galaxy), and snd_poll() is the
 * sound driver's only chance to run -- the Atari port shipped SILENT because
 * both loops here lacked it. */
#include <stdint.h>

#include "input.h"
#include "sid.h"
#include "vdc.h"

#define IRQ_OFF() __asm__("di")
#define IRQ_ON()  __asm__("ei")

#define PUTPNT  (*(unsigned char * volatile *)0xF3F8)
#define GETPNT  (*(unsigned char * volatile *)0xF3FA)
#define KEYBUF  ((unsigned char *)0xFBF0)
#define BUFEND  ((unsigned char *)0xFC18)        /* KEYBUF + 40 */

/* MSX character codes for the keys with no printable character. */
#define MSX_BS     0x08
#define MSX_RET    0x0D
#define MSX_ESC    0x1B
#define MSX_UP     0x1E
#define MSX_DOWN   0x1F
#define MSX_DEL    0x7F

uint16_t kb_entropy;

/* Takes one character off the ring, or 0 if it is empty. */
static unsigned char take(void)
{
    unsigned char *g, c;

    IRQ_OFF();
    g = GETPNT;
    if (g == PUTPNT) {
        IRQ_ON();
        return 0;
    }
    c = *g++;
    if (g == BUFEND)
        g = KEYBUF;
    GETPNT = g;
    IRQ_ON();
    return c;
}

/* Throw away whatever the keyboard queued before the game asked -- the
   Amiga's title dismissed itself on the RETURN that launched it. */
void kb_init(void)
{
    IRQ_OFF();
    GETPNT = PUTPNT;
    IRQ_ON();
}

char kb_waitkey(void)
{
    unsigned char c;

    for (;;) {
        c = take();
        switch (c) {
        case 0:
            break;
        case MSX_UP:    return KB_UP;
        case MSX_DOWN:  return KB_DOWN;
        case MSX_RET:   return KB_RETURN;
        case MSX_ESC:   return KB_ESC;
        case MSX_BS:
        case MSX_DEL:   return KB_DELETE;
        default:
            if (c >= 'a' && c <= 'z')           /* the UI compares upper case */
                return (char)(c - 'a' + 'A');
            if (c >= ' ' && c < MSX_DEL)
                return (char)c;
            continue;          /* GRAPH/CODE glyphs, other controls: ignore */
        }
        snd_poll();
        kb_entropy++;
        wait_vsync();
    }
}

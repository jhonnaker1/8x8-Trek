#ifndef F256KERN_H
#define F256KERN_H

/* The FoenixMCP MicroKernel ABI, only the parts this port calls.
 *
 * NOT VENDORED. The upstream `api.h` (Jessie Oberreuter's, GPL3 with a Linux-
 * style linking exception that explicitly blesses including it) is a cc65
 * header carrying the whole kernel -- block devices, filesystem, directory,
 * network, display, config. This port calls six entries, so it describes six,
 * the way p4bank.c describes the Plus/4's KERNAL rather than importing one.
 * The layouts below are upstream's and were read off it; anything this file
 * gets wrong is a bug here, not there.
 *
 * TWO THINGS ABOUT THE SHAPE OF THIS API drive the whole port:
 *
 *   * THE JUMP TABLE SLOTS ARE FOUR BYTES, not the three a JMP needs. Assume
 *     three and every call after the first lands in the middle of an
 *     instruction.
 *   * EVERYTHING IS ONE EVENT QUEUE. Keyboard, file I/O and the frame tick all
 *     arrive through NextEvent, so a blocking read eats keystrokes and a
 *     frame wait eats file data. Every pump in this port has to hand back what
 *     it did not ask for rather than drop it.
 */

/* $FF00, four bytes per entry. */
#define K_NextEvent  0xFF00
#define K_ReadData   0xFF04
#define K_ReadExt    0xFF08
#define K_Yield      0xFF0C
#define K_SetTimer   0xFFF0

/* Kernel call arguments mount at $F0 -- which is why f256k.ld stops the
   compiler's zero page at $00F0 and says so. The layout is NOT flat:
 *
 *     $F0-$F1  events.dest      where NextEvent copies the event to
 *     $F2      events.pending   negative count of queued events
 *     $F3....  A UNION of every other call's arguments
 *     $F8-$FF  ext / extlen / buf / buflen / ptr
 *
 * THE UNION STARTS AT $F3, AND GETTING THAT WRONG IS SILENT. SetTimer's
 * `units` byte is the union's first, so it is $F3 -- writing it to $F0
 * instead overwrites the low byte of the event destination pointer, and the
 * kernel then copies events to whatever address that makes. The first run of
 * the frame-counter probe did exactly this: it reported 0.8 frames per second
 * and 158 events of type 0, and both numbers were the same bug.
 *
 * The event pointer being OUTSIDE the union is deliberate upstream -- it is
 * why one NextEvent destination survives every other kernel call. */
#define K_ARGS_EVENT   (*(struct f256_event **)0x00F0)
#define K_ARGS_PENDING (*(volatile signed char *)0x00F2)
#define K_ARG_UNION    0x00F3
#define K_TIMER_UNITS  (*(volatile unsigned char *)0x00F3)
#define K_TIMER_ABS    (*(volatile unsigned char *)0x00F4)
#define K_TIMER_COOKIE (*(volatile unsigned char *)0x00F5)

/* THE FRAME TIMER, AND IT IS NOT AN EVENT.
 *
 * `clock.TICK` exists in the event enum and NOTHING IN THE KERNEL EMITS IT --
 * a probe that waited for one counted zero in five seconds. What the kernel
 * actually offers is SetTimer, and its `units` byte has a QUERY bit: with it
 * set, the call queues nothing and returns the kernel's own frame counter in
 * A (delay.asm, `done: lda kernel.ticks / rts`).
 *
 * That is the better answer anyway. A frame wait built on an event would pull
 * from the one queue that also carries every keystroke and every byte of file
 * data, so waiting for a frame would eat input. A query touches none of it. */
#define TIMER_FRAMES   0x00
#define TIMER_SECONDS  0x01
#define TIMER_QUERY    0x80

struct f256_event {
    unsigned char type;
    unsigned char buf;      /* kernel's buf page id */
    unsigned char ext;      /* kernel's ext page id */
    unsigned char data[8];  /* union of the per-type payloads */
};
/* event_key_t, laid over data[]: keyboard, raw, ascii, flags. */
#define EV_KEY_ASCII(e) ((e).data[2])
#define EV_KEY_FLAGS(e) ((e).data[3])

/* AN EVENT'S TYPE IS ITS OFFSET IN THIS STRUCT. The kernel publishes the event
   list as a struct of uint16_t and the "type" byte is the member's offset, so
   mirroring the layout lets the compiler compute the constants -- and a
   hand-counted 78 for clock.TICK would be a number nobody could check. */
struct f256_events {
    unsigned int reserved, deprecated, GAME, DEVICE;
    struct { unsigned int PRESSED, RELEASED; } key;
    struct { unsigned int DELTA, CLICKS; } mouse;
    struct { unsigned int NAME, SIZE, DATA, WROTE, FORMATTED, ERROR; } block;
    struct { unsigned int SIZE, CREATED, CHECKED, DATA, WROTE, ERROR; } fs;
    struct { unsigned int NOT_FOUND, OPENED, DATA, WROTE, EOF_, CLOSED,
                          RENAMED, DELETED, ERROR, SEEK; } file;
    /* NINE MEMBERS, AND uno's VENDORED api.h HAS SEVEN. The copy in
       commodore-uno/f256/toolchain predates CREATED and DELETED, so every
       type from net.TCP on is four too low there -- which is exactly how the
       first run of the tick probe came to wait for type 78 (net.TCP) while
       calling it clock.TICK, and count zero. Taken from the kernel's own
       api.asm, which is the thing in the ROM. */
    struct { unsigned int OPENED, VOLUME, FILE, FREE, EOF_, CLOSED, ERROR,
                          CREATED, DELETED; } directory;
    struct { unsigned int TCP, UDP; } net;
    struct { unsigned int EXPIRED; } timer;
    struct { unsigned int TICK; } clock;   /* nothing emits this -- see above */
    struct { unsigned int IRQ; } irq;
};
#define EV(m) ((unsigned char)(unsigned int)(&(((struct f256_events *)0)->m)))

#endif

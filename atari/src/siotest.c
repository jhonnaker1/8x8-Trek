/* Can this port read and write disk sectors with NO DOS AT ALL?
 *
 * Everything the game does with files goes through CIO's `D:` handler, which
 * is Atari DOS -- and DOS is the reason there is no release bundle (its
 * DOS.SYS is Atari's code, not ours) and the reason $0700..$1FFF is not
 * available for writable data. Dropping it is worth 1,510 more bytes AND a
 * disk with nothing on it that is not ours.
 *
 * THIS PROVES THE PRIMITIVE BEFORE ANY OF THAT IS BUILT. SIO is the layer
 * under CIO: fill the Device Control Block at $0300 and JSR SIOV ($E459). No
 * filesystem, no handler, no DOS -- just "give me sector N".
 *
 * Sector 1 of the game disk is DOS's own boot record, which starts
 * 00 03 00 07 -- flags 0, three sectors, load at $0700. That is a known
 * answer, which is the only reason this is a test rather than a demonstration.
 *
 * Results land in globals for the bridge to peek; there is no screen here on
 * purpose, so a failure cannot be hidden by the video seam.
 */
#include <stdint.h>

#define DDEVIC (*(volatile unsigned char *)0x0300)
#define DUNIT  (*(volatile unsigned char *)0x0301)
#define DCOMND (*(volatile unsigned char *)0x0302)
#define DSTATS (*(volatile unsigned char *)0x0303)
#define DBUFLO (*(volatile unsigned char *)0x0304)
#define DBUFHI (*(volatile unsigned char *)0x0305)
#define DTIMLO (*(volatile unsigned char *)0x0306)
#define DBYTLO (*(volatile unsigned char *)0x0308)
#define DBYTHI (*(volatile unsigned char *)0x0309)
#define DAUX1  (*(volatile unsigned char *)0x030A)
#define DAUX2  (*(volatile unsigned char *)0x030B)

/* Peeked by tools/probe_sio.py. volatile so nothing folds them away: the
   program never reads them itself, which is exactly what makes a probe
   invisible to the optimiser -- see tools/session.py on the same trap. */
volatile unsigned char sio_status;
volatile unsigned char sio_bytes[8];
volatile unsigned char sio_wstatus;
volatile unsigned char sio_rb[8];
volatile unsigned char sio_done;

static unsigned char buf[128];

static void sio(void) {
    __asm__ volatile("jsr $E459" : : : "a", "x", "y", "memory", "p");
}

static unsigned char sector(unsigned char cmd, unsigned int n,
                           unsigned char stat) {
    DDEVIC = 0x31;                 /* disk */
    DUNIT  = 1;
    DCOMND = cmd;                  /* 'R' read, 'W' write-with-verify */
    DSTATS = stat;                 /* $40 read into memory, $80 write out */
    DBUFLO = (unsigned char)((unsigned int)buf & 0xFF);
    DBUFHI = (unsigned char)((unsigned int)buf >> 8);
    DTIMLO = 15;
    DBYTLO = 128;
    DBYTHI = 0;
    DAUX1  = (unsigned char)(n & 0xFF);
    DAUX2  = (unsigned char)(n >> 8);
    sio();
    return DSTATS;                 /* $01 is success */
}

int main(void) {
    unsigned char i;

    /* READ sector 1 -- DOS's boot record, a known answer. */
    sio_status = sector('R', 1, 0x40);
    for (i = 0; i < 8; i++) sio_bytes[i] = buf[i];

    /* WRITE and read back a scratch sector well past anything the game uses.
       720 is the last sector of a single-density disk; this image is enhanced
       density, so 800 is inside the medium but outside DOS's own map. */
    for (i = 0; i < 128; i++) buf[i] = (unsigned char)(0xA5 ^ i);
    sio_wstatus = sector('W', 800, 0x80);
    for (i = 0; i < 128; i++) buf[i] = 0;
    sector('R', 800, 0x40);
    for (i = 0; i < 8; i++) sio_rb[i] = buf[i];

    sio_done = 1;
    for (;;) { }
}

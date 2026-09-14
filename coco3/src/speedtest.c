/* DOES CoCo 3 DISK I/O SURVIVE 1.78 MHz? Item 55, on hardware, WITHOUT the card.
 *
 * WHY THIS EXISTS. `vdc_init()` writes `$FFD9`, so the CoCo 3 port runs at
 * double speed from the title screen on -- including every overlay load and
 * every SAVE -- and CoCo 3 disk I/O at double speed is a known hazard on real
 * hardware that MAME does not reproduce. The item could not be tested because
 * the port needs a SuperSprite FM+ and nobody here has one.
 *
 * BUT THE QUESTION IS A DISK QUESTION, AND THE DISK IS THE HALF THAT EXISTS.
 * This program touches no video at all: no `$FF7E`, no V9958, no YM2149. It
 * runs on a bare CoCo 3 with any disk controller, including a CoCo SDC.
 *
 * IT KEEPS THE ROM MAPPED, which is the whole reason it is short. The game
 * writes `$FFDF` and pages Disk BASIC away, which is why it has to take over
 * the NMI, IRQ and FIRQ vectors and supply its own handlers -- see
 * coco3storage.c, and the nine hours that cost. Here Disk BASIC is still
 * there, its handlers are still valid, `dskcon_init` works exactly as
 * designed, and the program can simply RTS back to BASIC when it is done.
 *
 * TWO PASSES, AND THE SLOW ONE IS THE CONTROL. A failure at 1.78 MHz means
 * nothing on its own -- it could be a tired drive, a marginal disk, a bad
 * cable. So the same sectors are read at 0.89 MHz first. What matters is the
 * DIFFERENCE between the two columns, not either one alone.
 *
 * EVERY SECTOR IS READ TWICE AND COMPARED, not just checked for a status
 * byte. A controller that reports success and hands back a wrong byte is
 * exactly the failure mode double speed is suspected of, and DCSTA cannot
 * see it.
 */
#include <stdint.h>
#include <dskcon-standalone.h>

/* $7F00, above the CLEAR ceiling the game already asks for, so the report
   survives the return to BASIC and PEEK can read it. Same address and same
   reason as writetest.c; see its note on why the stack must be below it. */
#define R ((unsigned char *)0x7F00)

#define SPEED_FAST (*(unsigned char *)0xFFD9)   /* the ADDRESS is the latch */
#define SPEED_SLOW (*(unsigned char *)0xFFD8)

/* Tracks 0..16 at eighteen sectors is 306 sectors, read twice per pass, twice
   over -- about 1,200 reads. Track 17 is the directory and is skipped only to
   keep the two passes reading identical ground. */
#define LAST_TRACK  16
#define SECTORS     18

static unsigned char a[256];
static unsigned char b[256];
static unsigned long dsk_handle;

static unsigned char rd(unsigned char trk, unsigned char sec, unsigned char *dst)
{
    DCOPC = 2;                  /* 2 = read */
    DCDRV = 0;
    DCTRK = trk;
    DCSEC = sec;
    DCBPT = dst;
    dskcon_processSector();
    return DCSTA;
}

/* Returns nothing; fills four report bytes at `base`:
     +0 status errors, +1 compare mismatches, +2 first bad track,
     +3 first bad sector.  Counts saturate at 255 rather than wrapping,
     because a count that wraps to zero reads exactly like a pass. */
static void pass(unsigned char base)
{
    unsigned char trk, sec, i;
    unsigned char sta = 0, bad = 0, badtrk = 0xFF, badsec = 0xFF;

    for (trk = 0; trk <= LAST_TRACK; trk++) {
        for (sec = 1; sec <= SECTORS; sec++) {
            unsigned char s1 = rd(trk, sec, a);
            unsigned char s2 = rd(trk, sec, b);
            unsigned char mismatch = 0;

            if (s1 || s2) {
                if (sta < 255) sta++;
                if (badtrk == 0xFF) { badtrk = trk; badsec = sec; }
                continue;               /* a failed read has nothing to compare */
            }
            for (i = 0; i < 255; i++)
                if (a[i] != b[i]) { mismatch = 1; break; }
            if (!mismatch && a[255] != b[255]) mismatch = 1;
            if (mismatch) {
                if (bad < 255) bad++;
                if (badtrk == 0xFF) { badtrk = trk; badsec = sec; }
            }
        }
        /* A crumb per track, so a machine that HANGS says where. A probe that
           can only report after it finishes cannot report a hang at all --
           which is how "the read is slow" and "the read is stuck" looked
           identical on this port for a day. */
        R[14] = trk;
        R[15] = (unsigned char)(R[15] + 1);
    }

    R[base]     = sta;
    R[base + 1] = bad;
    R[base + 2] = badtrk;
    R[base + 3] = badsec;
}

int main(void)
{
    unsigned char i;

    for (i = 0; i < 32; i++) R[i] = 0;
    R[0] = 0xA1;                        /* entered main, before anything can fail */

    asm { orcc #$50 }                   /* dskcon_init wants interrupts masked */
    dsk_handle = dskcon_init(dskcon_nmiService);
    asm { andcc #$AF }                  /* and DSKCON needs them back */

    /* THE CONTROL FIRST. If this column is not clean the disk or the drive is
       the problem and the fast column says nothing about the clock. */
    SPEED_SLOW = 0;
    R[1] = 0xB1;
    pass(4);

    SPEED_FAST = 0;
    R[1] = 0xB2;
    pass(8);

    /* BACK TO 0.89 MHz BEFORE RETURNING. Handing BASIC a machine at double
       speed is not this program's business, and the ROM was never asked. */
    SPEED_SLOW = 0;

    asm { orcc #$50 }
    dskcon_shutdown(dsk_handle);
    asm { andcc #$AF }

    R[1] = 0xB3;
    R[2] = LAST_TRACK + 1;
    R[3] = SECTORS;
    R[13] = 0x5A;                       /* the report is complete */
    return 0;                           /* ROM is still mapped: RTS to BASIC */
}

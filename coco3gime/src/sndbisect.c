/* WHICH LINE OF snd_init BREAKS THE DISK?
 *
 * The game hangs in dskcon_processSector on the 46th sector of its startup.
 * Track 21 sector 10 is granule 41, and mkdisk put MUSIC.DAT there -- so the
 * forty-five reads before it are fine and the first one AFTER snd_init() is
 * not. Masking IRQ across the DSKCON call changed nothing, which rules out
 * the transfer window and leaves snd_init's own writes.
 *
 * So: a read after every one of them, in order, with nothing else moving.
 * A step that hangs stops the probe there and the report says which -- the
 * bytes already written are the answer.
 *
 * This deliberately does NOT call snd_init. It repeats its body one line at a
 * time, because the question is which line, and a function call would answer
 * "snd_init", which is already known.
 */
#include "../../core/storage.h"

#define INIT0    (*(unsigned char *)0xFF90)
#define INIT1    (*(unsigned char *)0xFF91)
#define IRQENR   (*(unsigned char *)0xFF92)
#define FIRQENR  (*(unsigned char *)0xFF93)
#define TMRHI    (*(unsigned char *)0xFF94)
#define TMRLO    (*(unsigned char *)0xFF95)
#define PIA0_DA  (*(unsigned char *)0xFF00)
#define PIA0_CRA (*(unsigned char *)0xFF01)
#define PIA0_DB  (*(unsigned char *)0xFF02)
#define PIA0_CRB (*(unsigned char *)0xFF03)
#define PIA1_DA  (*(unsigned char *)0xFF20)
#define PIA1_CRA (*(unsigned char *)0xFF21)
#define PIA1_DB  (*(unsigned char *)0xFF22)
#define PIA1_CRB (*(unsigned char *)0xFF23)

#define r    ((unsigned char *)0x5F00)
#define DONE 0x5A

static unsigned int got;
static unsigned char buf[512];
static unsigned char sink;

#define TRY(n) r[n] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got)

int main(void)
{
    unsigned int i;

    for (i = 0; i < 32; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$5E00 }
    asm { sta $FFDF }
    INIT0 = 0x0C;

    TRY(1);                                              /* baseline */

    PIA1_CRA = (unsigned char)(PIA1_CRA & 0xFB);         /* select DDRA */
    PIA1_DA  = 0xFC;
    PIA1_CRA = 0x34;
    TRY(2);

    PIA0_CRA = 0x34;              TRY(3);
    PIA0_CRB = 0x34;              TRY(4);
    PIA1_CRB = 0x3C;              TRY(5);

    PIA0_CRA = (unsigned char)(PIA0_CRA & 0xFE);
    PIA0_CRB = (unsigned char)(PIA0_CRB & 0xFE);
    PIA1_CRA = (unsigned char)(PIA1_CRA & 0xFE);
    PIA1_CRB = (unsigned char)(PIA1_CRB & 0xFE);
    TRY(6);
    sink = PIA0_DA;  sink = PIA0_DB;
    sink = PIA1_DA;  sink = PIA1_DB;
    TRY(7);

    INIT1 = 0x20;                 TRY(8);
    IRQENR = 0x00;
    FIRQENR = 0x08;               TRY(9);

    *((unsigned char *)0xFEF7) = 0x7E;
    *((void **)0xFEF8) = (void *)0x2800;
    TRY(10);

    INIT0 = 0x2C;                 TRY(11);

    /* AND NOW THE PART THE FIRST VERSION LEFT OUT: actually take interrupts,
       and read the files the GAME reads, in the game's order. Every register
       write above is innocent on its own; the hang is on the first read after
       snd_init returns, and snd_init's last act is to clear I. */
    asm { andcc #$EF }
    TRY(12);
    r[13] = plat_read_all("STRINGS.DAT", buf, sizeof buf, &got);
    r[14] = plat_read_all("MUSIC.DAT",   buf, sizeof buf, &got);
    TRY(15);

    r[31] = DONE;
    for (;;) ;
}

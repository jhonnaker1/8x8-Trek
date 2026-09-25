/* FIRST SOUND for the PSG driver: the refusal beep, a gap, twelve seconds of
   the title track, then an effect alone -- a fixed script, so a recording
   can be checked against MUSIC.DAT for PITCH and TEMPO both. A pitch check
   is silent about tempo (the X16 played every note right at double speed).

   Far memory is not written yet, so this links MUSIC.DAT into the program
   and gives the driver a far_read over it. The game will not do that: it
   costs the whole file, and the budget is what far memory is for. */
#include <string.h>
#include "sid.h"
#include "farmem.h"

#define JIFFY (*(volatile unsigned int *)0xFC9E)

extern const unsigned char music_dat[];      /* build/musicdat.c */

void far_read(uint16_t off, void *dst, uint8_t len)
{
    memcpy(dst, (const void *)off, len);
}

static void bdos_puts(const char *s)
{
    (void)s;
    __asm
        ld   d, h            ; sdcccall(1): the pointer arrives in HL
        ld   e, l
        ld   c, #9
        push ix              ; the frame pointer -- BDOS makes no promise
        call 5
        pop  ix
    __endasm;
}

static void play_for(unsigned int jiffies)
{
    unsigned int t0 = JIFFY;
    while ((unsigned int)(JIFFY - t0) < jiffies)
        snd_poll();
}

void main(void)
{
    unsigned int per_sec;

    snd_init();
    per_sec = snd_region == REGION_PAL ? 50 : 60;
    bdos_puts(snd_region == REGION_PAL ? "PSG TEST: PAL, 50HZ\r\n$"
                                       : "PSG TEST: NTSC, 60HZ\r\n$");
    snd_music_data((unsigned int)music_dat, 1);

    snd_beep();
    play_for(per_sec);                       /* one second of silence */
    snd_music(MUS_TITLE);
    play_for(12 * per_sec);
    snd_music(MUS_NONE);
    play_for(per_sec / 2);
    snd_effect(SFX_B);
    play_for(2 * per_sec);
    snd_off();
    bdos_puts("DONE\r\n$");
}

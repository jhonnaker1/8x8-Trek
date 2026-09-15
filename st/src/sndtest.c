/* IS THE ST MAKING ANY SOUND AT ALL, and if not, where does it stop?
 *
 * WRITTEN BECAUSE THE PORT SHIPPED SILENT. Every automated run of this port
 * used `--sound off`, so every check that passed was a check about the
 * PICTURE, and nobody listened until Jamie played the release. A driver that
 * writes no registers looks exactly like a driver that writes the right ones
 * when you are reading a screenshot.
 *
 * The chain has five links and this reports each one, because "no sound" is
 * the same symptom for all of them:
 *
 *   1. MUSIC.DAT loaded into far memory at all
 *   2. snd_init ran and left the driver enabled
 *   3. the 200Hz clock is MOVING -- the tick comes from $4BA through Supexec,
 *      and a frozen counter means the driver never advances a note
 *   4. snd_poll actually wrote the PSG -- read back through Giaccess, which
 *      is a READ with the register number and no $80 bit
 *   5. the mixer's tone bit for channel A is CLEAR (they are active low)
 *
 * READING THE PSG BACK IS THE POINT. The C128's SID is write-only and the
 * project had to infer; the YM2149 reads back, so this can say what is
 * actually in the chip rather than what the driver believes it put there.
 */
#include <stdint.h>
#include <tos.h>

#include "../../c128/src/sid.h"
#include "../../core/farmem.h"
#include "../../core/storage.h"
#include "../falcon/src/music_data.h"

#define PSG_R(r)  Giaccess(0, (WORD)(r))

static long hz200_raw(void) { return *(volatile long *)0x4BAL; }
static long now_200(void) { return Supexec(hz200_raw); }

static void say(const char *s) { Cconws(s); Cconws("\r\n"); }

static void sayhex(const char *label, long v)
{
    static char buf[16];
    static const char hex[] = "0123456789ABCDEF";
    int i;
    Cconws(label);
    for (i = 7; i >= 0; i--) buf[7 - i] = hex[(v >> (i * 4)) & 0xF];
    buf[8] = '\0';
    Cconws(" $");
    Cconws(buf);
    Cconws("\r\n");
}

int main(void)
{
    uint16_t mb;
    long t0, t1;
    int i;

    say("");
    say("EGA Trek -- Atari ST sound probe");
    say("");

    /* 1. the file */
    mb = far_load("MUSIC.DAT");
    if (mb == FAR_NONE) {
        say("1. MUSIC.DAT: NOT LOADED -- far_load returned FAR_NONE.");
        say("   Nothing below this line can mean anything.");
    } else {
        sayhex("1. MUSIC.DAT loaded, base", (long)mb);
        sayhex("   far_size now", (long)far_size());
    }

    /* 2. the driver */
    snd_init();
    sayhex("2. snd_init done; snd_enabled", (long)snd_enabled());
    /* NO snd_region HERE: it is defined in c128/src/sid.c, the 6502 driver,
       and this machine's falconsnd.c does not have one. PAL/NTSC is a SID
       clock question and the YM2149 does not care. Referencing it was a link
       error, which is the honest answer. */

    /* 3. the clock -- a frozen $4BA is a driver that can never tick */
    t0 = now_200();
    for (i = 0; i < 400000; i++) { }
    t1 = now_200();
    sayhex("3. _hz_200 before", t0);
    sayhex("   _hz_200 after ", t1);
    if (t1 == t0) say("   THE 200Hz COUNTER DID NOT MOVE. The tick is dead.");

    /* 4/5. start the title tune and poll it the way kb_waitkey does */
    snd_music_data(mb, (unsigned char)(mb != FAR_NONE));
    snd_music(MUS_TITLE);
    /* POLL FOR A FIXED TIME, NOT A FIXED COUNT. The first version ran
       200,000 iterations and each one takes a Supexec trap; on an 8MHz 68000
       that is minutes, and the probe looked hung when it was only slow. Three
       seconds of 200Hz ticks is far more than the tune needs to start. */
    t0 = now_200();
    while (now_200() - t0 < 600L) snd_poll();

    say("");
    say("4. the PSG, read back through Giaccess:");
    sayhex("   R0 chan A period lo", (long)(PSG_R(0) & 0xFF));
    sayhex("   R1 chan A period hi", (long)(PSG_R(1) & 0x0F));
    sayhex("   R7 mixer           ", (long)(PSG_R(7) & 0xFF));
    sayhex("   R8 chan A volume   ", (long)(PSG_R(8) & 0x1F));
    say("");
    say("   R7 bit 0 CLEAR = channel A tone on (the bits are active low).");
    say("   R8 zero = silent however good the period is.");
    say("");
    say("Press a key.");
    Cconin();
    snd_off();
    return 0;
}

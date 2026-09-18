/* SN76489 sound for the F256K: two voices out of three, frame-paced off the
 * kernel's own clock.
 *
 * THE CHIP IS THE PSG, NOT THE SID, and that was a decision with a reason.
 * The F256K has SID sockets, but they are SOCKETS -- the only audio on the
 * board described as optional. The two PSGs are inside the Beatrix FPGA, so
 * every owner has them. Jamie's words: "we have to have sound, and neither of
 * us owns the machine."
 *
 * THERE ARE TWO OF THEM: left at $D600, right at $D610, and $D608 WRITES
 * BOTH. That is what this driver uses, so the game sounds the same whatever
 * the mixer at $D6A1 is set to -- which is left as the machine had it, since
 * changing a user's stereo routing to play a beep is not this program's
 * business.
 *
 * TWO VOICES OUT OF THREE. sid.h says why: voice 1 carries music, voice 2
 * effects, so a hit during the title track does not chop the tune. The
 * original had one PC speaker and could not. The third tone channel and the
 * noise channel are left alone -- there is nothing in the game that wants
 * them, and an unused channel at silent attenuation costs nothing.
 *
 * N FALLS AS PITCH RISES, WHICH IS THE OPPOSITE OF THE PLUS/4. The SN76489
 * divides: f = clock / (32 * N), so N is a period. TED counts up to 1024 and
 * its register RISES with pitch. tedsnd.c warns about getting that backwards
 * -- "a tune that plays its intervals inside out, which sounds wrong without
 * sounding obviously broken" -- and this is the port where the warning
 * applies in the other direction.
 *
 *     f = 3,579,545 / (32 * N) = 111,860.78 / N
 *
 * and the music format carries pitch in TENS of Hz, so N = 11186 / tenths --
 * the same numerator tedsnd.c already carries, arrived at from a different
 * chip. MEASURED in the scoping tone probe at 440.8, 998.4 and 200.2 Hz
 * against 440, 1000 and 200 wanted: within 0.1%, against a semitone of 5.9%.
 *
 * $D608 IS THE PSG ONLY ON I/O PAGE 0. The first tone probe of this port was
 * silent for a whole run because of it. Every write here sets the page.
 */
#include <stdint.h>
#include "../../c128/src/sid.h"
#include "../../c128/src/sidfreq.h"
#include "../../c128/src/music_data.h"
#include "../../core/farmem.h"
#include "f256vid.h"
#include "f256evt.h"

#define PSG (*(volatile unsigned char *)0xD608)   /* both chips at once */

#define PSG_DIV 11186U          /* 111860.78 / 10, for pitch in tens of Hz */
#define ATTEN_ON   0            /* 0 is LOUDEST on this chip */
#define ATTEN_OFF  15           /* and 15 is silence, not zero */

#define BEEP_TENTHS 44          /* 440Hz, the A every other port beeps */
#define BEEP_FRAMES 15          /* 250ms at 60Hz */

/* THE REGION IS NOT DETECTED HERE, AND THAT IS NOT AN OMISSION.
 *
 * On the Plus/4 and the IIgs the region is a fact about the MACHINE and had to
 * be measured off the hardware. Here it is a consequence of THIS PROGRAM'S OWN
 * CHOICE: f256vid.c sets $D001 for 480 lines, which is 60Hz. The 400-line mode
 * would be 70Hz and we do not use it.
 *
 * So the frame rate is 60.00 exactly -- measured twice, in a probe and again
 * through wait_vsync in the built driver, both 300 frames in 5.000 emulated
 * seconds. sidfreq.h's NTSC numerator assumes 59.826 and is therefore 0.3%
 * fast here: about a seventh of a second across the whole 45-second title
 * track, against the 0.06% that header calls inaudible. Recorded rather than
 * corrected, because a per-port numerator is a change to a shared header for
 * something nobody can hear. */
uint8_t snd_region = REGION_NTSC;

static uint8_t enabled = 1;
static unsigned int acc;
static unsigned char last_frame;

/* TWO COUNTERS, KEPT, because tools/tempo_f256.py is the only thing in this
   project that can see TEMPO. #42 on the list is a session where every burst
   frequency measured correct while the tune ran at double speed: a pitch tool
   is silent about rate. 60 frames and 18.2065 ticks a second is the answer. */
__attribute__((used, retain)) unsigned int snd_frames;
__attribute__((used, retain)) unsigned int snd_ticks;

static unsigned int mus, mus_head, sfx;
static unsigned char mus_on, mus_ok, note_left, sfx_on, sfx_left;
static unsigned char mus_buf[MUS_BYTES];

/* THE PAGE IS PART OF THE ADDRESS. Set without sei on purpose: snd_poll is
   called from the key wait and from the file wait, never from inside a video
   batch and never from an interrupt, so there is no batch to interrupt. The
   video driver always leaves the page at 0, so this is usually a no-op -- but
   "usually" is not a thing to build a sound driver on. */
static void psg(unsigned char b)
{
    F256_IO = F256_IO_REGS;
    PSG = b;
}

/* Registers are 0 ch0-tone, 1 ch0-vol, 2 ch1-tone, 3 ch1-vol, ... so a
   voice's two registers are (ch << 1) and (ch << 1) | 1. */
static void voice_off(unsigned char ch)
{
    psg((unsigned char)(0x80 | (((ch << 1) | 1) << 4) | ATTEN_OFF));
}

static void voice_note(unsigned char ch, unsigned char tenths)
{
    unsigned int n;

    if (tenths == 0) { voice_off(ch); return; }          /* a rest */

    /* Rounded, not truncated: one add, and free of the systematic flatness
       truncation gives across a whole tune. */
    n = (unsigned int)((PSG_DIV + (tenths >> 1)) / tenths);
    /* TEN BITS, and both ends matter. 0 is not silence on this chip -- the
       SN76489 treats a period of 0 as 1024, the LOWEST note it has, so a
       clamp to 0 would turn the top of the range into a rumble. */
    if (n > 1023u) n = 1023u;
    if (n == 0) n = 1;

    psg((unsigned char)(0x80 | ((ch << 1) << 4) | (n & 0x0F)));  /* latch + low 4 */
    psg((unsigned char)((n >> 4) & 0x3F));                       /* high 6 */
    psg((unsigned char)(0x80 | (((ch << 1) | 1) << 4) | ATTEN_ON));
}

void snd_init(void)
{
    unsigned char ch;
    /* Silence everything the chip has, including the two channels this port
       never uses: MCP or a previous program may have left them sounding, and
       "we do not touch it" is not the same as "it is quiet". */
    for (ch = 0; ch < 4; ch++)
        psg((unsigned char)(0x80 | (((ch << 1) | 1) << 4) | ATTEN_OFF));
    acc = 0;
    last_frame = f256_frames();
}

void snd_off(void)
{
    voice_off(0);
    voice_off(1);
    mus_on = 0;
    sfx_on = 0;
}

uint8_t snd_enabled(void) { return enabled; }

void snd_toggle(void)
{
    enabled = (uint8_t)!enabled;
    if (!enabled) snd_off();
}

/* Lifted out of far memory ONCE. Here far_read is a bank switch and a copy,
   not a drive, so this is cheap -- but it is still done once rather than two
   bytes a note, because farmem.h's "read in chunks, not bytes" rule is about
   exactly this and the shared driver shape assumes it. */
void snd_music_data(unsigned int base, unsigned char ok)
{
    unsigned int off;
    unsigned char n;

    mus_ok = 0;
    if (!ok) return;
    for (off = 0; off < MUS_BYTES; off = (unsigned int)(off + n)) {
        n = (unsigned char)((MUS_BYTES - off) > 128 ? 128 : (MUS_BYTES - off));
        far_read((unsigned int)(base + off), &mus_buf[off], n);
    }
    mus_ok = 1;
}

void snd_music(uint8_t track)
{
    if (!enabled || !mus_ok || track == MUS_NONE || track >= MUS_COUNT) {
        mus_on = 0;
        voice_off(0);
        return;
    }
    mus_head = mus_offset[track];
    mus = mus_head;
    mus_on = 1;
    note_left = 0;
    acc = 0;
}

void snd_effect(uint8_t track)
{
    if (!enabled || !mus_ok || track >= MUS_COUNT) return;
    sfx = mus_offset[track];
    sfx_on = 1;
    sfx_left = 0;
}

/* THE REFUSAL BEEP, blocking as it is in the original, and BOUNDED -- an
   unbounded wait on a frame source that has died is a machine that hangs with
   a voice sounding, which the X16 shipped once. */
void snd_beep(void)
{
    unsigned char frames = 0;
    unsigned int guard = 0;
    unsigned char last, now;

    if (!enabled) return;
    sfx_on = 0;
    voice_note(1, BEEP_TENTHS);
    last = f256_frames();
    while (frames < BEEP_FRAMES) {
        now = f256_frames();
        if (now != last) { frames = (unsigned char)(frames + (unsigned char)(now - last));
                           last = now; guard = 0; }
        else if (++guard == 0) break;
    }
    voice_off(1);
}

static void music_tick(void)
{
    if (mus_on) {
        if (note_left) note_left--;
        if (!note_left) {
            if (mus_buf[mus] == 0) {          /* a zero duration ends a track */
                mus = mus_head;               /* and it loops until stopped   */
                if (mus_buf[mus] == 0) { mus_on = 0; voice_off(0); return; }
            }
            note_left = mus_buf[mus];
            voice_note(0, mus_buf[mus + 1]);
            mus = (unsigned int)(mus + 2);
        }
    }
    if (sfx_on) {
        if (sfx_left) sfx_left--;
        if (!sfx_left) {
            if (mus_buf[sfx] == 0) { sfx_on = 0; voice_off(1); return; }
            sfx_left = mus_buf[sfx];
            voice_note(1, mus_buf[sfx + 1]);
            sfx = (unsigned int)(sfx + 2);
        }
    }
}

void snd_poll(void)
{
    unsigned char now, elapsed;

    if (!enabled || (!mus_on && !sfx_on)) return;

    /* THE FRAME SOURCE IS A KERNEL CALL, NOT A RASTER READ, and on this
       machine that is not a preference. MAME's f256k returns the horizontal
       dot position from the scan-line registers, so a raster-paced tune here
       would run at whatever aliasing produced. SetTimer with the QUERY bit
       returns the kernel's frame counter: 60.00 Hz, measured.

       COUNTED BY DIFFERENCE, NOT BY EQUALITY. The counter is a byte that
       wraps every 256 frames, and `if (now != last) one_frame()` loses every
       frame that passes while the program is busy elsewhere -- which on the
       end-of-game screens is a whole disk load. Taking the difference means a
       tune that was not polled for ten frames advances ten. */
    now = f256_frames();
    elapsed = (unsigned char)(now - last_frame);
    if (!elapsed) return;
    last_frame = now;

    while (elapsed--) {
        snd_frames++;
        acc = (unsigned int)(acc + snd_tick_num(snd_region));
        while (acc >= SND_TICK_DEN) {
            acc = (unsigned int)(acc - SND_TICK_DEN);
            snd_ticks++;
            music_tick();
        }
    }
}

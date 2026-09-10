/* THE REFUSAL BEEP ON THE X16, MEASURED RATHER THAN READ.
 *
 * `x16/src/x16snd.c`'s snd_beep() plays `voice_note(V_SFX, 20)` for six frame
 * ticks. Read as written that is 200Hz for about 100ms, where the original was
 * MEASURED at 440Hz for 250ms and three other ports implement that. But the
 * X16's clocks have already been wrong twice on this project -- RDTIM returns
 * zero forever and VERA's VSYNC flag never sets -- so "six frame ticks" is a
 * claim about a clock, not a duration, and the pitch is a claim about what
 * VERA does with a frequency word rather than about arithmetic.
 *
 * x16emu can record a WAV of its audio output, which no other emulator on this
 * project offers. So this measures the SOUND, not the registers: it calls the
 * shipping driver three times with silence between, and tools/sndtest.py reads
 * the pitch and the duration of each burst out of the recording.
 *
 * Three, not one: a duration is a number that can come out wrong, and a single
 * burst cannot show whether it is stable.
 *
 * NEVER PART OF THE GAME BUILD -- see the sndtest target in the Makefile.
 */
#include <stdint.h>

#include "../../c128/src/sid.h"

void chrout(char c);
__asm__(".global chrout\nchrout:\n jsr $FFD2\n rts\n");
static void say(const char *s) { while (*s) chrout(*s++); }

/* A GAP THE ANALYSIS CAN SEE. Not a frame count on purpose: the frame tick is
   part of what is under test here, and a test must not be paced by the clock
   it is measuring. A plain spin is unitless and that is fine -- the WAV's own
   sample rate is the ruler, and all this has to do is separate the bursts. */
static volatile uint16_t spin;
static void gap(void) {
    uint8_t n;
    for (n = 0; n < 4; n++) { spin = 0; do { } while (++spin); }
}

void snd_test_note(unsigned char tenths);
void snd_test_off(void);

int main(void) {
    uint8_t i;

    say("x16 sndtest: reference, then three beeps.\r");
    snd_init();

    /* THE REFERENCE COMES FIRST, and it is the point of the run.
       44 tenths is the A the original was measured at and three other ports
       play. It goes through the same voice_note() the beep does, so if this
       burst does not read 440Hz the analysis is wrong and nothing after it
       means anything. */
    gap();
    snd_test_note(44);          /* the driver believes this is 440Hz */
    gap();
    snd_test_off();
    gap();
    snd_test_note(88);          /* the driver believes this is 880Hz */
    gap();
    snd_test_off();

    /* Lead-in. x16emu's -wav ,auto starts recording on the first non-zero
       sample, so what it captures begins at the first beep either way. */
    gap();

    for (i = 0; i < 3; i++) {
        snd_beep();
        gap();
    }

    snd_off();
    gap();
    say("done.\r");
    return 0;
}

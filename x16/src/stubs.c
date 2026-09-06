/* Stubs so the WHOLE GAME can be linked before any of these seams is written.
 *
 * The point is one number: does EGA Trek fit the X16's code space? The scope
 * for this port quoted 38,655 bytes from cx16/lib/link.ld, and the Atari has
 * since taught this project that a linker region is NOMINAL, not available --
 * VBXE's window silently owned 8K of the address space there. So link it now
 * and read the real figure, rather than discovering it after the overlays are
 * cut. Nothing here does anything; they exist to satisfy the linker. */
#include <stdint.h>

/* OPAQUE ON PURPOSE. The first version of this file returned constants, and
   -Oz with LTO folded the entire game down to 1,564 bytes: kb_waitkey()
   returning 0 kills the command loop, plat_read_all() failing kills every
   load path, far_read() doing nothing empties the string pool. That is an
   INSTRUMENT ARTEFACT, not a measurement. Routing every return through a
   volatile makes the optimiser keep the code that consumes it. */
volatile unsigned char opaque = 0;
volatile unsigned int  opaque16 = 0;


void snd_init(void) {}
void snd_off(void) {}
void snd_music_data(unsigned int base, unsigned char ok) { (void)base; (void)ok; }
void snd_music(uint8_t t) { (void)t; }
void snd_effect(uint8_t t) { (void)t; }
void snd_beep(void) {}
void snd_poll(void) {}
uint8_t snd_enabled(void) { return opaque; }
void snd_toggle(void) {}





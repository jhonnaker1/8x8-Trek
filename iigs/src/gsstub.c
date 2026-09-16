/* THE SEAM, STUBBED, SO THE IMAGE CAN BE MEASURED BEFORE IT CAN BE PLAYED.
 *
 * This is the Plus/4's `make early` and it answers one question: how much of
 * the address space does the game's PORTABLE half occupy on this machine?
 * Twenty symbols are all that stand between the shared sources and a link --
 * video, keyboard, sound, storage, far memory and overlays -- and they are
 * listed here in the order the linker reported them missing, which is the
 * honest definition of the seam.
 *
 * WHAT THIS NUMBER IS AND IS NOT. It is a FLOOR, and a soft one. Every stub
 * here costs a handful of bytes where a driver will cost hundreds, and the
 * measured lesson from the Falcon port is worse than that: the video seam
 * cost 4,636 bytes for a 1,559-byte driver, because THE CALLERS GROW when the
 * thing they call becomes real. So read the figure as "not less than", never
 * as "about".
 *
 * `volatile` sinks, because a stub that does nothing is free to be deleted
 * along with everything that feeds it -- and then the measurement is of a
 * program the optimiser threw away.
 */
#include <stdint.h>

#include "../../core/storage.h"

volatile unsigned char gs_sink;
volatile unsigned int  gs_sink16;

/* ---- video ----
   GS_REAL_VIDEO links src/gsvid.c instead of these, which is what turns the
   size measurement from a floor into a figure for one real seam. The Falcon
   measured video at 4,636 bytes for a 1,559-byte driver because THE CALLERS
   GROW; this is the same subtraction done on this machine. */
#ifndef GS_REAL_VIDEO
void vdc_init(void) { gs_sink = 1; }
void scr_clear(void) { gs_sink = 2; }
void scr_put(unsigned char x, unsigned char y, unsigned char ch,
             unsigned char color)
{ gs_sink = (unsigned char)(x + y + ch + color); }
void scr_puts(unsigned char x, unsigned char y, const char *s,
              unsigned char color)
{ gs_sink = (unsigned char)(x + y + color + (unsigned char)*s); }
void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color)
{ gs_sink = (unsigned char)(x + y + w + ch + color); }
void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color)
{ gs_sink = (unsigned char)(x + y + h + ch + color); }
void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w,
                   unsigned char h, unsigned char ch, unsigned char color)
{ gs_sink = (unsigned char)(x + y + w + h + ch + color); }
void vdc_shutdown(void) { gs_sink = 7; }
void wait_vsync(void) { gs_sink = 8; }
void plat_exit(void) { for (;;) ; }

#endif  /* GS_REAL_VIDEO */

/* ---- the message log and far memory ----
   GS_REAL_FAR links src/gsfar.c instead, which puts BOTH in bank $01 and
   takes the 2,048-byte array below out of bank 0 entirely. */
#ifndef GS_REAL_FAR

/* ---- the message log, and this one is NOT a stub ----
   ui.c keeps a 32-entry log outside the program through three functions whose
   names are the C128's, because that is where the seam was first cut. Every
   port implements them against whatever spare memory it has; a IIgs has no
   VDC and, like the C64, has the room, so this is 2K of plain array. It is
   real here rather than stubbed precisely because it COSTS 2,048 bytes and
   the point of this build is the size. */
static unsigned char gs_log[32 * 64];
static unsigned int  gs_logp;
void vdc_set_address(unsigned int addr) { gs_logp = addr - 0x1000; }
void vdc_data_write(unsigned char value)
{ if (gs_logp < sizeof gs_log) gs_log[gs_logp++] = value; }
unsigned char vdc_data_read(void)
{ return (gs_logp < sizeof gs_log) ? gs_log[gs_logp++] : 0; }

#endif  /* GS_REAL_FAR -- the log half */

/* ---- keyboard ----
   GS_REAL_KEY links src/gskey.c instead. */
#ifndef GS_REAL_KEY
uint16_t kb_entropy;
volatile unsigned char kb_inject;
void kb_init(void) { gs_sink = 3; }
char kb_waitkey(void) { return (char)gs_sink; }

#endif  /* GS_REAL_KEY */

/* ---- sound ---- */
uint8_t snd_region;
void snd_init(void) { gs_sink = 4; }
void snd_music_data(unsigned int base, unsigned char ok)
{ gs_sink16 = base; gs_sink = ok; }
void snd_music(uint8_t track) { gs_sink = track; }
void snd_toggle(void) { gs_sink = 5; }
void snd_off(void) { gs_sink = 9; }
void snd_effect(uint8_t track) { gs_sink = track; }
void snd_beep(void) { gs_sink = 10; }
void snd_poll(void) { gs_sink = 11; }
uint8_t snd_enabled(void) { return gs_sink; }

/* ---- far memory (the string pool) ---- */
#ifndef GS_REAL_FAR
uint16_t far_load(const char *name) { gs_sink = (unsigned char)*name; return 0; }
void far_read(uint16_t off, void *dst, uint8_t len)
{ gs_sink16 = off; gs_sink = len; *(volatile unsigned char *)dst = 0; }

#endif  /* GS_REAL_FAR */

/* ---- overlays ----
   GS_REAL_OVL links src/gsovl.c instead. */
#ifndef GS_REAL_OVL
void ovl_load(uint8_t which) { gs_sink = which; }
#endif

/* ---- storage ----
   GS_REAL_STORAGE links src/gsblk.c instead of these. */
#ifndef GS_REAL_STORAGE
uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got)
{ gs_sink = (unsigned char)*name; gs_sink16 = max;
  *(volatile unsigned char *)buf = 0; *got = 0; return STOR_OK; }
uint8_t plat_write_all(const char *name, const void *buf, uint16_t len)
{ gs_sink = (unsigned char)*name; gs_sink16 = len;
  gs_sink = *(const unsigned char *)buf; return STOR_OK; }
uint8_t plat_open(const char *name) { gs_sink = (unsigned char)*name; return STOR_OK; }
uint16_t plat_read(void *buf, uint16_t len)
{ *(volatile unsigned char *)buf = 0; return len ? 0 : 0; }
void plat_close(void) { gs_sink = 6; }

#endif  /* GS_REAL_STORAGE */

/* The seams this port has NOT written yet, stubbed against the real headers
   so a signature cannot drift. Video is real now (msx2vid.c); these go one
   by one as each driver lands, and `make probe` reports what is left. */
#include <stdint.h>
#include "input.h"
#include "sid.h"
#include "storage.h"
#include "farmem.h"
#include "overlay.h"

uint16_t kb_entropy;
void kb_init(void) {}
char kb_waitkey(void) { return 0; }

void snd_init(void) {}
void snd_off(void) {}
void snd_beep(void) {}
void snd_toggle(void) {}
uint8_t snd_enabled(void) { return 0; }
void snd_music(uint8_t t) { (void)t; }
void snd_effect(uint8_t t) { (void)t; }
void snd_music_data(unsigned int b, unsigned char ok) { (void)b; (void)ok; }

uint8_t plat_open(const char *n) { (void)n; return 0; }
uint16_t plat_read(void *b, uint16_t l) { (void)b; (void)l; return 0; }
void plat_close(void) {}
uint8_t plat_read_all(const char *n, void *b, uint16_t m, uint16_t *g) { (void)n; (void)b; (void)m; (void)g; return 0; }
uint8_t plat_write_all(const char *n, const void *b, uint16_t l) { (void)n; (void)b; (void)l; return 0; }

uint16_t far_load(const char *n) { (void)n; return 0; }
void far_read(uint16_t o, void *d, uint8_t l) { (void)o; (void)d; (void)l; }
void ovl_load(uint8_t w) { (void)w; }

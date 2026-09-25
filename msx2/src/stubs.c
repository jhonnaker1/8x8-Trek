/* The seams this port has NOT written yet, stubbed against the real headers
   so a signature cannot drift. Video (msx2vid.c), sound (msx2snd.c) and the
   keyboard (msx2input.c) are real now; these go one by one as each driver
   lands, and `make probe` reports what is left. */
#include <stdint.h>
#include "storage.h"
#include "farmem.h"
#include "overlay.h"

uint8_t plat_open(const char *n) { (void)n; return 0; }
uint16_t plat_read(void *b, uint16_t l) { (void)b; (void)l; return 0; }
void plat_close(void) {}
uint8_t plat_read_all(const char *n, void *b, uint16_t m, uint16_t *g) { (void)n; (void)b; (void)m; (void)g; return 0; }
uint8_t plat_write_all(const char *n, const void *b, uint16_t l) { (void)n; (void)b; (void)l; return 0; }

uint16_t far_load(const char *n) { (void)n; return 0; }
void far_read(uint16_t o, void *d, uint8_t l) { (void)o; (void)d; (void)l; }
void ovl_load(uint8_t w) { (void)w; }

#ifndef ATARIMEM_H
#define ATARIMEM_H
#include <stdint.h>

/* Local to this port, both wider than core/farmem.h's byte-sized far_read.
   far_bulk moves an overlay image, 4,608 bytes at a time; far_write is what
   far_load pushes through the window, and what a test uses to plant a pattern
   in the store without needing the storage seam to exist yet. */
void far_bulk(uint16_t off, void *dst, uint16_t len);
void far_write(uint16_t off, const void *src, uint16_t len);

#endif

#ifndef X16MEM_H
#define X16MEM_H
#include <stdint.h>
/* Bulk read out of far memory, wider than core/farmem.h's far_read (uint8_t).
   Local to the X16 port -- the overlay loader moves 4K at a time. */
void far_bulk(uint16_t off, void *dst, uint16_t len);
#endif

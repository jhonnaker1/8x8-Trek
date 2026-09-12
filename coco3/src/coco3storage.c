/* Files for the CoCo 3 -- STUB. Disk BASIC's file routines through the
 * cartridge FDC, or OS-9. This is the seam the scope called NEW and it is the
 * one with the least precedent here: every other 6502 port talks to a KERNAL
 * or a DOS that answers on a device number.
 */
#include <stdint.h>

#include "../../core/storage.h"

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got)
{ (void)name; (void)buf; (void)max; if (got) *got = 0; return STOR_NOTFOUND; }

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len)
{ (void)name; (void)buf; (void)len; return STOR_ERROR; }

uint8_t plat_open(const char *name) { (void)name; return STOR_NOTFOUND; }
uint16_t plat_read(void *buf, uint16_t len) { (void)buf; (void)len; return 0; }
void plat_close(void) { }

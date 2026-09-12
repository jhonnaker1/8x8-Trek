/* Far memory for the CoCo 3 -- STUB, and deliberately NOT a 32K array.
 *
 * THE PLAN IS THE CARD'S VRAM. A SuperSprite FM+ carries 128K of video RAM
 * behind the V9958's I/O ports, which is exactly what the Atari does with
 * VBXE's VRAM: bulk data lives somewhere the CPU cannot address directly and
 * arrives a byte at a time. STRINGS.DAT is about 7.3K and MUSIC.DAT 0.4K, so
 * the whole store fits many times over in memory this machine otherwise
 * cannot use at all.
 *
 * A plain array like the Amiga's and the Falcon's would be 32K on a machine
 * with 64K of address space, which would make the `make early` number a lie.
 * So this stub allocates NOTHING and returns failure, and the size it reports
 * is the size of the code alone.
 */
#include <stdint.h>

#include "../../core/farmem.h"

uint16_t far_load(const char *name) { (void)name; return FAR_NONE; }

void far_read(uint16_t off, void *dst, uint8_t len)
{
    unsigned char *d = (unsigned char *)dst;
    (void)off;
    while (len--) *d++ = 0;
}

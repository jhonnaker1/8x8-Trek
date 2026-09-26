/* Far memory for the MSX2: VRAM PAGE 1.
 *
 * SCREEN 7 displays page 0 of the V9938's 128K and never page 1, so 64K sits
 * unused behind the same port the video driver writes. The two tenants,
 * STRINGS.DAT (7,492) and MUSIC.DAT (412), take an eighth of it.
 *
 * NOT THE MEMORY MAPPER. The game's code runs $0100..$D5F5, across all four
 * pages, so mapping a RAM segment in for a string would unmap code; VRAM
 * needs only an address set, from anywhere. The mapper is kept for the job
 * VRAM cannot do -- paging CODE -- if the budget ever needs it.
 *
 * far_read is called from the sound driver's tick as well as from the string
 * pool; vram_far_at() invalidates the message log's cached VDP address, so
 * neither can move the pointer under a log run. */
#include <stdint.h>

#include "farmem.h"
#include "storage.h"
#include "msx2.h"

__sfr __at 0x98 VDP_DATA;

static uint16_t far_len;

uint16_t far_load(const char *name)
{
    unsigned char chunk[16], i, n;
    uint16_t base = far_len;

    if (plat_open(name) != STOR_OK)
        return FAR_NONE;
    /* The address is set again for every chunk: plat_read may call DOS,
       and nothing here promises DOS leaves the VDP pointer alone. */
    while ((n = (unsigned char)plat_read(chunk, sizeof chunk)) != 0) {
        vram_far_at(far_len, 0x40);
        for (i = 0; i < n; i++)
            VDP_DATA = chunk[i];
        far_len += n;
    }
    plat_close();
    return base;
}

uint16_t far_size(void) { return far_len; }

void far_read(uint16_t off, void *dst, uint8_t len)
{
    unsigned char *d = (unsigned char *)dst;

    vram_far_at(off, 0x00);
    while (len--)
        *d++ = VDP_DATA;
}

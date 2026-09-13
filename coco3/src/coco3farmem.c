/* Far memory for the CoCo 3: the SuperSprite's own video RAM.
 *
 * THIS WAS A STUB FOR A WEEK AND THE GAME RAN WORDLESS BECAUSE OF IT.
 * far_load() returned FAR_NONE, so str_load() failed, so every S() returned
 * an empty string -- which core/strpool.h promises will happen and which the
 * game survives by design. The symptom was a title screen that drew
 * perfectly, a keypress, and then a cleared screen with a single cursor block
 * on it and no prose anywhere. Nothing byte-level caught it: every check in
 * this port asked about bytes it had chosen in advance. The first PICTURE
 * ever taken of the port found it (see tools/vramshot.py).
 *
 * THE PLAN THE STUB DESCRIBED IS THE ONE BUILT. A SuperSprite FM+ carries
 * 128K of video RAM behind the V9958's ports, and this port's display uses
 * 54,272 bytes of it plus 2K of message log at $E000 -- all inside the first
 * 64K. The SECOND 64K is untouched, unreachable by the 6809 any other way,
 * and exactly the shape core/farmem.h asks for: bulk read-only data that
 * arrives a byte at a time. The Atari reached the same answer through VBXE's
 * VRAM for the same reason.
 *
 * STREAMED, NOT SLURPED. plat_open/plat_read walk the file a sector at a time
 * so nothing needs a 7.3K buffer on a machine with 64K of address space --
 * and it exercises the streaming path, which the loader work never did: every
 * disk measurement so far went through plat_read_all.
 *
 * OFFSETS ARE uint16_t, so the store tops out at 64K, which is exactly the
 * bank. STRINGS.DAT is about 7.3K and MUSIC.DAT 0.4K.
 */
#include <stdint.h>

#include "../../core/farmem.h"
#include "../../core/storage.h"
#include "coco3vdp.h"

/* Bytes handed out so far. far_load APPENDS -- core/farmem.h is explicit that
   the store has more than one tenant, and that an earlier version loading at
   offset 0 every time had the music quietly overwrite the prose. */
static uint16_t far_used;

uint16_t far_load(const char *name)
{
    uint16_t base = far_used;
    unsigned char buf[64];
    uint16_t n;

    if (plat_open(name) != STOR_OK) return FAR_NONE;

    for (;;) {
        n = plat_read(buf, (uint16_t)sizeof(buf));
        if (n == 0) break;
        /* Re-address every chunk. The counter auto-increments across the
           chunk, but the screen or the log may have moved it in between --
           they share this chip. */
        vdp_far_write_at(far_used);
        {   uint16_t i;
            for (i = 0; i < n; i++) VDP_PORT = buf[i];
        }
        far_used = (uint16_t)(far_used + n);
    }
    plat_close();

    if (far_used == base) return FAR_NONE;      /* empty, or never read */
    return base;
}

uint16_t far_size(void)
{
    return far_used;
}

void far_read(uint16_t off, void *dst, uint8_t len)
{
    unsigned char *d = (unsigned char *)dst;

    vdp_far_read_at(off);
    while (len--) *d++ = VDP_PORT;
}

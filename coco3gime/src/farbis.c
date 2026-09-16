/* THE PATH THE GAME ACTUALLY TAKES, which is not the one the last probe took.
 *
 * sndbisect.c read MUSIC.DAT with plat_read_all and it worked, with interrupts
 * enabled and every one of snd_init's register writes applied. The game still
 * hangs on that file's first sector -- because snd_music_data does not call
 * plat_read_all. It calls far_read, and this port's far memory is the DISK
 * (gimemem.c): plat_raw_open and plat_raw_sector, a different code path with
 * its own sector cache and its own tenant table.
 *
 * So this does exactly what main() does, in main()'s order:
 *
 *     far_load("STRINGS.DAT")     tenant 0
 *     a few far_reads from it     the pool, as strpool.c would
 *     far_load("MUSIC.DAT")       tenant 1
 *     far_read the whole of it    as snd_music_data does, 128 at a time
 */
#include "../../core/storage.h"
#include "../../core/farmem.h"

#define r    ((unsigned char *)0x5F00)
#define DONE 0x5A

static unsigned char mus[512];

int main(void)
{
    unsigned int i, sb, mb, off;
    unsigned char n;

    for (i = 0; i < 32; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$5E00 }
    asm { sta $FFDF }
    *((unsigned char *)0xFF90) = 0x2C;

    sb = far_load("STRINGS.DAT");
    r[1] = (unsigned char)(sb >> 8);
    r[2] = (unsigned char)(sb & 0xFF);

    /* Read a bit of the pool, the way strpool.c does -- two bytes of offset
       and then a slot's worth -- so the sector cache is in the state the
       music read will find it in. */
    far_read(sb, mus, 2);        r[3] = mus[0];
    far_read((unsigned int)(sb + 600), mus, 32);  r[4] = 0xC1;

    mb = far_load("MUSIC.DAT");
    r[5] = (unsigned char)(mb >> 8);
    r[6] = (unsigned char)(mb & 0xFF);
    r[7] = 0xC2;                 /* far_load returned */

    for (off = 0; off < 412; off = (unsigned int)(off + n)) {
        n = (unsigned char)((412 - off) > 128 ? 128 : (412 - off));
        far_read((unsigned int)(mb + off), &mus[0], n);
        r[8] = (unsigned char)(off >> 8);
        r[9] = (unsigned char)(off & 0xFF);
    }
    r[10] = 0xC3;                /* the whole file came back */
    r[11] = mus[0];

    r[31] = DONE;
    for (;;) ;
}

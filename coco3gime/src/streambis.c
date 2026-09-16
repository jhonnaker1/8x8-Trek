/* DOES A POOL READ CORRUPT AN OPEN STREAM? The briefing did exactly this.
 *
 * ui_briefing streams BRIEF.TXT with plat_open/plat_read and, at every page
 * break, draws its footer with S(S_306). On this port S() is a DISK read --
 * far memory is the drive -- and coco3storage kept the open stream's current
 * sector in the same `secbuf` that raw sector reads landed in. So the footer
 * fetched a STRINGS.DAT sector over the briefing's buffered page, and the
 * stream resumed serving pool text: Jamie saw the word KILLED, string 301, at
 * the top of briefing page 2 with that page's own header missing.
 *
 * This reads the whole file in 64-byte chunks and does a far_read BETWEEN
 * EVERY CHUNK, which is the pattern the pager produces, and checksums what
 * came back. The host knows what the file's checksum is; anything else means
 * the stream was disturbed.
 */
#include "../../core/storage.h"
#include "../../core/farmem.h"

#define r    ((unsigned char *)0x5F00)
#define DONE 0x5A

static unsigned char buf[64];
static unsigned char pool[8];

int main(void)
{
    unsigned int i, total = 0, sum = 0, sb;
    unsigned char x = 0, n;

    for (i = 0; i < 32; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$5E00 }
    asm { sta $FFDF }
    *((unsigned char *)0xFF90) = 0x2C;

    sb = far_load("STRINGS.DAT");
    r[1] = (sb == 0xFFFF) ? 1 : 0;

    r[2] = plat_open("BRIEF.TXT");
    if (r[2] != STOR_OK) { r[31] = DONE; for (;;) ; }

    for (;;) {
        n = (unsigned char)plat_read(buf, sizeof buf);
        if (n == 0) break;
        for (i = 0; i < n; i++) {
            sum = (unsigned int)(sum + buf[i]);
            x ^= buf[i];
        }
        total = (unsigned int)(total + n);
        /* THE FOOTER'S READ, between chunks -- two bytes of the index and a
           few of the text, which is what S() does. */
        far_read(sb, pool, 2);
        far_read((unsigned int)(sb + 700), pool, 8);
    }
    plat_close();

    r[3] = (unsigned char)(total >> 8);
    r[4] = (unsigned char)(total & 0xFF);
    r[5] = (unsigned char)(sum >> 8);
    r[6] = (unsigned char)(sum & 0xFF);
    r[7] = x;
    r[31] = DONE;
    for (;;) ;
}

/* IS LOW RAM ACTUALLY FREE WHILE DOS IS RESIDENT?
 *
 * The port wants somewhere to put its biggest buffer that is not competing
 * with code, the way the C128's lowram at $1300..$1C00 does not. The Atari's
 * candidates are the OS "spare" areas below where DOS loads -- $0480..$06FF,
 * which every Atari memory map calls free -- and $1CFC..$1FFF, which is free
 * only because THIS DOS put MEMLO there.
 *
 * A MEMORY MAP IS NOT EVIDENCE ABOUT A RUNNING MACHINE. What matters is
 * whether the region survives the things this program actually does, and the
 * dangerous one is disk I/O: CIO and SIO are the OS and DOS code most likely
 * to want scratch space. So this fills the region, does real file work through
 * the shipping storage seam, and counts what came back changed.
 */
#include <stdint.h>

#include "vbxevid.h"
#include "../../core/storage.h"

#define LO   0x0480
#define HI   0x0700          /* exclusive */
#define MEMLO (*(volatile unsigned int *)0x02E7)

static unsigned char buf[128];
static unsigned char row = 2;

static unsigned char want(unsigned int a) {
    return (unsigned char)((a ^ 0x5A) & 0xFF);
}

static void num(unsigned char x, unsigned int n) {
    char t[6];
    unsigned char i = 0;
    if (!n) { scr_puts(x, row, "0", 15); return; }
    while (n && i < 5) { t[i++] = (char)('0' + n % 10); n /= 10; }
    while (i--) { char c[2]; c[0] = t[i]; c[1] = 0; scr_puts(x++, row, c, 15); }
}

static unsigned int damaged(void) {
    unsigned int a, bad = 0;
    for (a = LO; a < HI; a++)
        if (*(volatile unsigned char *)a != want(a)) bad++;
    return bad;
}

static void fill(void) {
    unsigned int a;
    for (a = LO; a < HI; a++) *(volatile unsigned char *)a = want(a);
}

static void report(const char *tag, unsigned int bad) {
    scr_puts(2, row, tag, bad ? 12 : 10);
    num(40, bad);
    scr_puts(48, row, bad ? "TOUCHED" : "INTACT", bad ? 12 : 10);
    row++;
}

int main(void) {
    uint16_t got;
    unsigned char i;

    vdc_init();
    scr_puts(2, 0, "LOW RAM $0480..$06FF WHILE DOS IS RESIDENT", 15);
    scr_puts(40, 1, "BYTES CHANGED", 7);

    scr_puts(2, row, "MEMLO reads", 14);
    num(40, MEMLO);
    row++;

    fill();
    report("after filling it", damaged());

    /* THE WHOLE-FILE PATH, which is what save and restore use. */
    plat_read_all("STRINGS.DAT", buf, (uint16_t)sizeof buf, &got);
    report("after plat_read_all", damaged());

    /* THE STREAMING PATH, over a long file, which is what far_load uses --
       many CIO calls and many SIO sector reads rather than one. */
    if (plat_open("OVERLAYS.BIN") == STOR_OK) {
        for (i = 0; i < 200; i++)
            if (!plat_read(buf, (uint16_t)sizeof buf)) break;
        plat_close();
    }
    report("after 200 streamed reads", damaged());

    /* AND A WRITE, because DOS allocates sectors and rewrites its own VTOC
       on the way through, which is the busiest it ever gets. */
    plat_write_all("LOWTEST.DAT", buf, (uint16_t)sizeof buf);
    report("after plat_write_all", damaged());

    for (;;) { }
    return 0;
}

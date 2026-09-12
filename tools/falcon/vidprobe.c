/* EGA Trek scope: ask a Falcon's VIDEL what its modes actually are.
   VgetSize(mode) returns the screen size in bytes, which pins
   width x height x planes without trusting any table. */
#include <tos.h>
#include <stdio.h>

static const char *bpsname(int b)
{
    switch (b) {
    case BPS1:  return "2c";
    case BPS2:  return "4c";
    case BPS4:  return "16c";
    case BPS8:  return "256c";
    case BPS16: return "truec";
    }
    return "?";
}

int main(void)
{
    int  cur = VsetMode(-1);
    int  vga, col, bps, vert;
    long sz;

    printf("current mode word = $%04x\r\n", cur & 0xffff);
    printf("mode  vga col80 vert bpp   VgetSize  implied WxH @4bpp\r\n");

    for (vga = 0; vga <= 1; vga++)
      for (vert = 0; vert <= 1; vert++)
        for (col = 0; col <= 1; col++)
          for (bps = BPS1; bps <= BPS8; bps++) {
            int m = (vga ? VGA : 0) | (vert ? VERTFLAG : 0)
                  | (col ? COL80 : 0) | bps;
            sz = VgetSize(m);
            if (sz <= 0) continue;
            printf("$%04x %3d %5d %4d %-5s %8ld", m, vga, col, vert, bpsname(bps), sz);
            /* 4bpp: bytes = w*h/2. Report h for the two plausible widths. */
            if (bps == BPS4)
                printf("   640x%ld or 320x%ld", sz / 320, sz / 160);
            printf("\r\n");
          }

    printf("DONE\r\n");
    return 0;
}

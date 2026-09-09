/* PROBE 5: the STREAMING read, which is the shape far_load actually uses.
 *
 * probe 3 read a whole file in one go with the ROM mapped throughout, and it
 * was byte-perfect. The game does something different: plat_open(), then
 * plat_read() into a 64-byte buffer over and over, with the ROM UNMAPPED
 * between every chunk because the briefing draws to the screen in between.
 * The string pool came back wrong in the game and probe 3 could not have
 * caught it -- so this links the REAL src/m65storage.c and drives it exactly
 * the way far_load does.
 *
 * A 20-second whole-game run per attempt is the wrong instrument for this.
 */
#include <stdint.h>
#include <mega65/conio.h>
#include "../../core/storage.h"
#include "m65vid.h"

extern uint8_t plat_dbg_stage, plat_dbg_status;

static uint8_t stage[64];

static void say(const char *s) { cputs((const unsigned char *)s); }

/* DOES THE C65 KERNAL ACTUALLY CLOBBER $02..$21? The shim in m65kernal.s was
   written on a WRONG diagnosis -- the real fault was an undeclared "p" clobber
   -- so before keeping ~50 bytes of it forever, measure whether the hazard is
   even real on this ROM. hyppo's read512 demonstrably was; that says nothing
   about the KERNAL.

   Fill, ONE call, snapshot, all inside a single asm block so no C runs in
   between and only the ROM can be responsible. */
uint8_t zp_before[32] __attribute__((used)), zp_after[32] __attribute__((used));

static void zp_probe_chrin(void)
{
    __asm__ volatile(
        "ldx #31\n"
        "1:\n\t"
        "txa\n\t"
        "eor #$5a\n\t"
        "sta $02,x\n\t"
        "sta zp_before,x\n\t"
        "dex\n\t"
        "bpl 1b\n\t"
        "jsr $ffcf\n\t"
        "ldx #31\n"
        "2:\n\t"
        "lda $02,x\n\t"
        "sta zp_after,x\n\t"
        "dex\n\t"
        "bpl 2b"
        ::: "a", "x", "y", "memory", "p");
}

int main(void)
{
    unsigned long total = 0;
    unsigned int chunks = 0;
    uint8_t csum = 0, rc;
    uint16_t n, i;

    vdc_init();
    gotoxy(2, 1); say("PROBE 5: STREAMING READ, 64 BYTES AT A TIME");
    gotoxy(2, 3); say("opening STRINGS.DAT ...");

    rc = plat_open("STRINGS.DAT");
    /* AT A FIXED ADDRESS, not on the screen. Reading these off a screenshot
       meant squinting at an EGA-palette glyph set, and I misread the DOS code
       once already. $C800 is free here: this probe has no overlays and its stack is at $D000. */
    { volatile uint8_t *d = (volatile uint8_t *)0xC800;
      d[0] = rc; d[1] = plat_dbg_stage; d[2] = plat_dbg_status;
    }
    gotoxy(2, 3); say("plat_open rc = "); cputhex(rc, 2);
    say("  stage ");  cputhex(plat_dbg_stage, 2);
    say("  dos ");    cputhex(plat_dbg_status, 2);
    say("   (1 cmdopen 2 open 3 status 4 chkin 5 ok)");
    if (rc != STOR_OK) { for (;;) { } }

    /* One CHRIN with a file open, then report which of $02..$21 moved. */
    zp_probe_chrin();
    { volatile uint8_t *d = (volatile uint8_t *)0xC820;
      uint8_t k, bad = 0;
      for (k = 0; k < 32; k++) {
          d[k] = (uint8_t)(zp_before[k] ^ zp_after[k]);
          if (d[k]) bad++;
      }
      d[32] = bad; }

    for (;;) {
        n = plat_read(stage, sizeof stage);
        if (n == 0) break;
        for (i = 0; i < n; i++) csum = (uint8_t)(csum ^ stage[i]);
        total += n;
        chunks++;
        if (chunks > 4000) break;          /* a runaway is a result too */
    }
    plat_close();

    { volatile uint8_t *d = (volatile uint8_t *)0xC800;
      d[6] = (uint8_t)(total & 0xFF); d[7] = (uint8_t)((total >> 8) & 0xFF);
      d[8] = (uint8_t)((total >> 16) & 0xFF);
      d[9] = csum; d[10] = (uint8_t)(chunks & 0xFF);
      d[11] = (uint8_t)(chunks >> 8); d[12] = 0xEE; }
    gotoxy(2, 5); say("bytes  = "); cputhex(total, 8);  say("   want 00001C6C");
    gotoxy(2, 6); say("chunks = "); cputhex(chunks, 4);
    gotoxy(2, 7); say("xor    = "); cputhex(csum, 2);
    for (;;) { }
}

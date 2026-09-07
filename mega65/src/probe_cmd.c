/* PROBE 4: the three things the write path needs that nothing has tested.
 *
 *   1. OVERWRITING. Probes 2 and 3 wrote to a blank disk. Saving twice is the
 *      normal case, and CBM drives do not silently replace a file --
 *      c128/src/storage.c scratches first, on the command channel. Untested
 *      on this DOS.
 *   2. THE COMMAND CHANNEL. LFN 15 / secondary 15 and its error text are the
 *      whole basis for STOR_NOTFOUND vs STOR_ERROR. Without it plat_read_all
 *      cannot tell "no save yet" from "broken disk", and the setup screen
 *      either hides a restore that exists or offers one that does not.
 *   3. THE WINDOW DURING A READ. Probe 2 showed $C000..$CBFF survives a
 *      WRITE. A read may use different DOS buffers -- and the briefing streams
 *      from disk WHILE AN OVERLAY IS LOADED, so a buffer parked there would
 *      quietly corrupt whatever code is in the window.
 *
 * STEP 3 IS A DELIBERATE FAILURE. Writing the same name twice with no scratch
 * in between should be refused. If it is not, the scratch is not load-bearing
 * and step 5 proves nothing -- a probe where every step succeeds cannot tell
 * you which step mattered.
 *
 * Order and expected error codes:
 *   1 cmd channel on a fresh disk      00  OK
 *   2 write, file absent               00  OK
 *   3 write again, NO scratch          63  FILE EXISTS      <- must fail
 *   4 scratch                          01  FILES SCRATCHED
 *   5 write again, after scratch       00  OK
 *   6 open a name that is not there    62  FILE NOT FOUND
 *   7 read the file back               content + window intact
 */
#include <stdint.h>
#include <mega65/conio.h>

#define NAME "TREKSAVE"
#define PAYLOAD 8

static char nbuf[24];
static volatile unsigned char nlo, nhi, nlen, lfn_g, sa_g, obyte;
volatile unsigned char inbyte, instat;

static volatile unsigned char stage;
static char s_init[6], s_a[6], s_b[6], s_scr[6], s_c[6], s_miss[6], s_read[6];
static unsigned char rbuf[PAYLOAD + 2];
static unsigned char rlen;

#define WIN     ((volatile unsigned char *)0xC000)
#define WIN_LEN 0x0C00
static unsigned int win_bad, win_first;

static void say(const char *s) { cputs((const unsigned char *)s); }

/* Config C, established by probe 2: BASIC out, ROMC in. Mapped once around
   the whole sequence -- every variable this probe touches is below $A000. */
#define MAP()   __asm__ volatile("sei\n\tlda #$3e\n\tsta $01\n\t"           \
                                 "lda #$64\n\tsta $d030" ::: "a", "memory")
#define UNMAP() __asm__ volatile("lda #$3e\n\tsta $01\n\t"                  \
                                 "lda #$44\n\tsta $d030\n\tcli" ::: "a", "memory")

static void set_name(const char *s)
{
    unsigned char i = 0;
    while (s[i]) { nbuf[i] = s[i]; i++; }
    nlen = i;
    nlo  = (unsigned char)(unsigned)nbuf;
    nhi  = (unsigned char)(((unsigned)nbuf) >> 8);
    __asm__ volatile("lda nlen\n\tldx nlo\n\tldy nhi\n\tjsr $ffbd"
                     ::: "a", "x", "y", "memory");
}

static void set_noname(void)
{
    __asm__ volatile("lda #0\n\tldx #0\n\tldy #0\n\tjsr $ffbd"
                     ::: "a", "x", "y", "memory");
}

static void k_open(unsigned char lfn, unsigned char sa)
{
    lfn_g = lfn; sa_g = sa;
    __asm__ volatile("lda lfn_g\n\tldx #8\n\tldy sa_g\n\tjsr $ffba\n\t"
                     "jsr $ffc0" ::: "a", "x", "y", "memory");
}
static void k_close(unsigned char lfn)
{
    lfn_g = lfn;
    __asm__ volatile("lda lfn_g\n\tjsr $ffc3" ::: "a", "x", "y", "memory");
}
static void k_chkout(unsigned char lfn)
{
    lfn_g = lfn;
    __asm__ volatile("ldx lfn_g\n\tjsr $ffc9" ::: "a", "x", "memory");
}
static void k_chkin(unsigned char lfn)
{
    lfn_g = lfn;
    __asm__ volatile("ldx lfn_g\n\tjsr $ffc6" ::: "a", "x", "memory");
}
static void k_out(unsigned char c)
{
    obyte = c;
    __asm__ volatile("lda obyte\n\tjsr $ffd2" ::: "a", "memory");
}
static unsigned char k_in(void)
{
    __asm__ volatile("jsr $ffcf\n\tsta inbyte\n\tjsr $ffb7\n\tsta instat"
                     ::: "a", "x", "y", "memory");
    return inbyte;
}
static void k_clrchn(void)
{
    __asm__ volatile("jsr $ffcc" ::: "a", "x", "y", "memory");
}

/* Open 15, read the first five characters of the error text, drain the rest,
   close. The code is the first two -- "00", "63", "62". */
static void status(char *out)
{
    unsigned char i;
    set_noname();
    k_open(15, 15);
    k_chkin(15);
    for (i = 0; i < 5; i++) out[i] = (char)k_in();
    out[5] = 0;
    for (i = 0; i < 60; i++) { if (instat) break; if (k_in() == 0x0D) break; }
    k_clrchn();
    k_close(15);
}

static void write_file(void)
{
    unsigned char i;
    set_name(NAME ",S,W");
    k_open(2, 2);
    k_chkout(2);
    for (i = 0; i < PAYLOAD; i++) k_out((unsigned char)('A' + i));
    k_clrchn();
    k_close(2);
}

int main(void)
{
    unsigned int i;

    conioinit();
    clrscr();
    gotoxy(2, 1); say("PROBE 4: OVERWRITE, ERROR CHANNEL, WINDOW DURING A READ");
    gotoxy(2, 3); say("running ...");

    for (i = 0; i < WIN_LEN; i++) WIN[i] = (unsigned char)((i ^ 0x5A) & 0xFF);

    MAP();
    stage = 1; status(s_init);                    /* fresh disk           */
    stage = 2; write_file(); status(s_a);         /* absent -> should work */
    stage = 3; write_file(); status(s_b);         /* present, NO scratch   */
    stage = 4; set_noname(); k_open(15, 15); k_chkout(15);
               { const char *c = "S0:" NAME; unsigned char j = 0;
                 while (c[j]) k_out((unsigned char)c[j++]); }
               k_clrchn(); k_close(15); status(s_scr);
    stage = 5; write_file(); status(s_c);         /* after scratch         */
    stage = 6; set_name("NOPE,S,R"); k_open(2, 2); status(s_miss); k_close(2);
    stage = 7; set_name(NAME ",S,R"); k_open(2, 2); k_chkin(2);
               rlen = 0;
               for (i = 0; i < sizeof rbuf; i++) {
                   unsigned char c = k_in();
                   if (instat & 0x80) break;
                   rbuf[rlen++] = c;
                   if (instat & 0x40) break;
               }
               k_clrchn(); k_close(2); status(s_read);
    stage = 8;
    UNMAP();

    win_bad = 0; win_first = 0xFFFF;
    for (i = 0; i < WIN_LEN; i++)
        if (WIN[i] != (unsigned char)((i ^ 0x5A) & 0xFF)) {
            if (win_first == 0xFFFF) win_first = i;
            win_bad++;
        }

    gotoxy(2, 3);  say("stage (8 = finished) = "); cputhex(stage, 2);
    gotoxy(2, 5);  say("1 fresh disk      "); say(s_init);  say("  want 00");
    gotoxy(2, 6);  say("2 write, absent   "); say(s_a);     say("  want 00");
    gotoxy(2, 7);  say("3 write, NO scr   "); say(s_b);     say("  want 63");
    gotoxy(2, 8);  say("4 scratch         "); say(s_scr);   say("  want 01");
    gotoxy(2, 9);  say("5 write, scratched"); say(s_c);     say("  want 00");
    gotoxy(2, 10); say("6 missing file    "); say(s_miss);  say("  want 62");
    gotoxy(2, 11); say("7 read back       "); say(s_read);  say("  want 00");
    gotoxy(2, 13); say("bytes read = "); cputhex(rlen, 2); say("  want 08   data ");
    for (i = 0; i < rlen && i < 8; i++) cputhex(rbuf[i], 2);
    gotoxy(2, 14); say("window $C000-$CBFF changed = "); cputhex(win_bad, 4);
    say("  first +"); cputhex(win_first, 4);

    for (;;) { }
}

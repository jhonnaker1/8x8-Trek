/* Does the storage seam actually move bytes? Writes a known payload, reads it
   back, compares, and reports over CHROUT so `x16emu -echo` answers without a
   screenshot. Device 8 is HostFS under the emulator, so the file lands in the
   working directory as a real file. */
#include <stdint.h>
#include <string.h>
#include "../../core/storage.h"

void chrout(char c);
__asm__(".global chrout\nchrout:\n jsr $FFD2\n rts\n");
static void say(const char *s) { while (*s) chrout(*s++); }
static void line(const char *s) { say(s); chrout(13); }
static void hex(unsigned char v) { const char *h="0123456789ABCDEF";
    chrout(h[(v>>4)&15]); chrout(h[v&15]); }

#define N 300
#define CAP (N + 16)   /* BIGGER THAN THE FILE, as real callers size it: the
                          seam deliberately reports STOR_ERROR when a read
                          exactly fills the buffer, because it cannot tell a
                          full buffer from a truncated one. Reading with
                          max == filesize trips that guard on purpose. */
static unsigned char out[N], in[CAP];

int main(void) {
    uint16_t got = 0;
    uint8_t rc;
    unsigned int i, bad = 0;

    for (i = 0; i < N; i++) out[i] = (unsigned char)((i * 7 + 11) & 0xFF);

    line("STORE: BEGIN");
    rc = plat_write_all("TREKTEST", out, N);
    say("STORE: WRITE RC="); hex(rc); chrout(13);

    memset(in, 0, CAP);
    rc = plat_read_all("TREKTEST", in, CAP, &got);
    say("STORE: READ RC="); hex(rc);
    say(" GOT="); hex((unsigned char)(got >> 8)); hex((unsigned char)(got & 0xFF));
    chrout(13);

    for (i = 0; i < N; i++) if (in[i] != out[i]) bad++;
    say("STORE: MISMATCHES="); hex((unsigned char)(bad >> 8)); hex((unsigned char)(bad & 0xFF));
    chrout(13);
    line(bad == 0 && got == N ? "STORE: ROUNDTRIP PASS" : "STORE: ROUNDTRIP FAIL");

    /* VERIFY BY MAKING IT FAIL. A read that always says OK proves nothing:
       a missing file must come back STOR_NOTFOUND (1), not STOR_OK, and not
       a silent empty buffer. */
    got = 0xFFFF;
    rc = plat_read_all("NOSUCHFILE", in, CAP, &got);
    say("STORE: MISSING RC="); hex(rc);
    say(" GOT="); hex((unsigned char)(got >> 8)); hex((unsigned char)(got & 0xFF));
    chrout(13);
    line(rc == STOR_NOTFOUND ? "STORE: MISSING PASS" : "STORE: MISSING FAIL");

    for (;;) { }
    return 0;
}

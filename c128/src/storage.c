#include <cbm.h>
#include <string.h>

#include "../../core/storage.h"

/* The C128 half of the disk seam: KERNAL file I/O behind core/storage.h.
 *
 * NOT IN THE BUILD YET, and deliberately so. This compiles clean and measures
 * 815 bytes of CODE; the C128 binary had 210 bytes of MAIN left when it was
 * written, and linking it in overflows by 2,920 once core/hof.c comes with
 * it. It is committed compiled-and-measured rather than held back, because
 * the number is the point: it is what says how much room the port has to find
 * before it can read a file at all. Add it to SRC in c128/Makefile the day
 * there is room. See NOTES.md, "The C128 binary hit its ceiling".
 *
 * Device 8, and the caller's opaque token becomes a CBM DOS filename here and
 * nowhere else. `0:` selects drive 0 of the unit -- a 1541 has only one drive
 * but wants the prefix anyway, and a 1571 or an SD2IEC needs it. `,S,R` and
 * `,S,W` name a SEQuential file opened for Read or Write; without the type
 * the DOS guesses, and what it guesses for a write is a PRG.
 *
 * `@0:` on the write is "replace". Without it, opening an existing file for
 * write is error 63, FILE EXISTS -- and the hall of fame is rewritten every
 * time it changes, so replace is the normal case rather than the exception.
 *
 * WHY THE STATUS CHANNEL. Opening a file that does not exist SUCCEEDS on a
 * CBM drive. The failure only appears when you ask the command channel, and a
 * port that skips that step reads an empty hall of fame as a valid one full
 * of nothing. That is the trap this file exists to absorb, and it is why
 * plat_read_all can return STOR_NOTFOUND at all.
 */

#define DEV      8
#define LFN_DATA 2
#define LFN_CMD  15

/* Long enough for "@0:" + a 16-character CBM name + ",S,W" + NUL. */
static char fname[24];

static const char *cbm_name(const char *name, char mode) {
    char *p = fname;

    if (mode == 'W') { *p++ = '@'; }
    *p++ = '0'; *p++ = ':';
    strcpy(p, name);
    p += strlen(name);
    *p++ = ','; *p++ = 'S';
    *p++ = ','; *p++ = mode;
    *p = '\0';
    return fname;
}

/* The DOS error code, or 0 if the drive is happy. The command channel answers
   with "NN, MESSAGE,TT,SS" in PETSCII digits; only the number is wanted.
   Returns 255 if the channel itself could not be opened, which on this
   machine means no drive is attached. */
static unsigned char dos_status(void) {
    char buf[8];
    int n;

    if (cbm_open(LFN_CMD, DEV, LFN_CMD, "")) return 255;
    n = cbm_read(LFN_CMD, buf, 2);
    cbm_close(LFN_CMD);

    if (n != 2) return 255;
    return (unsigned char)((buf[0] - '0') * 10 + (buf[1] - '0'));
}

/* 62 is FILE NOT FOUND. Everything else that is not 0 is a real fault. */
static uint8_t classify(unsigned char code) {
    if (code == 0)  return STOR_OK;
    if (code == 62) return STOR_NOTFOUND;
    return STOR_ERROR;
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got) {
    int n;
    unsigned char st;

    *got = 0;
    if (cbm_open(LFN_DATA, DEV, LFN_DATA, cbm_name(name, 'R')))
        return STOR_ERROR;

    /* Ask BEFORE reading. A missing file opens cleanly and then returns zero
       bytes, which is indistinguishable from an empty file until you look. */
    st = dos_status();
    if (st) { cbm_close(LFN_DATA); return classify(st); }

    n = cbm_read(LFN_DATA, buf, max);
    cbm_close(LFN_DATA);

    if (n < 0) return STOR_ERROR;
    *got = (uint16_t)n;

    /* A file that filled the buffer exactly might have had more to give, and
       we cannot tell -- so treat a full buffer as an error rather than hand
       back a silent truncation. Callers size with the format's own length. */
    if ((uint16_t)n == max) return STOR_ERROR;
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len) {
    int n;
    unsigned char st;

    if (cbm_open(LFN_DATA, DEV, LFN_DATA, cbm_name(name, 'W')))
        return STOR_ERROR;

    n = cbm_write(LFN_DATA, buf, len);
    cbm_close(LFN_DATA);

    /* AFTER the close, not before it: on a CBM drive a write is not committed
       until the file is closed, and a disk-full shows up at that point. */
    st = dos_status();
    if (st) return classify(st);
    if (n < 0 || (uint16_t)n != len) return STOR_ERROR;
    return STOR_OK;
}

/* Streaming, for the briefing. One file at a time by contract, so the open
   closes whatever was left behind rather than trusting the caller. */
static uint8_t streaming = 0;

uint8_t plat_open(const char *name) {
    unsigned char st;

    plat_close();
    if (cbm_open(LFN_DATA, DEV, LFN_DATA, cbm_name(name, 'R')))
        return STOR_ERROR;

    st = dos_status();
    if (st) { cbm_close(LFN_DATA); return classify(st); }

    streaming = 1;
    return STOR_OK;
}

uint16_t plat_read(void *buf, uint16_t len) {
    int n;

    if (!streaming) return 0;
    n = cbm_read(LFN_DATA, buf, len);
    return (n > 0) ? (uint16_t)n : 0;
}

void plat_close(void) {
    if (streaming) { cbm_close(LFN_DATA); streaming = 0; }
}

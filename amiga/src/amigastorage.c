/* Files for the Amiga: AmigaDOS Open/Read/Write/Close under the five plat_*
 * functions of core/storage.h.
 *
 * THE EASIEST STORAGE SEAM ON THE PROJECT, and it is the first one where
 * WRITING WORKS. There is no KERNAL channel to open and close in the right
 * order, no hypervisor trap, no device number, no secondary address, no 8K
 * window to negotiate and no 512-byte sector to buffer; here it is one
 * Write(). (~~The MEGA65's plat_write_all() is still a stub returning
 * STOR_ERROR because writing there needs a low-memory trampoline nobody has
 * built.~~ **Not since 2026-09-08: the MEGA65 dropped the Hypervisor for the
 * C65 DOS on device 8 and its SAVE is verified as a round trip. A negative
 * about ANOTHER port, in this port's file, is the one nobody re-reads.**)
 *
 * PROGDIR:, AND THAT IS THE WHOLE OF THE PATH DESIGN. The game asks for
 * "STRINGS.DAT" and "EGATREK.SAV" -- bare names, because on the other three
 * machines a bare name means "the disk in the drive". Here it would mean "the
 * shell's current directory", which is wherever the player happened to be
 * standing when they typed the command, and the data files are in the
 * program's own drawer. PROGDIR: is AmigaDOS's automatic assign for exactly
 * that drawer, so prefixing it makes `work:egatrek` find its files whether it
 * was run from WORK:, from SYS:, or from Workbench.
 *
 * core/storage.h's contract is deliberately weaker than this machine can
 * manage -- it promises only NOTFOUND against ERROR, because that is the most
 * every target can tell apart. IoErr() distinguishes far more, and the extra
 * detail is thrown away here rather than smuggled into a fourth return code
 * the shared UI would not know what to do with.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>

#include "../../core/storage.h"

/* Long enough for PROGDIR: plus the longest name the game asks for, which is
   a filename the player typed at the SAVE prompt (18 bytes in ui.c). */
static char path[64];
static BPTR stream;          /* the one file plat_open/plat_read stream */

/* Open() wants a STRPTR -- unsigned char * on this toolchain -- while
   storage.h speaks const char *. The cast lives HERE, once, at the boundary,
   rather than at each of the four call sites. */
static STRPTR fixname(const char *name) {
    int i = 0, j = 0;
    static const char pre[] = "PROGDIR:";

    while (pre[i] && j < (int)sizeof path - 1) path[j++] = pre[i++];
    while (name[0] && j < (int)sizeof path - 1) path[j++] = *name++;
    path[j] = '\0';
    return (STRPTR)path;
}

/* NOTFOUND is the one distinction the shared UI acts on: the setup screen
   offers to restore a saved game and must tell "there is no save" apart from
   "the disk went wrong", and the hall of fame treats a missing file as an
   empty table rather than a failure. */
static uint8_t open_error(void) {
    return (IoErr() == ERROR_OBJECT_NOT_FOUND) ? STOR_NOTFOUND : STOR_ERROR;
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got) {
    BPTR fh;
    LONG n;

    *got = 0;
    fh = Open(fixname(name), MODE_OLDFILE);
    if (!fh) return open_error();

    /* ASKS FOR ONE MORE BYTE THAN IT WILL ACCEPT. storage.h: "A file longer
       than max is an error, not a truncation -- silently reading half a save
       is worse than refusing." A read of exactly `max` cannot tell a file
       that fits from one that was cut off, so this reads max+1 into a buffer
       that has room for max and rejects anything that fills it. */
    n = Read(fh, buf, (LONG)max);
    if (n >= 0 && n == (LONG)max) {
        char extra;
        if (Read(fh, &extra, 1) > 0) { Close(fh); return STOR_ERROR; }
    }
    Close(fh);

    if (n < 0) return STOR_ERROR;
    *got = (uint16_t)n;
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len) {
    BPTR fh;
    LONG n;

    fh = Open(fixname(name), MODE_NEWFILE);   /* creates or truncates */
    if (!fh) return STOR_ERROR;               /* never NOTFOUND on a write */

    n = Write(fh, (APTR)buf, (LONG)len);

    /* CLOSE DECIDES WHETHER IT WORKED. A short Write is a failure, but so is
       a Close that cannot flush -- a full disk usually shows up there and not
       before, and reporting success on a save that never landed is the worst
       outcome this seam has. */
    if (!Close(fh)) return STOR_ERROR;
    return (n == (LONG)len) ? STOR_OK : STOR_ERROR;
}

uint8_t plat_open(const char *name) {
    plat_close();                 /* one at a time, per storage.h */
    stream = Open(fixname(name), MODE_OLDFILE);
    return stream ? STOR_OK : open_error();
}

uint16_t plat_read(void *buf, uint16_t len) {
    LONG n;

    if (!stream) return 0;
    n = Read(stream, buf, (LONG)len);
    return (n > 0) ? (uint16_t)n : 0;   /* 0 at end of file, and on error */
}

void plat_close(void) {
    if (stream) { Close(stream); stream = 0; }
}

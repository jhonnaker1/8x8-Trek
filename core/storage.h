#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

/* The platform half of the disk seam: four functions, one per port.
 *
 * This header lives beside the core so that every port implements the SAME
 * contract, but `core/` never calls any of it -- the core has no I/O at all.
 * The callers are the UI and main.
 *
 * NAMES ARE OPAQUE TOKENS. A caller says "TREK.SCR" and the platform decides
 * what that means: `0:TREK.SCR,S,R` on a 1541, `PROGDIR:trek.scr` on the
 * Amiga, `D:TREK.SCR` through an Atari IOCB, an MCP path on the F256. The
 * core and the UI must never see a path, a device number, a drive letter or a
 * logical file number. That rule is what makes this one line of difference
 * per port rather than a rewrite.
 *
 * ERRORS ARE DELIBERATELY COARSE. "File not found" and "no drive attached"
 * are not the same condition, and only some of these platforms can tell them
 * apart -- so the contract promises only the distinction every one of them
 * can actually make.
 */

#define STOR_OK        0
#define STOR_NOTFOUND  1   /* no such file; on some platforms also "no drive" */
#define STOR_ERROR     2   /* anything else: full disk, write protect, I/O */

/* Whole-file read. `got` receives the byte count. Returns a STOR_* code.
   A file longer than `max` is an error, not a truncation -- silently reading
   half a save is worse than refusing. */
uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got);

/* Whole-file write, replacing anything already there. */
uint8_t plat_write_all(const char *name, const void *buf, uint16_t len);

/* Streaming read, for the briefing pages -- the one file too big to hold in
   memory on the 8-bit targets. One at a time: plat_open() closes whatever was
   open before it. plat_read() returns bytes read, 0 at end of file. */
uint8_t  plat_open(const char *name);
uint16_t plat_read(void *buf, uint16_t len);
void     plat_close(void);

#endif

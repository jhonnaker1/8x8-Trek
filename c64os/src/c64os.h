#ifndef C64OS_H
#define C64OS_H

/* What this port's own files share with each other and with src/app.s. The
   seams the shared code calls are declared where they always are -- vdc.h,
   input.h, sid.h, core/storage.h, core/farmem.h, core/overlay.h. */

#include <stdint.h>

/* src/app.s */
unsigned int  os_readkey(void);          /* next key off C64 OS's queue, or 0xFFFF */
unsigned char os_loadlib(unsigned char c1, unsigned char c2, unsigned char a);
unsigned char os_bkalloc(unsigned char libpage, unsigned char banks);
void os_present(void);
void app_exit(void);

/* HOW FAR STARTUP GOT, for tools/rig.py to read while it happens: the first
   run went black after the load and the machine's RAM was gone before the
   diagnosis could look -- a record written as it goes survives in the
   harness's log even when memory does not. 1 screen, 2 keys, 3 library,
   4 banks, 5/6 a far-store file started/stored, 10+n overlay n loading,
   40 overlays done, 50 waiting for a key. Lives in osvid.c. */
extern volatile unsigned char c64os_stage;

/* src/osvid.c: copy the buffers to the screen if anything changed. */
void osv_flush(void);

/* src/osfile.c: where the app's files are, read out of C64 OS's own record
   of the app (appfileref) -- the device, and "<partition>//path/:" to put in
   front of every file name, so nothing depends on the drive's current
   directory. */
extern unsigned char c64os_dev;
const char *c64os_prefix(void);

/* src/osfile.c: the two REU banks bkalloc_ gave the game, as REU bank
   numbers (appreubk added). 0 = none. c64os_libpage is memory.lib's page,
   kept for tools/rig.py: the bank map bkalloc_ scanned is still in its
   buffer, at page*256+$214, when the game halts for want of banks. */
extern unsigned char c64os_bank;
extern volatile unsigned char c64os_libpage;

#endif

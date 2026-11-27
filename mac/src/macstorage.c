/* Files for the Macintosh: the program's own resources for the data it
 * ships, and the File Manager for the files a player makes.
 *
 * THE DATA IS INSIDE THE PROGRAM, by Jamie's decision (2026-11-26): copying
 * the application is the whole installation, and no STRINGS.DAT can go
 * missing beside it. src/egatrek.r puts STRINGS.DAT, MUSIC.DAT and BRIEF.TXT
 * in as 'EGAd' resources under those very names, so the five functions of
 * core/storage.h keep their callers unchanged: a name with a resource is
 * answered from the resource, and anything else -- the save, the hall of
 * fame -- is a FILE.
 *
 * FILES GO IN THE APPLICATION'S OWN FOLDER, found from the application's
 * open resource fork (CurApRefNum) through PBGetFCBInfo, which gives its
 * real volume and directory ID. NOT the "default directory": under System 6
 * with MultiFinder and under System 7 that is whatever the Finder last left
 * it as, and a save that lands somewhere the player never looks is the
 * Amiga's PROGDIR: lesson in a different form.
 *
 * storage.h's contract: NOTFOUND against ERROR, and a file longer than the
 * caller's buffer is an ERROR, never a truncation -- the Falcon's first draft
 * truncated silently and its storetest caught it.
 */
#include <Files.h>
#include <Resources.h>
#include <Memory.h>
#include <SegLoad.h>
#include <string.h>

#include "../../core/storage.h"

#define DATA_TYPE  'EGAd'
#define FILE_CREATOR 'EGAt'
#define FILE_TYPE  'EGAs'

/* The application's resource file, from GetAppParms rather than the
   CurApRefNum low-memory global: gcc 16 at -O2 rejects a read through a
   constant address as out of bounds, and the trap says the same thing. */
static short app_refnum(void)
{
    Str255 name;
    short ref;
    Handle params;
    GetAppParms(name, &ref, &params);
    return ref;
}

static short app_vref;
static long  app_dir;
static unsigned char located;

/* C string -> Pascal string. Names are 8.3 and short; 31 is HFS's limit. */
static void to_pascal(const char *s, Str255 out)
{
    size_t n = strlen(s);
    if (n > 31) n = 31;
    out[0] = (unsigned char)n;
    memcpy(out + 1, s, n);
}

static void locate_app(void)
{
    FCBPBRec pb;
    Str255 name;

    if (located) return;
    located = 1;
    memset(&pb, 0, sizeof pb);
    pb.ioNamePtr = name;
    pb.ioRefNum = app_refnum();
    pb.ioFCBIndx = 0;
    if (PBGetFCBInfoSync(&pb) == noErr) {
        app_vref = pb.ioFCBVRefNum;
        app_dir = pb.ioFCBParID;
    }
    /* On failure both stay 0: the default volume and directory, which is
       where the Finder launched us from on every system that fails this. */
}

static uint8_t map_err(OSErr e)
{
    return (e == fnfErr || e == dirNFErr || e == nsvErr) ? STOR_NOTFOUND : STOR_ERROR;
}

/* The program's own data, if it has a resource of this name. */
static Handle data_resource(const char *name)
{
    Str255 p;
    Handle h;
    short cur = CurResFile();

    /* Get1 looks in ONE file, and that file is made the application's own
       for the call: a desk accessory or a font file can change the current
       one, and a System file resource of the same name must never answer. */
    to_pascal(name, p);
    UseResFile(app_refnum());
    h = Get1NamedResource(DATA_TYPE, p);
    UseResFile(cur);
    return h;
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got)
{
    Handle h;
    short ref;
    long size, n;
    OSErr e;
    Str255 p;

    if (got) *got = 0;

    h = data_resource(name);
    if (h) {
        size = GetHandleSize(h);
        if (size > (long)max) { ReleaseResource(h); return STOR_ERROR; }
        BlockMoveData(*h, buf, size);
        ReleaseResource(h);
        if (got) *got = (uint16_t)size;
        return STOR_OK;
    }

    locate_app();
    to_pascal(name, p);
    e = HOpen(app_vref, app_dir, p, fsRdPerm, &ref);
    if (e != noErr) return map_err(e);
    if (GetEOF(ref, &size) != noErr || size > (long)max) {
        FSClose(ref);
        return STOR_ERROR;
    }
    n = size;
    e = FSRead(ref, &n, buf);
    FSClose(ref);
    if (e != noErr && e != eofErr) return STOR_ERROR;
    if (n != size) return STOR_ERROR;
    if (got) *got = (uint16_t)n;
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len)
{
    short ref;
    long n = len;
    OSErr e;
    Str255 p;

    locate_app();
    to_pascal(name, p);
    e = HCreate(app_vref, app_dir, p, FILE_CREATOR, FILE_TYPE);
    if (e != noErr && e != dupFNErr) return map_err(e);
    e = HOpen(app_vref, app_dir, p, fsRdWrPerm, &ref);
    if (e != noErr) return map_err(e);
    /* Truncate first: a shorter save must not keep the old one's tail. */
    e = SetEOF(ref, 0);
    if (e == noErr) e = FSWrite(ref, &n, buf);
    FSClose(ref);
    FlushVol(NULL, app_vref);
    if (e != noErr || n != (long)len) return STOR_ERROR;
    return STOR_OK;
}

/* ONE STREAM AT A TIME, from a resource or a file. The briefing is the
   reason this exists: the C128 has no room to buffer it, so every port
   streams it a page at a time, and the seam's shape stays the 8-bit
   machines' here too. */
static Handle s_res;
static long   s_pos;
static short  s_ref = -1;

void plat_close(void)
{
    if (s_res) { HUnlock(s_res); ReleaseResource(s_res); s_res = NULL; }
    if (s_ref >= 0) { FSClose(s_ref); s_ref = -1; }
}

uint8_t plat_open(const char *name)
{
    OSErr e;
    Str255 p;

    plat_close();
    s_res = data_resource(name);
    if (s_res) {
        HLock(s_res);
        s_pos = 0;
        return STOR_OK;
    }
    locate_app();
    to_pascal(name, p);
    e = HOpen(app_vref, app_dir, p, fsRdPerm, &s_ref);
    if (e != noErr) { s_ref = -1; return map_err(e); }
    return STOR_OK;
}

uint16_t plat_read(void *buf, uint16_t len)
{
    if (s_res) {
        long left = GetHandleSize(s_res) - s_pos;
        long n = (left < (long)len) ? left : (long)len;
        if (n <= 0) return 0;
        BlockMoveData(*s_res + s_pos, buf, n);
        s_pos += n;
        return (uint16_t)n;
    }
    if (s_ref >= 0) {
        long n = len;
        OSErr e = FSRead(s_ref, &n, buf);
        if (e != noErr && e != eofErr) return 0;
        return (uint16_t)n;
    }
    return 0;
}

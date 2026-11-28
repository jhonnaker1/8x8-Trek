/* Files for the NeXT: plain stdio, under a UNIX that has all of it.
 *
 * THE DATA IS INSIDE THE APPLICATION, as on the Mac, but the NeXT's way: an
 * application is a FOLDER, EGATrek.app, and STRINGS.DAT, MUSIC.DAT and
 * BRIEF.TXT sit in it beside the program. Copying EGATrek.app is the whole
 * installation, and the Workspace shows it as one thing.
 *
 * THE FILES A PLAYER MAKES -- EGATREK.SAV and the hall of fame, TREK.SCR --
 * go in the folder that HOLDS EGATrek.app, as they go beside the application
 * on every other port. Never inside the wrapper: a player who copies a newer
 * EGATrek.app over the old one would lose them.
 *
 * Both folders come from nextvid.m, which asks the application's bundle
 * where it is; neither is the current directory, which is wherever the
 * Workspace or a Terminal happened to be.
 *
 * storage.h's contract: NOTFOUND against ERROR, and a file longer than the
 * caller's buffer is an ERROR, never a truncation.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "../../core/storage.h"
#include "nextvid.h"

/* The data files the program ships, answered from inside the wrapper. */
static int is_data(const char *name)
{
    return strcmp(name, "STRINGS.DAT") == 0 || strcmp(name, "MUSIC.DAT") == 0 ||
           strcmp(name, "BRIEF.TXT") == 0;
}

static void path_for(const char *name, char *out, int size)
{
    const char *dir = is_data(name) ? next_app_dir() : next_save_dir();
    int n = (int)strlen(dir);

    if (n + 2 + (int)strlen(name) > size) { out[0] = 0; return; }
    strcpy(out, dir);
    if (n && out[n - 1] != '/') strcat(out, "/");
    strcat(out, name);
}

static uint8_t open_err(void)
{
    return (errno == ENOENT || errno == ENOTDIR) ? STOR_NOTFOUND : STOR_ERROR;
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got)
{
    char path[1024];
    FILE *f;
    long size;
    size_t n;

    if (got) *got = 0;
    path_for(name, path, (int)sizeof path);
    if (!path[0]) return STOR_ERROR;
    f = fopen(path, "r");
    if (!f) return open_err();
    if (fseek(f, 0L, SEEK_END) != 0 || (size = ftell(f)) < 0 ||
        size > (long)max || fseek(f, 0L, SEEK_SET) != 0) {
        fclose(f);
        return STOR_ERROR;
    }
    n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if ((long)n != size) return STOR_ERROR;
    if (got) *got = (uint16_t)n;
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len)
{
    char path[1024];
    FILE *f;
    size_t n;
    int bad;

    path_for(name, path, (int)sizeof path);
    if (!path[0]) return STOR_ERROR;
    f = fopen(path, "w");          /* replaces: a shorter save keeps no tail */
    if (!f) return open_err();
    n = fwrite(buf, 1, len, f);
    bad = ferror(f);
    if (fclose(f) != 0) bad = 1;
    return (bad || n != len) ? STOR_ERROR : STOR_OK;
}

/* ONE STREAM AT A TIME: the briefing, a page at a time, the 8-bit seam's
   shape kept here too. */
static FILE *stream;

void plat_close(void)
{
    if (stream) { fclose(stream); stream = NULL; }
}

uint8_t plat_open(const char *name)
{
    char path[1024];

    plat_close();
    path_for(name, path, (int)sizeof path);
    if (!path[0]) return STOR_ERROR;
    stream = fopen(path, "r");
    return stream ? STOR_OK : open_err();
}

uint16_t plat_read(void *buf, uint16_t len)
{
    if (!stream) return 0;
    return (uint16_t)fread(buf, 1, len, stream);
}

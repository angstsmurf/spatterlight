/* fileresource.c: images and sounds from plain files, as Glk resources.

   Blorb games number their own resources.  A terp whose game keeps its
   pictures and sounds somewhere else -- a chunk of the game file, a file
   beside it, a file the terp decoded into a temp directory -- registers each
   one here and gets back a resource number that glk_image_draw* and
   glk_schannel_play* then find in the app's cache.

   garglk_add_resource_from_file() is Gargoyle's call for this, so terp code
   written for Gargoyle works here unchanged; gli_add_resource_from_path()
   is the Spatterlight-only form taking any path.

   A resource number is a hash of what was registered -- usage, path, offset
   and length -- so a chunk of a file that stays put (the game file, a file
   beside it) gets the same number in every session, and repeat
   registrations cost one cache lookup.  A temp file with a random name
   gets a number that only holds for the session.  Numbers have bit 30 set
   and bit 31 clear: positive as an int (win_loadimage and the terps' own
   bookkeeping take ints) and clear of the small numbers Blorb gives its
   resources. */

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <limits.h>

#include "glkimp.h"
#include "gi_blorb.h"

typedef struct {
    glui32 usage;
    glui32 id;
    glui32 offset;
    glui32 length;
    char *path;
} fileresource_t;

static fileresource_t *registered = NULL;
static size_t numregistered = 0;
static size_t maxregistered = 0;

static glui32 resource_hash(glui32 usage, const char *path,
                            glui32 offset, glui32 length)
{
    /* FNV-1a over the path, then the three numbers. */
    glui32 hash = 2166136261u;
    const unsigned char *p;

    for (p = (const unsigned char *)path; *p; p++)
        hash = (hash ^ *p) * 16777619u;
    hash = (hash ^ usage) * 16777619u;
    hash = (hash ^ offset) * 16777619u;
    hash = (hash ^ length) * 16777619u;
    return (hash & 0x3fffffffu) | 0x40000000u;
}

/* The number this resource has, or will have, in this session: its hash,
   stepped past any number already taken by a different resource. */
static fileresource_t *find_or_add(glui32 usage, const char *path,
                                   glui32 offset, glui32 length)
{
    glui32 id = resource_hash(usage, path, offset, length);
    size_t i;
    int clash;

    for (i = 0; i < numregistered; i++) {
        fileresource_t *r = &registered[i];
        if (r->usage == usage && r->offset == offset && r->length == length
            && strcmp(r->path, path) == 0)
            return r;
    }

    do {
        clash = 0;
        for (i = 0; i < numregistered; i++)
            if (registered[i].usage == usage && registered[i].id == id) {
                id = ((id + 1) & 0x3fffffffu) | 0x40000000u;
                clash = 1;
                break;
            }
    } while (clash);

    if (numregistered == maxregistered) {
        size_t newmax = maxregistered ? maxregistered * 2 : 32;
        fileresource_t *grown = realloc(registered, newmax * sizeof *grown);
        if (grown == NULL)
            return NULL;
        registered = grown;
        maxregistered = newmax;
    }
    registered[numregistered].path = strdup(path);
    if (registered[numregistered].path == NULL)
        return NULL;
    registered[numregistered].usage = usage;
    registered[numregistered].id = id;
    registered[numregistered].offset = offset;
    registered[numregistered].length = length;
    return &registered[numregistered++];
}

glui32 gli_add_resource_from_path(glui32 usage, const char *path,
                                  glui32 offset, glui32 length)
{
    struct stat info;
    fileresource_t *r;

    if (path == NULL || *path == '\0')
        return 0;
    if (usage == giblorb_ID_Pict) {
        if (!gli_enable_graphics)
            return 0;
    } else if (usage == giblorb_ID_Snd) {
        if (!gli_enable_sound)
            return 0;
    } else {
        return 0;
    }

    /* The chunk has to be there: a missing file, or one too short for it,
       is a failure now rather than a blank picture or silence later. */
    if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)
        || info.st_size > 0x7fffffff || (off_t)offset >= info.st_size)
        return 0;
    if (length == 0)
        length = (glui32)(info.st_size - offset);
    if ((off_t)offset + length > info.st_size)
        return 0;

    r = find_or_add(usage, path, offset, length);
    if (r == NULL)
        return 0;

    if (usage == giblorb_ID_Pict) {
        if (!win_findimage((int)r->id))
            win_loadimage((int)r->id, r->path, (int)offset, (int)length);
    } else {
        if (!win_findsound((int)r->id))
            win_loadsound((int)r->id, r->path, (int)offset, (int)length);
    }
    return r->id;
}

/* `name` in the game's directory, matched exactly or, failing that, ignoring
   case: games written on Windows name their files with whatever capitals
   the author happened to type.  Writes the full path to `out`. */
static int game_dir_file(const char *name, char *out, size_t size)
{
    struct stat info;
    struct dirent *entry;
    DIR *dir;
    int found = 0;

    if (gli_parentdir == NULL)
        return 0;
    if ((size_t)snprintf(out, size, "%s/%s", gli_parentdir, name) >= size)
        return 0;
    if (stat(out, &info) == 0)
        return 1;

    dir = opendir(gli_parentdir);
    if (dir == NULL)
        return 0;
    while ((entry = readdir(dir)) != NULL)
        if (strcasecmp(entry->d_name, name) == 0) {
            found = (size_t)snprintf(out, size, "%s/%s", gli_parentdir,
                                     entry->d_name) < size;
            break;
        }
    closedir(dir);
    return found;
}

glui32 garglk_add_resource_from_file(glui32 usage, const char *filename,
                                     glui32 offset, glui32 len)
{
    char path[PATH_MAX];

    /* A plain name in the game's directory, as in Gargoyle: no path of its
       own, and so no way out of that directory. */
    if (filename == NULL || *filename == '\0' || strchr(filename, '/')
        || strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
        return 0;
    if (!game_dir_file(filename, path, sizeof path))
        return 0;
    return gli_add_resource_from_path(usage, path, offset, len);
}

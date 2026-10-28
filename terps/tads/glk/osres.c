/* osres.c -- a game's images and sounds, as Glk resources
 *
 * HTML TADS games name their pictures and sounds by URL-style paths,
 * "pics/title.jpg".  A name is found, as HTML TADS finds it:
 *
 *   - in the resource maps: the HTMLRES section of a TADS 2 game file and
 *     of its .rs0-.rs9 files, or the MRES blocks of a TADS 3 image and its
 *     .3rN files, which the engines read and pass on to us (the appctx
 *     callbacks for TADS 2, CVmHostIfcGlk in t23run.cpp for TADS 3);
 *   - failing that, as a file in the game's folder or under it.  Nothing
 *     outside the folder is reachable: an absolute path, a drive letter or
 *     URL scheme, or a ".." is never looked up.
 *
 * Names match ignoring case, with %xx decoded and "\" taken as "/", since
 * games written on Windows name their files with whatever capitals and
 * separators the author typed.  A loose file is matched the same way, one
 * folder at a time.
 *
 * A resource is registered with the Glk library when it is first used,
 * which gives it a resource number for glk_image_draw and
 * glk_schannel_play.  In Spatterlight that is gli_add_resource_from_path,
 * which takes any path; Gargoyle's garglk_add_resource_from_file takes only
 * a file in the game's folder, so there a loose file in a subfolder can't
 * be had.  The library keeps its own record of what it was given, and
 * hands back the same number for the same chunk of the same file.
 */

#ifndef _WIN32
#include <dirent.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <limits.h>

#ifdef _WIN32
#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#endif

#include "os.h"
#include "appctx.h"
#include "glk.h"
#include "gi_blorb.h"
#include "osres.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

#define NAME_MAX_LEN 1024

typedef struct resfile
{
    char *path;
    unsigned long base;
} resfile;

typedef struct resentry
{
    char *name;                 /* decoded (norm_name) */
    int fileno;                 /* or -1 for a link */
    unsigned long ofs, siz;
    char *link;                 /* a link's local file */
} resentry;

static resfile *files = NULL;
static int nfiles = 0;

static resentry *entries = NULL;
static size_t nentries = 0, maxentries = 0;

/* ------------------------------------------------------------------------ */
/*
 *   Names
 */

static int hexval(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/*
 *   'name' with %xx decoded, "\" as "/" and any leading "./" dropped, in
 *   'out'.  Returns false if it doesn't fit or has a NUL in it.
 */
static int norm_name(const char *name, size_t len, char *out, size_t size)
{
    size_t i, n = 0;

    for (i = 0 ; i < len ; ++i)
    {
        char c = name[i];

        if (c == '%' && i + 2 < len
            && hexval(name[i + 1]) >= 0 && hexval(name[i + 2]) >= 0)
        {
            c = (char)(hexval(name[i + 1]) * 16 + hexval(name[i + 2]));
            i += 2;
        }
        if (c == '\\')
            c = '/';
        if (c == '\0' || n + 1 >= size)
            return 0;
        out[n++] = c;
        if (n == 2 && out[0] == '.' && out[1] == '/')
            n = 0;
    }
    out[n] = '\0';
    return 1;
}

/* ------------------------------------------------------------------------ */
/*
 *   The engines' side
 */

void osres_set_file(int fileno, const char *path)
{
    if (fileno < 0 || path == NULL)
        return;
    if (fileno >= nfiles)
    {
        resfile *grown = realloc(files, (fileno + 1) * sizeof(*grown));
        if (grown == NULL)
            return;
        memset(grown + nfiles, 0, (fileno + 1 - nfiles) * sizeof(*grown));
        files = grown;
        nfiles = fileno + 1;
    }
    free(files[fileno].path);
    files[fileno].path = strdup(path);
}

void osres_set_base(int fileno, unsigned long base)
{
    if (fileno >= 0 && fileno < nfiles)
        files[fileno].base = base;
    else
    {
        /* a base for a file we haven't been told the name of yet */
        osres_set_file(fileno, "");
        if (fileno >= 0 && fileno < nfiles)
            files[fileno].base = base;
    }
}

static resentry *new_entry(const char *name, size_t namelen)
{
    char buf[NAME_MAX_LEN];
    resentry *e;

    if (!norm_name(name, namelen, buf, sizeof(buf)) || buf[0] == '\0')
        return NULL;
    if (nentries == maxentries)
    {
        size_t newmax = maxentries ? maxentries * 2 : 64;
        resentry *grown = realloc(entries, newmax * sizeof(*grown));
        if (grown == NULL)
            return NULL;
        entries = grown;
        maxentries = newmax;
    }
    e = &entries[nentries];
    memset(e, 0, sizeof(*e));
    e->name = strdup(buf);
    if (e->name == NULL)
        return NULL;
    ++nentries;
    return e;
}

void osres_add(int fileno, unsigned long ofs, unsigned long siz,
               const char *name, size_t namelen)
{
    resentry *e = new_entry(name, namelen);

    if (e == NULL)
        return;
    e->fileno = fileno;
    e->ofs = ofs;
    e->siz = siz;
}

void osres_add_link(const char *path, size_t pathlen,
                    const char *name, size_t namelen)
{
    resentry *e = new_entry(name, namelen);

    if (e == NULL)
        return;
    e->fileno = -1;
    e->link = malloc(pathlen + 1);
    if (e->link != NULL)
    {
        memcpy(e->link, path, pathlen);
        e->link[pathlen] = '\0';
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   TADS 2's host context
 */

static void t2_set_game_name(void *ctx, const char *fname)
{
    (void)ctx;
    osres_set_file(0, fname);
}

static void t2_set_resmap_seek(void *ctx, unsigned long seekpos, int fileno)
{
    (void)ctx;
    osres_set_base(fileno, seekpos);
}

static void t2_add_resource(void *ctx, unsigned long ofs, unsigned long siz,
                            const char *nm, size_t nmlen, int fileno)
{
    (void)ctx;
    osres_add(fileno, ofs, siz, nm, nmlen);
}

static void t2_add_resource_link(void *ctx, const char *fname,
                                 size_t fnamelen, const char *resname,
                                 size_t resnamelen)
{
    (void)ctx;
    osres_add_link(fname, fnamelen, resname, resnamelen);
}

/* file 0 is the game file, so a resource file is never 0 */
static int t2_add_resfile(void *ctx, const char *fname)
{
    int fileno = nfiles ? nfiles : 1;

    (void)ctx;
    osres_set_file(fileno, fname);
    return fileno;
}

static int t2_resfile_exists(void *ctx, const char *res_name,
                             size_t res_name_len)
{
    (void)ctx;
    return osres_exists(res_name, res_name_len);
}

struct appctxdef *osres_t2_appctx(void)
{
    static appctxdef ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.set_game_name = t2_set_game_name;
    ctx.set_resmap_seek = t2_set_resmap_seek;
    ctx.add_resource = t2_add_resource;
    ctx.add_resource_link = t2_add_resource_link;
    ctx.add_resfile = t2_add_resfile;
    ctx.resfile_exists = t2_resfile_exists;
    return &ctx;
}

/* ------------------------------------------------------------------------ */
/*
 *   Lookup
 */

/*
 *   The last entry of that name, as in HTML TADS: a resource file can
 *   replace a resource of the game file or of an earlier resource file
 *   (Threnody's .gam has 2x2 placeholders for the pictures in its .rs1)
 */
static resentry *find_entry(const char *norm)
{
    size_t i;

    for (i = nentries ; i-- > 0 ; )
        if (strcasecmp(entries[i].name, norm) == 0)
            return &entries[i];
    return NULL;
}

/* the last folder separator in a path */
static const char *last_slash(const char *path)
{
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *back = strrchr(path, '\\');

    if (back != NULL && (slash == NULL || back > slash))
        slash = back;
#endif
    return slash;
}

/* the game's folder, from the game file's path, in 'out' */
static int game_dir(char *out, size_t size)
{
    const char *slash;
    size_t len;

    if (nfiles == 0 || files[0].path == NULL || files[0].path[0] == '\0')
        return 0;
    slash = last_slash(files[0].path);
    if (slash == NULL)
    {
        if (size < 2)
            return 0;
        strcpy(out, ".");
        return 1;
    }
    len = (size_t)(slash - files[0].path);
    if (len == 0)
        len = 1;                /* "/game.gam" */
    if (len >= size)
        return 0;
    memcpy(out, files[0].path, len);
    out[len] = '\0';
    return 1;
}

/*
 *   The file a name not in the maps stands for: 'norm' under the game's
 *   folder, each part matched exactly or else ignoring case.  Writes the
 *   path to 'out'.
 */
static int find_loose(const char *norm, char *out, size_t size)
{
    char part[NAME_MAX_LEN];
    const char *p = norm;
    struct stat info;

    if (norm[0] == '/' || strchr(norm, ':') != NULL)
        return 0;
    if (!game_dir(out, size))
        return 0;

    while (*p != '\0')
    {
        const char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        size_t base = strlen(out);
#ifndef _WIN32
        DIR *dir;
        struct dirent *entry;
        int found = 0;
#endif

        if (len >= sizeof(part))
            return 0;
        memcpy(part, p, len);
        part[len] = '\0';
        p += len;
        if (*p == '/')
            ++p;
        if (len == 0 || strcmp(part, ".") == 0)
            continue;
        if (strcmp(part, "..") == 0)
            return 0;

        if (base + 1 + len >= size)
            return 0;
        out[base] = '/';
        strcpy(out + base + 1, part);
        if (stat(out, &info) == 0)
            continue;

#ifdef _WIN32
        /* Windows file names are matched ignoring case already */
        return 0;
#else
        out[base] = '\0';
        dir = opendir(out);
        if (dir == NULL)
            return 0;
        while ((entry = readdir(dir)) != NULL)
            if (strcasecmp(entry->d_name, part) == 0
                && base + 1 + strlen(entry->d_name) < size)
            {
                out[base] = '/';
                strcpy(out + base + 1, entry->d_name);
                found = 1;
                break;
            }
        closedir(dir);
        if (!found)
            return 0;
#endif
    }
    return stat(out, &info) == 0 && S_ISREG(info.st_mode);
}

/*
 *   Where the resource's bytes are: a file, and a chunk of it (a length of
 *   0 is the whole file)
 */
static int locate(const char *name, size_t len, char *path, size_t size,
                  unsigned long *ofs, unsigned long *siz)
{
    char norm[NAME_MAX_LEN];
    resentry *e;

    if (!norm_name(name, len, norm, sizeof(norm)) || norm[0] == '\0')
        return 0;

    e = find_entry(norm);
    if (e != NULL && e->fileno >= 0)
    {
        if (e->fileno >= nfiles || files[e->fileno].path == NULL
            || files[e->fileno].path[0] == '\0' || e->siz == 0)
            return 0;
        if (strlen(files[e->fileno].path) >= size)
            return 0;
        strcpy(path, files[e->fileno].path);
        *ofs = files[e->fileno].base + e->ofs;
        *siz = e->siz;
        return 1;
    }
    if (e != NULL && e->link != NULL)
    {
        /* a link is the compiler's own record, so it's trusted */
        if (e->link[0] == '/')
        {
            if (strlen(e->link) >= size)
                return 0;
            strcpy(path, e->link);
        }
        else if (!find_loose(e->link, path, size))
            return 0;
        *ofs = *siz = 0;
        return 1;
    }

    if (!find_loose(norm, path, size))
        return 0;
    *ofs = *siz = 0;
    return 1;
}

glui32 osres_find(const char *name, size_t len, glui32 usage)
{
    char path[PATH_MAX];
    unsigned long ofs, siz;

    if (!locate(name, len, path, sizeof(path), &ofs, &siz))
        return 0;
    if (ofs > 0x7fffffffUL || siz > 0x7fffffffUL)
        return 0;
#ifdef SPATTERLIGHT
    return gli_add_resource_from_path(usage, path, (glui32)ofs, (glui32)siz);
#elif defined(GLK_MODULE_GARGLK_FILE_RESOURCES)
    {
        /* only a file in the game's folder; the library looks there */
        char dir[PATH_MAX];
        const char *slash = last_slash(path);
        size_t dirlen = slash ? (size_t)(slash - path) : 0;

        struct stat st;

        if (!game_dir(dir, sizeof(dir)) || strlen(dir) != dirlen
            || strncmp(dir, path, dirlen) != 0)
            return 0;

        /* a loose file is all of it, which the library needs the size of */
        if (siz == 0)
        {
            if (stat(path, &st) != 0 || st.st_size <= 0
                || st.st_size > 0x7fffffffL)
                return 0;
            siz = (unsigned long)st.st_size;
        }
        return garglk_add_resource_from_file(usage, slash ? slash + 1 : path,
                                             (glui32)ofs, (glui32)siz);
    }
#else
    (void)usage;
    return 0;
#endif
}

int osres_exists(const char *name, size_t len)
{
    char path[PATH_MAX];
    unsigned long ofs, siz;

    return locate(name, len, path, sizeof(path), &ofs, &siz);
}

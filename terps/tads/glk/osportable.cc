/* Adapted from src/osportable.cc from FrobTADS 1.2.3.
 * FrobTADS copyright (C) 2009 Nikos Chantziaras.
 */

/* This file implements some of the functions described in
 * tads2/osifc.h.  We don't need to implement them all, as most of them
 * are provided by tads2/osnoui.c and tads2/osgen3.c.
 *
 * This file implements the "portable" subset of these functions;
 * functions that depend upon curses/ncurses are defined in oscurses.cc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef GARGOYLE
#include "charmap.h"
#include <time.h>
#include <dirent.h>
#include <limits.h>

#if defined(_WIN32)
#include <windows.h>
#define MKDIR_TAKES_ONE_ARG 1
#define lstat stat
#elif defined(__APPLE__)
#define HAVE_GETTIMEOFDAY
#include <sys/time.h>
#else
#define HAVE_CLOCK_GETTIME
#endif
#define HAVE_MKDIR 1
#define T3_RES_DIR 0
#define T3_INC_DIR 0
#define T3_LIB_DIR 0
#define T3_LOG_FILE 0
#endif /* GARGOYLE */

#ifdef SPATTERLIGHT
#include "charmap.h"
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include "glk.h"
#define HAVE_GETTIMEOFDAY
#include <sys/time.h>
#define HAVE_MKDIR 1
#define T3_RES_DIR 0
#define T3_INC_DIR 0
#define T3_LIB_DIR 0
#define T3_LOG_FILE 0
#endif /* SPATTERLIGHT */

#ifdef SPATTERLIGHT
extern "C" glui32 gli_determinism;

/* <time.h> finds tads3/test/data/time.h, in the target's header paths */
extern "C" {
time_t time(time_t *);
struct tm *gmtime(const time_t *);
struct tm *localtime(const time_t *);
}

/*
 *   The clock under the determinism testing theme: the time of day is fixed
 *   at noon on 1 January 2000, in UTC, and the millisecond timer starts at 0
 *   and goes forward a millisecond each time it's read (so a loop waiting
 *   for it ends), and by the length of every sleep and timer event, so a
 *   game sees the same times on every run.
 */
#define VCLOCK_EPOCH 946728000

static unsigned long vclock_ms;

int osglk_vclock(void)
{
    return gli_determinism != 0;
}

long osglk_vclock_ms(void)
{
    return (long)++vclock_ms;
}

void osglk_vclock_advance(unsigned long ms)
{
    vclock_ms += ms;
}

void osglk_vclock_time_ns(time_t *seconds, long *nanoseconds)
{
    unsigned long ms = ++vclock_ms;
    *seconds = VCLOCK_EPOCH + ms / 1000;
    *nanoseconds = (long)(ms % 1000) * 1000000;
}

time_t osglk_time(time_t *t)
{
    time_t now = gli_determinism ? VCLOCK_EPOCH + vclock_ms / 1000 : time(0);
    if (t)
        *t = now;
    return now;
}

struct tm *osglk_localtime(const time_t *t)
{
    return gli_determinism ? gmtime(t) : localtime(t);
}

#endif /* SPATTERLIGHT */

/* Safe strcpy.
 * (Copied from tads2/msdos/osdos.c)
 */
static void
safe_strcpy(char *dst, size_t dstlen, const char *src)
{
    size_t copylen;

    /* do nothing if there's no output buffer */
    if (dst == 0 || dstlen == 0)
        return;

    /* do nothing if the source and destination buffers are the same */
    if (dst == src)
        return;

    /* use an empty string if given a null string */
    if (src == 0)
        src = "";

    /* 
     *   figure the copy length - use the smaller of the actual string size
     *   or the available buffer size, minus one for the null terminator 
     */
    copylen = strlen(src);
    if (copylen > dstlen - 1)
        copylen = dstlen - 1;

    /* copy the string (or as much as we can) */
    memcpy(dst, src, copylen);

    /* null-terminate it */
    dst[copylen] = '\0';
}


/* Open text file for reading and writing.
 */
osfildef*
osfoprwt( const char* fname, os_filetype_t )
{
//  assert(fname != 0);
    // Try opening the file in read/write mode.
    osfildef* fp = fopen(fname, "r+");
    // If opening the file failed, it probably means that it doesn't
    // exist.  In that case, create a new file in read/write mode.
    if (fp == 0) fp = fopen(fname, "w+");
    return fp;
}


/* Open binary file for reading/writing.
 */
osfildef*
osfoprwb( const char* fname, os_filetype_t )
{
//  assert(fname != 0);
    osfildef* fp = fopen(fname, "r+b");
    if (fp == 0) fp = fopen(fname, "w+b");
    return fp;
}

/* Duplicate a file hand.e
 */
osfildef*
osfdup(osfildef *orig, const char *mode)
{
    char realmode[5];
    char *p = realmode;
    const char *m;

    /* verify that there aren't any unrecognized mode flags */
    for (m = mode ; *m != '\0' ; ++m)
    {
        if (strchr("rw+bst", *m) == 0)
            return 0;
    }

    /* figure the read/write mode - translate r+ and w+ to r+ */
    if ((mode[0] == 'r' || mode[0] == 'w') && mode[1] == '+')
	{
		*p++ = 'r';
		*p++ = '+';
	}
    else if (mode[0] == 'r')
        *p++ = 'r';
    else if (mode[0] == 'w')
        *p++ = 'w';
    else
        return 0;

    /* end the mode string */
    *p = '\0';

    /* duplicate the handle in the given mode */
    int fd = dup(fileno(orig));
    if (fd < 0)
        return 0;
    return fdopen(fd, mode);
}


/* Convert string to all-lowercase.
 */
char*
os_strlwr( char* s )
{
//  assert(s != 0);
    for (int i = 0; s[i] != '\0'; ++i) {
        s[i] = tolower((unsigned char)s[i]);
    }
    return s;
}


/* Seek to the resource file embedded in the current executable file.
 *
 * We don't support this (and probably never will).
 */
osfildef*
os_exeseek( const char*, const char* )
{
    return static_cast<osfildef*>(0);
}


/* Get the temporary file path.
 *
 * tads2/osnoui.c defines a DOS version of this routine when MSDOS is
 * defined.
 */
#ifndef MSDOS
void
os_get_tmp_path( char* buf )
{
#ifdef _WIN32
    GetTempPath(OSFNMAX, buf);
#else
    // Try the usual env. variable first.
    const char* tmpDir = getenv("TMPDIR");

    // If no such variable exists, try P_tmpdir (if defined in
    // <stdio.h>).
#ifdef P_tmpdir
    if (tmpDir == 0 or tmpDir[0] == '\0') {
        tmpDir = P_tmpdir;
    }
#endif

    // If we still lack a path, use "/tmp".
    if (tmpDir == 0 or tmpDir[0] == '\0') {
        tmpDir = "/tmp";
    }

    // If the directory doesn't exist or is not a directory, don't
    // provide anything at all (which means that the current
    // directory will be used).
    struct stat inf;
    int statRet = stat(tmpDir, &inf);
    if (statRet != 0 or (inf.st_mode & S_IFMT) != S_IFDIR) {
        buf[0] = '\0';
        return;
    }

    strncpy(buf, tmpDir, OSFNMAX);

    // Append a slash if necessary.
    size_t len = strlen(buf);
    if (buf[len - 1] != '/') {
        buf[len] = '/';
        buf[len + 1] = '\0';
    }
#endif
}
#endif // not MSDOS


/* Create a directory.
 */
int
os_mkdir( const char* dir, int create_parents )
{
    //assert(dir != 0);

    if (dir[0] == '\0')
        return true;

    // Copy the directory name to a new string so we can strip any trailing
    // path seperators.
    size_t len = strlen(dir);
    char* tmp = new char[len + 1];
    strncpy(tmp, dir, len);
    while (tmp[len - 1] == OSPATHCHAR)
        --len;
    tmp[len] = '\0';

    // If we're creating intermediate diretories, and the path contains
    // multiple elements, recursively create the parent directories first.
    if (create_parents and strchr(tmp, OSPATHCHAR) != 0) {
        char par[OSFNMAX];

        // Extract the parent path.
        os_get_path_name(par, sizeof(par), tmp);

        // If the parent doesn't already exist, create it recursively.
        if (osfacc(par) != 0 and not os_mkdir(par, true)) {
            delete[] tmp;
            return false;
        }
    }

    // Create the directory.
    int ret =
#if HAVE_MKDIR
#   if MKDIR_TAKES_ONE_ARG
             mkdir(tmp);
#   else
             mkdir(tmp, S_IRWXU | S_IRWXG | S_IRWXO);
#   endif
#elif HAVE__MKDIR
             _mkdir(tmp);
#else
#   error "Neither mkdir() nor _mkdir() is available on this system."
#endif
    delete[] tmp;
    return ret == 0;
}

/* Remove a directory.
 */
int
os_rmdir( const char *dir )
{
    return rmdir(dir) == 0;
}

/* Get a file's mode/type.  This returns the same information as
 * the 'mode' member of os_file_stat_t from os_file_stat(), so we
 * simply call that routine and return the value.
 */
int
osfmode( const char *fname, int follow_links, unsigned long *mode,
         unsigned long* attr )
{
    os_file_stat_t s;
    int ok;
    if ((ok = os_file_stat(fname, follow_links, &s)) != false) {
        if (mode != NULL)
            *mode = s.mode;
        if (attr != NULL)
            *attr = s.attrs;
    }
    return ok;
}

/* Get full stat() information on a file.
 *
 * TODO: Windows implementation for mingw.
 */
int
os_file_stat( const char *fname, int follow_links, os_file_stat_t *s )
{
    struct stat buf;
    if ((follow_links ? stat(fname, &buf) : lstat(fname, &buf)) != 0)
        return false;

    s->sizelo = (uint32_t)(buf.st_size & 0xFFFFFFFF);
    s->sizehi = sizeof(buf.st_size) > 4
                ? (uint32_t)((buf.st_size >> 32) & 0xFFFFFFFF)
                : 0;
    s->cre_time = buf.st_ctime;
    s->mod_time = buf.st_mtime;
    s->acc_time = buf.st_atime;
    s->mode = buf.st_mode;
    s->attrs = 0;

    if (os_get_root_name(fname)[0] == '.') {
        s->attrs |= OSFATTR_HIDDEN;
    }

    /* XXX
     * This needs to be implemented for MinGW.
     */
#ifndef _WIN32
    // If we're the owner, check if we have read/write access.
    if (geteuid() == buf.st_uid) {
        if (buf.st_mode & S_IRUSR)
            s->attrs |= OSFATTR_READ;
        if (buf.st_mode & S_IWUSR)
            s->attrs |= OSFATTR_WRITE;
        return true;
    }

    // Check if one of our groups matches the file's group and if so, check
    // for read/write access.

    // Also reserve a spot for the effective group ID, which might
    // not be included in the list in our next call.
    int grpSize = getgroups(0, NULL) + 1;
    // Paranoia.
    if (grpSize > NGROUPS_MAX or grpSize < 0)
        return false;
    gid_t* groups = new gid_t[grpSize];
    if (getgroups(grpSize - 1, groups + 1) < 0) {
        delete[] groups;
        return false;
    }
    groups[0] = getegid();
    int i;
    for (i = 0; i < grpSize and buf.st_gid != groups[i]; ++i)
        ;
    delete[] groups;
    if (i < grpSize) {
        if (buf.st_mode & S_IRGRP)
            s->attrs |= OSFATTR_READ;
        if (buf.st_mode & S_IWGRP)
            s->attrs |= OSFATTR_WRITE;
        return true;
    }

    // We're neither the owner of the file nor do we belong to its
    // group.  Check whether the file is world readable/writable.
    if (buf.st_mode & S_IROTH)
        s->attrs |= OSFATTR_READ;
    if (buf.st_mode & S_IWOTH)
        s->attrs |= OSFATTR_WRITE;
#endif
    return true;
}

/* Manually resolve a symbolic link.
 */
int
os_resolve_symlink( const char *fname, char *target, size_t target_size )
{
#ifdef _WIN32
    return false;
#else
    // get the stat() information for the *undereferenced* link; if
    // it's not actually a link, there's nothing to resolve
    struct stat buf;
    if (lstat(fname, &buf) != 0 or (buf.st_mode & S_IFLNK) == 0)
        return false;

    // read the link contents (maxing out at the buffer size)
    size_t copylen = (size_t)buf.st_size;
    if (copylen > target_size - 1)
        copylen = target_size - 1;
    if (readlink(fname, target, copylen) < 0)
        return false;

    // null-terminate the result and return success
    target[copylen] = '\0';
    return true;
#endif
}


/* Get the current system high-precision timer.
 *
 * We provide four (4) implementations of this function:
 *
 *   1. clock_gettime() is available
 *   2. gettimeofday() is available
 *   3. ftime() is available
 *   4. Neither is available - fall back to time()
 *
 * Note that HAVE_CLOCK_GETTIME, HAVE_GETTIMEOFDAY and HAVE_FTIME are
 * mutually exclusive; if one of them is defined, the others aren't.  No
 * need for #else here.
 *
 * Although not required by the TADS VM, these implementations will
 * always return 0 on the first call.
 */
#ifdef HAVE_CLOCK_GETTIME
// The system has the clock_gettime() function.
long
os_get_sys_clock_ms( void )
{
    // We need to remember the exact time this function has been
    // called for the first time, and use that time as our
    // zero-point.  On each call, we simply return the difference
    // in milliseconds between the current time and our zero point.
    static struct timespec zeroPoint;

    // Did we get the zero point yet?
    static bool initialized = false;

    // Not all systems provide a monotonic clock; check if it's
    // available before falling back to the global system-clock.  A
    // monotonic clock is guaranteed not to change while the system
    // is running, so we prefer it over the global clock.
    static const clockid_t clockType =
#ifdef HAVE_CLOCK_MONOTONIC
        CLOCK_MONOTONIC;
#else
        CLOCK_REALTIME;
#endif
    // We must get the current time in each call.
    struct timespec currTime;

    // Initialize our zero-point, if not already done so.
    if (not initialized) {
        clock_gettime(clockType, &zeroPoint);
        initialized = true;
    }

    // Get the current time.
    clock_gettime(clockType, &currTime);

    // Note that tv_nsec contains *nano*seconds, not milliseconds,
    // so we need to convert it; a millisec is 1.000.000 nanosecs.
    return (currTime.tv_sec - zeroPoint.tv_sec) * 1000
         + (currTime.tv_nsec - zeroPoint.tv_nsec) / 1000000;
}

/* Get the time since the Unix Epoch in seconds and nanoseconds.
 */
void
os_time_ns( os_time_t *seconds, long *nanoseconds )
{
    // Get the current time.
    static const clockid_t clockType = CLOCK_REALTIME;
    struct timespec currTime;
    clock_gettime(clockType, &currTime);

    // return the data
    *seconds = currTime.tv_sec;
    *nanoseconds = currTime.tv_nsec;
}
#endif // HAVE_CLOCK_GETTIME

#ifdef HAVE_GETTIMEOFDAY
// The system has the gettimeofday() function.
long
os_get_sys_clock_ms( void )
{
    static struct timeval zeroPoint;
    static bool initialized = false;
    // gettimeofday() needs the timezone as a second argument.  This
    // is obsolete today, but we don't care anyway; we just pass
    // something.
    struct timezone bogus = {0, 0};
    struct timeval currTime;

#ifdef SPATTERLIGHT
    if (osglk_vclock())
        return osglk_vclock_ms();
#endif

    if (not initialized) {
        gettimeofday(&zeroPoint, &bogus);
        initialized = true;
    }

    gettimeofday(&currTime, &bogus);

    // 'tv_usec' contains *micro*seconds, not milliseconds.  A
    // millisec is 1.000 microsecs.
    return (currTime.tv_sec - zeroPoint.tv_sec) * 1000 + (currTime.tv_usec - zeroPoint.tv_usec) / 1000;
}

/* Get the time since the Unix Epoch in seconds and nanoseconds.
 */
void
os_time_ns( os_time_t *seconds, long *nanoseconds )
{
#ifdef SPATTERLIGHT
    if (osglk_vclock()) {
        osglk_vclock_time_ns(seconds, nanoseconds);
        return;
    }
#endif

    // get the time
    struct timezone bogus = {0, 0};
    struct timeval currTime;
    gettimeofday(&currTime, &bogus);

    // return the data, converting milliseconds to nanoseconds
    *seconds = currTime.tv_sec;
    *nanoseconds = currTime.tv_usec * 1000;
}
#endif // HAVE_GETTIMEOFDAY


#ifdef HAVE_FTIME
// The system has the ftime() function.  ftime() is in <sys/timeb.h>.
#include <sys/timeb.h>
long
os_get_sys_clock_ms( void )
{
    static timeb zeroPoint;
    static bool initialized = false;
    struct timeb currTime;

    if (not initialized) {
        ftime(&zeroPoint);
        initialized = true;
    }

    ftime(&currTime);
    return (currTime.time - zeroPoint.time) * 1000 + (currTime.millitm - zeroPoint.millitm);
}

/* Get the time since the Unix Epoch in seconds and nanoseconds.
 */
void
os_time_ns( os_time_t *seconds, long *nanoseconds )
{
    // get the time
    struct timeb currTime;
    ftime(&currTime);

    // return the data, converting milliseconds to nanoseconds
    *seconds = currTime.time;
    *nanoseconds = (long)currTime.millitm * 1000000;
}
#endif // HAVE_FTIME


#ifndef HAVE_CLOCK_GETTIME
#ifndef HAVE_GETTIMEOFDAY
#ifndef HAVE_FTIME
// Use the standard time() function from the C library.  We lose
// millisecond-precision, but that's the best we can do with time().
long
os_get_sys_clock_ms( void )
{
    static time_t zeroPoint = time(0);
    return (time(0) - zeroPoint) * 1000;
}

void
os_time_ns( os_time_t *seconds, long *nanoseconds )
{
    // get the regular time() value in seconds; we have no
    // higher precision element to return, so return zero
    *seconds = time(0);
    *nanoseconds = 0;
}
#endif
#endif
#endif


/* Get filename from startup parameter, if possible.
 *
 * TODO: Find out what this is supposed to do.
 */
int
os_paramfile( char* )
{
    return 0;
}


/* Open a directory search.
 */
int os_open_dir(const char *dirname, osdirhdl_t *hdl)
{
    return (*hdl = opendir(dirname)) != NULL;
}

/* Read the next result in a directory search.
 */
int os_read_dir(osdirhdl_t hdl, char *buf, size_t buflen)
{
    // Read the next directory entry - if we've exhausted the search,
    // return failure.
    struct dirent *d = readdir(hdl);
    if (d == 0)
        return false;

    // return this entry
    safe_strcpy(buf, buflen, d->d_name);
    return true;
}

/* Close a directory search.
 */
void os_close_dir(osdirhdl_t hdl)
{
    closedir(hdl);
}


/* Determine if the given filename refers to a special file.
 *
 * tads2/osnoui.c defines its own version when MSDOS is defined.
 */
#ifndef MSDOS
os_specfile_t
os_is_special_file( const char* fname )
{
    // We also check for "./" and "../" instead of just "." and
    // "..".  (We use OSPATHCHAR instead of '/' though.)
    const char selfWithSep[3] = {'.', OSPATHCHAR, '\0'};
    const char parentWithSep[4] = {'.', '.', OSPATHCHAR, '\0'};
    if ((strcmp(fname, ".") == 0) or (strcmp(fname, selfWithSep) == 0)) return OS_SPECFILE_SELF;
    if ((strcmp(fname, "..") == 0) or (strcmp(fname, parentWithSep) == 0)) return OS_SPECFILE_PARENT;
    return OS_SPECFILE_NONE;
}
#endif


extern "C" void canonicalize_path(char *path);

#ifdef _WIN32
/* This file only ever calls realpath() with resolved_name as NULL. */
static char *realpath(const char *file_name, char *resolved_name)
{
    resolved_name = (char *)malloc(PATH_MAX + 1);
    if (resolved_name == NULL)
        return NULL;

    GetFullPathName(file_name, PATH_MAX + 1, resolved_name, NULL);

    return resolved_name;
}
#endif


/* Resolve symbolic links in a path.  It's okay for 'buf' and 'path'
 * to point to the same buffer if you wish to resolve a path in place.
 */
static void
resolve_path( char *buf, size_t buflen, const char *path )
{
    // Starting with the full path string, try resolving the path with
    // realpath().  The tricky bit is that realpath() will fail if any
    // component of the path doesn't exist, but we need to resolve paths
    // for prospective filenames, such as files or directories we're
    // about to create.  So if realpath() fails, remove the last path
    // component and try again with the remainder.  Repeat until we
    // can resolve a real path, or run out of components to remove.
    // The point of this algorithm is that it will resolve as much of
    // the path as actually exists in the file system, ensuring that
    // we resolve any links that affect the path.  Any portion of the
    // path that doesn't exist obviously can't refer to a link, so it
    // will be taken literally.  Once we've resolved the longest prefix,
    // tack the stripped portion back on to form the fully resolved
    // path.

    // make a writable copy of the path to work with
    size_t pathl = strlen(path);
    char *mypath = new char[pathl + 1];
    memcpy(mypath, path, pathl + 1);

    // start at the very end of the path, with no stripped suffix yet
    char *suffix = mypath + pathl;
    char sl = '\0';

    // keep going until we resolve something or run out of path
    for (;;)
    {
        // resolve the current prefix, allocating the result
        char *rpath = realpath(mypath, 0);

        // un-split the path
        *suffix = sl;

        // if we resolved the prefix, return the result
        if (rpath != 0)
        {
            // success - if we separated a suffix, reattach it
            if (*suffix != '\0')
            {
                // reattach the suffix (the part after the '/')
                for ( ; *suffix == '/' ; ++suffix) ;
                os_build_full_path(buf, buflen, rpath, suffix);
            }
            else
            {
                // no suffix, so we resolved the entire path
                safe_strcpy(buf, buflen, rpath);
            }

            // done with the resolved path
            free(rpath);

            // ...and done searching
            break;
        }

        // no luck with realpath(); search for the '/' at the end of the
        // previous component in the path 
        for ( ; suffix > mypath && *(suffix-1) != '/' ; --suffix) ;

        // skip any redundant slashes
        for ( ; suffix > mypath && *(suffix-1) == '/' ; --suffix) ;

        // if we're at the root element, we're out of path elements
        if (suffix == mypath)
        {
            // we can't resolve any part of the path, so just return the
            // original path unchanged
            safe_strcpy(buf, buflen, mypath);
            break;
        }

        // split the path here into prefix and suffix, and try again
        sl = *suffix;
        *suffix = '\0';
    }

    // done with our writable copy of the path
    delete [] mypath;
}

/* Is the given file in the given directory?
 */
int
os_is_file_in_dir( const char* filename, const char* path,
                   int allow_subdirs, int match_self )
{
    char filename_buf[OSFNMAX], path_buf[OSFNMAX];
    size_t flen, plen;

    // Absolute-ize the filename, if necessary.
    if (not os_is_file_absolute(filename)) {
        os_get_abs_filename(filename_buf, sizeof(filename_buf), filename);
        filename = filename_buf;
    }

    // Absolute-ize the path, if necessary.
    if (not os_is_file_absolute(path)) {
        os_get_abs_filename(path_buf, sizeof(path_buf), path);
        path = path_buf;
    }

    // Canonicalize the paths, to remove .. and . elements - this will make
    // it possible to directly compare the path strings.  Also resolve it
    // to the extent possible, to make sure we're not fooled by symbolic
    // links.
    safe_strcpy(filename_buf, sizeof(filename_buf), filename);
    canonicalize_path(filename_buf);
    resolve_path(filename_buf, sizeof(filename_buf), filename_buf);
    filename = filename_buf;

    safe_strcpy(path_buf, sizeof(path_buf), path);
    canonicalize_path(path_buf);
    resolve_path(path_buf, sizeof(path_buf), path_buf);
    path = path_buf;

    // Get the length of the filename and the length of the path.
    flen = strlen(filename);
    plen = strlen(path);

    // If the path ends in a separator character, ignore that.
    if (plen > 0 and path[plen-1] == '/')
        --plen;

    // if the names match, return true if and only if we're matching the
    // directory to itself
    if (plen == flen && memcmp(filename, path, flen) == 0)
        return match_self;

    // Check that the filename has 'path' as its path prefix.  First, check
    // that the leading substring of the filename matches 'path', ignoring
    // case.  Note that we need the filename to be at least two characters
    // longer than the path: it must have a path separator after the path
    // name, and at least one character for a filename past that.
    if (flen < plen + 2 or memcmp(filename, path, plen) != 0)
        return false;

    // Okay, 'path' is the leading substring of 'filename'; next make sure
    // that this prefix actually ends at a path separator character in the
    // filename.  (This is necessary so that we don't confuse "c:\a\b.txt"
    // as matching "c:\abc\d.txt" - if we only matched the "c:\a" prefix,
    // we'd miss the fact that the file is actually in directory "c:\abc",
    // not "c:\a".)
    if (filename[plen] != '/')
        return false;

    // We're good on the path prefix - we definitely have a file that's
    // within the 'path' directory or one of its subdirectories.  If we're
    // allowed to match on subdirectories, we already have our answer
    // (true).  If we're not allowed to match subdirectories, we still have
    // one more check, which is that the rest of the filename is free of
    // path separator charactres.  If it is, we have a file that's directly
    // in the 'path' directory; otherwise it's in a subdirectory of 'path'
    // and thus isn't a match.
    if (allow_subdirs) {
        // Filename is in the 'path' directory or one of its
        // subdirectories, and we're allowed to match on subdirectories, so
        // we have a match.
        return true;
    }

    // We're not allowed to match subdirectories, so scan the rest of
    // the filename for path separators.  If we find any, the file is
    // in a subdirectory of 'path' rather than directly in 'path'
    // itself, so it's not a match.  If we don't find any separators,
    // we have a file directly in 'path', so it's a match.
    const char* p;
    for (p = filename; *p != '\0' and *p != '/' ; ++p)
        ;

    // If we reached the end of the string without finding a path
    // separator character, it's a match .
    return *p == '\0';
}


/* Get the absolute path for a given filename.
 */
int
os_get_abs_filename( char* buf, size_t buflen, const char* filename )
{
    // If the filename is already absolute, copy it; otherwise combine
    // it with the working directory.
    if (os_is_file_absolute(filename))
    {
        // absolute - copy it as-is
        safe_strcpy(buf, buflen, filename);
    }
    else
    {
        // combine it with the working directory to get the path
        char pwd[OSFNMAX];
        if (getcwd(pwd, sizeof(pwd)) != 0)
            os_build_full_path(buf, buflen, pwd, filename);
        else
            safe_strcpy(buf, buflen, filename);
    }

    // canonicalize the result
    canonicalize_path(buf);

    // Try getting the canonical path from the OS (allocating the
    // result buffer).
    char* newpath = realpath(filename, 0);
    if (newpath != 0) {
        // Copy the output (truncating if it's too long).
        safe_strcpy(buf, buflen, newpath);
        free(newpath);
        return true;
    }

    // realpath() failed, but that's okay - realpath() only works if the
    // path refers to an existing file, but it's valid for callers to
    // pass non-existent filenames, such as names of files they're about
    // to create, or hypothetical paths being used for comparison
    // purposes or for future use.  Simply return the canonical path
    // name we generated above.
    return true;
}


/* ------------------------------------------------------------------------ */
/*
 * Get the file system roots.  Unix has the lovely unified namespace with
 * just the one root, /, so this is quite simple.
 */
size_t os_get_root_dirs(char *buf, size_t buflen)
{
    static const char ret[] = { '/', 0, 0 };
    
    // if there's room, copy the root string "/" and an extra null
    // terminator for the overall list
    if (buflen >= sizeof(ret))
        memcpy(buf, ret, sizeof(ret));

    // return the required size
    return sizeof(ret);
}


/* ------------------------------------------------------------------------ */
/* Get a special directory path.
 *
 * If env. variables exist, they always override the compile-time
 * defaults.
 *
 * We ignore the argv parameter, since on Unix binaries aren't stored
 * together with data on disk.
 */
void
os_get_special_path( char* buf, size_t buflen, const char* argv0, int id )
{
    const char* res;

    switch (id) {
      case OS_GSP_T3_RES:
        res = getenv("T3_RESDIR");
        if (res == 0 or res[0] == '\0') {
            res = T3_RES_DIR;
        }
        break;

      case OS_GSP_T3_INC:
        res = getenv("T3_INCDIR");
        if (res == 0 or res[0] == '\0') {
            res = T3_INC_DIR;
        }
        break;

      case OS_GSP_T3_LIB:
        res = getenv("T3_LIBDIR");
        if (res == 0 or res[0] == '\0') {
            res = T3_LIB_DIR;
        }
        break;

      case OS_GSP_T3_USER_LIBS:
        // There's no compile-time default for user libs.
        res = getenv("T3_USERLIBDIR");
        break;

      case OS_GSP_T3_SYSCONFIG:
        res = getenv("T3_CONFIG");
        if (res == 0 and argv0 != 0) {
            os_get_path_name(buf, buflen, argv0);
            return;
        }
        break;

      case OS_GSP_LOGFILE:
        res = getenv("T3_LOGDIR");
        if (res == 0 or res[0] == '\0') {
            res = T3_LOG_FILE;
        }
        break;

      default:
        // TODO: We could print a warning here to inform the
        // user that we're outdated.
        res = 0;
    }

    if (res != 0) {
        // Only use the detected path if it exists and is a
        // directory.
        struct stat inf;
        int statRet = stat(res, &inf);
        if (statRet == 0 and (inf.st_mode & S_IFMT) == S_IFDIR) {
            strncpy(buf, res, buflen - 1);
            return;
        }
    }
    // Indicate failure.
    buf[0] = '\0';
}


/* Receive notification that a character mapping file has been loaded.
 */
void
os_advise_load_charmap( char* /*id*/, char* /*ldesc*/, char* /*sysinfo*/ )
{
}


/* T3 post-load UI initialization.  This is a hook provided by the T3
 * VM to allow the UI layer to do any extra initialization that depends
 * upon the contents of the loaded game file.  We don't currently need
 * any extra initialization here.
 */
#ifdef RUNTIME
void
os_init_ui_after_load( class CVmBifTable*, class CVmMetaTable* )
{
    // No extra initialization required.
}
#endif


/* Get the full filename (including directory path) to the executable
 * file, given the argv[0] parameter passed into the main program.
 *
 * On Unix, you can't tell what the executable's name is by just looking
 * at argv, so we always indicate failure.  No big deal though; this
 * information is only used when the interpreter's executable is bundled
 * with a game, and we don't support this at all.
 */
int
os_get_exe_filename( char*, size_t, const char* )
{
    return false;
}


/* Translate a character from the HTML 4 Unicode character set to the
 * current character set used for display.
 *
 * Note that this function is used only by Tads 2.  Tads 3 does mappings
 * automatically.
 *
 * We omit this implementation when not compiling the interpreter (in
 * which case os_xlat_html4 will have been defined as an empty macro in
 * osfrobtads.h).  We do this because we don't want to introduce any
 * TADS 3 dependencies upon the TADS 2 compiler, which should compile
 * regardless of whether the TADS 3 sources are available or not.
 */
#ifndef os_xlat_html4
void
os_xlat_html4( unsigned int html4_char, char* result, size_t result_buf_len )
{
    // HTML 4 characters are Unicode.  Tads 3 provides just the
    // right mapper: Unicode to ASCII.  We make it static in order
    // not to create a mapper on each call and save CPU cycles.
    static CCharmapToLocalASCII mapper;
    result[mapper.map_char(html4_char, result, result_buf_len)] = '\0';
}
#endif

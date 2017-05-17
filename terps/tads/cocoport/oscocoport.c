/* This file implements some of the functions described in
 * tads2/osifc.h.  We don't need to implement them all, as most of them
 * are provided by tads2/osnoui.c and tads2/osgen3.c.
 *
 * This file implements the "portable" subset of these functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>

#include "os.h"

int
memicmp( const char* s1, const char* s2, size_t len )
{
    int i;
    for (i = 0; i < len; i++)
    {
        if (tolower(s1[i]) != tolower(s2[i]))
            return (int)tolower(s1[i]) - (int)tolower(s2[i]);
    }
    return 0;
}

/* Open text file for reading and writing.
 */
osfildef*
osfoprwt( const char* fname, os_filetype_t x )
{
    //	assert(fname != 0);
    osfildef* fp = fopen(fname, "a+");
    // If the operation was successful, position the seek pointer to
    // the beginning of the file.
    if (fp != 0) rewind(fp);
    return fp;
}


/* Open binary file for reading/writing.
 */
osfildef*
osfoprwb( const char* fname, os_filetype_t x )
{
    //	assert(fname != 0);
    osfildef* fp = fopen(fname, "a+b");
    if (fp != 0) rewind(fp);
    return fp;
}


/* Convert string to all-lowercase.
 */
char*
os_strlwr( char* s )
{
    int i;
    //	assert(s != 0);
    for (i = 0; s[i] != '\0'; ++i) {
        s[i] = tolower(s[i]);
    }
    return s;
}


/* Seek to the resource file embedded in the current executable file.
 *
 * We don't support this (and probably never will).
 */
osfildef*
os_exeseek( const char* a, const char* b )
{
    return 0;
}


/* These implementations of os_create_tempfile() and osfdel_temp() are
 * better than the default ones provided by tads2/osnoui.c, but we can't
 * use them since osnoui enforces its own implementations.  They're
 * "better" because they let the system decide where to put the tempfile
 * and how to name it.  Also, this kind of tempfile is deleted
 * automaticly when closed or when the program terminates normally.  For
 * security reasons, this is the preferred way to deal with tempfiles.
 *
 * tmpfile() is portable, mind you; it's listed in SVID 3, POSIX,
 * BSD 4.3, SUSv2 and, most importantly, ISO 9899.
 */
/* Create and open a temporary file.
 */
/*
osfildef*
os_create_tempfile( const char* fname, char* buf )
{
    if (fname != 0 and fname[0] != '\0') {
        // A filename has been specified; use it.
        return fopen(fname, "w+b");
    }

    //ASSERT(buf != 0);

    // No filename needed; create a nameless tempfile.
    buf[0] = '\0';
    return tmpfile();
}
 */


/* Delete a temporary file created with os_create_tempfile().
 */
/*
int
osfdel_temp( const char* fname )
{
    //ASSERT(fname != 0);

    if (fname[0] == '\0' || remove(fname) == 0) {
        // Either it was a nameless temp-file and has been
        // already deleted by the system, or deleting it
        // succeeded.  In either case, the operation was
        // successful.
        return 0;
    }
    // It was not a nameless tempfile and remove() failed.
    return 1;
}
 */


/* Get the temporary file path.
 *
 * tads2/osnoui.c defines a DOS version of this routine when MSDOS is
 * defined.
 */
#ifndef MSDOS
void
os_get_tmp_path( char* buf )
{
    strcpy(buf, "");
    // Try the usual env. variable first.
    const char* tmpDir = getenv("TMPDIR");

    // If no such variable exists, try P_tmpdir (if defined in
    // <stdio.h>).
#ifdef P_tmpdir
    if (tmpDir == 0 || tmpDir[0] == '\0') {
        tmpDir = P_tmpdir;
    }
#endif

    // If we still lack a path, use "/tmp".
    if (tmpDir == 0 || tmpDir[0] == '\0') {
        tmpDir = "/tmp";
    }

    strcpy(buf, tmpDir);

    // Append a slash if necessary.
    size_t len = strlen(buf);
    if (buf[len - 1] != '/') {
        buf[len] = '/';
        buf[len + 1] = '\0';
    }
}
#endif // not MSDOS


/* Get a suitable seed for a random number generator.
 *
 * We don't just use the system-clock, but build a number as random as
 * the mechanisms of the standard C-library allow.  This is somewhat of
 * an overkill, but it's simple enough so here goes.  Someone has to
 * write a nuclear war simulator in TADS to test this thoroughly.
 */
void
os_rand( long* val )
{
    //assert(val != 0);
    // Use the current time as the first seed.
    srand((unsigned int)(time(0)));

    // Create the final seed by generating a random number using
    // high-order bits, because on some systems the low-order bits
    // aren't very random.
    *val = 1 + (long)((double)(65535) * rand() / (RAND_MAX + 1.0));
}


/* Get the current system high-precision timer.
 *
 * Although not required by the TADS VM, these implementations will
 * always return 0 on the first call.
 */

long
os_get_sys_clock_ms( void )
{
    static struct timeval zeroPoint;
    static int initialized = 0;
    // gettimeofday() needs the timezone as a second argument.  This
    // is obsolete today, but we don't care anyway; we just pass
    // something.
    struct timeval currTime;

    if (!initialized) {
        gettimeofday(&zeroPoint, 0);
        initialized = 1;
    }

    gettimeofday(&currTime, 0);

    // 'tv_usec' contains *micro*seconds, not milliseconds.  A
    // millisec is 1.000 microsecs.
    return (currTime.tv_sec - zeroPoint.tv_sec) * 1000 + (currTime.tv_usec - zeroPoint.tv_usec) / 1000;
}


/* Get filename from startup parameter, if possible.
 *
 * TODO: Find out what this is supposed to do.
 */
int
os_paramfile( char* a )
{
    return 0;
}


/* Get a special directory path.
 *
 * If env. variables exist, they always override the compile-time
 * defaults.
 *
 * We ignore the argv parameter, since on Unix binaries aren't stored
 * together with data on disk.
 */
void
os_get_special_path( char* buf, size_t buflen, const char* a, int id_ )
{
    const char* res;

    switch (id_)
    {
    case OS_GSP_T3_RES:
        res = getenv("T3_RESDIR");
        break;
    case OS_GSP_T3_INC:
        res = getenv("T3_INCDIR");
        break;
    case OS_GSP_T3_LIB:
        res = getenv("T3_LIBDIR");
        break;
    case OS_GSP_T3_USER_LIBS:
        // There's no compile-time default for user libs.
        res = getenv("T3_USERLIBDIR");
        break;
    default:
        // TODO: We could print a warning here to inform the
        // user that we're outdated.
        res = 0;
    }

    // Indicate failure.
    buf[0] = '\0';
}


/* Generate a filename for a character-set mapping file.
 *
 * TODO: Implement it.
 */
void
os_gen_charmap_filename( char* filename, char* internal_id, char* argv0 )
{
    /*ASSERT(filename != 0);*/
    filename[0] = '\0';
}


/* Receive notification that a character mapping file has been loaded.
 */
void
os_advise_load_charmap( char* id_, char* ldesc, char* sysinfo )
{
}


/* Get the full filename (including directory path) to the executable
 * file, given the argv[0] parameter passed into the main program.
 *
 * On Unix, you can't tell what the executable's name is by just looking
 * at argv, so we always indicate failure.  No big deal though; this
 * information is only used when the interpreter's executable is bundled
 * with a game, and we don't support this at all.
 */
int
os_get_exe_filename( char* a, size_t b, const char* c )
{
    return 0;
}


/* Generate the name of the character set mapping table for Unicode
 * characters to and from the given local character set.
 *
 * FIXME: Find out the local character set.  For now, we only support
 * 7-bit ASCII.
 */
void
os_get_charmap( char* mapname, int charmap_id )
{
    switch(charmap_id) {
    case OS_CHARMAP_DISPLAY:
        strcpy(mapname, "asc7dflt");
        break;
    case OS_CHARMAP_FILENAME:
    case OS_CHARMAP_FILECONTENTS:
        strcpy(mapname, "asc7dflt");
        break;
    default:
        strcpy(mapname, "asc7dflt");
        break;
    }
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
#ifdef NEVER
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


/* =====================================================================

 Yield CPU.
 *
 * I don't think that we need this.  It's only useful for Windows 3.x
 * and maybe pre-X Mac OS and other systems with brain damaged
 * multitasking.
 */
int
os_yield( void )
{
    return 0;
}


/* Sleep for a while.
 *
 * The compiler doesn't need this.
 */
void
os_sleep_ms( long delay_in_milliseconds )
{
}

/*
 *   Install/uninstall the break handler.  If possible, the OS code should
 *   set (if 'install' is true) or clear (if 'install' is false) a signal
 *   handler for keyboard break signals (control-C, etc, depending on
 *   local convention).  The OS code should set its own handler routine,
 *   which should note that a break occurred with an internal flag; the
 *   portable code uses os_break() from time to time to poll this flag.
 */
void os_instbrk(int install)
{
}

/*
 *   Set the saved-game extension.  Most platforms don't need to do
 *   anything with this information, and in fact most platforms won't even
 *   have a way of letting the game author set the saved game extension,
 *   so this trivial implementation is suitable for most systems.
 *   
 *   The purpose of setting a saved game extension is to support platforms
 *   (such as Windows) where the filename suffix is used to associate
 *   document files with applications.  Each stand-alone executable
 *   generated on such platforms must have a unique saved game extension,
 *   so that the system can associate each game's saved position files
 *   with that game's executable.  
 */
void os_set_save_ext(const char *ext)
{
    /* ignore the setting */
}


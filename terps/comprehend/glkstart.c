/* glkstart.c -- entry point for the Comprehend interpreter on Spatterlight.
 * Captures the storyfile from glkunix_arguments; the C++ glk_main()
 * picks it up via comprehend_storyfile.
 */

#include "glk.h"
#include "glkstart.h"

const char *comprehend_storyfile = NULL;
const char *comprehend_gameid = NULL;

/* Spatterlight sets this from its preferences. Elsewhere it stays 0, and
   pictures are drawn in one go, unless the -s option is given. */
extern int gli_slowdraw;

/* Which picture modes to start in: 0 = more colours (double hi-res on Apple
   II, PCjr on DOS), 1 = fewer. A preference in Spatterlight; elsewhere it is
   1 unless the -c option is given. */
extern int gli_comprehend_graphics;

glkunix_argumentlist_t glkunix_arguments[] = {
    { "-g", glkunix_arg_ValueFollows, "-g gameid: One of transylvania, crimsoncrown, ootopos, talisman, transylvaniav2, covetedmirror (default: transylvania)" },
#ifndef SPATTERLIGHT
    { "-s", glkunix_arg_NoValue, "-s: Draw pictures gradually, as the original games do" },
    { "-c", glkunix_arg_NoValue, "-c: Use the picture modes with more colours where a game has them: double hi-res on Apple II, PCjr on DOS" },
#endif
    { "", glkunix_arg_ValueFollows, "filename: The game data file (tr.gda / cc1.gda / g0)." },
    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    /* Give glkimp our identity so save/work/autosave files land in
     * "Comprehend Files" rather than the "(null) Files" fallback that a
     * default "Unknown" program name produces (see fileref.m gFolderMap).
     * CheapGlk (headless) has no garglk extensions, so skip it there. */
#ifndef COMPREHEND_HEADLESS
    garglk_set_program_name("Comprehend");
#endif

    for (int i = 1; i < data->argc; ++i) {
        const char *a = data->argv[i];
        if (a[0] == '-' && a[1] == 'g' && a[2] == '\0' && i + 1 < data->argc) {
            comprehend_gameid = data->argv[++i];
#ifndef SPATTERLIGHT
        } else if (a[0] == '-' && a[1] == 's' && a[2] == '\0') {
            gli_slowdraw = 1;
        } else if (a[0] == '-' && a[1] == 'c' && a[2] == '\0') {
            gli_comprehend_graphics = 0;
#endif
        } else if (a[0] != '-') {
            comprehend_storyfile = a;
        }
    }
    return 1;
}

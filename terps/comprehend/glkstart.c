/* glkstart.c -- entry point for the Comprehend interpreter on Spatterlight.
 * Captures the storyfile from glkunix_arguments; the C++ glk_main()
 * picks it up via comprehend_storyfile.
 */

#include "glk.h"
#include "glkstart.h"

const char *comprehend_storyfile = NULL;
const char *comprehend_gameid = NULL;

glkunix_argumentlist_t glkunix_arguments[] = {
    { "-g", glkunix_arg_ValueFollows, "-g gameid: One of transylvania, crimsoncrown, ootopos, talisman, transylvaniav2, covetedmirror (default: transylvania)" },
    { "", glkunix_arg_ValueFollows, "filename: The game data file (tr.gda / cc1.gda / g0)." },
    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    for (int i = 1; i < data->argc; ++i) {
        const char *a = data->argv[i];
        if (a[0] == '-' && a[1] == 'g' && a[2] == '\0' && i + 1 < data->argc) {
            comprehend_gameid = data->argv[++i];
        } else if (a[0] != '-') {
            comprehend_storyfile = a;
        }
    }
    return 1;
}

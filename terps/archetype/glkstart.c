/* glkstart.c -- entry point for the Archetype interpreter on Spatterlight.
 * Mirrors the glkstart.c found in other Spatterlight terps (e.g. advsys/glkstart.c).
 * Stores the game filename for archetype_main() to pick up; the actual Glk
 * setup happens there because that function is C++.
 */

#include "glk.h"
#include "glkstart.h"

const char *archetype_storyfile = NULL;

glkunix_argumentlist_t glkunix_arguments[] = {
    { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    if (data->argc == 2)
        archetype_storyfile = data->argv[1];
    return 1;
}

/* Stubs for the Spatterlight front-end symbols the alan2 sources reference
   when built with -DSPATTERLIGHT, in the style of
   terps/scott/test/headless_stubs.c. In the real app these live in glkimp;
   the headless CheapGlk build has no glkimp, so define them here.

   gli_enable_graphics stays on so a scripted run still exercises the
   PCX/ILBM-to-BMP conversion in glkmedia.c; the drawing itself is inert
   because CheapGlk refuses graphics windows and reports images as
   unavailable. Registration only hands out a number, and CheapGlk hands
   out no sound channels. */

#include "glk.h"

int gli_enable_graphics = 1;
int gli_enable_sound = 1;

/* Hands out numbers as glkimp would, without the app to load into. */
glui32 gli_add_resource_from_path(glui32 usage, const char *path,
                                  glui32 offset, glui32 length)
{
    static glui32 next = 0x40000000u;
    (void)usage; (void)path; (void)offset; (void)length;
    return next++;
}

/* Gargoyle extensions called from glkstart.c, absent from CheapGlk */
void garglk_set_program_name(const char *name)
{
    (void)name;
}

void garglk_set_program_info(const char *info)
{
    (void)info;
}

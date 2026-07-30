/* Stubs for the Spatterlight front-end symbols the alan2 sources reference
   when built with -DSPATTERLIGHT, in the style of
   terps/scott/test/headless_stubs.c. In the real app these live in glkimp;
   the headless CheapGlk build has no glkimp, so define them here.

   gli_enable_graphics stays on so a scripted run still exercises the
   PCX/ILBM-to-BMP conversion in glkmedia.c; the drawing itself is inert
   because CheapGlk refuses graphics windows and reports images as
   unavailable. Sound registration is a no-op and CheapGlk hands out no
   sound channels. */

#include "glk.h"

int gli_enable_graphics = 1;
int gli_enable_sound = 1;

void win_loadimage(int resno, const char *filename, int offset, int reslen)
{
    (void)resno; (void)filename; (void)offset; (void)reslen;
}

void win_loadsound(int resno, char *filename, int offset, int reslen)
{
    (void)resno; (void)filename; (void)offset; (void)reslen;
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

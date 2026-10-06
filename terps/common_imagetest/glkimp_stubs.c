/* Definitions of the Spatterlight front-end (glkimp) globals and helpers that
   the interpreter sources reference, for the headless builds that link
   CheapGlk instead of glkimp. Not built on its own: it is #included, after the
   knobs below, by
     - common_imagetest/image_glk.c        (Plus / TaylorMade image probes)
     - scott/test/headless_stubs.c         (scott_hl and its test tools)
     - scott/saga/test/extract_stubs.c     (the SAGA image dumpers, titest,
                                            scenetest)
   so that each build keeps a single stub translation unit.

   Knobs (the values that differ between those builds):
     GLKIMP_ENABLE_GRAPHICS  gli_enable_graphics              (default 1)
     GLKIMP_GBGCOL           gbgcol                           (default 0xffffff)
     GLKIMP_SCREENHEIGHT     gli_screenheight                 (default 24)
     GLKIMP_UTF8             gli_utf8output and gli_utf8input (default 0)
     GLKIMP_NO_CHEAPGLK_MAIN set when the link keeps CheapGlk's main.o, which
                             then defines gli_screenwidth/height, gli_utf8*,
                             gli_debugger and gli_get_dataresource_info.

   gli_slowdraw and gli_determinism are CheapGlk's (cgmisc.c) in every build.
   The declarations are in glkimp.h (this folder) or, for the SAGA test
   harnesses, Spatterlight's own glkimp.h. */

#include <stdint.h>

#include "glk.h"

#ifndef GLKIMP_ENABLE_GRAPHICS
#define GLKIMP_ENABLE_GRAPHICS 1
#endif
#ifndef GLKIMP_GBGCOL
#define GLKIMP_GBGCOL 0xffffff
#endif
#ifndef GLKIMP_SCREENHEIGHT
#define GLKIMP_SCREENHEIGHT 24
#endif
#ifndef GLKIMP_UTF8
#define GLKIMP_UTF8 0
#endif

/* User settings, normally from the Spatterlight preferences: no delays or
   flicker, and the "nothing forced" value (0) for inventory and palette. */
int gli_enable_graphics = GLKIMP_ENABLE_GRAPHICS;
int gli_sa_delays = 0;
int gli_flicker = 0;
int gli_sa_inventory = 0;
int gli_sa_palette = 0;
int gli_utf = 1;

/* Foreground/background colours, normally from the Spatterlight theme. */
uint32_t gfgcol = 0x000000;
uint32_t gbgcol = GLKIMP_GBGCOL;

#ifndef GLKIMP_NO_CHEAPGLK_MAIN
/* What CheapGlk's main.c would define; these builds have their own main(). */
int gli_screenwidth = 80;
int gli_screenheight = GLKIMP_SCREENHEIGHT;
int gli_utf8output = GLKIMP_UTF8;
int gli_utf8input = GLKIMP_UTF8;
int gli_debugger = 0;

int gli_get_dataresource_info(int num, void **ptr, glui32 *len, int *isbinary)
{
    (void)num; (void)ptr; (void)len; (void)isbinary;
    return 0;
}
#endif

/* Only used by the -z self-test entry point, which no headless build takes. */
void win_testresult(int result)
{
    (void)result;
}

/* The ZX Spectrum beeper (TaylorMade); silent here. */
void win_beep_zx(int pitch, int cycles)
{
    (void)pitch; (void)cycles;
}

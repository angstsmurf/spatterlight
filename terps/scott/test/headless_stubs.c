/* Stubs for the Spatterlight front-end symbols that the ScottFree sources
   reference outside of #ifdef SPATTERLIGHT guards. In the real app these live
   in glkimp; the headless CheapGlk build has no glkimp, so define them here
   (common_imagetest/glkimp_stubs.c, shared with the image probes and the SAGA
   image dumpers).

   Deliberately absent (provided elsewhere in the headless link):
     - gli_slowdraw, gli_determinism        -> CheapGlk (cgmisc.c)
     - gli_screenwidth/height, gli_debugger,
       gli_get_dataresource_info            -> CheapGlk (main.c)
     - FindGlkWindowWithRock                -> common_utils (common_utils.c) */

#include <stdlib.h>

/* Graphics are unavailable under CheapGlk (it refuses wintype_Graphics), so the
   drawing paths are inert; a white background. */
#define GLKIMP_ENABLE_GRAPHICS 0
#define GLKIMP_GBGCOL 0xffffff
#define GLKIMP_NO_CHEAPGLK_MAIN
#include "../../common_imagetest/glkimp_stubs.c"

/* Force the engine down its determinism path (fixed RNG seed) before glk_main
   reads the flag. Without this the headless build seeds the RNG from the clock,
   as the app does with the Determinism theme option off, and a game with random
   events (Adventureland's chiggers, the bees) yields a different transcript on
   every run -- which makes before/after diffing useless. CheapGlk owns the
   definition (cgmisc.c); we only flip it. */
extern int gli_determinism;
/* Key presses the way the app's command scripts type them (cgmisc.c), so that
   a "hit enter" pause does not swallow the command after it. Only the command
   script tests ask for it (test/scripts/run_script_tests.py): the goldens of
   "make check" were made without, and have no echoed commands in them. */
extern int gli_script_keys;

__attribute__((constructor)) static void headless_force_determinism(void)
{
    gli_determinism = 1;
    gli_script_keys = getenv("SCOTT_SCRIPT_KEYS") != NULL;
}

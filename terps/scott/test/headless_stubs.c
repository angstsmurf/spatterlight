/* Stubs for the Spatterlight front-end symbols that the ScottFree sources
   reference outside of #ifdef SPATTERLIGHT guards. In the real app these live
   in glkimp; the headless CheapGlk build has no glkimp, so define them here.

   Deliberately absent (provided elsewhere in the headless link):
     - gli_slowdraw, gli_determinism        -> CheapGlk (cgmisc.c)
     - gli_screenwidth/height, gli_debugger,
       gli_get_dataresource_info            -> CheapGlk (main.c)
     - FindGlkWindowWithRock                -> common_utils (common_utils.c) */

#include <stdint.h>

#include "glk.h"

/* Graphics are unavailable under CheapGlk (it refuses wintype_Graphics), so the
   drawing paths are inert. Delays and flicker are off, and the "nothing forced"
   settings (0) keep the engine on its default inventory/palette behaviour. */
int gli_enable_graphics = 0;
int gli_sa_delays = 0;
int gli_flicker = 0;
int gli_sa_inventory = 0;
int gli_sa_palette = 0;
int gli_utf = 1;

/* Foreground/background colours, normally taken from the Spatterlight theme. */
uint32_t gfgcol = 0x000000;
uint32_t gbgcol = 0xffffff;

/* Only used by the -z self-test entry point, which the harness never takes. */
void win_testresult(int result)
{
    (void)result;
}

/* Force the engine down its determinism path (fixed RNG seed) before glk_main
   reads the flag. Without this the headless build seeds the RNG from the clock,
   as the app does with the Determinism theme option off, and a game with random
   events (Adventureland's chiggers, the bees) yields a different transcript on
   every run -- which makes before/after diffing useless. CheapGlk owns the
   definition (cgmisc.c); we only flip it. */
extern int gli_determinism;

__attribute__((constructor)) static void headless_force_determinism(void)
{
    gli_determinism = 1;
}

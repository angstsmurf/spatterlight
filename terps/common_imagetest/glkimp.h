/* Headless stand-in for Spatterlight's glkimp.h, for the builds that link
   CheapGlk instead of glkimp: the image probes of the Plus and TaylorMade
   interpreters (found first on their include path) and the ScottFree headless
   build (through scott/test/glkimp.h).

   The interpreter sources include "glkimp.h" under #ifdef SPATTERLIGHT and also
   reference a few of these globals outside the guards. The real header comes
   from the Cocoa front-end; here we only declare the handful of symbols the
   interpreters actually use. The definitions are in glkimp_stubs.c, except
   gli_slowdraw and gli_determinism, which are CheapGlk's (cgmisc.c). */

#ifndef HEADLESS_GLKIMP_H
#define HEADLESS_GLKIMP_H

#include <stdint.h>

#include "glk.h"

/* User settings, normally driven by the Spatterlight preferences UI. */
extern int gli_enable_graphics;
extern int gli_sa_delays;
extern int gli_flicker;
extern int gli_sa_inventory;
extern int gli_sa_palette;
extern int gli_slowdraw;    /* CheapGlk */
extern int gli_determinism; /* CheapGlk */
#ifndef gli_debugger        /* cheapglk.h makes it (0) without debugger support */
extern int gli_debugger;
#endif
extern int gli_utf;
extern int gli_screenwidth;
extern int gli_screenheight;

extern uint32_t gfgcol;
extern uint32_t gbgcol;

extern winid_t FindGlkWindowWithRock(glui32 rock);
extern void win_testresult(int result);
extern void win_beep_zx(int pitch, int cycles);
extern int gli_get_dataresource_info(int resno, void **ptr, glui32 *size,
                                     int *isbinary);

#endif /* HEADLESS_GLKIMP_H */

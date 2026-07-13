/* Headless stand-in for Spatterlight's glkimp.h.

   The ScottFree sources include "glkimp.h" under #ifdef SPATTERLIGHT and also
   reference a few of these globals outside the guards. The real header comes
   from the Cocoa front-end; here we only declare the handful of symbols the
   interpreter actually uses. The definitions live in headless_stubs.c (and, for
   gli_slowdraw / gli_determinism, in CheapGlk's cgmisc.c). */

#ifndef SCOTT_HEADLESS_GLKIMP_H
#define SCOTT_HEADLESS_GLKIMP_H

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
extern int gli_debugger;
extern int gli_utf;
extern int gli_screenwidth;
extern int gli_screenheight;

extern uint32_t gfgcol;
extern uint32_t gbgcol;

extern winid_t FindGlkWindowWithRock(glui32 rock);
extern void win_testresult(int result);
extern int gli_get_dataresource_info(int resno, void **ptr, glui32 *size,
                                     int *isbinary);

#endif /* SCOTT_HEADLESS_GLKIMP_H */

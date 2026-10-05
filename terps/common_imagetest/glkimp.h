/* Image probe stand-in for Spatterlight's glkimp.h: the handful of front-end
   globals the Plus and TaylorMade sources use. Defined in image_glk.c, except
   gli_slowdraw and gli_determinism, which are CheapGlk's (cgmisc.c). */

#ifndef IMAGE_PROBE_GLKIMP_H
#define IMAGE_PROBE_GLKIMP_H

#include <stdint.h>

#include "glk.h"

extern int gli_enable_graphics;
extern int gli_sa_delays;
extern int gli_sa_inventory;
extern int gli_sa_palette;
extern int gli_slowdraw;
extern int gli_determinism;

extern uint32_t gfgcol;
extern uint32_t gbgcol;

extern winid_t FindGlkWindowWithRock(glui32 rock);
extern void win_beep_zx(int duration, int pitch);

#endif

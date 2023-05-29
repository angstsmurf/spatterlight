// vim: set ft=cpp:

#ifndef ZTERP_DRAWPNG_H
#define ZTERP_DRAWPNG_H

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
}
#endif

#include "io.h"
#include "types.h"

void clear_virtual_draw(void);
void virtual_draw(int picidx, int x, int y);
void extract_palette(int picnum);
int draw_arthur_frame_scaled(winid_t winid, int x, int y, int width, int height);


#endif

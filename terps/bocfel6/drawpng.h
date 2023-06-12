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

void clear_image_buffer(void);
bool isadaptive(int picnum);
void extract_palette(int picnum);
void draw_arthur_side_images(winid_t winid);
void draw_indexed_png_scaled(winid_t winid, glui32 picnum, glsi32 x, glsi32 y, float scalefactor, bool flipped);
void flush_waiting_images(winid_t winid);
void draw_to_buffer(winid_t winid, int picnum, int x, int y);
void flush_bitmap(winid_t winid);
#endif

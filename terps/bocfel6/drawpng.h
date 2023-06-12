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
bool isadaptive(int picnum);
void extract_palette(int picnum);
int draw_arthur_room_and_border(winid_t winid);
int draw_zork0_frame_scaled(winid_t winid, glui32 picnum);
void draw_indexed_png_scaled(winid_t winid, glui32 picnum, glsi32 x, glsi32 y, float scalefactor, bool flipped);
void flush_waiting_images(winid_t winid);
void draw_to_buffer(winid_t winid, int picnum, int x, int y);
void flush_bitmap(winid_t winid);
#endif

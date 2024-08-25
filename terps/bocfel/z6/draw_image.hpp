//
//  draw_image.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-17.
//

#ifndef draw_image_hpp
#define draw_image_hpp

extern "C" {
#include "glk.h"
#include "glkimp.h"
}

#include <stdio.h>
#include "v6_image.h"

ImageStruct *find_image(int picnum);
bool get_image_size(int picnum, int *width, int *height);
void writeToTIFF(const char *name, uint8_t *data, size_t size, uint32_t width);
void clear_image_buffer(void);
void flush_bitmap(winid_t winid);
void extract_palette(ImageStruct *image);
ImageStruct *recreate_image(glui32 picnum, int flipped);
void draw_inline_image(winid_t winid, glui32 picnum, glsi32 x, glsi32 y,  float scalefactor, bool flipped);
void draw_to_buffer(winid_t winid, int picnum, int x, int y);
void draw_to_pixmap(ImageStruct *image, uint8_t **pixmap, int *pixmapsize, int screenwidth, int x, int y, bool flipped);
void draw_to_pixmap_unscaled(int image, int x, int y);
void draw_to_pixmap_unscaled_flipped(int image, int x, int y);
void ensure_pixmap(winid_t winid);
void draw_arthur_side_images(winid_t winid);
void draw_centered_title_image(int picnum);
void draw_rectangle_on_bitmap(glui32 color, int x, int y, int width, int height);

extern int last_slideshow_pic;
extern int32_t monochrome_black;
extern int32_t monochrome_white;

#endif /* draw_image_hpp */

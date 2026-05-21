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
void clear_image_buffer(void);
void flush_bitmap(winid_t winid);
void extract_palette(ImageStruct *image);
void extract_palette_from_picnum(int picnum);
ImageStruct *recreate_image(glui32 picnum, int flipped);
void draw_inline_image(winid_t winid, glui32 picnum, glsi32 x, glsi32 y,  float scalefactor, bool flipped);
void draw_to_buffer(winid_t winid, int picnum, int x, int y);
void draw_to_pixmap(ImageStruct *image, uint8_t **pixmap, int *pixmapsize, int screenwidth, int x, int y, bool flipped);
void draw_to_pixmap_unscaled(int image, int x, int y);
void draw_to_pixmap_unscaled_using_current_palette(int image, int x, int y);
void draw_to_pixmap_unscaled_flipped_using_current_palette(int image, int x, int y);
void draw_to_pixmap_using_current_palette(ImageStruct *image, uint8_t **pixmap, int *pixmapsize, int screenwidth, int x, int y, float xscale, float yscale, bool flipped, bool);
void ensure_pixmap(winid_t winid);
void draw_arthur_side_images(winid_t winid);
void extend_shogun_border(int desired_height, int lowest_drawn_pixel, int start_copy_from);
void extend_zork_zero_border(void);
void extend_pillars(int top_cut, int foot_height, int total_height,
                              int pillar_height, int overlap, bool flip,
                              bool hint_foot, int desired_height = 0);
void extend_underground_pillars(int top_cut, int foot_height, int total_height,
                                int pillar_height, int stone_height, int pillar_width);
void extend_jungle_pillars(int top_cut, int total_height,
                           int pillar_height, int overlap);
void extend_mac_bw_castle_pillars(void);
void draw_border_common(int border, int BL, int BR,
                        int border_height, int border_top, int pillar_top,
                        int left_margin, int cga_lowest_adjust,
                        bool is_hint_border, bool draw_non_hint_rect);
void draw_centered_image(int picnum, float scale, int width, int height);
float draw_centered_title_image(int picnum);
void draw_rectangle_on_bitmap(glui32 color, int x, int y, int width, int height);
//void fudge_for_apple_2_maze(bool on);

extern int last_slideshow_pic;
extern int32_t monochrome_black;
extern int32_t monochrome_white;

extern bool image_needs_redraw;

#endif /* draw_image_hpp */

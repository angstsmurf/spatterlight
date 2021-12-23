//
//  sagadraw.h
//  SagaDraw
//
//  Created by Administrator on 2021-12-15.
//

#ifndef sagadraw_h
#define sagadraw_h


//void draw_saga_picture(int picture_number, const char *saga_filename, const char *saga_game, const char *saga_palette, winid_t window, int pixel_size, int x_offset, int file_offset);

void draw_saga_picture_number(int picture_number);
void draw_saga_picture_at_pos(int picture_number, int x, int y);

void draw_sherwood(int loc);
void animate_waterfall(int stage);
void animate_waterfall_cave(int stage);
void animate_lightning(int stage);

void saga_setup(const char *saga_filename, const char *saga_game, int file_offset, const char *saga_palette);
void putpixel(glsi32 x, glsi32 y, int32_t color);
void rectfill (int32_t x, int32_t y, int32_t width, int32_t height, int32_t color);

#endif /* sagadraw_h */

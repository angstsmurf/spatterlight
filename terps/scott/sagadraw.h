//
//  sagadraw.h
//  SagaDraw
//
//  Created by Administrator on 2021-12-15.
//

#ifndef sagadraw_h
#define sagadraw_h

#include "glk.h"

typedef struct {
  uint8_t *imagedata;
  uint8_t xoff;
  uint8_t yoff;
  uint8_t width;
  uint8_t height;
} Image;

struct image_rec {
  uint8_t firstfour[4];
  size_t size;
  uint8_t checksum;
};

uint8_t *draw_saga_picture_from_data(uint8_t *dataptr, int xsize, int ysize,
                                     int xoff, int yoff);

void draw_saga_picture_number(int picture_number);
void draw_saga_picture_at_pos(int picture_number, int x, int y);
void draw_image_from_buffer(void);
void flip(uint8_t character[]);

void saga_setup(size_t imgoffset);
void saga_setup_with_list(struct image_rec *rec);

void putpixel(glsi32 x, glsi32 y, int32_t color);
void rectfill(int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color);

void switch_palettes(int pal1, int pal2);

#endif /* sagadraw_h */

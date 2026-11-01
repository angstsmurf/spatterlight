#ifndef draw_png_h
#define draw_png_h

#include <cstddef>
#include <cstdint>

#include "v6_image.h"

uint8_t *extract_palette_from_png_data(uint8_t *data, size_t length);
uint8_t *draw_png(ImageStruct *image);

#endif

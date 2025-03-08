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
#include "v6_image.h"

uint8_t *extract_palette_from_png_data(uint8_t *data, size_t length);
uint8_t *draw_png(ImageStruct *image, bool use_previous_palette);

#endif

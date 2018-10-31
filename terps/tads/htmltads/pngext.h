
#define PNG_INTERNAL
#include <png.h>
#include <pngconf.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *   Dither to rainbow cube.  Use the same way as png_set_dither.  We'll
 *   automatically generate a rainbox palette.  The rainbow palette will
 *   have 216 colors (6 levels each of R, G, and B).  
 */
void png_set_rainbow_dither(png_structp png_ptr);


#ifdef __cplusplus
}
#endif

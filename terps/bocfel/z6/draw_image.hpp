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

#include <cstdint>
#include <vector>

#include "v6_image.h"

// The RGBA canvas (kBytesPerPixel bytes per pixel) that the borders and
// pictures of a screen are composited into before flush_bitmap() hands the
// result to Glk. It is `width_px` pixels wide, the width of the original
// hardware screen, and always holds a whole number of rows.
struct Pixmap {
    std::vector<uint8_t> data;
    // 0 if there is no pixmap. Nothing is drawn until ensure_pixmap() has
    // made one.
    int width_px = 0;

    bool exists() const { return width_px != 0; }
    int stride() const { return width_px * kBytesPerPixel; }
    int rows() const { return exists() ? (int)(data.size() / stride()) : 0; }
    uint8_t *row(int y) { return data.data() + (size_t)y * stride(); }
    const uint8_t *row(int y) const { return data.data() + (size_t)y * stride(); }
    uint8_t *pixel(int x, int y) { return row(y) + x * kBytesPerPixel; }
    const uint8_t *pixel(int x, int y) const { return row(y) + x * kBytesPerPixel; }

    // Replace the pixmap with a transparent one of `width` x `height` pixels.
    void create(int width, int height);
    // Discard the pixmap.
    void clear();
    // Grow the pixmap downward with transparent rows until it has at least
    // `count` of them. Returns false, leaving it alone, if there is no
    // pixmap or `count` is not in 1..kMaxImageDimension.
    bool ensure_rows(int count);
    // Drop every row from `count` on. Never grows the pixmap.
    void clip_to_height(int count);
    // Make `count` rows from row `y` on transparent, as far as they exist.
    void erase_rows(int y, int count);
    // Composite `src`, `src_size` bytes of RGBA rows `src_width` pixels wide,
    // with its top left corner at (x, y). See the definition.
    void draw(const uint8_t *src, int src_size, int src_width, int x, int y, bool flipped);
    // Fill a rectangle with an opaque colour.
    void fill(glui32 color, int x, int y, int width, int height);

private:
    bool prepare_dest(int *x, int y, int width, int height);
};

// The one pixmap, shared by draw_image.cpp and draw_border.cpp.
extern Pixmap pixmap;

ImageStruct *find_image(int picnum);
bool get_image_size(int picnum, int *width, int *height);
void clear_image_buffer(void);
void flush_bitmap(winid_t winid);
void extract_palette(ImageStruct *image);
void extract_palette_from_picnum(int picnum);
ImageStruct *recreate_image(glui32 picnum, int flipped);
void forget_sent_images(void);
void draw_inline_image(winid_t winid, glui32 picnum, glsi32 x, glsi32 y,  float scalefactor, bool flipped);
void draw_to_buffer(winid_t winid, int picnum, int x, int y);
void draw_to_pixmap_unscaled(int image, int x, int y);
void draw_to_pixmap_unscaled_using_current_palette(int image, int x, int y);
void draw_to_pixmap_unscaled_flipped_using_current_palette(int image, int x, int y);
void ensure_pixmap(winid_t winid);
void draw_centered_image(int picnum, float scale, int width, int height);
float draw_centered_title_image(int picnum);
void draw_rectangle_on_bitmap(glui32 color, int x, int y, int width, int height);
bool sample_pixmap_pixel(int x, int y, glui32 *color);

extern int last_slideshow_pic;
extern int32_t monochrome_black;
extern int32_t monochrome_white;

extern bool image_needs_redraw;

#endif /* draw_image_hpp */

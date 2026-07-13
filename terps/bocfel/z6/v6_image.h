//
//  v6_image.h
//  Spatterlight
//
//  Created by Administrator on 2023-07-17.
//

#ifndef v6_image_h
#define v6_image_h

#include "types.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

enum GraphicsType {
    kGraphicsTypeAmiga,
    kGraphicsTypeMacBW,
    kGraphicsTypeApple2,
    kGraphicsTypeVGA,
    kGraphicsTypeEGA,
    kGraphicsTypeCGA,
    kGraphicsTypeBlorb,
    kGraphicsTypeNoGraphics,
};

struct ImageStruct {
    int index;
    int width;
    int height;
    bool transparency;
    int transparent_color;
    uint8_t *data;
    size_t datasize;
    GraphicsType type;
    uint8_t *palette;
    uint8_t *huffman_tree;
};

typedef struct ImageStruct ImageStruct;

// Bytes per pixel in every RGBA buffer the decoders and compositors pass around.
static constexpr int kBytesPerPixel = 4;

// Upper bound on either dimension of an image buffer. Image dimensions are
// read straight out of the game file (a 16-bit field in the legacy formats, the
// IHDR chunk in the Blorb PNGs), so a corrupt or crafted file can name any size
// it likes. Every picture Infocom shipped is a few hundred pixels on a side, and
// this cap also keeps width * height * kBytesPerPixel two orders of magnitude
// below INT_MAX -- which matters because the pixmap size fields are `int`.
static constexpr int kMaxImageDimension = 8192;

// Size in bytes of a width x height buffer with `bytes_per_pixel` bytes per
// pixel (1 for palette indices, kBytesPerPixel for RGBA). Returns 0 -- meaning
// "refuse" -- for non-positive dimensions, dimensions over kMaxImageDimension,
// or a bad bytes_per_pixel. Computed in size_t, so it can't wrap the way
// `width * height * 4` in int can.
size_t image_buffer_size(int width, int height, int bytes_per_pixel);

// Allocate a zeroed image buffer of image_buffer_size(...) bytes. This is the
// single choke point for image allocations: it validates the dimensions,
// computes the size without overflow, and null-checks the allocation, so
// callers only have to check for nullptr. On success *size_out (if non-null)
// receives the buffer size; on failure it receives 0. Caller frees with free().
//
// Zeroed, not uninitialized: a truncated or corrupt stream leaves part of the
// buffer undecoded, and those pixels must read as palette index 0 / transparent
// rather than as garbage.
uint8_t *image_alloc(int width, int height, int bytes_per_pixel, size_t *size_out);

extern ImageStruct *raw_images;
extern int image_count;
extern uint8_t global_palette[16 * 3];

extern float imagescalex;
extern float imagescaley;
extern float hw_screenwidth;
extern float pixelwidth;

extern GraphicsType graphics_type;

#endif /* v6_image_h */

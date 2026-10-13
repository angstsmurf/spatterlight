//
//  v6_image.h
//  Spatterlight
//
//  Created by Administrator on 2023-07-17.
//

#ifndef v6_image_h
#define v6_image_h

#include <cstdlib>
#include <memory>

#include "types.h"

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
    // The image's own palette, or nullptr. Two layouts, by type:
    //   legacy formats (Amiga, VGA, ...): a count byte, then that many RGB
    //     triplets; these are colours 2 and up (0 is transparent, 1 the
    //     border colour).
    //   kGraphicsTypeBlorb: 16 RGB triplets (48 bytes, zero-padded), no count
    //     byte, gamma-corrected; triplet N is colour N.
    // extract_palette() copies it as it is, so global_palette has the layout
    // of the graphics type in use.
    uint8_t *palette;
    uint8_t *huffman_tree;
};

// Bytes per pixel in every RGBA buffer the decoders and compositors pass around.
static constexpr int kBytesPerPixel = 4;

// Store 0xRRGGBB as the R, G and B bytes of the pixel at `pixel`.
static inline void rgb_to_bytes(uint32_t rgb, uint8_t *pixel) {
    pixel[0] = (rgb >> 16) & 0xff;
    pixel[1] = (rgb >> 8) & 0xff;
    pixel[2] = rgb & 0xff;
}

// The R, G and B bytes of the pixel at `pixel` as 0xRRGGBB.
static inline uint32_t bytes_to_rgb(const uint8_t *pixel) {
    return ((uint32_t)pixel[0] << 16) | ((uint32_t)pixel[1] << 8) | pixel[2];
}

// An image buffer from image_alloc() (or malloc) that frees itself.
struct ImageBufFree {
    void operator()(uint8_t *buffer) const { free(buffer); }
};
using ImageBuf = std::unique_ptr<uint8_t[], ImageBufFree>;

// Upper bound on either dimension of an image buffer. Image dimensions are
// read straight out of the game file (a 16-bit field in the legacy formats, the
// IHDR chunk in the Blorb PNGs), so a corrupt or crafted file can name any size
// it likes. Every picture Infocom shipped is a few hundred pixels on a side, and
// this cap also keeps width * height * kBytesPerPixel two orders of magnitude
// below INT_MAX -- which matters because Glk is handed buffer sizes as `int`.
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

// Expand width x height palette indices to a new RGBA buffer (caller frees),
// looking each one up in the 16-entry `palette`. An index of 16 or more, and
// the image's transparent colour if it has one, give a transparent pixel.
// Returns nullptr if `indices` is null or on allocation failure.
uint8_t *expand_indexed(const uint8_t *indices, const ImageStruct *image, const uint8_t palette[16][3]);

extern ImageStruct *raw_images;
extern int image_count;
extern uint8_t global_palette[16 * 3];

extern float imagescalex;
extern float imagescaley;
extern int hw_screenwidth;
extern float pixelwidth;
// Set the two above to the native screen width and pixel aspect of `type`.
void set_hw_screen_geometry(GraphicsType type);

extern GraphicsType graphics_type;

#endif /* v6_image_h */

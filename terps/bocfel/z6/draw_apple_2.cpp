//
//  draw_apple_2.c
//  A2InfocomWozExtractor
//
//  Created by Administrator on 2023-07-14.
//
//  Decompresses and renders Infocom V6 Apple II images. The compressed
//  format uses a simple RLE scheme with vertical XOR delta encoding
//  (each scanline is XOR'd against the one above). After decompression,
//  the 4-bit palette indices are expanded to 32-bit RGBA pixels using
//  an Apple II color palette approximating the AppleWin emulator output.

#include <stdlib.h>
#include <string.h>

#include "draw_apple_2.h"

struct rgb_struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

typedef struct rgb_struct rgb_t;

// 16-color Apple II palette, hand-crafted to approximate the output
// colours of the AppleWin emulator. Indexed 0x0-0xF.
static const rgb_t apple2_palette[] =
{
    {0x0,  0x0,  0x0 }, /* 0x0: Black */
    {0xd4, 0x3a, 0x48}, /* 0x1: Dark Red */
    {0x91, 0x67, 0x21}, /* 0x2: Brown */
    {0xe6, 0x6f, 0x00}, /* 0x3: Orange */
    {0x39, 0x82, 0x38}, /* 0x4: Dark Green */
    {0x68, 0x68, 0x68}, /* 0x5: Dark Gray */
    {0x67, 0xdf, 0x42}, /* 0x6: Light Green */
    {0xff, 0xff, 0x00}, /* 0x7: Yellow */
    {0x0c, 0x1c, 0xa2}, /* 0x8: Dark Blue */
    {0xe6, 0x28, 0xff}, /* 0x9: Purple */
    {0xb8, 0xb8, 0xb8}, /* 0xA: Light Grey */
    {0xf3, 0xb3, 0xa2}, /* 0xB: Pink */
    {0x2f, 0x3e, 0xe5}, /* 0xC: Medium Blue */
    {0x79, 0xa6, 0xde}, /* 0xD: Light Blue */
    {0x58, 0xf4, 0xbf}, /* 0xE: Aquamarine */
    {0xff, 0xff, 0xff}  /* 0xF: White */
};

// Converts an array of palette-indexed pixels into a 32-bit RGBA pixmap.
// Each input byte is looked up in the Apple II palette and expanded to
// 4 bytes (R, G, B, A). Transparent pixels (matching transparent_color
// when transparency is enabled) are left as zeroed-out RGBA (fully
// transparent). Takes ownership of and frees the input data array.
static uint8_t *deindex(uint8_t *data, size_t size, ImageStruct *image) {
    uint8_t *pixmap = (uint8_t *)calloc(1, size * 4);
    int pixpos = 0;
    for (int i = 0; i < size; i++) {
        rgb_t color;
        uint8_t wordval = data[i];
        if (!(image->transparency && image->transparent_color == wordval)) {
            color = apple2_palette[wordval];
            if (pixpos >= size * 4)
                break;
            uint8_t *ptr = &pixmap[pixpos];
            *ptr++ = color.red;
            *ptr++ = color.green;
            *ptr++ = color.blue;
            *ptr++ = 0xff;
        }
        pixpos += 4;
    }
    free(data);
    return pixmap;
}

// Decompresses Apple II image data from an ImageStruct.
// The format is: pairs of (color_index, run_length) bytes encoded as a
// bitstream starting at offset 3. Each byte pair consists of a color
// value (0x0-0xF) followed by a repeat count. If the next byte is
// <= 0x0F or we're at the end of data, the repeat count defaults to 1
// (0x0F - 0x0E). Otherwise, the repeat count is (byte - 0x0E).
// After decoding each pixel, a vertical XOR pass is applied inline:
// each pixel is XOR'd with the one directly above it (one scanline
// width earlier), reconstructing the image from delta-encoded scanlines.
static uint8_t *decompress_apple2(ImageStruct *image) {
    if (image == nullptr || image->data == nullptr)
        return nullptr;

    uint8_t bytevalue, repeats, counter;

    uint8_t *ptr = image->data + 3;  // Skip 3-byte image header
    size_t finalsize = image->width * image->height;
    uint8_t *result = (uint8_t *)calloc(1, finalsize);
    if (result == nullptr)
        exit(1);

    size_t writtenbytes = 0;

    do {
        long bytesread = ptr - image->data;
        if (bytesread >= image->datasize)
            return result;

        // Read the color/value byte.
        bytevalue = *ptr++;

        // Read the repeat count byte. If we're at end of data or the
        // next byte is a color value (<=0x0F), use a default count of 1.
        if (bytesread + 1 >= image->datasize || *ptr <= 0xf) {
            repeats = 0xf;
        } else {
            repeats = *ptr++;
        }

        // Emit (repeats - 0x0E) copies of bytevalue, applying the
        // vertical XOR delta inline for scanlines below the first.
        for (counter = repeats - 0xe; counter != 0; counter--) {
            result[writtenbytes] = bytevalue;
            if (writtenbytes >= image->width) {
                result[writtenbytes] = result[writtenbytes] ^ result[writtenbytes - image->width];
            }
            writtenbytes++;
            if (writtenbytes == finalsize) {
                // Image fully decoded. Trim the compressed data buffer to
                // the actual bytes consumed, freeing any trailing excess.
                size_t newsize = ptr - image->data;
                if (newsize < image->datasize) {
                    void *temp = realloc(image->data, newsize);
                    if (temp == nullptr) {
                        fprintf(stderr, "realloc error\n");
                        exit(1);
                    }
                    image->data = (uint8_t *)temp;
                    fprintf(stderr, "decompress_apple2: Shaved off %lu bytes. New size: %lu\n", image->datasize - newsize, newsize);
                    image->datasize = newsize;
                }
                return result;
            }
        }

    } while (writtenbytes < finalsize);
    return result;
}

// Public entry point: decompresses an Apple II image and converts the
// palette-indexed result to a 32-bit RGBA pixmap ready for display.
uint8_t *draw_apple2(ImageStruct *image) {
    uint8_t *result = decompress_apple2(image);
    if (result)
        return deindex(result, image->width * image->height, image);
    return nullptr;
}

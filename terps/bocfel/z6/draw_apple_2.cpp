//
//  draw_apple_2.c
//  Apple 2 Infocom Woz extractor
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

typedef struct  {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_t;

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
// transparent). Takes ownership of and frees the input indexed_pixels array.
static uint8_t *deindex(uint8_t *indexed_pixels, size_t pixel_count, ImageStruct *image) {
    if (indexed_pixels == nullptr || image == nullptr || pixel_count == 0) {
        free(indexed_pixels);
        return nullptr;
    }

    uint8_t *rgba_buffer = (uint8_t *)calloc(pixel_count, 4);
    if (rgba_buffer == nullptr) {
        free(indexed_pixels);
        return nullptr;
    }

    for (size_t pixel_index = 0; pixel_index < pixel_count; pixel_index++) {
        uint8_t color_index = indexed_pixels[pixel_index] & 0x0f;
        if (image->transparency && image->transparent_color == color_index)
            continue;
        rgb_t color = apple2_palette[color_index];
        size_t write_offset = pixel_index * 4;
        rgba_buffer[write_offset + 0] = color.red;
        rgba_buffer[write_offset + 1] = color.green;
        rgba_buffer[write_offset + 2] = color.blue;
        rgba_buffer[write_offset + 3] = 0xff;
    }

    free(indexed_pixels);
    return rgba_buffer;
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
// (The last part seems to be pure obfuscation.)
static uint8_t *decompress_apple2(ImageStruct *image) {
    if (image == nullptr || image->data == nullptr)
        return nullptr;

    static const size_t kHeaderSize = 3;

    if (image->width <= 0 || image->height <= 0 || image->datasize < kHeaderSize)
        return nullptr;

    uint8_t *read_ptr = image->data + kHeaderSize;
    uint8_t *data_end = image->data + image->datasize;
    size_t pixel_count = (size_t)image->width * (size_t)image->height;
    uint8_t *pixel_buffer = (uint8_t *)calloc(1, pixel_count);
    if (pixel_buffer == nullptr)
        return nullptr;

    size_t pixel_offset = 0;

    while (pixel_offset < pixel_count && read_ptr < data_end) {
        uint8_t color_value = *read_ptr++;

        uint8_t run_length;
        if (read_ptr >= data_end || *read_ptr <= 0x0f) {
            run_length = 1;
        } else {
            run_length = *read_ptr++ - 0x0e;
        }

        for (uint8_t remaining = run_length; remaining != 0; remaining--) {
            pixel_buffer[pixel_offset] = color_value;
            if (pixel_offset >= (size_t)image->width) {
                pixel_buffer[pixel_offset] ^= pixel_buffer[pixel_offset - image->width];
            }
            pixel_offset++;
            if (pixel_offset == pixel_count) {
                // Image fully decoded. Trim the compressed data buffer to
                // the actual bytes consumed, freeing any trailing excess.
                size_t consumed_bytes = read_ptr - image->data;
                if (consumed_bytes < image->datasize) {
                    void *temp = realloc(image->data, consumed_bytes);
                    if (temp == nullptr) {
                        fprintf(stderr, "realloc error\n");
                        exit(1);
                    }
                    image->data = (uint8_t *)temp;
                    fprintf(stderr, "decompress_apple2: Shaved off %lu bytes. New size: %lu\n", image->datasize - consumed_bytes, consumed_bytes);
                    image->datasize = consumed_bytes;
                }
                return pixel_buffer;
            }
        }
    }
    return pixel_buffer;
}

// Public entry point: decompresses an Apple II image and converts the
// palette-indexed result to a 32-bit RGBA pixmap ready for display.
uint8_t *draw_apple2(ImageStruct *image) {
    uint8_t *result = decompress_apple2(image);
    // Use the same size_t product decompress_apple2 sized its buffer with;
    // an int product could overflow and disagree, making deindex read past
    // the decompressed buffer.
    return deindex(result, (size_t)image->width * (size_t)image->height, image);
}

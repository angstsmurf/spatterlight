// Copyright 2010-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

extern "C" {
#include <zlib.h>
#include <math.h>
#include "glkimp.h"
}

#include "iff.h"
#include "draw_png.h"
#include "draw_image.hpp"
#include "zterp.h"

#define DEBUG_PNG 0

#define debug_png_print(fmt, ...)                    \
do {                                         \
if (DEBUG_PNG)                       \
fprintf(stderr, fmt, ##__VA_ARGS__); \
} while (0)

extern float imagescalex;
extern float imagescaley;

struct png_chunk {
    uint32_t length;
    uint32_t type;
    uint8_t *data;
    uint32_t crc;
};

struct png_header {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t colour_type;
    uint8_t compression_method;
    uint8_t filter_method;
    uint8_t interlace_method;
};

uint8_t global_palette[16 * 3];

static struct png_header pngheader;

static uint8_t *decompress_idat(uint8_t *data, size_t datalength, size_t max_output_size)
{
    char* output = new char[max_output_size];

    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    infstream.avail_in = (unsigned)datalength; // size of input
    infstream.next_in = (Bytef *)data; // input char array
    infstream.avail_out = (unsigned)max_output_size; // size of output
    infstream.next_out = (Bytef *)output; // output char array

    inflateInit2(&infstream, MAX_WBITS|0x20);
    inflate(&infstream, Z_NO_FLUSH);
    inflateEnd(&infstream);

    return (uint8_t *)output;
}

static uint32_t read32be(uint8_t *ptr, int index) { return (static_cast<uint32_t>(ptr[index]) << 24) | (static_cast<uint32_t>(ptr[index + 1]) << 16) | (static_cast<uint32_t>(ptr[index + 2]) <<  8) | (static_cast<uint32_t>(ptr[index + 3]) <<  0);
}

static bool read_png(uint8_t *data, size_t datalength, struct png_chunk *chunks, int *dataidx, int *palidx) {

    *dataidx = -1;
    *palidx = -1;

    if (datalength <= 16)
        return false;
    uint8_t *ptr = data;
    if (ptr[0] != 0x89 ||
        ptr[1] != 0x50 ||
        ptr[2] != 0x4e ||
        ptr[3] != 0x47 ||
        ptr[4] != 0x0d ||
        ptr[5] != 0x0a ||
        ptr[6] != 0x1a ||
        ptr[7] != 0x0a) {
        debug_png_print("Not a png!\n");
        return false;
    }

    ptr += 8;

    int i;

    for (i = 0; i < 64 && ptr - data < datalength; i++) {
        chunks[i].length = read32be(ptr, 0);
        chunks[i].type = read32be(ptr, 4);
        debug_png_print("Chunk %d is of type 0x%08x (%c%c%c%c)\n", i, chunks[i].type, ptr[4], ptr[5], ptr[6], ptr[7]);
        if (chunks[i].type == IFF::TypeID("IDAT").val())
            *dataidx = i;
        if (chunks[i].type == IFF::TypeID("PLTE").val())
            *palidx = i;
        if (chunks[i].type == IFF::TypeID("gAMA").val()) {
//            int gamma = read32be(ptr, 8);
//            debug_png_print("Found gAMA chunk with value %d (%f)\n", gamma, 1/((double)gamma / 100000.0));
        }
        chunks[i].data = &ptr[8];
        ptr = ptr + 8 + chunks[i].length;
        chunks[i].crc = read32be(ptr, 0);
        ptr += 4;
    }

    if (i < 2) {
        debug_png_print("Bad PNG Data\n");
        return false;
    }

    int l = i - 1;

    if (chunks[l].type != IFF::TypeID("IEND").val()) {
        debug_png_print("IEND chunk not found! Wanted 0x%08x, got 0x%08x!\n",IFF::TypeID("IEND").val(), chunks[l].type);
        return false;
    } else {
        debug_png_print("IEND chunk found!\n");
    }

    if (chunks[0].type != IFF::TypeID("IHDR").val()) {
        debug_png_print("IHDR chunk not found! Wanted 0x%08x, got 0x%08x!\n",IFF::TypeID("IHDR").val(), chunks[0].type);
        return false;
    }

    ptr = chunks[0].data;
    pngheader.width = read32be(ptr, 0);
    debug_png_print("Width: %d\n", pngheader.width);
    pngheader.height = read32be(ptr, 4);
    debug_png_print("Height: %d\n", pngheader.height);
    pngheader.bit_depth = ptr[8];
    debug_png_print("Bit depth: %d\n", pngheader.bit_depth);
    pngheader.colour_type = ptr[9];
    debug_png_print("Colour type: %d\n", pngheader.colour_type);
    pngheader.compression_method = ptr[10];
    debug_png_print("Compression method: %d\n", pngheader.compression_method);
    pngheader.filter_method = ptr[11];
    debug_png_print("Filter method: %d\n", pngheader.filter_method);
    pngheader.interlace_method = ptr[12];
    debug_png_print("Interlace method: %d\n", pngheader.interlace_method);

    return true;
}

static int adjust_gamma(int intValue) {
    float theLinearValue = (float)intValue / 256.0f;

    float x = 1.22223173f;  // 2.2 / 1.799986;

    return theLinearValue <= 0.0031308f ? (int)(theLinearValue * 12.92f * 256) : (int)((powf (theLinearValue, 1.0f/x) * 1.055f - 0.055f) * 256) ;
}

uint8_t *extract_palette_from_png_data(uint8_t *data, size_t length) {
    struct png_chunk chunks[64];
    int dataidx, palidx;
    if (read_png(data, length, chunks, &dataidx, &palidx) == false || palidx == -1)
        return nullptr;
    if (chunks[palidx].length % 3 != 0)
        return nullptr;
    int numcolors = chunks[palidx].length / 3;
    debug_png_print("Constructing palette with %d entries\n", numcolors);
    uint8_t local_palette[16][3];
    for (int i = 0; i < chunks[palidx].length; i += 3) {
        int index = i / 3;
        local_palette[index][0] = adjust_gamma(chunks[palidx].data[i]);
        local_palette[index][1] = adjust_gamma(chunks[palidx].data[i + 1]);
        local_palette[index][2] = adjust_gamma(chunks[palidx].data[i + 2]);
        debug_png_print("Palette %d: red 0x%x green 0x%x blue 0x%x\n", index, local_palette[index][0], local_palette[index][1], local_palette[index][2]);
    }
    uint8_t *result = (uint8_t *)calloc(16, 3);
    if (result != nullptr) {
        memcpy(result, local_palette, numcolors * 3);
    }
    return result;
}

static uint8_t *set_pixel(int palval, uint8_t *ptr) {
    if (palval >= palentries) {
        return ptr + 4;
    }
    if (palval != 0) {
        int index = palval * 3;
        *ptr++ = global_palette[index++]; // Red
        *ptr++ = global_palette[index++]; // Green
        *ptr++ = global_palette[index];   // Blue
        *ptr++ = 0xff; // Fully opaque
    } else {
        ptr += 4;
    }
    return ptr;
}

static bool draw_indexed_png(uint8_t **pixmapptr, int *pixmapsize, int pixwidth, int x, int y, uint8_t *data, size_t datasize, bool flipped) {

    struct png_chunk chunks[64];

    int dataidx, palidx, xpos, ypos;

    bool result = read_png(data, datasize, chunks, &dataidx, &palidx);

    if (!result)
        return false;

    if (pngheader.bit_depth != 2 && pngheader.bit_depth != 4) {
        fprintf(stderr, "Unsupported bit depth %d!\n", pngheader.bit_depth);
        return false;
    }

    uint8_t *pixmap = *pixmapptr;

    int newsize = pixwidth * (y + pngheader.height) * 4;
    if (*pixmapsize < newsize) {
        uint8_t *temp = (uint8_t *)calloc(1, newsize);
        memcpy(temp, pixmap, *pixmapsize);
        *pixmapsize = newsize;
        free(pixmap);
        pixmap = temp;
    }

    int bits_per_col = 8 / pngheader.bit_depth;

    int stride = pngheader.width / bits_per_col + 1 + (pngheader.width % 2);

    size_t needed = stride * pngheader.height;
    debug_png_print("Needed space for image data: %zu\n", needed);

    uint8_t *inflate_output = decompress_idat(chunks[dataidx].data, chunks[dataidx].length, needed);

    uint8_t *pixptr;

    for (int i = 0; i < needed; i++) {
        ypos = y + (i / stride);
        if (flipped)
            ypos = pngheader.height + y - ypos - 1;
        
        xpos = x + (i % stride - 1) * bits_per_col;

        // Skip the "filter" byte at the start of each row
        if (xpos < x || xpos > pngheader.width + x)
            continue;
        if (ypos >= pngheader.height + y || ypos < y)
            break;
        uint8_t nibble = inflate_output[i] >> (8 - pngheader.bit_depth);
        pixptr = &pixmap[(ypos * pixwidth + xpos) * 4];
        if (pixptr - pixmap + 4 > *pixmapsize)
            break;
        set_pixel(nibble, pixptr);
        pixptr = &pixmap[(ypos * pixwidth + xpos + 1) * 4];
        uint8_t nibble2;
        if (pngheader.bit_depth == 4) {
            nibble2 = inflate_output[i] & 0xf;
        } else {
            nibble2 = (inflate_output[i] >> 4) & 3;
            set_pixel(nibble2, pixptr);
            pixptr = &pixmap[(ypos * pixwidth + xpos + 2) * 4];
            nibble2 = (inflate_output[i] >> 2) & 3;
            set_pixel(nibble2, pixptr);
            pixptr = &pixmap[(ypos * pixwidth + xpos + 3) * 4];
            nibble2 = inflate_output[i] & 3;
        }
        set_pixel(nibble2, pixptr);
    }
    *pixmapptr = pixmap;
    return true;
}

uint8_t *draw_png(ImageStruct *image) {
    int32_t pixmapsize = image->width * image->height * 4;
    uint8_t *pixmap = (uint8_t *)calloc(1, pixmapsize);
    extract_palette(image);
    if (draw_indexed_png(&pixmap, &pixmapsize, image->width, 0, 0, image->data, image->datasize, false) == false) {
        free(pixmap);
        pixmap = nullptr;
    }
    return pixmap;
}

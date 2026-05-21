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

// Minimal PNG decoder for the indexed-color images shipped in the Blorb
// resources used by Infocom V6 games. Only the chunks we care about (IHDR,
// PLTE, IDAT, IEND) are inspected, and only bit depths 2 and 4 with paletted
// color are rendered. The decoded pixels are composited onto a 32-bit RGBA
// canvas using a gamma-corrected palette.

extern "C" {
#include <zlib.h>
#include <math.h>
#include "glkimp.h"
}

#include "iff.h"
#include "draw_png.h"
#include "draw_image.hpp"
#include "zterp.h"

// Set to 1 to enable verbose chunk/header/palette logging on stderr.
#define DEBUG_PNG 0

#define debug_png_print(fmt, ...)                    \
do {                                         \
if (DEBUG_PNG)                       \
fprintf(stderr, fmt, ##__VA_ARGS__); \
} while (0)

extern float imagescalex;
extern float imagescaley;

// One PNG chunk as it appears on disk: a 32-bit big-endian length, a
// 4-byte FourCC type, a pointer into the source buffer for the chunk's
// payload, and a 32-bit CRC trailer.
struct png_chunk {
    uint32_t length;
    uint32_t type;
    uint8_t *data;
    uint32_t crc;
};

// Decoded fields of the PNG IHDR chunk.
struct png_header {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t colour_type;
    uint8_t compression_method;
    uint8_t filter_method;
    uint8_t interlace_method;
};

// Number of entries in `global_palette`. Color indices >= this are treated
// as out of range by set_pixel().
static constexpr int kMaxPaletteEntries = 16;

static const int kBytesPerColor = 3;

// Active 16-entry RGB palette used by set_pixel. Populated by
// extract_palette() (defined elsewhere) or extract_palette_from_png_data().
uint8_t global_palette[kMaxPaletteEntries * kBytesPerColor];

// zlib-inflate the IDAT payload into a freshly allocated buffer of exactly
// max_output_size bytes. The caller is responsible for deleting the returned
// buffer with delete[].
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

// Read a big-endian 32-bit integer from ptr starting at byte offset index.
static uint32_t read32be(uint8_t *ptr, int index) { return (static_cast<uint32_t>(ptr[index]) << 24) | (static_cast<uint32_t>(ptr[index + 1]) << 16) | (static_cast<uint32_t>(ptr[index + 2]) <<  8) | (static_cast<uint32_t>(ptr[index + 3]) <<  0);
}

// Parse the PNG signature and walk the chunk table, recording each chunk
// in `chunks` (caller-provided, must hold at least kMaxChunks entries) and
// filling in `header` from the IHDR chunk.
//
// On success, *idat_index is set to the index of the IDAT chunk, and (if
// non-null) *plte_index is set to the PLTE chunk index, or -1 if absent.
// Returns false if the data is too short, not a PNG, has a missing IHDR
// or IEND, or has a truncated chunk.
static bool read_png(uint8_t *png_data, size_t png_size, struct png_chunk *chunks, int *idat_index, int *plte_index, struct png_header *header) {
    static const uint8_t kPNGSignature[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    static const size_t kSignatureSize = 8;
    static const size_t kChunkHeaderSize = 8;
    static const size_t kChunkCRCSize = 4;
    static const int kMaxChunks = 64;
    static const size_t kIHDRDataSize = 13;

    if (png_data == nullptr || idat_index == nullptr)
        return false;

    *idat_index = -1;
    if (plte_index != nullptr)
        *plte_index = -1;

    if (png_size < kSignatureSize + kChunkHeaderSize)
        return false;

    if (memcmp(png_data, kPNGSignature, kSignatureSize) != 0) {
        debug_png_print("Not a png!\n");
        return false;
    }

    uint8_t *read_ptr = png_data + kSignatureSize;
    uint8_t *data_end = png_data + png_size;

    int chunk_count;
    for (chunk_count = 0; chunk_count < kMaxChunks; chunk_count++) {
        size_t bytes_remaining = (size_t)(data_end - read_ptr);
        if (bytes_remaining < kChunkHeaderSize)
            break;

        chunks[chunk_count].length = read32be(read_ptr, 0);
        chunks[chunk_count].type = read32be(read_ptr, 4);
        debug_png_print("Chunk %d is of type 0x%08x (%c%c%c%c)\n", chunk_count, chunks[chunk_count].type, read_ptr[4], read_ptr[5], read_ptr[6], read_ptr[7]);

        if (chunks[chunk_count].type == IFF::TypeID("IDAT").val())
            *idat_index = chunk_count;
        if (plte_index != nullptr && chunks[chunk_count].type == IFF::TypeID("PLTE").val())
            *plte_index = chunk_count;

        chunks[chunk_count].data = &read_ptr[kChunkHeaderSize];

        // Verify chunk data + CRC fits within the file
        size_t chunk_payload = (size_t)chunks[chunk_count].length + kChunkCRCSize;
        if (bytes_remaining - kChunkHeaderSize < chunk_payload)
            break;

        read_ptr += kChunkHeaderSize + chunks[chunk_count].length;
        chunks[chunk_count].crc = read32be(read_ptr, 0);
        read_ptr += kChunkCRCSize;
    }

    if (chunk_count < 2) {
        debug_png_print("Bad PNG Data\n");
        return false;
    }

    int last_chunk = chunk_count - 1;

    if (chunks[last_chunk].type != IFF::TypeID("IEND").val()) {
        debug_png_print("IEND chunk not found! Wanted 0x%08x, got 0x%08x!\n", IFF::TypeID("IEND").val(), chunks[last_chunk].type);
        return false;
    } else {
        debug_png_print("IEND chunk found!\n");
    }

    if (chunks[0].type != IFF::TypeID("IHDR").val()) {
        debug_png_print("IHDR chunk not found! Wanted 0x%08x, got 0x%08x!\n", IFF::TypeID("IHDR").val(), chunks[0].type);
        return false;
    }

    if (chunks[0].length < kIHDRDataSize)
        return false;

    uint8_t *ihdr_data = chunks[0].data;
    header->width = read32be(ihdr_data, 0);
    debug_png_print("Width: %d\n", header->width);
    header->height = read32be(ihdr_data, 4);
    debug_png_print("Height: %d\n", header->height);
    header->bit_depth = ihdr_data[8];
    debug_png_print("Bit depth: %d\n", header->bit_depth);
    header->colour_type = ihdr_data[9];
    debug_png_print("Colour type: %d\n", header->colour_type);
    header->compression_method = ihdr_data[10];
    debug_png_print("Compression method: %d\n", header->compression_method);
    header->filter_method = ihdr_data[11];
    debug_png_print("Filter method: %d\n", header->filter_method);
    header->interlace_method = ihdr_data[12];
    debug_png_print("Interlace method: %d\n", header->interlace_method);

    return true;
}

// Convert a single 0..255 color sample from the gamma-1.8 space used by the
// Infocom V6 artwork to approximate sRGB (gamma ~2.2), so the palette colors
// look correct on modern displays.
static int adjust_gamma(int intValue) {
    float theLinearValue = (float)intValue / 256.0f;

    float x = 1.22223173f;  // 2.2 / 1.799986;

    return theLinearValue <= 0.0031308f ? (int)(theLinearValue * 12.92f * 256) : (int)((powf (theLinearValue, 1.0f/x) * 1.055f - 0.055f) * 256) ;
}

// Allocate and return a gamma-corrected 16-entry RGB palette (48 bytes,
// caller frees with free()) from the PLTE chunk of the given PNG. Returns
// nullptr if the PNG has no PLTE, the PLTE length isn't a multiple of 3,
// or memory allocation fails. Excess entries beyond 16 are ignored; fewer
// than 16 entries leaves the trailing slots zeroed.
uint8_t *extract_palette_from_png_data(uint8_t *png_data, size_t png_size) {
    struct png_chunk chunks[64];
    struct png_header header;
    int idat_index, plte_index;
    if (!read_png(png_data, png_size, chunks, &idat_index, &plte_index, &header) || plte_index == -1)
        return nullptr;
    if (chunks[plte_index].length % kBytesPerColor != 0)
        return nullptr;

    int color_count = chunks[plte_index].length / kBytesPerColor;
    if (color_count > kMaxPaletteEntries)
        color_count = kMaxPaletteEntries;

    debug_png_print("Constructing palette with %d entries\n", color_count);

    uint8_t *palette = (uint8_t *)calloc(kMaxPaletteEntries, kBytesPerColor);
    if (palette == nullptr)
        return nullptr;

    for (int color_index = 0; color_index < color_count; color_index++) {
        int offset = color_index * kBytesPerColor;
        palette[offset + 0] = adjust_gamma(chunks[plte_index].data[offset + 0]);
        palette[offset + 1] = adjust_gamma(chunks[plte_index].data[offset + 1]);
        palette[offset + 2] = adjust_gamma(chunks[plte_index].data[offset + 2]);
        debug_png_print("Palette %d: red 0x%x green 0x%x blue 0x%x\n", color_index, palette[offset], palette[offset + 1], palette[offset + 2]);
    }

    return palette;
}

// Write one fully-opaque RGBA pixel at write_ptr using global_palette entry
// color_index. Index 0 is treated as transparent (left untouched), as are
// indices that fall outside the active palette.
static void set_pixel(int color_index, uint8_t *write_ptr) {
    if (color_index <= 0 || color_index >= kMaxPaletteEntries)
        return;
    int palette_offset = color_index * kBytesPerColor;
    write_ptr[0] = global_palette[palette_offset + 0];
    write_ptr[1] = global_palette[palette_offset + 1];
    write_ptr[2] = global_palette[palette_offset + 2];
    write_ptr[3] = 0xff;
}

// Decode an indexed PNG and composite it onto an RGBA canvas at
// (dest_x, dest_y), using global_palette for color lookup. If the canvas is
// too short to fit the image at the requested offset it is reallocated in
// place (the caller's pointer and size are updated). When `flipped` is true
// the image is drawn upside-down. Only bit depths 2 and 4 are supported.
// Returns false on malformed PNG data or unsupported bit depth.
static bool draw_indexed_png(uint8_t **canvas_ptr, int *canvas_size, int canvas_width, int dest_x, int dest_y, uint8_t *png_data, size_t png_size, bool flipped) {

    struct png_chunk chunks[64];
    struct png_header pngheader;

    int idat_index;

    if (!read_png(png_data, png_size, chunks, &idat_index, nullptr, &pngheader))
        return false;

    if (idat_index < 0)
        return false;

    if (pngheader.bit_depth != 2 && pngheader.bit_depth != 4) {
        fprintf(stderr, "Unsupported bit depth %d!\n", pngheader.bit_depth);
        return false;
    }

    uint8_t *canvas = *canvas_ptr;

    int required_size = canvas_width * (dest_y + pngheader.height) * 4;
    if (*canvas_size < required_size) {
        uint8_t *expanded = (uint8_t *)calloc(1, required_size);
        if (expanded == nullptr)
            return false;
        memcpy(expanded, canvas, *canvas_size);
        *canvas_size = required_size;
        free(canvas);
        canvas = expanded;
    }

    int pixels_per_byte = 8 / pngheader.bit_depth;

    int row_stride = pngheader.width / pixels_per_byte + 1 + (pngheader.width % 2);

    size_t decompressed_size = (size_t)row_stride * pngheader.height;
    debug_png_print("Needed space for image data: %zu\n", decompressed_size);

    uint8_t *decompressed = decompress_idat(chunks[idat_index].data, chunks[idat_index].length, decompressed_size);

    for (size_t byte_index = 0; byte_index < decompressed_size; byte_index++) {
        int draw_y = dest_y + (int)(byte_index / row_stride);
        if (flipped)
            draw_y = pngheader.height + dest_y - draw_y - 1;

        int draw_x = dest_x + ((int)(byte_index % row_stride) - 1) * pixels_per_byte;

        // Skip the filter byte at the start of each row
        if (draw_x < dest_x || draw_x > (int)pngheader.width + dest_x)
            continue;
        if (draw_y >= (int)pngheader.height + dest_y || draw_y < dest_y)
            break;

        // Bounds-check the last pixel this byte will produce
        int last_pixel_x = draw_x + pixels_per_byte - 1;
        uint8_t *last_write = &canvas[(draw_y * canvas_width + last_pixel_x) * 4];
        if (last_write - canvas + 4 > *canvas_size)
            break;

        uint8_t *write_ptr = &canvas[(draw_y * canvas_width + draw_x) * 4];
        uint8_t first_pixel = decompressed[byte_index] >> (8 - pngheader.bit_depth);
        set_pixel(first_pixel, write_ptr);

        write_ptr = &canvas[(draw_y * canvas_width + draw_x + 1) * 4];
        uint8_t next_pixel;
        if (pngheader.bit_depth == 4) {
            next_pixel = decompressed[byte_index] & 0xf;
        } else {
            next_pixel = (decompressed[byte_index] >> 4) & 3;
            set_pixel(next_pixel, write_ptr);
            write_ptr = &canvas[(draw_y * canvas_width + draw_x + 2) * 4];
            next_pixel = (decompressed[byte_index] >> 2) & 3;
            set_pixel(next_pixel, write_ptr);
            write_ptr = &canvas[(draw_y * canvas_width + draw_x + 3) * 4];
            next_pixel = decompressed[byte_index] & 3;
        }
        set_pixel(next_pixel, write_ptr);
    }
    delete[] decompressed;
    *canvas_ptr = canvas;
    return true;
}

// Top-level entry: allocate a fresh RGBA pixmap sized to the image and
// render the PNG into it. If use_previous_palette is false, the image's
// palette is (re)extracted into global_palette first; otherwise the palette
// left over from the previous call is reused, which lets a series of images
// share a consistent color table. Returns nullptr on failure.
uint8_t *draw_png(ImageStruct *image, bool use_previous_palette) {
    int32_t pixmapsize = image->width * image->height * 4;
    uint8_t *pixmap = (uint8_t *)calloc(1, pixmapsize);
    if (!use_previous_palette)
        extract_palette(image);
    if (draw_indexed_png(&pixmap, &pixmapsize, image->width, 0, 0, image->data, image->datasize, false) == false) {
        free(pixmap);
        pixmap = nullptr;
    }
    return pixmap;
}

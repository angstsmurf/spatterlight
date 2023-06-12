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
#include "drawpng.h"
#include "writetiff.h"

#define DEBUG_PNG 0

#define debug_png_print(fmt, ...)                    \
do {                                         \
if (DEBUG_PNG)                       \
fprintf(stderr, fmt, ##__VA_ARGS__); \
} while (0)

extern float imagescalex;
extern float imagescaley;

size_t writeToFile(const char *name, uint8_t *data, size_t size)
{
    FILE *fptr = fopen(name, "w");

    if (fptr == NULL) {
        fprintf(stderr, "File open error!\n");
        return 0;
    }

    size_t result = fwrite(data, 1, size, fptr);

    fclose(fptr);
    return result;
}

void writeToTIFF(const char *name, uint8_t *data, size_t size, uint32_t width)
{
    FILE *fptr = fopen(name, "w");

    if (fptr == NULL) {
        fprintf(stderr, "File open error!\n");
        return;
    }

    fprintf(stderr, "writeToTIFF: name \"%s\", width %d height %lu\n", name, width, size / width);

    writetiff(fptr, data, (uint32_t)size, width);

    fclose(fptr);
    return;
}

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

struct rbg {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

static struct rbg *palette = NULL;

static struct png_header pngheader;

uint8_t *decompress_idat(uint8_t *data, size_t datalength, size_t max_output_size)
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

    fprintf(stderr, "decompress_idat: output length == %d\n", infstream.avail_out);

    return (uint8_t *)output;
}

static uint32_t read32be(uint8_t *ptr, int index) { return (static_cast<uint32_t>(ptr[index]) << 24) | (static_cast<uint32_t>(ptr[index + 1]) << 16) | (static_cast<uint32_t>(ptr[index + 2]) <<  8) | (static_cast<uint32_t>(ptr[index + 3]) <<  0);
}

bool isadaptive(int picnum) {
    auto map = giblorb_get_resource_map();
    if (map == nullptr)
        return false;
    giblorb_result_t res;
    if (giblorb_load_chunk_by_type(map, giblorb_method_Memory, &res, IFF::TypeID(&"APal").val(), 0) == giblorb_err_None) {
        for (int i = 0; i < res.length; i += 4) {
            uint32_t entry = read32be((uint8_t *)res.data.ptr, i);
            if (entry == picnum) {
                debug_png_print("isadaptive: This picture has an adaptive palette. Don't extract palette.\n");
                return true;
            }
        }
    } else {
        debug_png_print("isadaptive: Found no APal chunk!\n");
    }
    return false;
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
    //        89 50 4E 47 0D 0A 1A 0A

    ptr += 8;

    int i;

    for (i = 0; i < 64 && ptr - data < datalength; i++) {
        chunks[i].length = read32be(ptr, 0);
        chunks[i].type = read32be(ptr, 4);
        debug_png_print("Chunk %d is of type 0x%08x (%c%c%c%c)\n", i, chunks[i].type, ptr[4], ptr[5], ptr[6], ptr[7]);
        if (chunks[i].type == IFF::TypeID(&"IDAT").val())
            *dataidx = i;
        if (chunks[i].type == IFF::TypeID(&"PLTE").val())
            *palidx = i;
        if (chunks[i].type == IFF::TypeID(&"gAMA").val()) {
            int gamma = read32be(ptr, 8);
            debug_png_print("Found gAMA chunk with value %d (%f)\n", gamma, 1/((double)gamma / 100000.0));

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

    if (chunks[l].type != IFF::TypeID(&"IEND").val()) {
        debug_png_print("IEND chunk not found! Wanted 0x%08x, got 0x%08x!\n",IFF::TypeID(&"IEND").val(), chunks[l].type);
        return false;
    } else {
        debug_png_print("IEND chunk found!\n");
    }

    if (chunks[0].type != IFF::TypeID(&"IHDR").val()) {
        debug_png_print("IHDR chunk not found! Wanted 0x%08x, got 0x%08x!\n",IFF::TypeID(&"IHDR").val(), chunks[0].type);
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

int adjust_gamma(int intValue) {
    float theLinearValue = (float)intValue / 256.0f;

    float x = 2.2 / 1.799986;

    return theLinearValue <= 0.0031308f ? (int)(theLinearValue * 12.92f * 256) : (int)((powf (theLinearValue, 1.0f/x) * 1.055f - 0.055f) * 256) ;
}

static int palentries = 0;

void extract_palette(int picnum) {

    auto map = giblorb_get_resource_map();
    if (map == nullptr) {
        return;
    }
    giblorb_result_t res;
    if (giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Pict, picnum) != giblorb_err_None) {
        return;
    }

    if(isadaptive(picnum))
        return;

    struct png_chunk chunks[64];
    int dataidx, palidx;
    if (read_png((uint8_t *)res.data.ptr, res.length, chunks, &dataidx, &palidx) == false || palidx == -1)
        return;
    if (chunks[palidx].length % 3 != 0)
        return;
    if (palette != NULL)
        free(palette);
    palentries = chunks[palidx].length / 3;
    debug_png_print("Constructing palette with %d entries\n", palentries);
    palette = (struct rbg *)malloc(sizeof(struct rbg) * palentries);
    for (int i = 0; i < chunks[palidx].length; i += 3) {
        int index = i / 3;
        palette[index].red = adjust_gamma(chunks[palidx].data[i]);
        palette[index].green = adjust_gamma(chunks[palidx].data[i + 1]);
        palette[index].blue = adjust_gamma(chunks[palidx].data[i + 2]);
        debug_png_print("Palette %d: red 0x%x green 0x%x blue 0x%x\n", index, palette[index].red, palette[index].green, palette[index].blue);
    }
}


static uint8_t *set_pixel(int palval, uint8_t *ptr) {
    if (palval >= palentries) {
        return ptr + 4;
    }
    if (palval != 0) {
        *ptr++ = palette[palval].red;
        *ptr++ = palette[palval].green;
        *ptr++ = palette[palval].blue;
        *ptr++ = 255; // Fully opaque
    } else {
        ptr += 4;
    }
    return ptr;
}

static void draw_indexed_png(uint8_t **pixmapptr, int *pixmapsize, int pixwidth, int x, int y, giblorb_result_t res, bool flipped) {

    struct png_chunk chunks[64];

    int dataidx, palidx, xpos, ypos;

    bool result = read_png((uint8_t *)res.data.ptr, res.length, chunks, &dataidx, &palidx);

    if (!result)
        return;

    if (pngheader.bit_depth != 2 && pngheader.bit_depth != 4) {
        fprintf(stderr, "Unsupported bit depth %d!\n", pngheader.bit_depth);
        return;
    }

    uint8_t *pixmap = *pixmapptr;

    int newsize = pixwidth * (y + pngheader.height) * 4;
    if (*pixmapsize < newsize) {
        uint8_t *temp = (uint8_t *)malloc(newsize);
        memset(temp, 0, newsize);
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

    uint8_t *pixptr = pixmap;

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
        pixptr = set_pixel(nibble2, pixptr);
    }
    *pixmapptr = pixmap;
}

uint8_t *pixmap = nullptr;
frefid_t fileref = nullptr;

void clear_image_buffer(void) {
    fprintf(stderr, "Clearing buffer!\n");
    if (pixmap != nullptr) {
        free(pixmap);
        pixmap = nullptr;
    }
    if (fileref != nullptr) {
        glk_fileref_destroy(fileref);
        fileref = nullptr;
    }
}

void draw_arthur_side_images(winid_t winid) {
    auto map = giblorb_get_resource_map();
    giblorb_result_t res;
    if (giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Pict, 170) == giblorb_err_None) {

        glui32 w, h;
        glk_image_get_info(170, &w, &h);

        int32_t pixmapsize = w * h * 4;
        uint8_t *pixmap = (uint8_t *)malloc(pixmapsize);
        // Fill with 0: every pixel is initially transparent
        memset(pixmap, 0, pixmapsize);

        draw_indexed_png(&pixmap, &pixmapsize, w, 0, 0, res, false);

        if (fileref == nullptr)
            fileref = glk_fileref_create_temp(fileusage_Data, NULL);
        const char *filename = fileref->filename;
        fprintf(stderr, "draw_arthur_frame_scaled: temp filename: \"%s\"\n", filename);

        writeToTIFF(filename, pixmap, pixmapsize, w);

        win_purgeimage(173);
        win_loadimage(173, filename, 0, pixmapsize);

        int width = gscreenw - gcellw * 2;
        float ratio = 84.0 / 314.0;

        int yorigin = width * ratio + 6;
        win_drawimage(winid->peer, gcellw + 4, yorigin, 6, gscreenh - yorigin - gcellh);
        win_drawimage(winid->peer, gcellw + 4, yorigin, 6, 200);
        int right_xorigin = width - 3;
        win_drawimage(winid->peer, right_xorigin, yorigin, 6, gscreenh - yorigin - gcellh);
        win_drawimage(winid->peer, right_xorigin, yorigin, 6, 200);

        free(pixmap);
    }
}

static struct {
    int pic;
    int pic1;
    int pic2;
} mapper[] = {
    { 5, 497, 498},
    { 6, 501, 502},
    { 7, 499, 500},
    { 8, 503, 504},
};

void draw_indexed_png_scaled(winid_t winid, glui32 picnum, glsi32 x, glsi32 y,  float scalefactor, bool flipped) {
    fprintf(stderr, "Drawing indexed png %d scaled to %f\n", picnum, scalefactor);
    auto map = giblorb_get_resource_map();
    giblorb_result_t res;

    glui32 width, height;
    glk_image_get_info(picnum, &width, &height);
    if (giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Pict, picnum) == giblorb_err_None) {
        int32_t pixmapsize = width * height * 4;
        fprintf(stderr, "Allocated %d bytes\n", pixmapsize);
        uint8_t *pixmap = (uint8_t *)malloc(pixmapsize);
        // Fill with 0: every pixel is initially transparent
        memset(pixmap, 0, pixmapsize);
        draw_indexed_png(&pixmap, &pixmapsize, width, 0, 0, res, flipped);

        if (fileref == nullptr)
            fileref = glk_fileref_create_temp(fileusage_Data, NULL);

        const char *filename = fileref->filename;
        fprintf(stderr, "draw_indexed_png_scaled: temp filename: \"%s\"\n", filename);

        writeToTIFF(filename, pixmap, pixmapsize, width);
        free(pixmap);

        win_purgeimage(picnum);
        win_loadimage(picnum, filename, 0, pixmapsize);
        glk_fileref_destroy(fileref);
        fileref = nullptr;
        win_drawimage(winid->peer, x, y, width * scalefactor, height * scalefactor);
    }
}

int pixlength = 320 * 200 * 4;

void flush_bitmap(winid_t winid) {
    fprintf(stderr, "flush_bitmap win %d\n", winid->peer);
    if (winid->type != wintype_Graphics)
        fprintf(stderr, "ERROR: window is not graphics\n");
    if (pixmap == nullptr) {
        fprintf(stderr, "flush_bitmap: No pixmap\n");
        return;
    }

    if (fileref == nullptr)
        fileref = glk_fileref_create_temp(fileusage_Data, NULL);
    const char *filename = fileref->filename;
    fprintf(stderr, "flush_bitmap: temp filename: \"%s\"\n", filename);
    writeToTIFF(filename, pixmap, pixlength, 320);

    int height = pixlength / (320 * 4);

    win_purgeimage(600);
    win_loadimage(600, filename, 0, pixlength);
    win_drawimage(winid->peer, 0, 0, gscreenw, (float)gscreenw / 320.0 * height);
}

void draw_to_buffer(winid_t winid, int picnum, int x, int y) {
    fprintf(stderr, "draw_to_buffer %d picnum %d x: %d y: %d\n", winid->peer, picnum, x, y);
    auto map = giblorb_get_resource_map();
    if (map == nullptr)
        return;
    giblorb_result_t res;
    giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Pict, picnum);
    if (pixmap == nullptr) {
        win_sizewin(winid->peer, 0, 0, gscreenw, gscreenh);
        glk_window_fill_rect(winid, gbgcol, 0, 0, gscreenw, gscreenh);
        pixmap = (uint8_t *)malloc(pixlength);
    }
    draw_indexed_png(&pixmap, &pixlength, 320, (float)x / imagescalex, (float)y / imagescaley, res, false);
}

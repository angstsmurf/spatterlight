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

static bool isadaptive(int picnum) {
    auto map = giblorb_get_resource_map();
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

void extract_palette(int picnum) {

    auto map = giblorb_get_resource_map();
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
    int palentries = chunks[palidx].length / 3;
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
    if (palval != 0) {
        *ptr++ = palette[palval].red;
        *ptr++ = palette[palval].green;
        *ptr++ = palette[palval].blue;
        *ptr++ = 255; // Fully opaque
                         //            glui32 glk_color = ((palette[nibble].red << 16) | (palette[nibble].green << 8) | palette[nibble].blue);
                         //            glk_window_fill_rect(winid, glk_color, xpos * 2 + x, ypos * 2 + y, 2, 2);
    } else {
        ptr += 4;
    }
    return ptr;
}

//void draw_indexed_png(winid_t winid, int x, int y, giblorb_result_t res) {
static void draw_indexed_png(uint8_t *pixmap, size_t pixlength, int pixwidth, int x, int y, giblorb_result_t res) {

    struct png_chunk chunks[64];

    int dataidx, palidx;

    bool result = read_png((uint8_t *)res.data.ptr, res.length, chunks, &dataidx, &palidx);

    if (!result)
        return;

    fprintf(stderr, "draw_indexed_png: width: %d\n", pngheader.width);

    //    size_t needed = (pngheader.width + 1) * (pngheader.height + 1);
    size_t needed = 13272;
    debug_png_print("Needed space for image data: %zu\n", needed);

    uint8_t *inflate_output = decompress_idat(chunks[dataidx].data, chunks[dataidx].length, needed);


//    writeToFile("/Users/administrator/Desktop/dum", inflate_output, needed);


    //    total_out    uLong    13272

    int xpos = x;
    int ypos = y;
    int stride = pngheader.width / 2 + 1 + (pngheader.width % 2);

    uint8_t *pixptr = pixmap;

    for (int i = 0; i < needed; i++) {
        ypos = y + (i / stride);
        xpos = x + (i % stride) * 2;
        xpos -= 2;

        // Skip the "filter" byte at the start of each row
        if (xpos < x || xpos > pngheader.width + x)
            continue;
        if (ypos > pngheader.height + y - (pngheader.height % 2))
            break;
        uint8_t nibble = inflate_output[i] >> 4;
        pixptr = &pixmap[(ypos * pixwidth + xpos) * 4];
        if (pixptr - pixmap >= pixlength)
            break;
        pixptr = set_pixel(nibble, pixptr);
        pixptr = &pixmap[(ypos * pixwidth + xpos + 1) * 4];
        uint8_t nibble2 = inflate_output[i] & 0xf;
        pixptr = set_pixel(nibble2, pixptr);
    }

}

struct VirtualDrawList {
    int pic_idx = -1;
    int x;
    int y;
};

static VirtualDrawList drawlist[8];

void clear_virtual_draw(void) {
    fprintf(stderr, "Clearing virtual draws\n");
    for (int i = 0; i < 8; i++) {
        if (drawlist[i].pic_idx == -1)
            return;
        drawlist[i].pic_idx = -1;
        fprintf(stderr, "Set drawlist %d to -1\n", i);
    }
}

void virtual_draw(int picidx, int x, int y) {
    for (int i = 0; i < 8; i++) {
        if (drawlist[i].pic_idx == -1) {
            drawlist[i].pic_idx = picidx;
            fprintf(stderr, "virtual_draw image %d at index %d\n", picidx, i);
            drawlist[i].x = x;
            drawlist[i].y = y;
            return;
        }
    }
    fprintf(stderr, "Error: VirtualDrawList full!\n");
}

int draw_arthur_frame_scaled(winid_t winid, int x, int y, int width, int height) {
    win_sizewin(winid->peer, 0, 0, gscreenw, gscreenh);
    glk_window_fill_rect(winid, gbgcol, 0, 0, gscreenw, gscreenh);
    auto map = giblorb_get_resource_map();
    giblorb_result_t res;

    int yorigin = 0;
    if (giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Pict, 54) == giblorb_err_None) {

        float ratio = (float)width / (float)height;

//        int pixheight = (float)gscreenw / ratio;
        int pixheight = height;



        fprintf(stderr, "draw_arthur_frame_scaled: gscreenw: %d gscreenh: %d width %d ratio %f pixheight %d x:%d y:%d\n", gscreenw, gscreenh, width, ratio, pixheight, x, y);

        int32_t pixlength = width * pixheight * 4;
        fprintf(stderr, "Allocated %d bytes\n", pixlength);
        uint8_t *pixmap = (uint8_t *)malloc(pixlength);
        // Fill with 0: every pixel is initially transparent
        memset(pixmap, 0, pixlength);

        draw_indexed_png(pixmap, pixlength, width, 0, 0, res);
//
//        giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Pict, 170);
//        draw_indexed_png(pixmap, pixlength, width, 2, 84 + 8, res);
//
//        giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Pict, 171);
//        draw_indexed_png(pixmap, pixlength, width, 310, 84 + 8, res);


//        writeToFile("/Users/administrator/Desktop/lastpic.png", (uint8_t *)res.data.ptr, res.length);


        if (drawlist[0].pic_idx != -1) {
            int offsetx = 0, offsety = 0;
            int i = 0;
            while (i < 8 && drawlist[i].pic_idx != -1) {
                glui32 w, h;
                glk_image_get_info(drawlist[i].pic_idx, &w, &h);
                if (i == 0) {
                    offsetx = drawlist[0].x;
                    offsety = drawlist[0].y;
                }
                giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Pict, drawlist[i].pic_idx);
//              float scale = 2.0 * (float)gscreenw / 640.0;
                int scale = 2.0;

                draw_indexed_png(pixmap, pixlength, width, 92 + (drawlist[i].x - offsetx) / scale, 7 + (drawlist[i].y - offsety) / scale, res);
                fprintf(stderr, "Drawing from draw list: image %d at xpos %d and ypos %d. offsetx:%d offsety:%d\n", drawlist[i].pic_idx, 92 + (drawlist[i].x - offsetx) / scale, 7 + (drawlist[i].y - offsety) / scale, offsetx, offsety);
                fprintf(stderr, "drawlist[%d].x: %d drawlist[%d].y: %d\n", i, drawlist[i].x, i, drawlist[i].y);
                i++;
            }
        }

        const char *filename = "/Users/administrator/Desktop/dum.tiff";

        writeToTIFF(filename, pixmap, pixlength, width);
        free(pixmap);

        win_purgeimage(172);
        win_loadimage(172, filename, 0, pixlength);
        ratio = 84.0 / 314.0;
        int width = gscreenw - gcellw * 2 - 4;
        yorigin = width * ratio;
        win_drawimage(winid->peer, gcellw + 4, gcellh, width, yorigin);
    }

    if (giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Pict, 170) == giblorb_err_None) {

        glui32 w, h;
        glk_image_get_info(170, &w, &h);

        int32_t pixlength = w * h * 4;
        uint8_t *pixmap = (uint8_t *)malloc(pixlength);
        // Fill with 0: every pixel is initially transparent
        memset(pixmap, 0, pixlength);

        draw_indexed_png(pixmap, pixlength, w, 0, 0, res);

        const char *filename = "/Users/administrator/Desktop/dum2.tiff";

        writeToTIFF(filename, pixmap, pixlength, w);

        win_purgeimage(173);
        win_loadimage(173, filename, 0, pixlength);

        int width = gscreenw - gcellw * 2;
        float ratio = 84.0 / 314.0;

        yorigin = width * ratio + gcellh - 1;
        win_drawimage(winid->peer, gcellw + 8, yorigin, 6, gscreenh - yorigin - gcellh);
        win_drawimage(winid->peer, gcellw + 8, yorigin, 6, 200);
        win_drawimage(winid->peer, 687, yorigin, 6, gscreenh - yorigin - gcellh);
        win_drawimage(winid->peer, 687, yorigin, 6, 200);

        free(pixmap);

    }
    if (ggridmarginy == 0)
        yorigin -= (yorigin % (int)gcellh);
    return yorigin;
//
//    if (giblorb_load_resource(map, giblorb_method_Memory, &res, giblorb_ID_Pict, 171) == giblorb_err_None) {
//        writeToFile("/Users/administrator/Desktop/171.png", (uint8_t *)res.data.ptr, res.length);    }
}

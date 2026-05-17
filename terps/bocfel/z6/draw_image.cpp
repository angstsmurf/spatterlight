//
//  draw_image.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-17.
//

extern "C" {
#include "glkimp.h"
}
#include <cmath>
#include "v6_image.h"
#include "draw_png.h"
#include "draw_apple_2.h"
#include "decompress_amiga.hpp"
#include "decompress_vga.hpp"
#include "writetiff.hpp"
#include "memory.h"
#include "screen.h"
#include "util.h"
#include "zterp.h"
#include "draw_image.hpp"
#include "arthur.hpp"
#include "shogun.hpp"
#include "v6_specific.h"
#include "v6_shared.hpp"
#include "options.h"

ImageStruct *raw_images;
int image_count;

bool image_needs_redraw = true;

enum PaletteColours {
    RED   = 0,
    GREEN = 1,
    BLUE  = 2
};

// The colormap data for the default EGA color palette was taken from
// http://en.wikipedia.org/wiki/Enhanced_Graphics_Adapter.
static const uint8_t ega_colormap[16][3] =
{
    {   0,  0,  0 },
    {   0,  0,170 },
    {   0,170,  0 },
    {   0,170,170 },
    { 170,  0,  0 },
    { 170,  0,170 },
    { 170, 85,  0 }, // -> This is { 170,170, 0 } in pix2gif (?).
    { 170,170,170 },
    {  85, 85, 85 },
    {  85, 85,255 },
    {  85,255, 85 },
    {  85,255,255 },
    { 255, 85, 85 },
    { 255, 85,255 },
    { 255,255, 85 },
    { 255,255,255 }
};

float hw_screenwidth = 320.0;
float pixelwidth = 1.0;

ImageStruct *find_image(int picnum) {
    for (int i = 0; i < image_count; i++) {
        if (raw_images[i].index == picnum)
            return &raw_images[i];
    }
    return nullptr;
}

int palentries = 16;

static uint8_t *pixmap = nullptr;

static int pixlength = hw_screenwidth * 200 * 4;
extern winid_t current_graphics_buf_win;

extern glui32 user_selected_background;

int32_t monochrome_black = 0;
int32_t monochrome_white = 0xffffff;

void ensure_pixmap(winid_t winid) {
    if (winid == nullptr) {
        fprintf(stderr, "ensure_pixmap called with a null winid!\n");
        return;
    }
    if (pixmap == nullptr) {
        win_sizewin(winid->peer, 0, 0, gscreenw, gscreenh);
        glk_window_set_background_color(winid, user_selected_background);
        pixmap = (uint8_t *)calloc(1, pixlength);
    }
}

//void fudge_for_apple_2_maze(bool on) {
//    if (graphics_type == kGraphicsTypeApple2 && get_global(sg.MAZE_WIDTH) != 19) {
//        if (screenmode != MODE_SHOGUN_MAZE)
//            on = false;
//        if (on) {
//            if (hw_screenwidth == 280)
//                return;
//            hw_screenwidth = 280;
//            pixelwidth = 1.0;
//        } else {
//            if (hw_screenwidth == 140)
//                return;
//            hw_screenwidth = 140;
//            pixelwidth = 2.0;
//        }
//        pixlength = hw_screenwidth * 200 * 4;
//        imagescalex = (float)gscreenw / hw_screenwidth;
//        free(pixmap);
//        pixmap = nullptr;
//        ensure_pixmap(current_graphics_buf_win);
//    }
//}

void writeToTIFF(const char *name, uint8_t *data, size_t size, uint32_t width) {
    FILE *fptr = fopen(name, "w");

    if (fptr == NULL) {
        fprintf(stderr, "File open error!\n");
        return;
    }

    fprintf(stderr, "writeToTIFF: \"%s\"\n", name);

    writetiff(fptr, data, (uint32_t)size, width);

    fclose(fptr);
    return;
}

void extract_palette(ImageStruct *image) {
    if (image && image->palette) {
        memcpy(global_palette, image->palette, 16 * 3);
    }
}

static char *create_temp_tiff_file_name(void) {
    fileref_t *fileref = glk_fileref_create_temp(fileusage_Data, NULL);
    if (fileref == nullptr)
        return nullptr;
    const char *filename = glkunix_fileref_get_filename(fileref);
    size_t filenamelength = strlen(filename);
    char *tiffname = (char *)malloc(filenamelength + 6);
    memcpy(tiffname, filename, filenamelength + 1);
    glk_fileref_delete_file(fileref);
    glk_fileref_destroy(fileref);
    strncat(tiffname, ".tiff", 5);
    return tiffname;
}

void flush_bitmap(winid_t winid) {
    if (winid == nullptr)
        return;
    if (winid->type != wintype_Graphics) {
        fprintf(stderr, "ERROR: window is not graphics\n");
    }
    if (pixmap == nullptr) {
        glk_window_clear(winid);
        return;
    }

    glk_window_set_background_color(winid, user_selected_background);
    if (image_needs_redraw)
        glk_window_clear(winid);

    char *filename = create_temp_tiff_file_name();
    if (filename == nullptr)
        return;
    writeToTIFF(filename, pixmap, pixlength, hw_screenwidth);
    win_purgeimage(600, filename, pixlength);
    free(filename);
    win_drawimage(winid->peer, 0, 0, gscreenw, (float)gscreenw / pixelwidth * pixlength / (4 * hw_screenwidth * hw_screenwidth));
    image_needs_redraw = false;
}

extern GraphicsType graphics_type;

bool get_image_size(int picnum, int *width, int *height) {
    ImageStruct *image = find_image(picnum);
    if (image == nullptr) {
        if (width != nullptr)
            *width = 0;
        if (height != nullptr)
            *height = 0;
        return false;
    }
    if (width != nullptr)
        *width = image->width;
    if (height != nullptr)
        *height = image->height;
    return true;
}

// Composites a source bitmap onto a destination bitmap at position (x, y).
// If flipped is true, source rows are drawn in reverse order (vertical flip).
// Source pixels with zero alpha are treated as transparent and skipped.
// The destination buffer is reallocated (extended downward) if too small.
static void draw_bitmap_on_bitmap(const uint8_t *src_pixels, int src_buffer_size, int src_width,
                                  uint8_t **dst_pixels, int *dst_buffer_size, int dst_width,
                                  int dest_x, int dest_y, bool flipped) {
    static const int kBytesPerPixel = 4;

    if (src_pixels == nullptr || dst_pixels == nullptr || *dst_pixels == nullptr)
        return;
    if (src_width <= 0 || dst_width <= 0)
        return;

    image_needs_redraw = true;

    const int src_stride = src_width * kBytesPerPixel;
    const int src_height = src_buffer_size / src_stride;
    if (src_height <= 0)
        return;

    // Clamp dest_x to keep the source within the destination width
    if (dest_x < 0)
        dest_x = 0;
    if (src_width > dst_width)
        return;
    if (dest_x > dst_width - src_width)
        dest_x = dst_width - src_width;

    // Extend destination buffer downward if necessary
    const int required_size = dst_width * (dest_y + src_height) * kBytesPerPixel;
    if (*dst_buffer_size < required_size) {
        uint8_t *expanded = (uint8_t *)calloc(1, required_size);
        if (expanded == nullptr)
            return;
        memcpy(expanded, *dst_pixels, *dst_buffer_size);
        *dst_buffer_size = required_size;
        free(*dst_pixels);
        *dst_pixels = expanded;
    }

    for (int row = 0; row < src_height; row++) {
        const int draw_y = dest_y + row;
        if (draw_y < 0)
            continue;

        const int src_row = flipped ? (src_height - 1 - row) : row;
        const uint8_t *src_line = src_pixels + src_row * src_stride;
        uint8_t *dst_line = *dst_pixels + (draw_y * dst_width + dest_x) * kBytesPerPixel;

        for (int col = 0; col < src_width; col++) {
            const int pixel_offset = col * kBytesPerPixel;
            if (src_line[pixel_offset + 3] != 0)
                memcpy(dst_line + pixel_offset, src_line + pixel_offset, kBytesPerPixel);
        }
    }
}

#define GET_RGB(p)    (((uint8_t)*(p) << 16) | ((uint8_t) *(p + 1) << 8) | *(p + 2))

// Draws the foot (shadow) section of the Mac B/W Shogun/Zork Zero hint borders onto
// the pixmap with transparency: white pixels (monochrome_white with full
// alpha) are treated as transparent and skipped, so the tiled pillar
// pattern shows through.
//
// This makes the bottom of the pillars look better on light backgrounds.

static void draw_hint_menu_feet_mac(uint8_t *smallbitmap, int smallbitmapsize, uint8_t **largebitmap, int *largebitmapsize, int y) {

    int xpos, ypos;

    int width = hw_screenwidth;
    int stride = width * 4;
    int height = smallbitmapsize / stride;

    uint8_t *pixptr;

    for (int i = 0; i < smallbitmapsize; i += 4) {

            ypos = y + i / stride;


        if (ypos < 0) {
            continue;
        }

        xpos = (i % stride) / 4;

        // Clip at left and right edge
        if (xpos < 0 || xpos >= width)
            continue;
        if (ypos >= height + y || ypos < y)
            break;

        pixptr = *largebitmap + ((ypos * width + xpos) * 4);

        // Stop if we have reached the end
        if (pixptr - *largebitmap + 3 > *largebitmapsize || i + 3 > smallbitmapsize) {
            break;
        } else {
            uint32_t source_rgb = GET_RGB(smallbitmap + i);
            if (!(source_rgb == monochrome_white && *(smallbitmap + i + 3) == 0xff))
                memcpy(pixptr, smallbitmap + i, 4);
        }
    }
}

void draw_rectangle_on_bitmap(glui32 color, int dest_x, int dest_y, int rect_width, int rect_height) {
    static const int kBytesPerPixel = 4;

    int screen_width = (int)hw_screenwidth;

    if (rect_width <= 0 || rect_height <= 0 || rect_width > screen_width)
        return;

    if (dest_x < 0)
        dest_x = 0;
    if (dest_x > screen_width - rect_width)
        dest_x = screen_width - rect_width;

    // Extend pixmap downward if necessary
    int required_size = screen_width * (dest_y + rect_height) * kBytesPerPixel;
    if (pixlength < required_size) {
        uint8_t *expanded = (uint8_t *)calloc(1, required_size);
        if (expanded == nullptr)
            return;
        memcpy(expanded, pixmap, pixlength);
        pixlength = required_size;
        free(pixmap);
        pixmap = expanded;
    }

    uint8_t red   = (color >> 16) & 0xff;
    uint8_t green = (color >> 8) & 0xff;
    uint8_t blue  = color & 0xff;

    for (int row = 0; row < rect_height; row++) {
        int draw_y = dest_y + row;
        if (draw_y < 0)
            continue;

        uint8_t *write_ptr = pixmap + (draw_y * screen_width + dest_x) * kBytesPerPixel;
        if (write_ptr - pixmap + rect_width * kBytesPerPixel > pixlength)
            break;

        for (int col = 0; col < rect_width; col++) {
            write_ptr[0] = red;
            write_ptr[1] = green;
            write_ptr[2] = blue;
            write_ptr[3] = 0xff;
            write_ptr += kBytesPerPixel;
        }
    }
}

static uint8_t *flip_bitmap(ImageStruct *image, uint8_t *source) {
    static const int kBytesPerPixel = 4;

    if (source == nullptr || image == nullptr)
        return nullptr;

    size_t row_bytes = (size_t)image->width * kBytesPerPixel;
    size_t buffer_size = row_bytes * (size_t)image->height;
    uint8_t *flipped = (uint8_t *)malloc(buffer_size);
    if (flipped == nullptr) {
        free(source);
        return nullptr;
    }

    for (size_t src_offset = 0; src_offset < buffer_size; src_offset += row_bytes) {
        memcpy(flipped + src_offset, source + buffer_size - src_offset - row_bytes, row_bytes);
    }

    free(source);
    return flipped;
}

static uint8_t *draw_opaque_cga(ImageStruct *image) {
    static const int kBytesPerPixel = 4;
    static const int kBitsPerByte = 8;

    if (image == nullptr || image->data == nullptr)
        return nullptr;
    if (image->width <= 0 || image->height <= 0)
        return nullptr;

    int padded_width = image->width;

    // Opaque CGA images pad their stride to a multiple of 8
    if (padded_width % kBitsPerByte != 0)
        padded_width = (padded_width / kBitsPerByte + 1) * kBitsPerByte;

    size_t packed_size = (size_t)padded_width * image->height;

    // Temporarily widen image->width so decompress_vga allocates for the padded stride
    int original_width = image->width;
    image->width = padded_width;
    uint8_t *decompressed = decompress_vga(image);
    image->width = original_width;

    if (decompressed == nullptr)
        return nullptr;

    size_t rgba_size = packed_size * kBytesPerPixel + padded_width;
    uint8_t *rgba_buffer = (uint8_t *)calloc(1, rgba_size);
    if (rgba_buffer == nullptr) {
        free(decompressed);
        return nullptr;
    }

    // Opaque CGA images store 8 pixels per byte
    // (unlike monochrome Mac images which use the same wasteful format as transparent ones)
    for (size_t byte_index = 0; byte_index < packed_size; byte_index++) {
        uint8_t packed_byte = decompressed[byte_index];
        for (int bit = 0; bit < kBitsPerByte; bit++) {
            size_t rgba_offset = (byte_index * kBitsPerByte + bit) * kBytesPerPixel;
            if (rgba_offset + 3 >= rgba_size)
                break;
            uint32_t color = (packed_byte >> (7 - bit) & 1) ? monochrome_white : monochrome_black;
            rgba_buffer[rgba_offset + 0] = color >> 16;
            rgba_buffer[rgba_offset + 1] = (color >> 8) & 0xff;
            rgba_buffer[rgba_offset + 2] = color & 0xff;
            rgba_buffer[rgba_offset + 3] = 0xff;
        }
    }

    uint8_t *output = nullptr;

    // Shave off padding bits
    if (padded_width > image->width) {
        size_t row_bytes = (size_t)image->width * kBytesPerPixel;
        size_t output_size = row_bytes * image->height;
        output = (uint8_t *)malloc(output_size);
        if (output == nullptr)
            exit(1);
        size_t padded_row_bytes = (size_t)padded_width * kBytesPerPixel;
        uint8_t *write_ptr = output;
        for (int row = 0; row < image->height; row++) {
            memcpy(write_ptr, rgba_buffer + padded_row_bytes * row, row_bytes);
            write_ptr += row_bytes;
        }
        free(rgba_buffer);
    } else {
        output = rgba_buffer;
    }

    free(decompressed);
    return output;
}

static uint8_t *draw_amiga_mac_cga_ega_vga(ImageStruct *image, bool use_previous_palette) {
    static const unsigned kMaxPaletteColors = 14;
    static const int kPaletteSize = 16;
    static const int kBytesPerColor = 3;
    static const int kBytesPerPixel = 4;

    if (image == nullptr || image->data == nullptr)
        return nullptr;

    uint8_t color_table[kPaletteSize][kBytesPerColor] = {};

    unsigned palette_color_count = kMaxPaletteColors;

    if (image->palette != nullptr && !use_previous_palette) {
        palette_color_count = image->palette[0];
        /* Fix for some buggy _Arthur_ pictures. */
        if (palette_color_count > kMaxPaletteColors)
            palette_color_count = kMaxPaletteColors;
        memcpy(&color_table[2][RED], &image->palette[1], palette_color_count * kBytesPerColor);
    } else if (image->type == kGraphicsTypeMacBW || image->type == kGraphicsTypeCGA) {
        color_table[2][RED] = monochrome_white >> 16;
        color_table[2][GREEN] = (monochrome_white >> 8) & 0xff;
        color_table[2][BLUE] = monochrome_white & 0xff;

        color_table[3][RED] = monochrome_black >> 16;
        color_table[3][GREEN] = (monochrome_black >> 8) & 0xff;
        color_table[3][BLUE] = monochrome_black & 0xff;
    } else if (image->type == kGraphicsTypeEGA) {
        memcpy(color_table, ega_colormap, kPaletteSize * kBytesPerColor);
    } else {
        memcpy(&color_table[2][RED], &global_palette[1], palette_color_count * kBytesPerColor);
    }

    size_t pixel_count = (size_t)image->width * (size_t)image->height;

    uint8_t *decompressed = nullptr;
    if (image->type == kGraphicsTypeVGA || image->type == kGraphicsTypeEGA || image->type == kGraphicsTypeCGA) {
        decompressed = decompress_vga(image);
    } else {
        decompressed = decompress_amiga(image);
    }

    if (decompressed == nullptr)
        return nullptr;

    size_t rgba_size = pixel_count * kBytesPerPixel;
    uint8_t *rgba_buffer = (uint8_t *)malloc(rgba_size);
    if (rgba_buffer == nullptr) {
        free(decompressed);
        return nullptr;
    }

    memset(rgba_buffer, 0xff, rgba_size);

    for (size_t pixel_index = 0; pixel_index < pixel_count; pixel_index++) {
        uint8_t color_index = decompressed[pixel_index];
        if (color_index >= kPaletteSize ||
            (image->transparency && image->transparent_color == color_index)) {
            rgba_buffer[pixel_index * kBytesPerPixel + 3] = 0;
            continue;
        }
        size_t write_offset = pixel_index * kBytesPerPixel;
        rgba_buffer[write_offset + 0] = color_table[color_index][RED];
        rgba_buffer[write_offset + 1] = color_table[color_index][GREEN];
        rgba_buffer[write_offset + 2] = color_table[color_index][BLUE];
        rgba_buffer[write_offset + 3] = 0xff;
    }

    free(decompressed);
    return rgba_buffer;
}

static uint8_t *decompress_image(ImageStruct *image, bool use_previous_palette) {
    uint8_t *result = nullptr;
    switch (image->type) {
        case kGraphicsTypeApple2:
            result = draw_apple2(image);
            break;
        case kGraphicsTypeBlorb:
            result = draw_png(image, use_previous_palette);
            break;
        case kGraphicsTypeCGA:
            if (!image->transparency) {
                result = draw_opaque_cga(image);
                break;
            } // fallthrough
        case kGraphicsTypeEGA:
        case kGraphicsTypeVGA:
        case kGraphicsTypeMacBW:
        case kGraphicsTypeAmiga:
            result = draw_amiga_mac_cga_ega_vga(image, use_previous_palette);
            break;
        default:
            break;
    }
    return result;
}


void extract_palette_from_picnum(int picnum) {
    ImageStruct *image = find_image(picnum);
    if (image != nullptr) {
        extract_palette(image);
    }
}

void draw_to_pixmap_palette_optional(ImageStruct *image, uint8_t **pixmap, int *pixmapsize, int screenwidth, int x, int y, float xscale, float yscale, bool flipped, bool use_previous_palette) {
    if (*pixmap == nullptr) {
        fprintf(stderr, "draw_to_pixmap_palette_optional called with a nullptr pixmap!\n");
        return;
    }
    uint8_t *result = decompress_image(image, use_previous_palette);
    if (result != nullptr) {
        if (!use_previous_palette)
            extract_palette(image);
        draw_bitmap_on_bitmap(result, image->width * image->height * 4, image->width, pixmap, pixmapsize, screenwidth, (float)x / xscale, (float)y / yscale, flipped);
        free(result);
    }
}

void draw_to_pixmap(ImageStruct *image, uint8_t **pixmap, int *pixmapsize, int screenwidth, int x, int y, float xscale, float yscale, bool flipped) {
    draw_to_pixmap_palette_optional(image, pixmap, pixmapsize, screenwidth, x, y, xscale, yscale, flipped, false);
}

void draw_to_pixmap_using_current_palette(ImageStruct *image, uint8_t **pixmap, int *pixmapsize, int screenwidth, int x, int y, float xscale, float yscale, bool flipped) {
    draw_to_pixmap_palette_optional(image, pixmap, pixmapsize, screenwidth, x, y, xscale, yscale, flipped, true);
}

void draw_to_buffer(winid_t winid, int picnum, int x, int y) {
    ImageStruct *image = find_image(picnum);
    if (image == nullptr)
        return;

    ensure_pixmap(winid);
    draw_to_pixmap(image, &pixmap, &pixlength, hw_screenwidth, x, y, imagescalex, imagescaley, false);
}

ImageStruct *recreate_image(glui32 picnum, int flipped) {
    ImageStruct *image = find_image(picnum);
    if (image == nullptr)
        return nullptr;

    uint8_t *result = decompress_image(image, false);
    if (result == nullptr)
        return nullptr;

    extract_palette(image);
    int32_t pixmapsize = image->width * image->height * 4;

    char *filename = create_temp_tiff_file_name();

    if (filename == nullptr)
        return nullptr;

    if (flipped) {
        result = flip_bitmap(image, result);
    }

    writeToTIFF(filename, result, pixmapsize, image->width);
    free(result);

    win_purgeimage(picnum, filename, pixmapsize);
    free(filename);
    return image;
}

void draw_inline_image(winid_t winid, glui32 picnum, glsi32 x, glsi32 y, float scalefactor, bool flipped) {
    ImageStruct *image = recreate_image(picnum, flipped);
    if (image == nullptr)
        return;

    float xscalefactor = scalefactor * pixelwidth;

    if (x < 0)
        x = 0;

    win_drawimage(winid->peer, x, y, image->width * xscalefactor, image->height * scalefactor);
}

void draw_to_pixmap_unscaled(int image, int x, int y) {
    ImageStruct *img = find_image(image);
    if (img != nullptr)
        draw_to_pixmap(img, &pixmap, &pixlength, hw_screenwidth, x, y, 1, 1, false);
}

void draw_to_pixmap_unscaled_using_current_palette(int image, int x, int y) {
    ImageStruct *img = find_image(image);
    if (img != nullptr)
        draw_to_pixmap_using_current_palette(img, &pixmap, &pixlength, hw_screenwidth, x, y, 1, 1, false);
}

void draw_to_pixmap_unscaled_flipped_using_current_palette(int image, int x, int y) {
    ImageStruct *img = find_image(image);
    if (img != nullptr)
        draw_to_pixmap_using_current_palette(img, &pixmap, &pixlength, hw_screenwidth, x, y, 1, 1, true);
}

uint8_t *copy_lines_from_bitmap(int start_y, int line_count, size_t *out_size) {
    static const int kBytesPerPixel = 4;

    if (out_size == nullptr)
        return nullptr;
    *out_size = 0;

    if (start_y < 0 || line_count <= 0 || pixmap == nullptr)
        return nullptr;

    int row_stride = hw_screenwidth * kBytesPerPixel;
    size_t start_offset = (size_t)start_y * row_stride;
    if (start_offset >= (size_t)pixlength)
        return nullptr;

    size_t available = (size_t)pixlength - start_offset;
    size_t copy_size = (size_t)line_count * row_stride;
    if (copy_size > available)
        copy_size = available;

    uint8_t *line_buffer = (uint8_t *)malloc(copy_size);
    if (line_buffer == nullptr)
        return nullptr;

    memcpy(line_buffer, pixmap + start_offset, copy_size);
    *out_size = copy_size;
    return line_buffer;
}

uint8_t *copy_rect_from_bitmap(int x, int y, int width, int height, size_t *size) {
    *size = 0;
    int src_stride = (int)hw_screenwidth * 4;
    int row_bytes = width * 4;
    size_t result_size = (size_t)height * row_bytes;
    uint8_t *result = (uint8_t *)malloc(result_size);
    if (result == nullptr) {
        return nullptr;
    }

    for (int row = 0; row < height; row++) {
        int src_offset = (y + row) * src_stride + x * 4;
        if (src_offset + row_bytes > pixlength) {
            memset(result + (size_t)row * row_bytes, 0, (size_t)(height - row) * row_bytes);
            break;
        }
        memcpy(result + (size_t)row * row_bytes, pixmap + src_offset, row_bytes);
    }

    *size = result_size;
    return result;
}

void erase_lines_in_bitmap(int y, int height) {
    int stride = hw_screenwidth * 4;
    int startpos = y * stride;
    if (startpos > pixlength)
        return;
    size_t size = height * stride;
    if (startpos + size > pixlength)
        size = pixlength - startpos;
    memset(pixmap + startpos, 0, size);
}

extern int arthur_pic_top_margin;

extern bool showing_wide_arthur_room_image;

// Draws the Arthur graphical frame: the top banner image (pic 54), optional
// wide room image, and the left/right side pole images (pics 170/171).
// If the screen is taller than the artwork, extends the poles downward by
// tiling a 2-line strip from near the bottom, then draws the foot section.
void draw_arthur_side_images(winid_t winid) {
    ensure_pixmap(winid);

    adjust_arthur_top_margin();
    const int top_margin = arthur_pic_top_margin;

    // Platform-specific horizontal offsets for positioning the frame images.
    // left_margin: x-offset for the top banner image.
    // left_offset/right_offset: additional nudge for the side pole images.
    int left_margin = 0;
    int left_offset = 0;
    int right_offset = 2;

    if (graphics_type == kGraphicsTypeMacBW) {
        left_margin = 5;
    } else if (graphics_type == kGraphicsTypeAmiga) {
        left_margin = 2;
    } else if (graphics_type == kGraphicsTypeApple2) {
        left_margin = 0;
    } else {
        // EGA, CGA, VGA, Blorb
        left_offset = -1;
        if (graphics_type == kGraphicsTypeVGA || graphics_type == kGraphicsTypeBlorb) {
            right_offset = 9;
            left_margin = 3;
        } else {
            right_offset = 18;
            left_margin = 6;
        }
    }

    // Pic 54 is the top banner image that frames the room image
    int image_height;
    get_image_size(54, nullptr, &image_height);
    float hw_screenheight = image_height + top_margin;

    const int stride = hw_screenwidth * 4;
    const int actual_pixmap_height = pixlength / stride;

    // Erase any stale content below the banner before drawing
    if (hw_screenheight < actual_pixmap_height)
        erase_lines_in_bitmap(hw_screenheight, actual_pixmap_height - hw_screenheight);
    draw_to_pixmap_unscaled(54, left_margin, top_margin);

    if (showing_wide_arthur_room_image) {
        arthur_draw_room_image(current_picture);
    }

    // Pic 100 provides the room image dimensions, which determine
    // where the side poles start vertically (y_margin)
    int x_margin = 0, y_margin = 0;
    get_image_size(100, &x_margin, &y_margin);

    // Pics 170/171 are the left/right side pole images
    const int pole_y = y_margin + top_margin - 2;
    draw_to_pixmap_unscaled(170, left_margin + 1 + left_offset, pole_y);
    draw_to_pixmap_unscaled(171, hw_screenwidth - x_margin + right_offset, pole_y);

    get_image_size(170, nullptr, &image_height);
    if (image_height)
        hw_screenheight = pole_y + image_height;

    const float factor = (float)gscreenw / hw_screenwidth / pixelwidth;
    const int desired_height = ceil(gscreenh / factor - arthur_pic_top_margin - 1);

    // Extend the poles downward if the screen is taller than the artwork.
    // We tile a 2-line horizontal strip taken from 90% of the way down,
    // then cap it with the original foot section.
    if (desired_height > hw_screenheight) {
        int place_to_cut = (int)(hw_screenheight * 0.9);
        int foot_height = (int)hw_screenheight - place_to_cut;
        int total_height = (int)hw_screenheight;
        // On Amiga, the foot is shorter (excludes the top margin area),
        // so adjust total_height to keep the foot anchored at place_to_cut.
        if (graphics_type == kGraphicsTypeAmiga) {
            foot_height -= top_margin;
            total_height -= top_margin;
        }
        extend_pillars(place_to_cut, foot_height, total_height,
                                 2, 0, false, false, desired_height);
    }

    flush_bitmap(winid);
}

// Constants for the Shogun Mac B/W hint border pillar tiling.
// The hint border image is 298 lines tall. The top 55 lines are the
// decorative header; below that is the repeatable pillar section,
// ending with a 4-line foot shadow at the bottom.
#define kBWHintTopCut 55       // Lines to skip at the top (header area)
#define kBWHintFootHeight 4    // Height of the foot shadow at the bottom
#define kBWHintTotalHeight 298 // Total height of the hint border image
#define kBWHintOverlap 20      // Overlap between adjacent tiles (pixels)
#define kBWHintStepHeight 20   // Rounding granularity for the truncated
                               // final tile; also the minimum gap required
                               // before tiling is attempted

// Tiles the pillar section of a border image downward to fill the screen.
// The image (total_height lines) has already been drawn. This function
// copies the pillar section (from top_cut, pillar_height lines) and tiles
// it repeatedly, then draws the foot at the bottom.
//
// bw_hint_foot: true = foot is drawn with transparency at the bottom of the
//                       tiled area, and leftover lines below are erased.
//                       Only true for Macintosh black-and-white hint screen pillars.
//              false = foot is drawn opaquely at the bottom of the screen.
// flip:        alternates vertical flip on each tile.
void extend_pillars(int top_cut, int foot_height, int total_height,
                              int pillar_height, int overlap, bool flip,
                              bool bw_hint_foot, int desired_height) {

    if (desired_height <= 0) {
        const float factor = (float)gscreenw / hw_screenwidth / pixelwidth;
        desired_height = ceil(gscreenh / factor);
    }

    if (desired_height < total_height || (bw_hint_foot && desired_height - total_height < kBWHintStepHeight))
        return;

    const int stride = pillar_height - overlap;

    size_t linessize;

    // Copy the pillar section we want to repeat
    uint8_t *section_to_repeat = copy_lines_from_bitmap(top_cut, pillar_height, &linessize);

    // Copy the foot
    size_t footsize;
    uint8_t *foot = copy_lines_from_bitmap(total_height - foot_height, foot_height, &footsize);

    // In hint_foot mode, the foot pixels are part of the tiled pillars and
    // tiling starts from total_height - overlap. Otherwise, erase the foot
    // first and start from below it.
    erase_lines_in_bitmap(total_height - foot_height, foot_height);
    int start_y;
    if (bw_hint_foot) {
        start_y = total_height - overlap;
    } else {
        start_y = total_height - foot_height - overlap;
    }

    bool parity = flip;

    if (is_spatterlight_zork0 && graphics_type == kGraphicsTypeEGA) {
        parity = true;
    }

    // Draw full-height tiles while they fit
    int ypos;
    for (ypos = start_y; ypos <= desired_height; ypos += stride) {
        draw_bitmap_on_bitmap(section_to_repeat, linessize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, parity);
        if (flip)
            parity = !parity;
    }
    free(section_to_repeat);

    // Erase everything in the foot area and below
    const int bitmap_height = (pixlength / 4) / hw_screenwidth;
    int foot_top = desired_height - foot_height;
    if (is_spatterlight_arthur)
        foot_top -= (foot_top & 1);
    erase_lines_in_bitmap(foot_top, bitmap_height - foot_top);

    // Draw the foot
    if (bw_hint_foot) {
        draw_hint_menu_feet_mac(foot, footsize, &pixmap, &pixlength, desired_height - foot_height);
        // Erase any garbage left at the bottom of the pixmap
        erase_lines_in_bitmap(ypos, bitmap_height - ypos);
    } else {
        draw_bitmap_on_bitmap(foot, footsize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, foot_top, false);
    }
    free(foot);
}

void extend_underground_pillars(int top_cut, int foot_height, int total_height,
                              int pillar_height, int stone_height, int pillar_width) {

    const float factor = (float)gscreenw / hw_screenwidth / pixelwidth;
    int desired_height = ceil(gscreenh / factor);

    if (desired_height < total_height)
        return;

    // Copy the pillar section we want to repeat (two stone blocks)
    size_t linessize;
    uint8_t *section_to_repeat = copy_lines_from_bitmap(top_cut, pillar_height, &linessize);

    size_t leftsize;
    uint8_t *left_section = copy_rect_from_bitmap(0, top_cut, pillar_width, pillar_height, &leftsize);

    size_t rightsize;
    uint8_t *right_section = copy_rect_from_bitmap(hw_screenwidth - pillar_width, top_cut, pillar_width, pillar_height, &rightsize);

    // Copy the foot
    size_t footsize;
    uint8_t *foot = copy_lines_from_bitmap(total_height - foot_height, foot_height, &footsize);

    // erase the foot and start from below it.
    erase_lines_in_bitmap(total_height - foot_height, foot_height);
    int start_y = total_height - foot_height;

    // Draw full-height tiles while they fit
    int ypos;
    bool parity = true;
    for (ypos = start_y; ypos + pillar_height <= desired_height; ypos += pillar_height) {
        draw_bitmap_on_bitmap(section_to_repeat, linessize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
        if (parity) {
            draw_bitmap_on_bitmap(right_section, rightsize, pillar_width, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
            draw_bitmap_on_bitmap(left_section, leftsize, pillar_width, &pixmap, &pixlength, hw_screenwidth, hw_screenwidth - pillar_width, ypos, false);
        }
        parity = !parity;
    }

    free(section_to_repeat);

    //  Erase a single "stone" at the bottom to reduce the off-screen part of the foot
    if (ypos + foot_height > desired_height && ypos - stone_height + foot_height > desired_height) {
        erase_lines_in_bitmap(ypos - stone_height, stone_height);
        ypos -= stone_height;
    }

    // Draw the foot
        draw_bitmap_on_bitmap(foot, footsize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
    free(foot);
}

#define kZ0MacTopCut 65
#define kZ0MacTotalHeight 300
#define kZ0MacUpperHeight 97
#define kZ0MacRingHeight 13
#define kZ0MacLowerHeight 110
#define kZ0MacFootHeight 15

void extend_mac_bw_castle_pillars(void) {

    const float factor = (float)gscreenw / hw_screenwidth / pixelwidth;
    int desired_height = ceil(gscreenh / factor) - 1;

    if (desired_height <= kZ0MacTotalHeight + 5)
        return;

    // Copy the pillar sections we want to repeat or move
    int ypos = kZ0MacTopCut;
    size_t uppersize;
    uint8_t *upper_section = copy_lines_from_bitmap(ypos, kZ0MacUpperHeight, &uppersize);
    ypos += kZ0MacUpperHeight;
    size_t ringsize;
    uint8_t *middle_ring = copy_lines_from_bitmap(ypos, kZ0MacRingHeight, &ringsize);
    ypos += kZ0MacRingHeight;
    size_t lowersize;
    uint8_t *lower_section = copy_lines_from_bitmap(ypos, kZ0MacLowerHeight, &lowersize);
    ypos += kZ0MacLowerHeight;
    size_t footsize;
    uint8_t *foot = copy_lines_from_bitmap(ypos, kZ0MacFootHeight, &footsize);

    // erase everything from below the top cut.
    erase_lines_in_bitmap(kZ0MacTopCut, kZ0MacTotalHeight - kZ0MacTopCut);
    ypos = kZ0MacTopCut;

    // Draw upper and lower tiles while they fit
    while (ypos <= desired_height) {
        draw_bitmap_on_bitmap(upper_section, uppersize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
        ypos += kZ0MacUpperHeight;
        if (ypos > desired_height)
            break;
        draw_bitmap_on_bitmap(lower_section, lowersize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
        ypos += kZ0MacLowerHeight;
    }
    free(upper_section);
    free(lower_section);

    // Draw the middle ring
    ypos = (desired_height + kZ0MacFootHeight + kZ0MacFootHeight) / 2;
    erase_lines_in_bitmap(ypos, kZ0MacRingHeight);
    draw_bitmap_on_bitmap(middle_ring, ringsize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
    free(middle_ring);

    // Draw the foot
    ypos = desired_height - kZ0MacFootHeight;
    erase_lines_in_bitmap(ypos, desired_height - ypos + 2);
    draw_bitmap_on_bitmap(foot, footsize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
    free(foot);
}

void extend_jungle_pillars(int top_cut, int total_height,
                                int pillar_height, int overlap) {

    const float factor = (float)gscreenw / hw_screenwidth / pixelwidth;
    int desired_height = ceil(gscreenh / factor);

    if (desired_height <= total_height)
        return;

    size_t linessize;

    // Copy the pillar section we want to repeat
    uint8_t *section_to_repeat = copy_lines_from_bitmap(top_cut, pillar_height, &linessize);

    // Draw full-height tiles while they fit
    int stepsize = pillar_height - overlap;
    int ypos;
    for (ypos = total_height - overlap; ypos <= desired_height; ypos += stepsize) {
        draw_bitmap_on_bitmap(section_to_repeat, linessize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
    }
    free(section_to_repeat);
}


// Extends a border vertically by tiling a strip of the already-drawn pixmap.
// Copies the region from start_copy_from to lowest_drawn_pixel and repeats
// it downward until desired_height is reached, truncating the last copy if
// needed. Used as the final vertical fill after border images and pillars
// have been drawn.
void extend_shogun_border(int desired_height, int lowest_drawn_pixel, int start_copy_from) {
    if (desired_height > lowest_drawn_pixel) {
        size_t copysize;
        int height = lowest_drawn_pixel - start_copy_from;
        if (height < 1)
            return;
        uint8_t *bit_to_repeat = copy_lines_from_bitmap(start_copy_from, height, &copysize);
        erase_lines_in_bitmap(lowest_drawn_pixel, desired_height - lowest_drawn_pixel);

        while (lowest_drawn_pixel < desired_height) {
            if (lowest_drawn_pixel + height > desired_height) {
                height = desired_height - lowest_drawn_pixel;
                copysize = hw_screenwidth * 4 * height;
            }
            draw_bitmap_on_bitmap(bit_to_repeat, copysize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, lowest_drawn_pixel, false);
            lowest_drawn_pixel += height;
        }
        free(bit_to_repeat);
    }
}

extern glui32 user_selected_foreground;

// Common border drawing pipeline shared between Zork Zero and Shogun.
// Draws the border image, attempts to draw side pillars, extends vertically
// for tall screens, draws a covering rectangle at the top, and flushes.
//
// Parameters:
//   border          – main border image number
//   BL, BR          – left/right pillar image numbers (-1 if none)
//   border_height   – height of the main border image (from get_image_size)
//   border_top      – y-offset where pillars begin (non-Amiga/Mac: border_height, else 0)
//   pillar_top      – y-offset to start copying from when extending
//   left_margin     – left margin for covering rectangle (0 for hint borders)
//   cga_lowest_adjust – CGA-specific adjustment subtracted from lowest_drawn_line
//   is_hint_border  – true for hint borders (controls covering rectangle logic)
//   draw_non_hint_rect – true to always draw covering rectangle (Shogun non-hint borders)
void draw_border_common(int border, int BL, int BR,
                        int border_height, int border_top, int pillar_top,
                        int left_margin, int cga_lowest_adjust,
                        bool is_hint_border, bool draw_non_hint_rect) {

    float factor = (float)gscreenw / hw_screenwidth / pixelwidth;
    int desired_height = ceil(gscreenh / factor);

    int width, height;
    int lowest_drawn_line = border_height + border_top;
    bool must_extend = (desired_height > lowest_drawn_line);

    // Step 1: Draw the main border image at the top of the pixmap
    draw_to_pixmap_unscaled_using_current_palette(border, 0, 0);

    // Step 2: Draw side pillars if available.
    // BL won't be found if graphics type is Amiga or Mac B/W, because
    // those platforms use a single combined border image instead of
    // separate top + left/right pillar images.
    if (find_image(BL)) {
        get_image_size(BL, &width, &height);
        draw_to_pixmap_unscaled(BL, 0, border_top);
        draw_to_pixmap_unscaled(BR, hw_screenwidth - width, border_top);
        lowest_drawn_line = height + border_top;
    } else {
        // Amiga and Mac border graphics are a single image with everything,
        // not separated into top and sides
        if (is_hint_border) {
            // Mac B/W hint borders have their own pillar tiling with
            // a transparent foot shadow; this handles full vertical
            // extension, so we set desired_height = 0 to skip the
            // generic extend_shogun_border below.
            if (must_extend && graphics_type == kGraphicsTypeMacBW) {
                desired_height = (desired_height / kBWHintStepHeight) * kBWHintStepHeight;
                extend_pillars(kBWHintTopCut, kBWHintFootHeight, kBWHintTotalHeight,
                                        kBWHintTotalHeight - kBWHintTopCut, kBWHintOverlap,
                                        false, true, desired_height);
                desired_height = 0;
            }
        } else if (must_extend && (graphics_type == kGraphicsTypeAmiga || graphics_type == kGraphicsTypeMacBW)) {
            // Shogun non-hint borders on Amiga/Mac B/W: the border image
            // is a single piece (no separate pillars). Extend vertically
            // by drawing a flipped (P_BORDER) or repeated (P_BORDER2) copy
            // below the original, with platform-specific overlap offsets.
            if (border == P_BORDER) {
                if (graphics_type == kGraphicsTypeMacBW) {
                    draw_to_pixmap_unscaled_flipped_using_current_palette(border, 0, border_height);
                    lowest_drawn_line = border_height * 2;
                } else {
                    draw_to_pixmap_unscaled_flipped_using_current_palette(border, 0, border_height - 2);
                    lowest_drawn_line = border_height * 2 - 2;
                }
            } else if (border == P_BORDER2) {
                if (graphics_type == kGraphicsTypeMacBW) {
                    draw_to_pixmap_unscaled_using_current_palette(border, 0, border_height - 35);
                    lowest_drawn_line = border_height * 2 - 35;
                } else {
                    draw_to_pixmap_unscaled_using_current_palette(border, 0, border_height - 22);
                    lowest_drawn_line = border_height * 2 - 22;
                }
            }
        }

        // Shogun non-hint borders on CGA/EGA/VGA: BL is never present,
        // but BR exists as a right-side pillar strip. Draw it on the
        // right, and if extending, place flipped copies on both sides
        // below the original to fill the height. CGA needs the border
        // redrawn on top because the pillar overwrites part of it.
        if (!is_hint_border && find_image(BR)) {
            get_image_size(BR, &width, &height);
            if (graphics_type == kGraphicsTypeCGA) {
                height -= 7;
            }
            if (must_extend)
                draw_to_pixmap_unscaled_flipped_using_current_palette(border, hw_screenwidth - width, border_top + height);
            draw_to_pixmap_unscaled_using_current_palette(BR, hw_screenwidth - width, border_top);
            lowest_drawn_line = border_top + height;
            if (must_extend) {
                draw_to_pixmap_unscaled_flipped_using_current_palette(BR, 0, border_top + height);
                lowest_drawn_line += height;
            }
            if (graphics_type == kGraphicsTypeCGA) {
                draw_to_pixmap_unscaled(border, 0, 0);
            }
        }
    }

    // Step 3: Fill any remaining vertical gap by tiling a strip of the
    // already-drawn pixmap downward (from pillar_top to lowest_drawn_line).
    if (must_extend) {
        if (graphics_type == kGraphicsTypeCGA) {
            lowest_drawn_line -= cga_lowest_adjust;
        }
        extend_shogun_border(desired_height, lowest_drawn_line, pillar_top);
    }

    // Step 4: Draw a covering rectangle of status-window color at the top.
    // The raw border graphics have a decorative top bar that doesn't fit
    // the header text, so the original interpreters cover it with a solid
    // color rectangle. For hint borders this is platform-dependent; for
    // non-hint Shogun borders it's always drawn (unless at the start menu).
    bool should_draw_covering_rectangle = draw_non_hint_rect;
    glui32 rectangle_color = user_selected_foreground;

    if (is_hint_border) {
        if (graphics_type == kGraphicsTypeCGA) {
            should_draw_covering_rectangle = true;
        } else {
            left_margin = 0;
            if (graphics_type == kGraphicsTypeMacBW) {
                should_draw_covering_rectangle = true;
            } else if (options.int_number == INTERP_MACINTOSH && graphics_type == kGraphicsTypeAmiga) {
                should_draw_covering_rectangle = true;
                rectangle_color = ROSE_TAUPE;
            }
        }
    }

    if (should_draw_covering_rectangle) {
        draw_rectangle_on_bitmap(rectangle_color, left_margin, 0, hw_screenwidth - left_margin * 2, V6_STATUS_WINDOW.y_size / imagescaley + 1);
    }
    flush_bitmap(current_graphics_buf_win);
}

extern winid_t current_graphics_buf_win;

void clear_image_buffer(void) {
    if (pixmap != nullptr) {
        free(pixmap);
        pixmap = nullptr;
    }
}

int last_slideshow_pic = -1;

void draw_centered_image(int picnum, float scale, int width, int height) {
    int x, y;
    if (width == 0 || height == 0) {
        get_image_size(picnum, &width, &height);
        if (width == 0 || height == 0) {
            return;
        }
    }

    x = (gscreenw - width * scale * pixelwidth) / 2;
    y = (gscreenh - height * scale) / 2;
    draw_inline_image(current_graphics_buf_win, picnum, x, y, scale, false);
    last_slideshow_pic = picnum;
}

float draw_centered_title_image(int picnum) {
    int width, height;
    get_image_size(picnum, &width, &height);

    float scale = (float)gscreenw / (width * pixelwidth);

    if (height * scale > gscreenh) {
        scale = (float)gscreenh / height;
    }

    draw_centered_image(picnum, scale, width, height);
    return scale;
}

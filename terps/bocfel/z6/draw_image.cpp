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
#include "util.h"
#include "zterp.h"
#include "arthur.hpp"
#include "shogun.hpp"
#include "v6_specific.h"
#include "v6_specific.h"

ImageStruct *raw_images;
int image_count;

extern int current_picture;

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

//    fprintf(stderr, "writeToTIFF: \"%s\"\n", name);

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

static void draw_bitmap_on_bitmap(uint8_t *smallbitmap, int smallbitmapsize, int smallbitmapwidth, uint8_t **largebitmap, int *largebitmapsize, int largebitmapwidth, int x, int y, bool flipped) {

    image_needs_redraw = true;
    
    int xpos, ypos;

    int width = smallbitmapwidth;
    int stride = width * 4;
    int height = smallbitmapsize / stride;

    if (x < 0)
        x = 0;

    if (x > largebitmapwidth)
        x = largebitmapwidth - width;

    int newsize = largebitmapwidth * (y + height) * 4;

    // Extend large bitmap downward if necessary
    if (*largebitmapsize < newsize) {
        uint8_t *temp = (uint8_t *)calloc(1, newsize);
        memcpy(temp, *largebitmap, *largebitmapsize);
        *largebitmapsize = newsize;
        free(*largebitmap);
        *largebitmap = temp;
    }

    uint8_t *pixptr;

    for (int i = 0; i < smallbitmapsize; i += 4) {

        if (!flipped) {
            ypos = y + i / stride;
        } else {
            ypos = y + (smallbitmapsize - i - 1) / stride;
        }

        if (ypos < 0) {
            continue;
        }

        xpos = x + (i % stride) / 4;

        // Clip at left and right edge
        if (xpos < x || xpos >= width + x)
            continue;
        if (ypos >= height + y || ypos < y)
            break;

        pixptr = *largebitmap + ((ypos * largebitmapwidth + xpos) * 4);

        // Stop if we have reached the end
        if (pixptr - *largebitmap + 3 > *largebitmapsize || i + 3 > smallbitmapsize) {
            break;
        } else if (*(smallbitmap + i + 3) != 0) { // Skip transparent pixels
            memcpy(pixptr, smallbitmap + i, 4);
        }
    }
}

#define GET_RGB(p)    (((uint8_t)*(p) << 16) | ((uint8_t) *(p + 1) << 8) | *(p + 2))

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

void draw_rectangle_on_bitmap(glui32 color, int x, int y, int width, int height) {

    int xpos, ypos;

    int stride = width * 4;

    if (x < 0)
        x = 0;

    int screenwidth = (int)hw_screenwidth;

    if (x > screenwidth)
        x = screenwidth - width;

    int newsize = screenwidth * (y + height) * 4;

    // Extend large bitmap downward if necessary
    if (pixlength < newsize) {
        uint8_t *temp = (uint8_t *)calloc(1, newsize);
        memcpy(temp, pixmap, pixlength);
        pixlength = newsize;
        free(pixmap);
        pixmap = temp;
    }

    uint8_t *pixptr;

    int rectsize = width * height * 4;

    uint8_t r, g, b;
    r = (color >> 16) & 0xff;
    g = (color >> 8) & 0xff;
    b = color & 0xff;

    for (int i = 0; i < rectsize; i += 4) {

        ypos = y + i / stride;

        if (ypos < 0) {
            continue;
        }

        xpos = x + (i % stride) / 4;

        // Clip at left and right edges
        if (xpos < x || xpos >= width + x)
            continue;
        if (ypos >= height + y || ypos < y)
            break;

        pixptr = pixmap + ((ypos * screenwidth + xpos) * 4);

        // Stop if we have reached the end
        if (pixptr - pixmap + 3 > pixlength || i + 3 > rectsize) {
            break;
        } else {
            *(pixptr) = r;
            *(pixptr + 1) = g;
            *(pixptr + 2) = b;
            *(pixptr + 3) = 0xff;
        }
    }
}

static uint8_t *flip_bitmap(ImageStruct *image, uint8_t *bitmap) {
    if (bitmap == nullptr || image == nullptr)
        return nullptr;
    size_t size = image->width * image->height * 4;
    uint8_t *flipped = (uint8_t *)malloc(size);
    if (flipped == nullptr)
        exit(1);
    int bytewidth = image->width * 4;
    for (int i = 0; i < size; i += bytewidth) {
        memcpy(flipped + i, bitmap + size - i - bytewidth, bytewidth);
    }
    free(bitmap);
    return flipped;
}

static uint8_t *draw_opaque_cga(ImageStruct *image) {
    if (image->data == nullptr)
        return nullptr;

    int paddedwidth = image->width;

    // Opaque CGA images pad their stride to a multiple of 8
    if (paddedwidth % 8 != 0) {
        paddedwidth = (paddedwidth / 8 + 1) * 8;
    }

    size_t size = paddedwidth * image->height;

    if (size <= 0)
        return nullptr;

    int storedwith = image->width;
    image->width = paddedwidth;

    uint8_t *data = decompress_vga(image);

    image->width = storedwith;

    uint8_t *pixmap = (uint8_t *)calloc(1, size * 4 + paddedwidth);
    uint8_t *result = nullptr;

    // Opaque CGA images store 8 pixels per byte
    // (unlike monochrome Mac images which use the same wasteful format as transparent ones)
    for (int i = 0; i < size; i++) {
        uint8_t byte = data[i];
        for (int j = 0; j < 8; j++) {
            int index = (i * 8 + j) * 4;
            if (index + 3 >= size * 4 + paddedwidth)
                break;
            uint8_t *ptr = &pixmap[index];
            if (byte >> (7 - j) & 1) {
                *ptr++ = monochrome_white >> 16;
                *ptr++ = (monochrome_white >> 8) & 0xff;
                *ptr++ = monochrome_white & 0xff;
            } else {
                *ptr++ = monochrome_black >> 16;
                *ptr++ = (monochrome_black >> 8) & 0xff;
                *ptr++ = monochrome_black & 0xff;
            }
            *ptr++ = 0xff;
        }
    }

    // Shave off padding bits
    if (paddedwidth > image->width) {
        size_t actualwidth = image->width * 4;
        size_t actualsize = actualwidth * image->height;
        result = (uint8_t *)malloc(actualsize);
        if (result == nullptr)
            exit(1);
        size_t totalpaddedwidth = paddedwidth * 4;
        uint8_t *ptr = result;
        for (int i = 0; i < image->height; i++) {
            memcpy(ptr, pixmap + totalpaddedwidth * i, actualwidth);
            ptr += actualwidth;
        }
        free(pixmap);
    } else {
        result = pixmap;
    }

    free(data);
    return result;
}

static uint8_t *draw_amiga_mac_cga_ega_vga(ImageStruct *image, bool use_previous_palette) {
    if (image->data == nullptr)
        return nullptr;

    uint8_t colourmap[16][3] = {};

    unsigned colours = 14;

    if (image->palette != nullptr && use_previous_palette == false) {
        colours = image->palette[0];
        /* Fix for some buggy _Arthur_ pictures. */
        if (colours > 14)
            colours = 14;
        memcpy(&colourmap[2][RED], &image->palette[1], colours * 3);
    } else if (image->type == kGraphicsTypeMacBW || image->type == kGraphicsTypeCGA) {
        // Monochrome images use index 2 for white (0xff) and 3 for black (0)
        // or fg / bg if gli_z6_colorize is set
        colourmap[2][RED] = monochrome_white >> 16;
        colourmap[2][GREEN] = (monochrome_white >> 8) & 0xff;
        colourmap[2][BLUE] = monochrome_white & 0xff;

        colourmap[3][RED] = monochrome_black >> 16;
        colourmap[3][GREEN] = (monochrome_black >> 8) & 0xff;
        colourmap[3][BLUE] = monochrome_black & 0xff;
    } else if (image->type == kGraphicsTypeEGA) {
        memcpy(colourmap, ega_colormap, 16 * 3);
    } else {
        // Copy previously used palette
        memcpy(&colourmap[2][RED], &global_palette[1], colours * 3);
    }

    size_t size = image->width * image->height;

    uint8_t *data = nullptr;
    if (image->type == kGraphicsTypeVGA || image->type == kGraphicsTypeEGA || image->type == kGraphicsTypeCGA) {
        data = decompress_vga(image);
    } else {
        data = decompress_amiga(image);
    }

    size_t pixmapsize = size * 4 + image->width;
    uint8_t *pixmap = (uint8_t *)malloc(pixmapsize);

    // Fill pixmap with white
    memset(pixmap, 0xff, pixmapsize);

    for (int i = 0; i < size; i++) {
        uint8_t color = data[i];
        if ((color > 15) ||
            (image->transparency && image->transparent_color == color)) {
            pixmap[i * 4 + 3] = 0;
            continue;
        }
        uint8_t *ptr = &pixmap[i * 4];
        *ptr++ = colourmap[color][RED];
        *ptr++ = colourmap[color][GREEN];
        *ptr++ = colourmap[color][BLUE];
        *ptr++ = 0xff;
    }

    free(data);
    return pixmap;
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

uint8_t *copy_lines_from_bitmap(int y, int height, size_t *size) {

    *size = 0;
    int stride = hw_screenwidth * 4;
    int startpos = y * stride;
    if (startpos > pixlength)
        return nullptr;
    *size = height * stride;
    if (startpos + *size > pixlength)
        *size = pixlength - startpos;
    uint8_t *result = (uint8_t *)malloc(*size);
    memcpy(result, pixmap + startpos, *size);

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
    bzero(pixmap + startpos, size);
}

extern int arthur_pic_top_margin;

extern bool showing_wide_arthur_room_image;

void draw_arthur_side_images(winid_t winid) {
    ensure_pixmap(winid);

    adjust_arthur_top_margin();
    int top_margin = arthur_pic_top_margin;
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
        left_offset = -1;
        if (graphics_type == kGraphicsTypeVGA || graphics_type == kGraphicsTypeBlorb) {
            right_offset = 9;
            left_margin = 3;
        } else {
            right_offset = 18;
            left_margin = 6;
        }
    }

    // 54 is top "banner" image (framing the room image)

    int image_height;
    get_image_size(54, nullptr, &image_height);
    float hw_screenheight = image_height + top_margin;

    int stride = hw_screenwidth * 4;
    int actual_pixmap_height = pixlength / stride;

    if (hw_screenheight < actual_pixmap_height)
        erase_lines_in_bitmap(hw_screenheight, actual_pixmap_height - hw_screenheight);
    draw_to_pixmap_unscaled(54, left_margin, top_margin);

    if (showing_wide_arthur_room_image) {
        arthur_draw_room_image(current_picture);
    }

    int x_margin = 0, y_margin = 0;
    get_image_size(100, &x_margin, &y_margin);

    // images 170 and 171 are side "banner pole" images

    draw_to_pixmap_unscaled(170, left_margin + 1 + left_offset, y_margin + top_margin - 2);
    draw_to_pixmap_unscaled(171, hw_screenwidth - x_margin + right_offset, y_margin + top_margin - 2);

    get_image_size(170, nullptr, &image_height);

    if (image_height)
        hw_screenheight = y_margin + top_margin - 2 + image_height;

    float factor = (float)gscreenw / hw_screenwidth / pixelwidth;

    int desired_height = ceil(gscreenh / factor - arthur_pic_top_margin - 1);

    if (desired_height > hw_screenheight) {
        int place_to_cut = hw_screenheight * 0.9;

        size_t linessize;
        uint8_t *two_lines_to_repeat = copy_lines_from_bitmap(place_to_cut, 2, &linessize);

        size_t footsize = 0;
        int height_of_foot = hw_screenheight - place_to_cut;
        if (graphics_type == kGraphicsTypeAmiga)
            height_of_foot -= top_margin;
        uint8_t *foot = copy_lines_from_bitmap(place_to_cut, height_of_foot, &footsize);
        int repetitions = (desired_height - place_to_cut - height_of_foot) / 2;

        int ypos = place_to_cut + 2;
        erase_lines_in_bitmap(ypos, hw_screenheight - ypos);

        for (int i = 0; i < repetitions; i++) {
            draw_bitmap_on_bitmap(two_lines_to_repeat, linessize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
            ypos += 2;
        }
        free(two_lines_to_repeat);

        draw_bitmap_on_bitmap(foot, footsize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
        free(foot);
    }

    flush_bitmap(winid);
}

#define kTopCut 55
#define kBottomCut 294
#define kTotalHeight 298
#define kStepHeight 20
#define kOverlap 20

void extend_mac_bw_hint_border(int desired_height) {

    // We have already drawn the pillars once (kTotalHeight lines).
    // Now we fill out the space below.
    int lines_to_fill = desired_height - kTotalHeight;

    if (lines_to_fill < kStepHeight) {
        // If the remaining space is smaller than kStepHeight, we bail.
        return;
    }

    size_t linessize;
    int pillar_height = kTotalHeight - kTopCut;
    int height_of_foot = kTotalHeight - kBottomCut;

    // We copy the section we want the repeat
    uint8_t *copied_pillars = copy_lines_from_bitmap(kTopCut, pillar_height, &linessize);

    // And the "shadow" at the bottom
    size_t footsize = 0;
    uint8_t *foot = copy_lines_from_bitmap(kBottomCut, height_of_foot, &footsize);

    int actual_height = pillar_height - kOverlap;
    int ypos = kTotalHeight - kOverlap;

    while (ypos + kOverlap < desired_height) {
        if (ypos + actual_height > desired_height) {
            // The next draw will overflow the desired height
            int overflow = ypos + actual_height - desired_height;

            // We cut off the repeated lines to make them fit
            if (pillar_height - overflow > kStepHeight) {
                // Round to "step height"
                overflow += ((pillar_height - overflow) % kStepHeight);
            }
            pillar_height -= overflow;
            actual_height -= overflow;
            free(copied_pillars);
            copied_pillars = copy_lines_from_bitmap(kTopCut, pillar_height, &linessize);
        }
        // Draw the pillars
        draw_bitmap_on_bitmap(copied_pillars, linessize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
        ypos += actual_height;
    }
    free(copied_pillars);

    // Draw the shadow
    draw_hint_menu_feet_mac(foot, footsize, &pixmap, &pixlength, ypos - height_of_foot);
    free(foot);

    // Erase any garbage left at the bottom of the pixmap
    int height_of_bitmap = (pixlength / 4) / hw_screenwidth;
    erase_lines_in_bitmap(ypos, height_of_bitmap - ypos);
}



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

extern winid_t current_graphics_buf_win;

void clear_image_buffer(void) {
    if (pixmap != nullptr) {
        free(pixmap);
        pixmap = nullptr;
    }
}

int last_slideshow_pic = -1;

#define K_PIC_SWORD_MERLIN 3

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

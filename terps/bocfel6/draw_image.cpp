//
//  draw_image.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-17.
//

extern "C" {
#include "glkimp.h"
}
#include "image.h"
#include "draw_png.h"
#include "draw_apple_2.h"
#include "decompress_amiga.hpp"
#include "decompress_vga.hpp"
#include "writetiff.h"
#include "util.h"

#include "draw_image.hpp"

ImageStruct *raw_images;
int image_count;

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

float hw_screenwidth = 320;
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

void extract_palette(ImageStruct *image) {
    if (image && image->palette) {
        memcpy(global_palette, image->palette, 16 * 3);
    }
}

static char *create_temp_tiff_file_name(void) {
    fileref_t *fileref = glk_fileref_create_temp(fileusage_Data, NULL);
    const char *filename = glkunix_fileref_get_filename(fileref);
    size_t filenamelength = strlen(filename);
    char *tiffname = (char *)malloc(filenamelength + 6);
    memcpy(tiffname, filename, filenamelength + 1);
    glk_fileref_destroy(fileref);
    strncat(tiffname, ".tiff", 5);
    return tiffname;
}

extern glui32 user_selected_background;

void flush_bitmap(winid_t winid) {
    fprintf(stderr, "flush_bitmap win %d\n", winid->peer);
    if (winid->type != wintype_Graphics)
        fprintf(stderr, "ERROR: window is not graphics\n");
    if (pixmap == nullptr) {
        fprintf(stderr, "flush_bitmap: No pixmap\n");
        return;
    }

    char *filename = create_temp_tiff_file_name();
    writeToTIFF(filename, pixmap, pixlength, hw_screenwidth);

    float height = pixlength / hw_screenwidth / 4;

    win_purgeimage(600);
    win_loadimage(600, filename, 0, pixlength);
    free(filename);
    glk_window_fill_rect(winid, user_selected_background , 0, 0, gscreenw, gscreenh);
    win_drawimage(winid->peer, 0, 0, gscreenw, (float)gscreenw / hw_screenwidth / pixelwidth * height);
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

    int xpos, ypos;

    int width = smallbitmapwidth;
    int stride = width * 4;
    int height = smallbitmapsize / stride;

    fprintf(stderr, "draw_bitmap_on_bitmap: x: %d y %d width: %d height: %d smallbitmapsize: %d largebitmapsize: %d\n", x, y, width, height, smallbitmapsize, *largebitmapsize);

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x > largebitmapwidth)
        x = largebitmapwidth - width;
    int largebitmapheight = *largebitmapsize / largebitmapwidth;
    if (y > largebitmapheight)
        y = largebitmapheight - height;

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
        ypos = y + (i / stride);
        if (flipped)
            ypos = height + y - ypos - 1;
        xpos = x + (i % stride) / 4;

        if (xpos < x || xpos > width + x)
            continue;
        if (ypos >= height + y || ypos < y)
            break;
        pixptr = *largebitmap + ((ypos * largebitmapwidth + xpos) * 4);
        if (pixptr - *largebitmap + 3 > *largebitmapsize || i + 3 > smallbitmapsize) {
            break;
        // Skip transparent pixels
        } else if (*(smallbitmap + i + 3) != 0) {
            memcpy(pixptr, smallbitmap + i, 4);
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


int32_t monochrome_black = 0;
int32_t monochrome_white = 0xffffff;

static uint8_t *draw_opaque_cga(ImageStruct *image) {
    if (image->data == nullptr)
        return nullptr;

    int paddedwidth = image->width;

    // Opaque CGA images pad their stride to a multiple of 8
    if (paddedwidth % 8 != 0) {
        paddedwidth = (paddedwidth / 8 + 1) * 8;
    }

    size_t size = paddedwidth * image->height;
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
            if (byte >> (7-j) & 1) {
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

static uint8_t *draw_amiga_mac_cga_ega_vga(ImageStruct *image) {
    if (image->data == nullptr)
        return nullptr;

    uint8_t colourmap[16][3] = {};

    unsigned colours = 14;

    if (image->palette != nullptr) {
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
        // Image has no palette, copy previously used palette
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

static uint8_t *decompress_image(ImageStruct *image) {
    uint8_t *result = nullptr;
    switch (image->type) {
        case kGraphicsTypeApple2:
            result = draw_apple2(image);
            break;
        case kGraphicsTypeBlorb:
            result = draw_png(image);
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
            result = draw_amiga_mac_cga_ega_vga(image);
            break;
        default:
            break;
    }
    return result;
}
void ensure_pixmap(winid_t winid) {
    if (pixmap == nullptr) {
        win_sizewin(winid->peer, 0, 0, gscreenw, gscreenh);
        glk_window_fill_rect(winid, gbgcol, 0, 0, gscreenw, gscreenh);
        pixmap = (uint8_t *)calloc(1, pixlength);
    }
}


void draw_to_pixmap(ImageStruct *image, uint8_t **pixmap, int *pixmapsize, int screenwidth, int x, int y, float xscale, float yscale) {
    uint8_t *result = decompress_image(image);
    if (result != nullptr) {
        extract_palette(image);
        draw_bitmap_on_bitmap(result, image->width * image->height * 4, image->width, pixmap, pixmapsize, screenwidth, (float)x / xscale, (float)y / yscale, false);
        free(result);
    }
}

void draw_to_buffer(winid_t winid, int picnum, int x, int y) {
    fprintf(stderr, "draw_to_buffer %d picnum %d x: %d y: %d\n", winid->peer, picnum, x, y);

    ImageStruct *image = find_image(picnum);
    if (image == nullptr)
        return;

    ensure_pixmap(winid);
    draw_to_pixmap(image, &pixmap, &pixlength, hw_screenwidth, x, y, imagescalex, imagescaley);
}

void draw_inline_image(winid_t winid, glui32 picnum, glsi32 x, glsi32 y,  float scalefactor, bool flipped) {
    fprintf(stderr, "Drawing image %d scaled to %f\n", picnum, scalefactor);

    ImageStruct *image = find_image(picnum);
    if (image == nullptr)
        return;

    uint8_t *result = decompress_image(image);
    if (result == nullptr)
        return;

    extract_palette(image);
    int32_t pixmapsize = image->width * image->height * 4;

    char *filename = create_temp_tiff_file_name();

    if (flipped) {
        result = flip_bitmap(image, result);
    }

    writeToTIFF(filename, result, pixmapsize, image->width);
    free(result);

    win_purgeimage(picnum);
    win_loadimage(picnum, filename, 0, pixmapsize);
    free(filename);
    float xscalefactor = scalefactor * pixelwidth;
    fprintf(stderr, "draw_inline_image: image width = %d * scale %f == %f\n", image->width, xscalefactor, image->width * xscalefactor);
    if (x < 0)
        x = 0;
    win_drawimage(winid->peer, x, y, image->width * xscalefactor, image->height * scalefactor);
}

void draw_to_pixmap_unscaled(int image, int x, int y) {
    ImageStruct *img = find_image(image);
    if (img != nullptr)
        draw_to_pixmap(img, &pixmap, &pixlength, hw_screenwidth, x, y, 1, 1);
}

extern int arthur_pic_top_margin;

void draw_arthur_side_images(winid_t winid) {
    ensure_pixmap(winid);

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

    // 54 is top border image

    draw_to_pixmap_unscaled(54, left_margin, top_margin);

    int x_margin = 0, y_margin = 0;
    get_image_size(100, &x_margin, &y_margin);

    // images 170 and 171 are side bar images

    draw_to_pixmap_unscaled(170, left_margin + 1 + left_offset, y_margin + top_margin - 2);
    draw_to_pixmap_unscaled(171, hw_screenwidth - x_margin + right_offset, y_margin + top_margin - 2);

    flush_bitmap(winid);
}


void clear_image_buffer(void) {
    fprintf(stderr, "Clearing buffer!\n");
    if (pixmap != nullptr) {
        free(pixmap);
        pixmap = nullptr;
    }
}

int last_slideshow_pic = -1;

void draw_centered_title_image(int picnum) {
    int x, y, width, height;
    get_image_size(picnum, &width, &height);
    ZASSERT(width <= hw_screenwidth, "image too wide");
    x = (hw_screenwidth - width) / 2;
    y = (gscreenh / imagescaley - height) / 2;
    draw_to_pixmap_unscaled(picnum, x, y);
    last_slideshow_pic = picnum;
    fprintf(stderr, "draw_centered_title_image: hw_screenwidth: %f image width: %d x: %d y: %d graphics_type: %d\n", hw_screenwidth, width, x, y, graphics_type);
}

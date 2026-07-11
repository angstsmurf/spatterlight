//
//  draw_image.cpp
//  Bocfel
//
//  Created by Administrator on 2023-07-17.
//
//  Bitmap compositing core for Infocom V6 graphics. Owns a single 32-bit
//  RGBA "pixmap" sized to the original hardware screen, into which all
//  image decoders write. Higher-level code calls draw_to_buffer /
//  draw_to_pixmap_* / extend_*_pillars to build up a frame, then
//  flush_bitmap() encodes the pixmap as a TIFF and hands it to the Glk
//  graphics window.
//

extern "C" {
#include "glkimp.h"
}
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstring>
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

// Loaded image table: parsed once at startup from the game's Blorb/image
// resources. find_image() looks images up here by picture number.
ImageStruct *raw_images;
int image_count;

// Set whenever something has changed the pixmap; cleared by flush_bitmap().
// When true, the next flush will glk_window_clear() the destination so
// stale pixels don't bleed through.
bool image_needs_redraw = true;

// Channel indices into an [R,G,B] color triplet.
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

// Width in source pixels of the original target hardware screen. Drawing
// is done in this coordinate space and scaled up to the Glk window by
// flush_bitmap(). pixelwidth is the aspect-ratio correction factor used
// for platforms whose pixels weren't square (e.g. CGA 2:1).
float hw_screenwidth = 320.0;
float pixelwidth = 1.0;

// Linear scan of the loaded image table for the entry whose Z-machine
// picture number matches picnum. Returns nullptr if no such image exists.
ImageStruct *find_image(int picnum) {
    for (int i = 0; i < image_count; i++) {
        if (raw_images[i].index == picnum)
            return &raw_images[i];
    }
    return nullptr;
}

// All bitmaps in this file are stored as interleaved RGBA — four bytes
// per pixel.
static constexpr int kBytesPerPixel = 4;

// Shared RGBA backing buffer all drawing routines composite into.
// Lazily allocated by ensure_pixmap(); freed by clear_image_buffer().
// Declared in draw_image.hpp so draw_border.cpp can read/write it too.
uint8_t *pixmap = nullptr;

// Current size of `pixmap` in bytes (NOT pixels). Grown by drawing routines
// when an image extends below the currently-allocated area.
int pixlength = hw_screenwidth * 200 * kBytesPerPixel;

// Effective foreground/background for monochrome (Mac B/W, CGA) artwork.
// Default to pure black/white but may be overwritten with theme colors.
int32_t monochrome_black = 0;
int32_t monochrome_white = 0xffffff;

// Lazily allocate the global pixmap and size the graphics window to fit
// the full screen. Safe to call repeatedly; a no-op once the pixmap exists.
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

// Serialize an RGBA buffer to a TIFF file at `name`. Used to hand the
// pixmap to the Glk image cache via a temp file. Silently no-ops if
// `size` doesn't fit in the uint32_t TIFF row count; logs to stderr on
// fopen failure with errno detail.
static void write_to_tiff(const char *name, uint8_t *data, size_t size, uint32_t width) {
    if (size > UINT32_MAX) {
        fprintf(stderr, "write_to_tiff: buffer too large (%zu bytes)\n", size);
        return;
    }

    FILE *fptr = fopen(name, "wb");
    if (fptr == nullptr) {
        fprintf(stderr, "write_to_tiff: fopen(\"%s\") failed: %s\n", name, strerror(errno));
        return;
    }

    writetiff(fptr, data, (uint32_t)size, width);

    fclose(fptr);
}

// Color-table layout shared by the legacy-platform decoders.
//
// Slots 0 and 1 are reserved as sentinels: index 0 means "transparent" and
// index 1 is the border color (handled by the caller). Actual palette
// entries therefore begin at slot 2, which is why several code paths below
// write to &color_table[kReservedPaletteSlots][...].
//
// On disk, both image->palette and the module-shared global_palette use the
// same legacy layout: a 1-byte count at offset 0 followed by interleaved
// RGB triplets, hence the `&...[1]` skip when copying from either source.
static constexpr int kPaletteSize = 16;
static constexpr int kReservedPaletteSlots = 2;
static constexpr unsigned kMaxPaletteColors = kPaletteSize - kReservedPaletteSlots;
static constexpr int kBytesPerColor = 3;

// Copy `image`'s 16-entry RGB palette into the module-shared global_palette
// (defined in draw_png.cpp) so subsequent decoders can resolve color
// indices. No-op if the image has no palette.
//
// Copying a fixed kPaletteSize*kBytesPerColor (48) bytes is safe because every
// producer of image->palette allocates at least that many: Blorb PNGs go
// through extract_palette_from_png_data(), which always calloc()s the full
// kMaxPaletteEntries*kBytesPerColor (48, zero-padded) no matter how few PLTE
// entries the file has, and the legacy per-image path allocates
// kColorPaletteSize (512). Keep that invariant if a new palette source is added.
void extract_palette(ImageStruct *image) {
    if (image && image->palette) {
        memcpy(global_palette, image->palette, kPaletteSize * kBytesPerColor);
    }
}

// Generate a unique filename ending in ".tiff" by taking a Glk temp file
// path and appending the extension. The underlying temp file is deleted
// immediately so only the path string is reused. Caller frees with free().
// Returns nullptr on failure.
//
// NOTE: each call must return a *distinct* path. win_purgeimage() only sends
// the filename to the host over IPC (see connect.c); the host reads the TIFF
// asynchronously, so reusing one path across images lets a later flush
// clobber a file the host hasn't read yet, producing broken/garbled images.
static char *create_temp_tiff_file_name(void) {
    fileref_t *fileref = glk_fileref_create_temp(fileusage_Data, NULL);
    if (fileref == nullptr)
        return nullptr;
    const char *filename = glkunix_fileref_get_filename(fileref);
    size_t filenamelength = strlen(filename);
    char *tiffname = (char *)malloc(filenamelength + 6);
    if (tiffname != nullptr) {
        memcpy(tiffname, filename, filenamelength + 1);
        strncat(tiffname, ".tiff", 5);
    }
    glk_fileref_delete_file(fileref);
    glk_fileref_destroy(fileref);
    return tiffname;
}

// Commit the accumulated pixmap to the Glk graphics window: write it out
// as a TIFF, hand it to the image cache under a fixed resource id (600),
// and stretch-blit it to fill the destination. If image_needs_redraw is
// set, the window is cleared first so previous frames don't show through.
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
    write_to_tiff(filename, pixmap, pixlength, hw_screenwidth);
    win_purgeimage(600, filename, pixlength);
    free(filename);
    win_drawimage(winid->peer, 0, 0, gscreenw, (float)gscreenw / pixelwidth * pixlength / (kBytesPerPixel * hw_screenwidth * hw_screenwidth));
    image_needs_redraw = false;
}

// Look up an image by picture number and return its native dimensions in
// hw_screen pixels through the (optionally non-null) width/height out-params.
// Returns true if the image exists; on miss, returns false and zeroes the
// outputs.
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
void draw_bitmap_on_bitmap(const uint8_t *src_pixels, int src_buffer_size, int src_width,
                           uint8_t **dst_pixels, int *dst_buffer_size, int dst_width,
                           int dest_x, int dest_y, bool flipped) {
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

    // Nothing to composite if the source lands entirely above the canvas.
    if (dest_y + src_height <= 0)
        return;

    // Extend destination buffer downward if necessary. Compute the required
    // size in size_t so the product can't wrap to a negative int and defeat
    // the grow check (which would let the row loop write past the allocation).
    const size_t required_size = (size_t)dst_width * (size_t)(dest_y + src_height) * kBytesPerPixel;
    if (required_size > INT_MAX)
        return;
    if ((size_t)*dst_buffer_size < required_size) {
        uint8_t *expanded = (uint8_t *)calloc(1, required_size);
        if (expanded == nullptr)
            return;
        memcpy(expanded, *dst_pixels, *dst_buffer_size);
        *dst_buffer_size = (int)required_size;
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

// Fill an opaque rectangle on the global pixmap at (dest_x, dest_y).
// Clamps dest_x into bounds, grows the pixmap downward if rect_height
// runs past the current allocation, and refuses requests wider than the
// hardware screen.
void draw_rectangle_on_bitmap(glui32 color, int dest_x, int dest_y, int rect_width, int rect_height) {
    int screen_width = (int)hw_screenwidth;

    if (rect_width <= 0 || rect_height <= 0 || rect_width > screen_width)
        return;

    if (dest_x < 0)
        dest_x = 0;
    if (dest_x > screen_width - rect_width)
        dest_x = screen_width - rect_width;

    // Nothing to fill if the rectangle lands entirely above the canvas.
    if (dest_y + rect_height <= 0)
        return;

    // Extend pixmap downward if necessary. size_t product avoids a negative
    // wrap that would skip the grow and let the fill loop run past the buffer.
    size_t required_size = (size_t)screen_width * (size_t)(dest_y + rect_height) * kBytesPerPixel;
    if (required_size > INT_MAX)
        return;
    if ((size_t)pixlength < required_size) {
        uint8_t *expanded = (uint8_t *)calloc(1, required_size);
        if (expanded == nullptr)
            return;
        memcpy(expanded, pixmap, pixlength);
        pixlength = (int)required_size;
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

// Return a freshly-allocated, vertically-flipped copy of `source` (an
// image->width × image->height RGBA buffer). Frees the original `source`
// either way. Returns nullptr on OOM.
static uint8_t *flip_bitmap(ImageStruct *image, uint8_t *source) {
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

// Decode an opaque CGA image into an RGBA buffer. Opaque CGA stores 8
// pixels per byte (one bit per pixel), with each row padded to a multiple
// of 8 pixels. We temporarily widen image->width so the VGA decompressor
// allocates the padded stride, expand bits to monochrome RGBA, then trim
// the padding columns off each row. Caller frees the returned buffer.
static uint8_t *draw_opaque_cga(ImageStruct *image) {
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

// Fill `color_table` with the appropriate 16-entry palette for `image`,
// honoring use_previous_palette as a request to reuse the currently active
// global_palette instead of the image's own palette.
static void populate_color_table(ImageStruct *image, bool use_previous_palette,
                                 uint8_t color_table[kPaletteSize][kBytesPerColor]) {
    memset(color_table, 0, kPaletteSize * kBytesPerColor);

    if (image->palette != nullptr && !use_previous_palette) {
        unsigned palette_color_count = image->palette[0];
        /* Fix for some buggy _Arthur_ pictures. */
        if (palette_color_count > kMaxPaletteColors)
            palette_color_count = kMaxPaletteColors;
        memcpy(&color_table[kReservedPaletteSlots][RED], &image->palette[1],
               palette_color_count * kBytesPerColor);
        return;
    }

    switch (image->type) {
        case kGraphicsTypeMacBW:
        case kGraphicsTypeCGA:
            color_table[2][RED]   = monochrome_white >> 16;
            color_table[2][GREEN] = (monochrome_white >> 8) & 0xff;
            color_table[2][BLUE]  = monochrome_white & 0xff;

            color_table[3][RED]   = monochrome_black >> 16;
            color_table[3][GREEN] = (monochrome_black >> 8) & 0xff;
            color_table[3][BLUE]  = monochrome_black & 0xff;
            break;
        case kGraphicsTypeEGA:
            memcpy(color_table, ega_colormap, kPaletteSize * kBytesPerColor);
            break;
        default:
            memcpy(&color_table[kReservedPaletteSlots][RED], &global_palette[1],
                   kMaxPaletteColors * kBytesPerColor);
            break;
    }
}

// Decode an indexed image for any of the legacy platforms (Amiga, Mac B/W,
// CGA, EGA, VGA). Dispatches to decompress_vga or decompress_amiga, then
// expands color indices to RGBA using populate_color_table(). Honors
// image->transparency. Caller frees the returned buffer.
static uint8_t *draw_amiga_mac_cga_ega_vga(ImageStruct *image, bool use_previous_palette) {
    if (image == nullptr || image->data == nullptr)
        return nullptr;

    uint8_t color_table[kPaletteSize][kBytesPerColor];
    populate_color_table(image, use_previous_palette, color_table);

    uint8_t *decompressed = nullptr;
    switch (image->type) {
        case kGraphicsTypeVGA:
        case kGraphicsTypeEGA:
        case kGraphicsTypeCGA:
            decompressed = decompress_vga(image);
            break;
        case kGraphicsTypeAmiga:
        case kGraphicsTypeMacBW:
            decompressed = decompress_amiga(image);
            break;
        default:
            fprintf(stderr, "draw_amiga_mac_cga_ega_vga: unsupported image type %d\n", image->type);
            return nullptr;
    }

    if (decompressed == nullptr)
        return nullptr;

    size_t pixel_count = (size_t)image->width * (size_t)image->height;
    size_t rgba_size = pixel_count * kBytesPerPixel;
    uint8_t *rgba_buffer = (uint8_t *)malloc(rgba_size);
    if (rgba_buffer == nullptr) {
        free(decompressed);
        return nullptr;
    }

    for (size_t pixel_index = 0; pixel_index < pixel_count; pixel_index++) {
        uint8_t color_index = decompressed[pixel_index];
        size_t write_offset = pixel_index * kBytesPerPixel;
        if (color_index >= kPaletteSize ||
            (image->transparency && image->transparent_color == color_index)) {
            rgba_buffer[write_offset + 0] = 0;
            rgba_buffer[write_offset + 1] = 0;
            rgba_buffer[write_offset + 2] = 0;
            rgba_buffer[write_offset + 3] = 0;
            continue;
        }
        rgba_buffer[write_offset + 0] = color_table[color_index][RED];
        rgba_buffer[write_offset + 1] = color_table[color_index][GREEN];
        rgba_buffer[write_offset + 2] = color_table[color_index][BLUE];
        rgba_buffer[write_offset + 3] = 0xff;
    }

    free(decompressed);
    return rgba_buffer;
}

// Dispatch image decoding to the right backend based on image->type and,
// for CGA, whether the image is opaque or transparent. Returns a freshly
// allocated RGBA buffer (caller frees) or nullptr on unsupported types.
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

// Convenience wrapper: look up an image by number and load its palette as
// the active global_palette. No-op if the image isn't found.
void extract_palette_from_picnum(int picnum) {
    ImageStruct *image = find_image(picnum);
    if (image != nullptr) {
        extract_palette(image);
    }
}

// Workhorse: decode an image and composite it at (x, y) into the caller's
// destination pixmap. x and y are in display coordinates; they get divided
// by xscale/yscale to land at the right offset in the hw_screen-space
// destination. If use_previous_palette is false the image's palette is
// also loaded as the new global_palette.
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

// draw_to_pixmap_palette_optional with palette replacement enabled.
void draw_to_pixmap(ImageStruct *image, uint8_t **pixmap, int *pixmapsize, int screenwidth, int x, int y, float xscale, float yscale, bool flipped) {
    draw_to_pixmap_palette_optional(image, pixmap, pixmapsize, screenwidth, x, y, xscale, yscale, flipped, false);
}

// As draw_to_pixmap, but keeps whatever palette is already loaded. Used
// when compositing several images that should share a single palette.
void draw_to_pixmap_using_current_palette(ImageStruct *image, uint8_t **pixmap, int *pixmapsize, int screenwidth, int x, int y, float xscale, float yscale, bool flipped) {
    draw_to_pixmap_palette_optional(image, pixmap, pixmapsize, screenwidth, x, y, xscale, yscale, flipped, true);
}

// Top-level "draw image N at (x, y)" entry point. Resolves the picture
// number, ensures the global pixmap is allocated for `winid`, and composites
// into it at the configured screen scaling factors.
void draw_to_buffer(winid_t winid, int picnum, int x, int y) {
    ImageStruct *image = find_image(picnum);
    if (image == nullptr)
        return;

    ensure_pixmap(winid);
    draw_to_pixmap(image, &pixmap, &pixlength, hw_screenwidth, x, y, imagescalex, imagescaley, false);
}

// Re-render image `picnum` (optionally flipped) into a TIFF and register
// it with the Glk image cache under its picture number, so subsequent
// win_drawimage() calls can find it. Returns the underlying ImageStruct
// (or nullptr on failure) so callers can read its dimensions.
ImageStruct *recreate_image(glui32 picnum, int flipped) {
    ImageStruct *image = find_image(picnum);
    if (image == nullptr)
        return nullptr;

    uint8_t *result = decompress_image(image, false);
    if (result == nullptr)
        return nullptr;

    extract_palette(image);
    int32_t pixmapsize = image->width * image->height * kBytesPerPixel;

    char *filename = create_temp_tiff_file_name();

    if (filename == nullptr)
        return nullptr;

    if (flipped) {
        result = flip_bitmap(image, result);
    }

    write_to_tiff(filename, result, pixmapsize, image->width);
    free(result);

    win_purgeimage(picnum, filename, pixmapsize);
    free(filename);
    return image;
}

// Decode `picnum` into the Glk image cache and immediately blit it to
// `winid` at (x, y), scaled by `scalefactor` (with pixelwidth applied on
// the x-axis to correct for non-square source pixels). Used to draw images
// that go directly into a text buffer or graphics window without first
// being composited into the global pixmap.
void draw_inline_image(winid_t winid, glui32 picnum, glsi32 x, glsi32 y, float scalefactor, bool flipped) {
    ImageStruct *image = recreate_image(picnum, flipped);
    if (image == nullptr)
        return;

    float xscalefactor = scalefactor * pixelwidth;

    if (x < 0)
        x = 0;

    win_drawimage(winid->peer, x, y, image->width * xscalefactor, image->height * scalefactor);
}

// Draw image `image` at (x, y) into the global pixmap with no scaling and
// load its palette as the new active palette. Coordinates are in hw_screen
// pixels.
void draw_to_pixmap_unscaled(int image, int x, int y) {
    ImageStruct *img = find_image(image);
    if (img != nullptr)
        draw_to_pixmap(img, &pixmap, &pixlength, hw_screenwidth, x, y, 1, 1, false);
}

// Like draw_to_pixmap_unscaled but keeps the currently active palette.
void draw_to_pixmap_unscaled_using_current_palette(int image, int x, int y) {
    ImageStruct *img = find_image(image);
    if (img != nullptr)
        draw_to_pixmap_using_current_palette(img, &pixmap, &pixlength, hw_screenwidth, x, y, 1, 1, false);
}

// Like draw_to_pixmap_unscaled_using_current_palette but vertically flipped.
// Used for tiled fills where alternating tiles should be mirrored.
void draw_to_pixmap_unscaled_flipped_using_current_palette(int image, int x, int y) {
    ImageStruct *img = find_image(image);
    if (img != nullptr)
        draw_to_pixmap_using_current_palette(img, &pixmap, &pixlength, hw_screenwidth, x, y, 1, 1, true);
}

// Discard the global pixmap so the next ensure_pixmap() reallocates a
// fresh zeroed buffer. Used when switching display modes or starting a
// new fullscreen scene.
void clear_image_buffer(void) {
    if (pixmap != nullptr) {
        free(pixmap);
        pixmap = nullptr;
    }
}

// Picture number of the most recently displayed centered slideshow image,
// or -1 if no slideshow image is currently shown. Read by resize logic to
// redraw the last image at the new size.
int last_slideshow_pic = -1;

// Draw image `picnum` centered in the Glk graphics window at the given
// scale. If width/height are 0 they're looked up from the image; if the
// image still has no dimensions the call is silently dropped. Updates
// last_slideshow_pic so resize handlers know what to restore.
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

// Draw a title screen image as large as it will fit in the current
// graphics window while preserving aspect ratio (limited by both width
// and height). Returns the chosen scale factor so callers can mirror it
// for related overlays.
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

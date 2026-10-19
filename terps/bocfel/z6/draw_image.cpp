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
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
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
// http://en.wikipedia.org/wiki/Enhanced_Graphics_Adapter. It is the same
// as Mcga_palette in Infocom's IBM interpreter sources (data.c), scaled
// from 6 to 8 bits per channel.
static const uint8_t ega_colormap[16][3] =
{
    {   0,  0,  0 },
    {   0,  0,170 },
    {   0,170,  0 },
    {   0,170,170 },
    { 170,  0,  0 },
    { 170,  0,170 },
    { 170, 85,  0 }, // Brown. pix2gif has { 170,170, 0 } here, but Infocom's own table agrees with this.
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
int hw_screenwidth = 320;
float pixelwidth = 1.0;

// Height in rows of a newly made pixmap. Drawing below that grows it.
static constexpr int kInitialPixmapRows = 200;

void set_hw_screen_geometry(GraphicsType type) {
    switch (type) {
        case kGraphicsTypeBlorb:
        case kGraphicsTypeVGA:
        case kGraphicsTypeAmiga:
        case kGraphicsTypeNoGraphics:
            hw_screenwidth = 320;
            pixelwidth = 1.0;
            break;
        case kGraphicsTypeMacBW:
            hw_screenwidth = 480;
            pixelwidth = 1.0;
            break;
        case kGraphicsTypeEGA:
        case kGraphicsTypeCGA:
            hw_screenwidth = 640;
            pixelwidth = 0.5;
            break;
        case kGraphicsTypeApple2:
            hw_screenwidth = 140;
            pixelwidth = 2.0;
            break;
    }
    // What was drawn for a screen of another width means nothing now.
    if (pixmap.exists() && pixmap.width_px != hw_screenwidth)
        pixmap.create(hw_screenwidth, kInitialPixmapRows);
}

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
// per pixel (kBytesPerPixel, from v6_image.h).

// Shared RGBA backing buffer all drawing routines composite into.
// Lazily made by ensure_pixmap(); discarded by clear_image_buffer().
// Declared in draw_image.hpp so draw_border.cpp can read/write it too.
Pixmap pixmap;

void Pixmap::create(int width, int height) {
    data.assign(image_buffer_size(width, height, kBytesPerPixel), 0);
    width_px = data.empty() ? 0 : width;
}

void Pixmap::clear() {
    data.clear();
    width_px = 0;
}

bool Pixmap::ensure_rows(int count) {
    if (!exists())
        return false;
    size_t required_size = image_buffer_size(width_px, count, kBytesPerPixel);
    if (required_size == 0)
        return false;
    if (data.size() < required_size)
        data.resize(required_size, 0);
    return true;
}

void Pixmap::clip_to_height(int count) {
    if (count > 0 && count < rows())
        data.resize((size_t)count * stride());
}

void Pixmap::erase_rows(int y, int count) {
    if (y < 0 || count <= 0 || y >= rows())
        return;
    count = std::min(count, rows() - y);
    memset(row(y), 0, (size_t)count * stride());
}

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
    if (!pixmap.exists()) {
        win_sizewin(winid->peer, 0, 0, gscreenw, gscreenh);
        glk_window_set_background_color(winid, user_selected_background);
        pixmap.create(hw_screenwidth, kInitialPixmapRows);
    }
}

// Serialize an RGBA buffer to a TIFF file at `path`. Used to hand the
// pixmap to the Glk image cache via a temp file. Silently no-ops if
// `size` doesn't fit in the uint32_t TIFF row count; logs to stderr on
// fopen failure with errno detail.
static void write_to_tiff(const std::string &path, const uint8_t *data, size_t size, uint32_t width) {
    if (size > UINT32_MAX) {
        fprintf(stderr, "write_to_tiff: buffer too large (%zu bytes)\n", size);
        return;
    }

    FILE *fptr = fopen(path.c_str(), "wb");
    if (fptr == nullptr) {
        fprintf(stderr, "write_to_tiff: fopen(\"%s\") failed: %s\n", path.c_str(), strerror(errno));
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
// For these decoders image->palette and the module-shared global_palette
// both have the legacy layout (see ImageStruct::palette): a 1-byte count at
// offset 0 followed by interleaved RGB triplets, hence the `&...[1]` skip
// when copying from either source.
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
// immediately so only the path string is reused. Returns an empty string on
// failure.
//
// NOTE: each call must return a *distinct* path. win_purgeimage() only sends
// the filename to the host over IPC (see connect.c); the host reads the TIFF
// asynchronously, so reusing one path across images lets a later flush
// clobber a file the host hasn't read yet, producing broken/garbled images.
static std::string create_temp_tiff_file_name(void) {
    fileref_t *fileref = glk_fileref_create_temp(fileusage_Data, NULL);
    if (fileref == nullptr)
        return "";
    std::string tiffname = glkunix_fileref_get_filename(fileref);
    tiffname += ".tiff";
    glk_fileref_delete_file(fileref);
    glk_fileref_destroy(fileref);
    return tiffname;
}

// What was last sent to the Glk image cache under each picture number: a
// digest of the RGBA pixels and their size. The host keeps the decoded
// image under that number until the next purge, so when a redraw would send
// identical pixels, the purge (a TIFF write, a message carrying it and a
// decode on the host side) can be replaced by a win_findimage() that makes
// the host's cached copy the current image. Zork Zero recreates its margin
// images on every draw and flushes an unchanged pixmap on most turns. The
// digest is taken of the output rather than of the inputs because the
// pixels depend on the palette and colour state of the moment.
namespace {

struct SentImage {
    uint64_t digest;
    int width;
    int height;
};

std::unordered_map<glui32, SentImage> sent_images;

// 64-bit FNV-1a.
uint64_t digest_pixels(const uint8_t *data, size_t size) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool operator==(const SentImage &a, const SentImage &b) {
    return a.digest == b.digest && a.width == b.width && a.height == b.height;
}

// Write `size` bytes of RGBA pixels to a TIFF and hand it to the host under
// `picnum`. Returns false if no temp file could be created.
bool purge_image(glui32 picnum, const uint8_t *data, size_t size, int width) {
    std::string filename = create_temp_tiff_file_name();
    if (filename.empty())
        return false;

    write_to_tiff(filename, data, size, width);
    win_purgeimage(picnum, filename.c_str());
    return true;
}

// Hand `size` bytes of RGBA pixels to the host under `picnum`, or, when the
// host already holds these exact pixels under that number, just make them
// its current image again: win_drawimage() draws whatever was last found,
// loaded or purged. Returns false if no temp file could be created.
bool send_image(glui32 picnum, const uint8_t *data, size_t size, int width, int height) {
    SentImage sent = { digest_pixels(data, size), width, height };
    auto previous = sent_images.find(picnum);
    if (previous != sent_images.end() && previous->second == sent && win_findimage(picnum)) {
        return true;
    }

    if (!purge_image(picnum, data, size, width))
        return false;
    // win_purgeimage() sends nothing while graphics are off, so the host
    // has nothing to hit then.
    if (gli_enable_graphics) {
        sent_images[picnum] = sent;
    } else {
        sent_images.erase(picnum);
    }
    return true;
}

// The composited pixmap is sent through a ring of resource numbers rather
// than one, because its contents alternate between a handful of states
// rather than staying put: the compass in the Zork Zero border lights the
// exits of the current room, so a walk between rooms cycles through as
// many pixmaps as there are exit patterns, and a single memo would miss
// every one of them. Each slot remembers what the host holds under its
// number; a flush that matches a slot finds it, otherwise it takes the
// slot used longest ago. The numbers lie above every picture number the
// games use.
constexpr glui32 kPixmapResourceBase = 600;
constexpr size_t kPixmapRingSize = 32;

struct PixmapSlot {
    SentImage sent;
    uint64_t last_used;
    bool held;
};

PixmapSlot pixmap_ring[kPixmapRingSize];
uint64_t pixmap_ring_clock;

void forget_pixmap_ring(void) {
    for (auto &slot : pixmap_ring) {
        slot.held = false;
    }
}

// Like send_image(), for the pixmap: find it under the slot that already
// holds these pixels, or purge it into the least recently used slot.
// Returns the resource number it was drawn from, or 0 on failure.
glui32 send_pixmap(const uint8_t *data, size_t size, int width, int height) {
    SentImage sent = { digest_pixels(data, size), width, height };
    PixmapSlot *oldest = nullptr;
    for (size_t i = 0; i < kPixmapRingSize; i++) {
        PixmapSlot &slot = pixmap_ring[i];
        glui32 resno = kPixmapResourceBase + (glui32)i;
        if (slot.held && slot.sent == sent) {
            if (win_findimage(resno)) {
                slot.last_used = ++pixmap_ring_clock;
                return resno;
            }
            // The host has let it go.
            slot.held = false;
        }
        // A free slot beats any held one; among held ones, the least
        // recently used.
        if (oldest == nullptr ||
            (!slot.held && oldest->held) ||
            (slot.held == oldest->held && slot.last_used < oldest->last_used)) {
            oldest = &slot;
        }
    }

    glui32 resno = kPixmapResourceBase + (glui32)(oldest - pixmap_ring);
    if (!purge_image(resno, data, size, width))
        return 0;
    oldest->sent = sent;
    oldest->last_used = ++pixmap_ring_clock;
    oldest->held = gli_enable_graphics;
    return resno;
}

} // namespace

// Forget what the host has been sent. Call whenever the image table is
// replaced: the same picture number may then stand for another picture.
void forget_sent_images(void) {
    sent_images.clear();
    forget_pixmap_ring();
}

// Commit the accumulated pixmap to the Glk graphics window: hand it to
// the image cache under one of the pixmap ring's resource numbers (see
// send_pixmap()) and stretch-blit it to fill the destination. If
// image_needs_redraw is set, the window is cleared first so previous
// frames don't show through.
void flush_bitmap(winid_t winid) {
    if (winid == nullptr)
        return;
    if (winid->type != wintype_Graphics) {
        fprintf(stderr, "ERROR: window is not graphics\n");
    }
    if (!pixmap.exists()) {
        glk_window_clear(winid);
        return;
    }

    glk_window_set_background_color(winid, user_selected_background);
    if (image_needs_redraw)
        glk_window_clear(winid);

    if (send_pixmap(pixmap.data.data(), pixmap.data.size(), pixmap.width_px, pixmap.rows()) == 0)
        return;
    // Stretch the pixmap to the full window width; its height follows from
    // the number of pixmap rows and the pixel aspect ratio.
    win_drawimage(winid->peer, 0, 0, gscreenw, (float)gscreenw / pixelwidth * pixmap.rows() / pixmap.width_px, 0, 0);
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

// The part draw() and fill() share: place a width x height block at (*x, y).
// Clamps *x so the block stays inside the pixmap's width and grows the pixmap
// downward to hold the block's last row. Returns false if there is nothing to
// draw: the block is wider than the pixmap, lies wholly above it, or the
// pixmap could not be grown.
bool Pixmap::prepare_dest(int *x, int y, int width, int height) {
    if (width > width_px)
        return false;
    if (*x < 0)
        *x = 0;
    if (*x > width_px - width)
        *x = width_px - width;

    if (y + height <= 0)
        return false;

    return ensure_rows(y + height);
}

// Composites a source bitmap onto the pixmap at position (x, y).
// If flipped is true, source rows are drawn in reverse order (vertical flip).
// Source pixels with zero alpha are treated as transparent and skipped.
// The pixmap is extended downward if too small.
void Pixmap::draw(const uint8_t *src, int src_size, int src_width, int x, int y, bool flipped) {
    if (src == nullptr || !exists() || src_width <= 0)
        return;

    image_needs_redraw = true;

    const int src_stride = src_width * kBytesPerPixel;
    const int src_height = src_size / src_stride;
    if (src_height <= 0)
        return;

    if (!prepare_dest(&x, y, src_width, src_height))
        return;

    for (int line = 0; line < src_height; line++) {
        const int draw_y = y + line;
        if (draw_y < 0)
            continue;

        const int src_row = flipped ? (src_height - 1 - line) : line;
        const uint8_t *src_line = src + src_row * src_stride;
        uint8_t *dst_line = pixel(x, draw_y);

        for (int col = 0; col < src_width; col++) {
            const int pixel_offset = col * kBytesPerPixel;
            if (src_line[pixel_offset + 3] != 0)
                memcpy(dst_line + pixel_offset, src_line + pixel_offset, kBytesPerPixel);
        }
    }
}

// Fill an opaque rectangle at (x, y). Clamps x into bounds, grows the pixmap
// downward if the rectangle runs past its last row, and refuses requests
// wider than the pixmap.
void Pixmap::fill(glui32 color, int x, int y, int width, int height) {
    if (!exists() || width <= 0 || height <= 0)
        return;
    if (!prepare_dest(&x, y, width, height))
        return;

    for (int line = std::max(0, -y); line < height; line++) {
        uint8_t *write_ptr = pixel(x, y + line);
        for (int col = 0; col < width; col++) {
            rgb_to_bytes(color, write_ptr);
            write_ptr[3] = 0xff;
            write_ptr += kBytesPerPixel;
        }
    }
}

// Fill an opaque rectangle on the global pixmap at (dest_x, dest_y), in
// hw_screen pixels.
void draw_rectangle_on_bitmap(glui32 color, int dest_x, int dest_y, int rect_width, int rect_height) {
    pixmap.fill(color, dest_x, dest_y, rect_width, rect_height);
}

// Read back an opaque pixel from the global pixmap as 0xRRGGBB.
// Returns false if (x, y) is out of bounds or the pixel is transparent,
// in which case *color is left untouched.
bool sample_pixmap_pixel(int x, int y, glui32 *color) {
    if (color == nullptr)
        return false;
    if (x < 0 || y < 0 || x >= pixmap.width_px || y >= pixmap.rows())
        return false;

    const uint8_t *pixel = pixmap.pixel(x, y);
    if (pixel[3] == 0)
        return false;

    *color = bytes_to_rgb(pixel);
    return true;
}

// Flip the image->width x image->height RGBA buffer `pixels` vertically,
// in place.
static void flip_bitmap(const ImageStruct *image, uint8_t *pixels) {
    size_t row_bytes = (size_t)image->width * kBytesPerPixel;
    for (int top = 0, bottom = image->height - 1; top < bottom; top++, bottom--) {
        std::swap_ranges(pixels + top * row_bytes, pixels + (top + 1) * row_bytes,
                         pixels + bottom * row_bytes);
    }
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
    ImageBuf decompressed(decompress_vga(image));
    image->width = original_width;

    if (decompressed == nullptr)
        return nullptr;

    size_t rgba_size = 0;
    ImageBuf rgba_buffer(image_alloc(padded_width, image->height, kBytesPerPixel, &rgba_size));
    if (rgba_buffer == nullptr)
        return nullptr;

    // Opaque CGA images store 8 pixels per byte
    // (unlike monochrome Mac images which use the same wasteful format as transparent ones)
    for (size_t byte_index = 0; byte_index < packed_size; byte_index++) {
        uint8_t packed_byte = decompressed[byte_index];
        for (int bit = 0; bit < kBitsPerByte; bit++) {
            size_t rgba_offset = (byte_index * kBitsPerByte + bit) * kBytesPerPixel;
            if (rgba_offset + 3 >= rgba_size)
                break;
            uint32_t color = (packed_byte >> (7 - bit) & 1) ? monochrome_white : monochrome_black;
            rgb_to_bytes(color, &rgba_buffer[rgba_offset]);
            rgba_buffer[rgba_offset + 3] = 0xff;
        }
    }

    if (padded_width == image->width)
        return rgba_buffer.release();

    // Shave off padding bits
    uint8_t *output = image_alloc(image->width, image->height, kBytesPerPixel, nullptr);
    if (output == nullptr)
        return nullptr;
    size_t row_bytes = (size_t)image->width * kBytesPerPixel;
    size_t padded_row_bytes = (size_t)padded_width * kBytesPerPixel;
    for (int row = 0; row < image->height; row++)
        memcpy(output + row_bytes * row, &rgba_buffer[padded_row_bytes * row], row_bytes);
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
        // The Arthur palettes claim 16 colors, two more than there is room
        // for after the reserved slots. Infocom's Amiga interpreter clamps
        // the count in the same way (ReadGFXPalette in gfx.c).
        if (palette_color_count > kMaxPaletteColors)
            palette_color_count = kMaxPaletteColors;
        memcpy(&color_table[kReservedPaletteSlots][RED], &image->palette[1],
               palette_color_count * kBytesPerColor);
        return;
    }

    switch (image->type) {
        case kGraphicsTypeMacBW:
        case kGraphicsTypeCGA:
            rgb_to_bytes(monochrome_white, color_table[2]);
            rgb_to_bytes(monochrome_black, color_table[3]);
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

uint8_t *expand_indexed(const uint8_t *indices, const ImageStruct *image, const uint8_t palette[16][3]) {
    if (indices == nullptr)
        return nullptr;

    // The same product the decoders size `indices` with, so the loop below
    // can't run off the end of it.
    size_t pixel_count = image_buffer_size(image->width, image->height, 1);
    uint8_t *rgba_buffer = image_alloc(image->width, image->height, kBytesPerPixel, nullptr);
    if (rgba_buffer == nullptr)
        return nullptr;

    for (size_t pixel_index = 0; pixel_index < pixel_count; pixel_index++) {
        uint8_t color_index = indices[pixel_index];
        if (color_index >= kPaletteSize ||
            (image->transparency && image->transparent_color == color_index)) {
            // Leave the pixel transparent black, as image_alloc() made it.
            continue;
        }
        uint8_t *pixel = rgba_buffer + pixel_index * kBytesPerPixel;
        memcpy(pixel, palette[color_index], kBytesPerColor);
        pixel[3] = 0xff;
    }
    return rgba_buffer;
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

    ImageBuf decompressed;
    switch (image->type) {
        case kGraphicsTypeVGA:
        case kGraphicsTypeEGA:
        case kGraphicsTypeCGA:
            decompressed.reset(decompress_vga(image));
            break;
        case kGraphicsTypeAmiga:
        case kGraphicsTypeMacBW:
            decompressed.reset(decompress_amiga(image));
            break;
        default:
            fprintf(stderr, "draw_amiga_mac_cga_ega_vga: unsupported image type %d\n", image->type);
            return nullptr;
    }

    return expand_indexed(decompressed.get(), image, color_table);
}

// Dispatch image decoding to the right backend based on image->type and,
// for CGA, whether the image is opaque or transparent. Unless
// use_previous_palette is set, the image's palette (if it has one) also
// becomes the active global_palette, for this image and the ones after it.
// Returns a freshly allocated RGBA buffer (caller frees) or nullptr on
// unsupported types.
static uint8_t *decompress_image(ImageStruct *image, bool use_previous_palette) {
    if (!use_previous_palette)
        extract_palette(image);

    switch (image->type) {
        case kGraphicsTypeApple2:
            return draw_apple2(image);
        case kGraphicsTypeBlorb:
            return draw_png(image);
        case kGraphicsTypeCGA:
            if (!image->transparency)
                return draw_opaque_cga(image);
            [[fallthrough]];
        case kGraphicsTypeEGA:
        case kGraphicsTypeVGA:
        case kGraphicsTypeMacBW:
        case kGraphicsTypeAmiga:
            return draw_amiga_mac_cga_ega_vga(image, use_previous_palette);
        default:
            return nullptr;
    }
}

// Convenience wrapper: look up an image by number and load its palette as
// the active global_palette. No-op if the image isn't found.
void extract_palette_from_picnum(int picnum) {
    ImageStruct *image = find_image(picnum);
    if (image != nullptr) {
        extract_palette(image);
    }
}

// Workhorse: decode picture `picnum` and composite it into the global pixmap
// with its top left corner at (x, y), in hw_screen pixels. Unless
// keep_palette is set, the image's palette also becomes the new
// global_palette. Does nothing if there is no such picture or no pixmap yet
// (see ensure_pixmap()).
static void draw_picture(int picnum, int x, int y, bool flipped, bool keep_palette) {
    ImageStruct *image = find_image(picnum);
    if (image == nullptr || !pixmap.exists())
        return;
    ImageBuf result(decompress_image(image, keep_palette));
    if (result == nullptr)
        return;
    int result_size = (int)image_buffer_size(image->width, image->height, kBytesPerPixel);
    pixmap.draw(result.get(), result_size, image->width, x, y, flipped);
}

// Top-level "draw image N at (x, y)" entry point, with x and y in display
// coordinates. Ensures the global pixmap is allocated for `winid` and
// composites the picture into it at the configured screen scaling factors.
void draw_to_buffer(winid_t winid, int picnum, int x, int y) {
    if (find_image(picnum) == nullptr)
        return;

    ensure_pixmap(winid);
    draw_picture(picnum, (float)x / imagescalex, (float)y / imagescaley, false, false);
}

// Re-render image `picnum` (optionally flipped) into a TIFF and register
// it with the Glk image cache under its picture number, so subsequent
// win_drawimage() calls can find it. Returns the underlying ImageStruct
// (or nullptr on failure) so callers can read its dimensions.
ImageStruct *recreate_image(glui32 picnum, int flipped) {
    ImageStruct *image = find_image(picnum);
    if (image == nullptr)
        return nullptr;

    ImageBuf result(decompress_image(image, false));
    if (result == nullptr)
        return nullptr;

    size_t pixmapsize = image_buffer_size(image->width, image->height, kBytesPerPixel);
    if (pixmapsize == 0)
        return nullptr;

    if (flipped) {
        flip_bitmap(image, result.get());
    }

    if (!send_image(picnum, result.get(), pixmapsize, image->width, image->height))
        return nullptr;
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

    win_drawimage(winid->peer, x, y, image->width * xscalefactor, image->height * scalefactor, 0, 0);
}

// Draw image `image` at (x, y) into the global pixmap with no scaling and
// load its palette as the new active palette. Coordinates are in hw_screen
// pixels.
void draw_to_pixmap_unscaled(int image, int x, int y) {
    draw_picture(image, x, y, false, false);
}

// Like draw_to_pixmap_unscaled but keeps the currently active palette. Used
// when compositing several images that should share a single palette.
void draw_to_pixmap_unscaled_using_current_palette(int image, int x, int y) {
    draw_picture(image, x, y, false, true);
}

// Like draw_to_pixmap_unscaled_using_current_palette but vertically flipped.
// Used for tiled fills where alternating tiles should be mirrored.
void draw_to_pixmap_unscaled_flipped_using_current_palette(int image, int x, int y) {
    draw_picture(image, x, y, true, true);
}

// Discard the global pixmap so the next ensure_pixmap() makes a fresh
// transparent one. Used when switching display modes or starting a
// new fullscreen scene.
void clear_image_buffer(void) {
    pixmap.clear();
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

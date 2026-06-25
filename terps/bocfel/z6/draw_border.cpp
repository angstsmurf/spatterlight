// Copyright 2010-2026 Chris Spiegel.
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

//  draw_border.cpp
//
//  Border, pillar, and frame compositing for Infocom V6 graphics. These
//  routines write into the shared RGBA pixmap owned by draw_image.cpp,
//  tiling and extending border artwork to fill arbitrarily tall Glk
//  windows. Used by Arthur, Shogun, and Zork Zero.
//
//  The original games did not do this, but it looks better with
//  modern screen sizes.

extern "C" {
#include "glkimp.h"
}
#include <cmath>
#include <cstring>
#include "v6_image.h"
#include "draw_image.hpp"
#include "screen.h"
#include "zterp.h"
#include "arthur.hpp"
#include "shogun.hpp"
#include "v6_specific.h"
#include "v6_shared.hpp"
#include "options.h"

// All bitmaps here are stored as interleaved RGBA — four bytes per pixel.
static constexpr int kBytesPerPixel = 4;

// Copy a horizontal strip (`line_count` rows starting at `start_y`) out of
// the global pixmap into a freshly allocated buffer. *out_size receives the
// byte size of the copy, which may be clipped if `start_y + line_count`
// runs past the pixmap. Returns nullptr if the start row is out of range
// or on OOM; caller frees with free().
static uint8_t *copy_lines_from_bitmap(int start_y, int line_count, size_t *out_size) {
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

// Copy a rectangular region of the global pixmap into a freshly allocated
// buffer of `width * height * 4` bytes. Rows that would read past the end
// of the pixmap are zero-filled instead. *size receives the buffer size.
// Caller frees with free().
static uint8_t *copy_rect_from_bitmap(int x, int y, int width, int height, size_t *size) {
    *size = 0;
    int src_stride = (int)hw_screenwidth * 4;
    int row_bytes = width * 4;
    size_t result_size = (size_t)height * row_bytes;
    uint8_t *result = (uint8_t *)malloc(result_size);
    if (result == nullptr) {
        return nullptr;
    }

    for (int row = 0; row < height; row++) {
        int src_offset = (y + row) * src_stride + x * kBytesPerPixel;
        if (src_offset + row_bytes > pixlength) {
            memset(result + (size_t)row * row_bytes, 0, (size_t)(height - row) * row_bytes);
            break;
        }
        memcpy(result + (size_t)row * row_bytes, pixmap + src_offset, row_bytes);
    }

    *size = result_size;
    return result;
}

// Zero out `height` rows of the global pixmap starting at row `y`. Clips
// the erase if it would run past the current allocation. Used to clear
// stale tiles before redrawing a section.
static void erase_lines_in_bitmap(int y, int height) {
    int stride = hw_screenwidth * 4;
    int startpos = y * stride;
    if (startpos > pixlength)
        return;
    size_t size = height * stride;
    if (startpos + size > pixlength)
        size = pixlength - startpos;
    memset(pixmap + startpos, 0, size);
}

// Clip the composited pixmap to exactly `desired_height` rows. The tiling
// routines below extend the pillars down to `desired_height`, but the last
// tile is always stamped whole, so it overshoots, and some extenders draw a
// foot just above the bottom without erasing what the overshoot left below it.
// flush_bitmap() scales the entire `pixlength`-tall pixmap into the Glk window,
// so those extra rows spill past the bottom of the screen and into the window
// border -- e.g. the foot of Zork Zero's pillars showing in the bottom-left
// border on the MAP screen after a resize. Trimming pixlength keeps the
// flushed image flush with the screen's bottom edge. (The pixmap buffer itself
// is left allocated; only the logical length used by flush_bitmap shrinks.)
static void clip_pixmap_to_height(int desired_height) {
    if (desired_height <= 0 || pixmap == nullptr)
        return;
    const long target = (long)desired_height * hw_screenwidth * kBytesPerPixel;
    if (target < pixlength)
        pixlength = (int)target;
}

// Tile a strip copied from rows `top_cut..top_cut+pillar_height` of the
// current pixmap downward, stamping it every `pillar_height - overlap`
// rows starting at `start_y`, until `desired_height` is reached. If `flip`
// is true, alternates the vertical flip of each tile starting from
// `initial_parity`; otherwise every tile is drawn with `initial_parity`.
// Returns the y-position immediately past the last tile drawn (i.e. where
// the next tile would have started), which the caller can use to anchor
// follow-up work below the tiled region.
// Tile an already-copied strip (`section_to_repeat`, `pillar_height` rows)
// downward, stamping it every `pillar_height - overlap` rows starting at
// `start_y` until `desired_height` is reached. Split out from
// tile_strip_down() so callers that must snapshot the source rows *before*
// erasing them (see extend_pillars) can copy first, then erase, then tile.
static int tile_section_down(uint8_t *section_to_repeat, size_t linessize,
                             int pillar_height, int overlap,
                             int start_y, int desired_height,
                             bool initial_parity, bool flip) {
    const int stride = pillar_height - overlap;
    bool parity = initial_parity;
    int ypos;
    for (ypos = start_y; ypos <= desired_height; ypos += stride) {
        draw_bitmap_on_bitmap(section_to_repeat, linessize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, parity);
        if (flip)
            parity = !parity;
    }
    return ypos;
}

static int tile_strip_down(int top_cut, int pillar_height, int overlap,
                           int start_y, int desired_height,
                           bool initial_parity, bool flip) {
    size_t linessize;
    uint8_t *section_to_repeat = copy_lines_from_bitmap(top_cut, pillar_height, &linessize);
    int ypos = tile_section_down(section_to_repeat, linessize, pillar_height, overlap,
                                 start_y, desired_height, initial_parity, flip);
    free(section_to_repeat);
    return ypos;
}

// Pack the three RGB bytes at `p` (pointer to an RGBA pixel) into a single
// 24-bit color value, dropping alpha. Used for fast color comparisons.
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

    for (int i = 0; i < smallbitmapsize; i += kBytesPerPixel) {
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

        pixptr = *largebitmap + ((ypos * width + xpos) * kBytesPerPixel);

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

    const int stride = hw_screenwidth * kBytesPerPixel;
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

// Constants for the Shogun and Zork 0 Mac B/W hint border pillar tiling.
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

    // Snapshot the pillar strip before erasing anything: for Arthur, top_cut
    // equals total_height - foot_height, so the strip's source rows sit inside
    // the foot region erased just below. Copying first preserves the live pole
    // texture (otherwise the tiled extension comes out blank).
    size_t linessize;
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

    // Zork Zero's EGA art expects the very first tile to be flipped.
    bool initial_parity = flip;
    if (is_spatterlight_zork0 && graphics_type == kGraphicsTypeEGA)
        initial_parity = true;

    int ypos = tile_section_down(section_to_repeat, linessize, pillar_height, overlap,
                                 start_y, desired_height,
                                 initial_parity, flip);
    free(section_to_repeat);

    // Erase everything in the foot area and below
    const int bitmap_height = (pixlength / kBytesPerPixel) / hw_screenwidth;
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

    clip_pixmap_to_height(desired_height);
}

// Vertically extend the Zork Zero "underground" castle pillars to fill a
// taller-than-original screen. Copies the pillar strip and the left/right
// side rectangles, then tiles them downward, alternating which sides get
// drawn so the masonry pattern stays consistent. The foot section is
// drawn last, and a final partial "stone" may be erased near the bottom
// so the foot lands flush against the screen edge.
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
    free(left_section);
    free(right_section);

    //  Erase a single "stone" at the bottom to reduce the off-screen part of the foot
    if (ypos + foot_height > desired_height && ypos - stone_height + foot_height > desired_height) {
        erase_lines_in_bitmap(ypos - stone_height, stone_height);
        ypos -= stone_height;
    }

    // Draw the foot
        draw_bitmap_on_bitmap(foot, footsize, hw_screenwidth, &pixmap, &pixlength, hw_screenwidth, 0, ypos, false);
    free(foot);

    clip_pixmap_to_height(desired_height);
}

#define kZ0MacTopCut 65
#define kZ0MacTotalHeight 300
#define kZ0MacUpperHeight 97
#define kZ0MacRingHeight 13
#define kZ0MacLowerHeight 110
#define kZ0MacFootHeight 15

// Vertically extend the Zork Zero Mac B/W castle border. The border image
// is split into upper, ring, lower, and foot sections; this routine tiles
// upper+lower repeatedly to fill the screen, then places the decorative
// middle ring near the vertical center and finally stamps the foot at the
// bottom. No-op if the destination screen isn't much taller than the
// original art.
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

    clip_pixmap_to_height(desired_height);
}

// Simple vertical tiler for the Zork Zero jungle border: copy a strip
// starting at `top_cut` and stamp it downward with the requested overlap
// until the screen is filled. No flipping, no foot handling.
void extend_jungle_pillars(int top_cut, int total_height,
                                int pillar_height, int overlap) {

    const float factor = (float)gscreenw / hw_screenwidth / pixelwidth;
    int desired_height = ceil(gscreenh / factor);

    if (desired_height <= total_height)
        return;

    tile_strip_down(top_cut, pillar_height, overlap,
                    total_height - overlap, desired_height,
                    /*initial_parity=*/false, /*flip=*/false);

    clip_pixmap_to_height(desired_height);
}


// Extends a border vertically by tiling a strip of the already-drawn pixmap.
// Copies the region from start_copy_from to lowest_drawn_pixel and repeats
// it downward until desired_height is reached, truncating the last copy if
// needed. Used as the final vertical fill after border images and pillars
// have been drawn.
void common_extend_border(int desired_height, int lowest_drawn_pixel, int start_copy_from) {
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

// Shogun's Amiga/Mac B/W borders are a single image with no separate
// pillars, so vertical extension is done by stamping a second copy of the
// border below the first (flipped for P_BORDER, plain for P_BORDER2) with
// platform-specific overlap offsets. Returns the new lowest drawn line.
// The caller will use this to extend the strips further if necessary.
static int shogun_extend_amiga_mac_border(int border, int border_height) {
    const bool mac = (graphics_type == kGraphicsTypeMacBW);
    if (border == P_BORDER) {
        int y = mac ? border_height : border_height - 2;
        draw_to_pixmap_unscaled_flipped_using_current_palette(border, 0, y);
        return mac ? border_height * 2 : border_height * 2 - 2;
    }
    if (border == P_BORDER2) {
        int y = mac ? border_height - 35 : border_height - 22;
        draw_to_pixmap_unscaled_using_current_palette(border, 0, y);
        return mac ? border_height * 2 - 35 : border_height * 2 - 22;
    }
    return border_height;
}

// Shogun's non-hint CGA/EGA/VGA borders have separate left and right border
// images (left_strip and right_strip). When this function is called, the
// left pillar is already drawn, so we draw right_strip on the right, and, if
// extending, drop flipped copies on both sides below the original.
// Returns the y pixel position of the lowest drawn line. The caller will use
// this to extend the strips further if necessary.
static int shogun_draw_right_strip(int left_strip, int right_strip, bool must_extend) {
    int width, height;
    get_image_size(right_strip, &width, &height);
    // Custom overlap offset for CGA graphics
    if (graphics_type == kGraphicsTypeCGA)
        height -= 7;

    if (must_extend)
        draw_to_pixmap_unscaled_flipped_using_current_palette(left_strip, hw_screenwidth - width, height);
    draw_to_pixmap_unscaled_using_current_palette(right_strip, hw_screenwidth - width, 0);

    int lowest_drawn_line = height;
    if (must_extend) {
        draw_to_pixmap_unscaled_flipped_using_current_palette(right_strip, 0, height);
        lowest_drawn_line += height;
    }

    return lowest_drawn_line;
}

// Common border drawing pipeline shared between Shogun (all borders)
// and Zork Zero (hint border only).
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
//   bottom_offset   – adjustment subtracted from lowest_drawn_line
//                     (to make repeated pattern seamless)
//   kind            – which variant we're drawing (hint / Shogun game / Shogun start menu)
void draw_border_common(int border, int BL, int BR,
                        int border_height, int border_top, int pillar_top,
                        int left_margin, int bottom_offset,
                        BorderKind kind) {

    const float factor = (float)gscreenw / hw_screenwidth / pixelwidth;
    int desired_height = ceil(gscreenh / factor);

    int lowest_drawn_line = border_height + border_top;
    const bool must_extend = (desired_height > lowest_drawn_line);
    const bool is_hint_border = (kind == BorderKind::Hint);
    // Amiga and Mac border graphics are a single image with everything,
    // not separated into top and sides.
    const bool single_piece_border = (graphics_type == kGraphicsTypeAmiga ||
                                      graphics_type == kGraphicsTypeMacBW);

    // Step 1: Draw the main border image at the top of the pixmap
    draw_to_pixmap_unscaled_using_current_palette(border, 0, 0);

    // Step 2: Draw side pillars if available.
    // BL won't be found on single-piece-border platforms.
    if (find_image(BL)) {
        int width, height;
        get_image_size(BL, &width, &height);
        draw_to_pixmap_unscaled(BL, 0, border_top);
        draw_to_pixmap_unscaled(BR, hw_screenwidth - width, border_top);
        lowest_drawn_line = height + border_top;
    } else {
        // When called by Zork Zero, is_hint_border is always true.
        if (is_hint_border) {
            // Mac B/W hint borders have their own pillar tiling with a
            // transparent foot shadow; this handles full vertical extension,
            // so we set desired_height = 0 to skip extend_shogun_border below.
            if (must_extend && graphics_type == kGraphicsTypeMacBW) {
                desired_height = (desired_height / kBWHintStepHeight) * kBWHintStepHeight;
                extend_pillars(kBWHintTopCut, kBWHintFootHeight, kBWHintTotalHeight,
                                        kBWHintTotalHeight - kBWHintTopCut, kBWHintOverlap,
                                        false, true, desired_height);
                desired_height = 0;
            }
        } else {
        // Only Shogun ever takes this path.
            if (must_extend && single_piece_border) {
                lowest_drawn_line = shogun_extend_amiga_mac_border(border, border_height);
            }
            if (find_image(BR)) {
                lowest_drawn_line = shogun_draw_right_strip(border, BR, must_extend);
            }
        }
    }

    // Step 3: Fill any remaining vertical gap by tiling a strip of the
    // already-drawn pixmap downward (from pillar_top to lowest_drawn_line - bottom_offset).
    if (must_extend) {
        common_extend_border(desired_height, lowest_drawn_line - bottom_offset, pillar_top);
    }

    // Step 4: Draw a covering rectangle of status-window color at the top.
    // The raw border graphics have a decorative top bar that doesn't fit
    // the header text, so the original interpreters cover it with a solid
    // color rectangle. Which platforms need it depends on the BorderKind.
    bool should_draw_covering_rectangle = false;
    glui32 rectangle_color = user_selected_foreground;

    switch (kind) {
        case BorderKind::ShogunGame:
            should_draw_covering_rectangle = true;
            break;
        case BorderKind::ShogunStartMenu:
            // No covering rectangle at the start menu.
            break;
        case BorderKind::Hint:
            // Hint borders cover the top bar on Mac B/W and on
            // Amiga running the Macintosh interpreter (with a special color).
            // On CGA, we just draw a solid-color top bar between the strips
            // (i.e we don't reset left_margin to 0.)
            switch (graphics_type) {
                case kGraphicsTypeMacBW:
                    left_margin = 0;
                    // Fallthrough
                case kGraphicsTypeCGA:
                    should_draw_covering_rectangle = true;
                    break;
                case kGraphicsTypeAmiga:
                    left_margin = 0;
                    if (options.int_number == INTERP_MACINTOSH) {
                        should_draw_covering_rectangle = true;
                        rectangle_color = ROSE_TAUPE;
                    }
                    break;
                case kGraphicsTypeEGA:
                    // The EGA hint border keeps its original top graphics; no
                    // covering rectangle is drawn over it.
                    break;
                default:
                    break;
            }
            break;
    }

    if (should_draw_covering_rectangle) {
        draw_rectangle_on_bitmap(rectangle_color, left_margin, 0, hw_screenwidth - left_margin * 2, V6_STATUS_WINDOW.y_size / imagescaley + 1);
    }
    flush_bitmap(current_graphics_buf_win);
}

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
#include <algorithm>
#include <cstring>
#include <vector>
#include "v6_image.h"
#include "draw_image.hpp"
#include "draw_border.hpp"
#include "screen.h"
#include "zterp.h"
#include "arthur.hpp"
#include "shogun.hpp"
#include "v6_specific.h"
#include "v6_shared.hpp"
#include "options.h"

// All bitmaps here are stored as interleaved RGBA — four bytes per pixel
// (kBytesPerPixel, from v6_image.h).

// A piece of the pixmap that has been copied out of it. Empty if the copy
// failed.
using Strip = std::vector<uint8_t>;

// Copy a horizontal strip (`line_count` rows starting at `start_y`) out of
// the global pixmap. The copy is clipped if `start_y + line_count` runs past
// the pixmap, and empty if the start row is out of range.
static Strip copy_lines_from_bitmap(int start_y, int line_count) {
    if (start_y < 0 || line_count <= 0 || start_y >= pixmap.rows())
        return {};

    line_count = std::min(line_count, pixmap.rows() - start_y);
    return Strip(pixmap.row(start_y), pixmap.row(start_y + line_count));
}

// Copy a rectangular region of the global pixmap (`width * height * 4`
// bytes). Rows that would read past the end of the pixmap are left zeroed.
static Strip copy_rect_from_bitmap(int x, int y, int width, int height) {
    if (!pixmap.exists() || x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x + width > pixmap.width_px)
        return {};
    int row_bytes = width * kBytesPerPixel;
    Strip result((size_t)height * row_bytes);

    for (int row = 0; row < height && y + row < pixmap.rows(); row++)
        memcpy(result.data() + (size_t)row * row_bytes, pixmap.pixel(x, y + row), row_bytes);

    return result;
}

// Composite the first `size` bytes of a strip of full-width rows onto the
// global pixmap with their top at row `y`, upside down if `flip` is set.
// Draws nothing if the strip is empty.
static void blit_rows(const Strip &rows, size_t size, int y, bool flip = false) {
    if (rows.empty())
        return;
    pixmap.draw(rows.data(), std::min(size, rows.size()), pixmap.width_px, 0, y, flip);
}

static void blit_rows(const Strip &rows, int y, bool flip = false) {
    blit_rows(rows, rows.size(), y, flip);
}

// Composite a strip `width` pixels wide with its top left corner at `x`, `y`.
static void blit_rect(const Strip &rect, int width, int x, int y) {
    if (rect.empty())
        return;
    pixmap.draw(rect.data(), rect.size(), width, x, y, false);
}

// The height of the Glk screen in rows of the pixmap, rounded up: how far
// down a border has to reach to fill the window.
static int screen_height_in_pixmap_rows(void) {
    const float factor = (float)gscreenw / hw_screenwidth / pixelwidth;
    return ceil(gscreenh / factor);
}

// Zero out `height` rows of the global pixmap starting at row `y`. Clips
// the erase if it would run past the last row. Used to clear stale tiles
// before redrawing a section.
static void erase_lines_in_bitmap(int y, int height) {
    pixmap.erase_rows(y, height);
}

// Clip the composited pixmap to exactly `desired_height` rows. The tiling
// routines below extend the pillars down to `desired_height`, but the last
// tile is always stamped whole, so it overshoots, and some extenders draw a
// foot just above the bottom without erasing what the overshoot left below it.
// flush_bitmap() scales the whole pixmap into the Glk window, so those extra
// rows spill past the bottom of the screen and into the window border -- e.g.
// the foot of Zork Zero's pillars showing in the bottom-left border on the MAP
// screen after a resize. Dropping the rows keeps the flushed image flush with
// the screen's bottom edge.
static void clip_pixmap_to_height(int desired_height) {
    pixmap.clip_to_height(desired_height);
}

// Tile an already-copied strip (`section_to_repeat`, `pillar_height` rows)
// downward, stamping it every `pillar_height - overlap` rows starting at
// `start_y` until `desired_height` is reached. Split out from
// tile_strip_down() so callers that must snapshot the source rows *before*
// erasing them (see extend_pillars) can copy first, then erase, then tile.
// If `flip` is true, alternates the vertical flip of each tile starting from
// `initial_parity`; otherwise every tile is drawn with `initial_parity`.
// Returns the y-position immediately past the last tile drawn (i.e. where
// the next tile would have started), which the caller can use to anchor
// follow-up work below the tiled region. An empty `section_to_repeat` (a failed
// copy) draws nothing.
static int tile_section_down(const Strip &section_to_repeat,
                             int pillar_height, int overlap,
                             int start_y, int desired_height,
                             bool initial_parity, bool flip) {
    const int stride = pillar_height - overlap;
    bool parity = initial_parity;
    int ypos;
    for (ypos = start_y; ypos <= desired_height; ypos += stride) {
        blit_rows(section_to_repeat, ypos, parity);
        if (flip)
            parity = !parity;
    }
    return ypos;
}

// Tile a strip copied from rows `top_cut..top_cut+pillar_height` of the
// current pixmap downward. See tile_section_down() for the parameters and
// the return value.
static int tile_strip_down(int top_cut, int pillar_height, int overlap,
                           int start_y, int desired_height,
                           bool initial_parity, bool flip) {
    Strip section_to_repeat = copy_lines_from_bitmap(top_cut, pillar_height);
    return tile_section_down(section_to_repeat, pillar_height, overlap,
                             start_y, desired_height, initial_parity, flip);
}

// Draws the foot (shadow) section of the Mac B/W Shogun/Zork Zero hint borders onto
// the pixmap with transparency: white pixels (monochrome_white with full
// alpha) are treated as transparent and skipped, so the tiled pillar
// pattern shows through.
//
// This makes the bottom of the pillars look better on light backgrounds.
static void draw_hint_menu_feet_mac(const Strip &foot, int y) {
    const int stride = pixmap.stride();
    if (stride == 0)
        return;
    const int foot_rows = (int)(foot.size() / stride);

    // The pixmap is not grown: rows of the foot below its end are dropped.
    for (int row = std::max(0, -y); row < foot_rows && y + row < pixmap.rows(); row++) {
        const uint8_t *source = foot.data() + (size_t)row * stride;
        uint8_t *dest = pixmap.row(y + row);
        for (int i = 0; i < stride; i += kBytesPerPixel) {
            uint32_t source_rgb = bytes_to_rgb(source + i);
            if (!(source_rgb == monochrome_white && source[i + 3] == 0xff))
                memcpy(dest + i, source + i, kBytesPerPixel);
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
    } else if (graphics_type != kGraphicsTypeApple2) {
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

    // The top banner image frames the room image
    int image_height;
    get_image_size(K_PIC_BANNER, nullptr, &image_height);
    float hw_screenheight = image_height + top_margin;

    const int actual_pixmap_height = pixmap.rows();

    // Erase any stale content below the banner before drawing
    if (hw_screenheight < actual_pixmap_height)
        erase_lines_in_bitmap(hw_screenheight, actual_pixmap_height - hw_screenheight);
    draw_to_pixmap_unscaled(K_PIC_BANNER, left_margin, top_margin);

    if (showing_wide_arthur_room_image) {
        arthur_draw_room_image(current_picture);
    }

    // K_PIC_BANNER_MARGIN provides the room image dimensions, which determine
    // where the side poles start vertically (y_margin)
    int x_margin = 0, y_margin = 0;
    get_image_size(K_PIC_BANNER_MARGIN, &x_margin, &y_margin);

    const int pole_y = y_margin + top_margin - 2;
    draw_to_pixmap_unscaled(K_PIC_LEFT_POLE, left_margin + 1 + left_offset, pole_y);
    draw_to_pixmap_unscaled(K_PIC_RIGHT_POLE, hw_screenwidth - x_margin + right_offset, pole_y);

    get_image_size(K_PIC_LEFT_POLE, nullptr, &image_height);
    if (image_height)
        hw_screenheight = pole_y + image_height;

    const int desired_height = screen_height_in_pixmap_rows() - arthur_pic_top_margin - 1;

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
static constexpr int kBWHintTopCut = 55;       // Lines to skip at the top (header area)
static constexpr int kBWHintFootHeight = 4;    // Height of the foot shadow at the bottom
static constexpr int kBWHintTotalHeight = 298; // Total height of the hint border image
static constexpr int kBWHintOverlap = 20;      // Overlap between adjacent tiles (pixels)
// Rounding granularity for the truncated final tile; also the minimum gap
// required before tiling is attempted
static constexpr int kBWHintStepHeight = 20;

// Height of the rounded corners at the top of the Amiga/Mac colour hint
// border. The covering rectangle skips this many lines so the curve survives.
static constexpr int kAmigaHintCornerHeight = 8;

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

    if (desired_height <= 0)
        desired_height = screen_height_in_pixmap_rows();

    if (desired_height < total_height || (bw_hint_foot && desired_height - total_height < kBWHintStepHeight))
        return;

    // Snapshot the pillar strip before erasing anything: for Arthur, top_cut
    // equals total_height - foot_height, so the strip's source rows sit inside
    // the foot region erased just below. Copying first preserves the live pole
    // texture (otherwise the tiled extension comes out blank).
    Strip section_to_repeat = copy_lines_from_bitmap(top_cut, pillar_height);

    // Copy the foot
    Strip foot = copy_lines_from_bitmap(total_height - foot_height, foot_height);

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

    int ypos = tile_section_down(section_to_repeat, pillar_height, overlap,
                                 start_y, desired_height,
                                 initial_parity, flip);

    // Erase everything in the foot area and below
    const int bitmap_height = pixmap.rows();
    int foot_top = desired_height - foot_height;
    // Arthur's poles are a 2-line dither, and the foot continues it. Keep the
    // foot in phase with the tiles (which start at start_y, not necessarily
    // an even row), or the pattern repeats a line where they meet.
    if (is_spatterlight_arthur && foot_top > start_y)
        foot_top -= (foot_top - start_y) % pillar_height;
    erase_lines_in_bitmap(foot_top, bitmap_height - foot_top);

    // Draw the foot
    if (bw_hint_foot) {
        draw_hint_menu_feet_mac(foot, desired_height - foot_height);
        // Erase any garbage left at the bottom of the pixmap
        erase_lines_in_bitmap(ypos, bitmap_height - ypos);
    } else {
        blit_rows(foot, foot_top);
    }

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

    int desired_height = screen_height_in_pixmap_rows();

    if (desired_height < total_height)
        return;

    // Copy the pillar section we want to repeat (two stone blocks)
    Strip section_to_repeat = copy_lines_from_bitmap(top_cut, pillar_height);

    Strip left_section = copy_rect_from_bitmap(0, top_cut, pillar_width, pillar_height);
    Strip right_section = copy_rect_from_bitmap(hw_screenwidth - pillar_width, top_cut, pillar_width, pillar_height);

    // Copy the foot
    Strip foot = copy_lines_from_bitmap(total_height - foot_height, foot_height);

    // erase the foot and start from below it.
    erase_lines_in_bitmap(total_height - foot_height, foot_height);
    int start_y = total_height - foot_height;

    // Draw full-height tiles while they fit
    int ypos;
    bool parity = true;
    for (ypos = start_y; ypos + pillar_height <= desired_height; ypos += pillar_height) {
        blit_rows(section_to_repeat, ypos);
        if (parity) {
            blit_rect(right_section, pillar_width, 0, ypos);
            blit_rect(left_section, pillar_width, hw_screenwidth - pillar_width, ypos);
        }
        parity = !parity;
    }

    //  Erase a single "stone" at the bottom to reduce the off-screen part of the foot
    if (ypos + foot_height > desired_height && ypos - stone_height + foot_height > desired_height) {
        erase_lines_in_bitmap(ypos - stone_height, stone_height);
        ypos -= stone_height;
    }

    // Draw the foot
    blit_rows(foot, ypos);

    clip_pixmap_to_height(desired_height);
}

static constexpr int kZ0MacTopCut = 65;
static constexpr int kZ0MacTotalHeight = 300;
static constexpr int kZ0MacUpperHeight = 97;
static constexpr int kZ0MacRingHeight = 13;
static constexpr int kZ0MacLowerHeight = 110;
static constexpr int kZ0MacFootHeight = 15;

// Vertically extend the Zork Zero Mac B/W castle border. The border image
// is split into upper, ring, lower, and foot sections; this routine tiles
// upper+lower repeatedly to fill the screen, then places the decorative
// middle ring near the vertical center and finally stamps the foot at the
// bottom. No-op if the destination screen isn't much taller than the
// original art.
void extend_mac_bw_castle_pillars(void) {

    int desired_height = screen_height_in_pixmap_rows() - 1;

    if (desired_height <= kZ0MacTotalHeight + 5)
        return;

    // Copy the pillar sections we want to repeat or move
    int ypos = kZ0MacTopCut;
    Strip upper_section = copy_lines_from_bitmap(ypos, kZ0MacUpperHeight);
    ypos += kZ0MacUpperHeight;
    Strip middle_ring = copy_lines_from_bitmap(ypos, kZ0MacRingHeight);
    ypos += kZ0MacRingHeight;
    Strip lower_section = copy_lines_from_bitmap(ypos, kZ0MacLowerHeight);
    ypos += kZ0MacLowerHeight;
    Strip foot = copy_lines_from_bitmap(ypos, kZ0MacFootHeight);

    // erase everything from below the top cut.
    erase_lines_in_bitmap(kZ0MacTopCut, kZ0MacTotalHeight - kZ0MacTopCut);
    ypos = kZ0MacTopCut;

    // Draw upper and lower tiles while they fit
    while (ypos <= desired_height) {
        blit_rows(upper_section, ypos);
        ypos += kZ0MacUpperHeight;
        if (ypos > desired_height)
            break;
        blit_rows(lower_section, ypos);
        ypos += kZ0MacLowerHeight;
    }

    // Draw the middle ring
    ypos = (desired_height + kZ0MacFootHeight + kZ0MacFootHeight) / 2;
    erase_lines_in_bitmap(ypos, kZ0MacRingHeight);
    blit_rows(middle_ring, ypos);

    // Draw the foot
    ypos = desired_height - kZ0MacFootHeight;
    erase_lines_in_bitmap(ypos, desired_height - ypos + 2);
    blit_rows(foot, ypos);

    clip_pixmap_to_height(desired_height);
}

// Simple vertical tiler for the Zork Zero jungle border: copy a strip
// starting at `top_cut` and stamp it downward with the requested overlap
// until the screen is filled. No flipping, no foot handling.
void extend_jungle_pillars(int top_cut, int total_height,
                                int pillar_height, int overlap) {

    int desired_height = screen_height_in_pixmap_rows();

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
static void common_extend_border(int desired_height, int lowest_drawn_pixel, int start_copy_from) {
    if (desired_height > lowest_drawn_pixel) {
        int height = lowest_drawn_pixel - start_copy_from;
        if (height < 1)
            return;
        Strip bit_to_repeat = copy_lines_from_bitmap(start_copy_from, height);
        if (bit_to_repeat.empty())
            return;
        size_t copysize = bit_to_repeat.size();
        erase_lines_in_bitmap(lowest_drawn_pixel, desired_height - lowest_drawn_pixel);

        while (lowest_drawn_pixel < desired_height) {
            if (lowest_drawn_pixel + height > desired_height) {
                height = desired_height - lowest_drawn_pixel;
                copysize = (size_t)hw_screenwidth * kBytesPerPixel * height;
            }
            blit_rows(bit_to_repeat, copysize, lowest_drawn_pixel);
            lowest_drawn_pixel += height;
        }
    }
}

// Shogun's Amiga/Mac B/W borders are a single image with no separate
// pillars, so vertical extension is done by stamping a second copy of the
// border below the first (flipped for P_BORDER, plain for P_BORDER2) with
// platform-specific overlap offsets. Returns the new lowest drawn line.
// The caller will use this to extend the strips further if necessary.
static int shogun_extend_amiga_mac_border(int border, int border_height) {
    const bool mac = (graphics_type == kGraphicsTypeMacBW);
    // How many rows the second copy overlaps the first, so that the pattern
    // carries on across the join.
    int overlap;
    if (border == P_BORDER) {
        overlap = mac ? 0 : 2;
        draw_to_pixmap_unscaled_flipped_using_current_palette(border, 0, border_height - overlap);
    } else if (border == P_BORDER2) {
        overlap = mac ? 35 : 22;
        draw_to_pixmap_unscaled_using_current_palette(border, 0, border_height - overlap);
    } else {
        return border_height;
    }
    return border_height * 2 - overlap;
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
    // The CGA strips end in kCGAStripOverlap rows that the copy below covers
    static constexpr int kCGAStripOverlap = 7;
    if (graphics_type == kGraphicsTypeCGA)
        height -= kCGAStripOverlap;

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

// Draws a rectangle of status-window color over the top of the border.
// The raw border graphics have a decorative top bar that doesn't fit
// the header text, so the original interpreters cover it with a solid
// color rectangle. Which platforms need it depends on the BorderKind.
// `left_margin` is how far in from the screen edges the rectangle starts,
// where it does not have to span the whole width.
static void draw_covering_rectangle(BorderKind kind, int left_margin) {
    bool should_draw_covering_rectangle = false;
    glui32 rectangle_color = user_selected_foreground;
    // Number of lines skipped at the top of the covering rectangle. Only the
    // Amiga/Mac colour hint border needs this, to keep its rounded corners.
    int rect_top = 0;

    switch (kind) {
        case BorderKind::ShogunGame:
            should_draw_covering_rectangle = true;
            break;
        case BorderKind::ShogunStartMenu:
            // No covering rectangle at the start menu.
            break;
        case BorderKind::Hint:
            // Hint borders cover the top bar on Mac B/W, on Amiga/Mac colour,
            // and on the PC formats (EGA/VGA/Blorb), where the bar takes the
            // colour of the top of the hint background art. That colour is per
            // game as well as per format: Zork Zero and Shogun ship different
            // EGA hint backgrounds (mauve dither vs flat brown).
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
                    // The Amiga/Mac colour border is rounded at the top two
                    // corners. A full-height bar would square them off, so
                    // start kAmigaHintCornerHeight lines down and drop the same
                    // number of lines from the bar: the curve stays visible and
                    // the header text below it still gets a solid background.
                    left_margin = 0;
                    rect_top = kAmigaHintCornerHeight;
                    should_draw_covering_rectangle = true;
                    if (options.int_number == INTERP_MACINTOSH) {
                        // The Macintosh interpreter draws this art with its own
                        // palette, for which the art's own colour is wrong.
                        rectangle_color = ROSE_TAUPE;
                    } else if (!sample_pixmap_pixel(hw_screenwidth / 2, rect_top, &rectangle_color)) {
                        should_draw_covering_rectangle = false;
                    }
                    break;
                case kGraphicsTypeEGA:
                    left_margin = 0;
                    if (is_spatterlight_zork0) {
                        // Zork Zero renders its EGA hint background as a
                        // two-colour dither, so no single pixel of the artwork
                        // is the colour the eye actually sees: sampling gives
                        // the bright magenta while the field reads as a muted
                        // mauve. HINT_MAUVE is the flat stand-in the rest of the EGA
                        // code already uses for it (it is what v6_set_colours
                        // gives the status window).
                        rectangle_color = HINT_MAUVE;
                        should_draw_covering_rectangle = true;
                    } else if (sample_pixmap_pixel(hw_screenwidth / 2, 0, &rectangle_color)) {
                        // Shogun, the other hint-border caller, uses a flat
                        // field instead (EGA brown, 0xaa5500), which is nothing
                        // like Zork Zero's mauve -- so read it off the artwork.
                        should_draw_covering_rectangle = true;
                    }
                    break;
                case kGraphicsTypeVGA:
                case kGraphicsTypeBlorb:
                    // These share the same artwork, whose top strip is a flat
                    // field of the hint background colour (0x806060 in the
                    // shipped images). Read it back out of the border we just
                    // drew rather than hardcoding it, so the bar tracks the
                    // image instead of drifting from it.
                    left_margin = 0;
                    if (sample_pixmap_pixel(hw_screenwidth / 2, 0, &rectangle_color)) {
                        should_draw_covering_rectangle = true;
                    }
                    break;
                default:
                    break;
            }
            break;
    }

    if (should_draw_covering_rectangle) {
        int rect_height = V6_STATUS_WINDOW.y_size / imagescaley + 1 - rect_top;
        if (rect_height > 0) {
            draw_rectangle_on_bitmap(rectangle_color, left_margin, rect_top,
                                     hw_screenwidth - left_margin * 2, rect_height);
        }
    }
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

    int desired_height = screen_height_in_pixmap_rows();

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

    // Step 4: Cover the top bar of the border
    draw_covering_rectangle(kind, left_margin);

    flush_bitmap(current_graphics_buf_win);
}

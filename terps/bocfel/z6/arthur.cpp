//
//  arthur.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//
//  Implements the custom UI for Infocom's "Arthur: The Quest for
//  Excalibur" (1989), a Z-machine version 6 interactive fiction game
//  set in Arthurian legend.
//
//  Arthur features multiple switchable display modes, cycled via function keys:
//    F1 - Picture mode: room illustration above the text area
//    F2 - Map mode: clickable overhead map with location markers
//    F3 - Inventory mode: item list shown in a grid window
//    F4 - Status mode: detailed character status in a grid window
//    F5 - Room description: verbose room text at the top. We use a Glk buffer window for this.
//    F6 - No graphics: text-only with no upper window
//
//  The screen layout consists of:
//    - Decorative banner borders with left/right pillars drawn to the
//      background graphics window
//    - A one-line status bar showing location, vehicle, time, and form
//    - A mode-dependent upper area (room image, map, inventory grid, etc.)
//    - The main text buffer for narrative output
//    - An optional "error window" (inverted grid) at the bottom for
//      errors and parser "out-of-world" messages.
//
//  Some room images are drawn with "stamp" overlays (small images positioned
//  relative to a reference offset image) for details like the sword in the stone.
//  Full-screen images are shown during the title sequence, endgame, and
//  special story events. I have provided VoiceOver descriptions of them
//  for accessibility.
//

#include "draw_image.hpp"
#include "draw_border.hpp"
#include "memory.h"
#include "objects.h"
#include "options.h"
#include "screen.h"
#include "unicode.h"
#include "zterp.h"
#include "v6_specific.h"
#include "v6_shared.hpp"

#include "arthur.hpp"

// Global lookup tables mapping Z-machine global variable indices, routine
// addresses, and table addresses to symbolic names. Populated at startup.
ArthurGlobals ag;
ArthurRoutines ar;
ArthurTables at;

// Picture number constants for Arthur's image resources.
// Full-screen images (displayed during title sequence and special events):
constexpr int K_PIC_TITLE = 1;  // Title screen
constexpr int K_PIC_SWORD = 2;  // Sword in the stone (base layer)
constexpr int K_PIC_SWORD_MERLIN = 3; // Merlin overlay drawn on top of the sword image

constexpr int K_PIC_ENDGAME = 84; // Victory image (Arthur holding Excalibur)
constexpr int K_PIC_ANGRY_DEMON = 85; // Demon image (game over scenario)

// Banner and layout reference images (K_PIC_BANNER and K_PIC_BANNER_MARGIN
// are in arthur.hpp):
constexpr int K_MAP_SCROLL = 137; // Map scroll background image

// Room images with special handling:
constexpr int K_PIC_CHURCHYARD = 4; // Standard-width room image (used for sizing reference)
constexpr int K_PIC_PARADE_AREA = 17; // Wide room image (spans full width)
constexpr int K_PIC_AIR_SCENE = 163; // Wide room image (flying scene)

// Stamp images (small overlays positioned by offset):
constexpr int K_PIC_ISLAND_DOOR = 158; // Island door stamp
constexpr int K_PIC_ISLAND_DOOR_OFF = 159; // Offset reference for the island door stamp
constexpr int K_PIC_STONE_2 = 9; // Stone stamp (has platform-specific offset fix)
constexpr int K_PIC_BOAR = 93;  // Charging boar stamp (MCGA/EGA offset rectangle is one pixel short)

// Arthur uses several Glk windows for different UI regions.
#define ARTHUR_ROOM_GRAPHIC_WIN windows[2]  // Upper area: room image, inventory, status, or room desc
#define ARTHUR_ERROR_WINDOW windows[3]      // Bottom inverted bar for author/error messages
// Left and right banners. Only used when erasing banner graphics: the
// banners themselves are drawn to window 7 (S-FULL, V6_GRAPHICS_BG).
#define ARTHUR_LEFT_BANNER_WINDOW windows[5]
#define ARTHUR_RIGHT_BANNER_WINDOW windows[6]

// Room images are drawn this many image pixels below arthur_pic_top_margin.
// The Mac pictures sit lower in their frame (and one pixel to the right).
constexpr int kRoomImageYOffset = 7;
constexpr int kRoomImageYOffsetMacBW = 11;

// The banner overlaps the map scroll by this many image pixels.
constexpr int kBannerOverlap = 4;

// Whether the current room image is a wide panoramic (parade area / air scene)
// vs. the standard narrower size. Affects centering and redraw logic.
bool showing_wide_arthur_room_image = false;

// Layout metrics computed from the banner margin images. These control where
// the status bar, text buffer, and room image are positioned vertically.
static int arthur_text_top_margin = -1;  // Top of status bar / text area in pixels
static int arthur_x_margin = 0;         // Horizontal margin for banner pillars in pixels

// Vertical offset (in image-space pixels) for drawing room images. Accounts
// for the banner height so images appear below the decorative frame.
int arthur_pic_top_margin = 0;

// Returns true if the picture is a "stamp" — a small overlay image that
// is positioned relative to a reference offset image (typically picnum-1).
// Stamps include things like the sword in the stone, the island door, etc.
static bool is_arthur_stamp_image(int picnum) {
    switch (picnum) {
        case 6:
        case 9:
        case 80:
        case 83:
        case 88:
        case 91:
        case 93:
        case 95:
        case 97:
        case 99:
        case 104:
        case 156:
        case 158:
        case 161:
        case 169:
            return true;
        default:
            break;
    }
    return false;
}

// Returns true if the picture is a room illustration (drawn in the upper
// area in picture mode). Most images 4-89 are room images, with specific
// exceptions for stamps and banners. Images 101-102 and 154-167 are also
// room images (wide scenes, alternate views).
static bool is_arthur_room_image(int picnum) {
    switch (picnum) {
        case 101:
        case 102:
        case 154:
        case 157:
        case 162:
        case 163: // K-PIC-AIR-SCENE
        case 165:
        case 166:
        case 167:
            return true;
        case 6:
        case 9:
        case 54:
        case 80:
        case 83:
        case 88:
            return false;
        default:
            if (picnum >= 4 && picnum <= 89)
                return true;
    }
    return false;
}

// Returns true if the picture is a map marker/icon (images 105-153).
bool is_arthur_map_image(int picnum) {
    return (picnum >= 105 && picnum <= 153);
}

// Calculates the vertical layout metrics from the banner/map images.
// The map scroll background height determines where the status bar starts,
// and the banner margin dummy image provides the border height offset for
// positioning room images. Called on resize and init.
void adjust_arthur_top_margin(void) {
    int mapheight, borderheight, margin;
    get_image_size(K_MAP_SCROLL, &margin, &mapheight); // Get height of map background image
    get_image_size(K_PIC_BANNER_MARGIN, nullptr, &borderheight); // Get height of border measuring dummy image
    float y_margin = mapheight * imagescaley;
    arthur_text_top_margin = ceil(y_margin / gcellh) * gcellh;
    arthur_pic_top_margin = arthur_text_top_margin / imagescaley - borderheight - kBannerOverlap;
    if (arthur_pic_top_margin < 0)
        arthur_pic_top_margin = 0;
    arthur_x_margin = margin * imagescalex;
}


// Draws a room image or stamp to the offscreen pixmap. Room images are
// centered horizontally using the churchyard image width as reference
// (or the actual width for wide panoramic scenes). Stamp images are
// positioned by reading the dimensions of their offset reference image
// (typically picnum-1) to determine the x,y displacement. Handles
// platform-specific adjustments (Mac B/W vertical offset, Amiga stone fix).
void arthur_draw_room_image(int picnum) {
    int x, y, width, height;

    if (picnum == K_PIC_PARADE_AREA || picnum == K_PIC_AIR_SCENE) {
        get_image_size(picnum, &width, &height);
        showing_wide_arthur_room_image = true;
    } else {
        get_image_size(K_PIC_CHURCHYARD, &width, &height);
        showing_wide_arthur_room_image = false;
    }

    x = ((hw_screenwidth - width) / 2.0);
    y = arthur_pic_top_margin;
    if (showing_wide_arthur_room_image)
        y--;

    if (graphics_type == kGraphicsTypeMacBW) {
        y += kRoomImageYOffsetMacBW;
        x++;
    } else {
        y += kRoomImageYOffset;
    }

    if (is_arthur_stamp_image(picnum)) {
        // All pictures have an offset (dummy) image. It usually is the one
        // before the actual picture, i.e. picture index - 1, except for the one of
        // the island door (K-PIC-ISLAND-DOOR is 158 and K-PIC-ISLAND-DOOR-OFF is 159.)
        if (picnum == K_PIC_ISLAND_DOOR)
            get_image_size(K_PIC_ISLAND_DOOR_OFF, &width, &height);
        else
            get_image_size(picnum - 1, &width, &height);

        // The Amiga version of the stone image seems to have an incorrect offset
        if (picnum == K_PIC_STONE_2 && graphics_type == kGraphicsTypeAmiga)
            width -= 4;

        // The offset rectangle (picture 92) for the charging boar is one
        // MCGA pixel too narrow: the stamp includes part of the tree rather
        // than relying on transparency, and with the game's value that tree
        // lands one pixel left of the room's tree. Verified by pixel-matching
        // the stamp's tree strip against room picture 89: MCGA fits at 35
        // (rectangle says 34), and EGA, at double horizontal resolution,
        // fits at 70 (rectangle says 68). CGA, Amiga, Mac and Apple II data
        // have not been checked, so they are left alone.
        if (picnum == K_PIC_BOAR) {
            if (graphics_type == kGraphicsTypeBlorb || graphics_type == kGraphicsTypeVGA)
                width++;
            else if (graphics_type == kGraphicsTypeEGA)
                width += 2;
        }

        x += width;
        y += height;
    } else if (!is_arthur_room_image(picnum)) {
        return;
    }

    draw_to_pixmap_unscaled(picnum, x, y);
}


// Draws a map marker/icon at the specified position on the map scroll.
// The x coordinate is offset by the banner margin width. Minor y adjustments
// are applied per platform and image number to align markers with the map.
static void arthur_draw_map_image(int picnum, int x, int y) {
    int x_margin;
    get_image_size(K_PIC_BANNER_MARGIN, &x_margin, nullptr);
    x += x_margin;
    if (options.int_number == INTERP_MACINTOSH)
        y--;
    if (graphics_type != kGraphicsTypeApple2 && graphics_type != kGraphicsTypeMacBW) {
        if (picnum >= 138 || graphics_type == kGraphicsTypeCGA) {
            y--;
        }
    }
    draw_to_pixmap_unscaled(picnum, x, y);
}

// Arthur's display modes, stored in the GL_WINDOW_TYPE Z-machine global.
// Each mode controls what appears in the upper area of the screen.
enum kArthurWindowType {
    K_WIN_NONE = 0,  // No graphics (text only)
    K_WIN_INVT = 1,  // Inventory list
    K_WIN_DESC = 2,  // Verbose room description
    K_WIN_STAT = 3,  // Character status display
    K_WIN_MAP = 4,   // Overhead map with clickable locations
    K_WIN_PICT = 5   // Room illustration (default)
};

// The six display modes: the function key that selects each one, the value
// the game keeps in GL_WINDOW_TYPE and our screen mode.
struct ArthurMode {
    uint8_t key;
    kArthurWindowType wintype;
    V6ScreenMode mode;
};

static constexpr ArthurMode arthur_modes[] = {
    { ZSCII_F1, K_WIN_PICT, MODE_NORMAL      },
    { ZSCII_F2, K_WIN_MAP,  MODE_MAP         },
    { ZSCII_F3, K_WIN_INVT, MODE_INVENTORY   },
    { ZSCII_F4, K_WIN_STAT, MODE_STATUS      },
    { ZSCII_F5, K_WIN_DESC, MODE_ROOM_DESC   },
    { ZSCII_F6, K_WIN_NONE, MODE_NO_GRAPHICS },
};

// Synchronizes the internal screenmode enum with the Z-machine's
// GL_WINDOW_TYPE global. Called after restore to ensure consistency.
void arthur_sync_screenmode(void) {
    kArthurWindowType window_type = (kArthurWindowType)get_global(ag.GL_WINDOW_TYPE);
    for (const ArthurMode &m : arthur_modes) {
        if (m.wintype == window_type) {
            screenmode = m.mode;
            return;
        }
    }
}

// With graphics turned off, the picture and map modes have nothing to
// show, so the game is put in its own text-only mode (the one F6 selects)
// instead. That changes GL_WINDOW_TYPE, which the game saves, so a game
// saved with graphics off comes back text-only. Turning graphics on
// again goes back to the pictures.
static void arthur_apply_graphics_setting(void) {
    kArthurWindowType window_type = (kArthurWindowType)get_global(ag.GL_WINDOW_TYPE);
    if (v6_graphics_off()) {
        if (window_type == K_WIN_PICT || window_type == K_WIN_MAP)
            set_global(ag.GL_WINDOW_TYPE, K_WIN_NONE);
        if (screenmode == MODE_NORMAL || screenmode == MODE_MAP)
            screenmode = MODE_NO_GRAPHICS;
    } else if (graphics_setting_changed && window_type == K_WIN_NONE && screenmode == MODE_NO_GRAPHICS) {
        set_global(ag.GL_WINDOW_TYPE, K_WIN_PICT);
        screenmode = MODE_NORMAL;
    }
}

// Sizes the status window and the main text window to fit between the
// banner pillars, with the text ending at screenheight. In map mode it also
// records where the map goes. Every mode with a border comes here.
static void arthur_layout_text_windows(int screenheight) {
    int x_margin = 0, y_margin = 0;
    get_image_size(K_PIC_BANNER_MARGIN, &x_margin, &y_margin);

    if (screenmode == MODE_MAP) {
        ARTHUR_ROOM_GRAPHIC_WIN.y_origin = 0;
        ARTHUR_ROOM_GRAPHIC_WIN.x_origin = x_margin;
        ARTHUR_ROOM_GRAPHIC_WIN.x_size = hw_screenwidth - 2 * x_margin;
    }

    x_margin *= imagescalex;

    if (screenmode != MODE_HINTS) {
        V6_STATUS_WINDOW.y_origin = arthur_text_top_margin;
        win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);
    }

    int width_in_chars = ((float)(gscreenw - 2 * x_margin) / gcellw);
    V6_STATUS_WINDOW.x_size = width_in_chars * gcellw;
    V6_STATUS_WINDOW.x_origin = (gscreenw - V6_STATUS_WINDOW.x_size) / 2;
    if (graphics_type == kGraphicsTypeApple2) {
        V6_STATUS_WINDOW.x_origin += imagescalex;
    }

    mainwin->x_origin = V6_STATUS_WINDOW.x_origin;
    mainwin->x_size = V6_STATUS_WINDOW.x_size;

    if (screenmode == MODE_HINTS) {
        V6_STATUS_WINDOW.y_size = 3 * gcellh + 2 * ggridmarginy;
        mainwin->y_origin = V6_STATUS_WINDOW.y_origin + V6_STATUS_WINDOW.y_size + gcellh;
    } else {
        V6_STATUS_WINDOW.y_size = gcellh + 2 * ggridmarginy;
        mainwin->y_origin = V6_STATUS_WINDOW.y_origin + V6_STATUS_WINDOW.y_size;
    }

    mainwin->y_size = screenheight - mainwin->y_origin - 1;

    v6_sizewin(&V6_STATUS_WINDOW);
    v6_sizewin(mainwin);
}

// Gives window 2 the kind of Glk window the mode wants: a text grid for the
// inventory and status lists, a text buffer for the room description, and
// none at all for the picture and the map, which are drawn to the
// background.
static void arthur_make_upper_window(void) {
    if (screenmode != MODE_STATUS && screenmode != MODE_INVENTORY && screenmode != MODE_ROOM_DESC) {
        v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
        return;
    }

    ARTHUR_ROOM_GRAPHIC_WIN.x_origin = V6_STATUS_WINDOW.x_origin;
    ARTHUR_ROOM_GRAPHIC_WIN.x_size = V6_STATUS_WINDOW.x_size;
    ARTHUR_ROOM_GRAPHIC_WIN.y_size = arthur_text_top_margin - 1;
    glui32 wintype = (screenmode == MODE_ROOM_DESC) ? wintype_TextBuffer : wintype_TextGrid;
    if (ARTHUR_ROOM_GRAPHIC_WIN.id == nullptr || ARTHUR_ROOM_GRAPHIC_WIN.id->type != wintype) {
        if (ARTHUR_ROOM_GRAPHIC_WIN.id != nullptr) {
            gli_delete_window(ARTHUR_ROOM_GRAPHIC_WIN.id);
            ARTHUR_ROOM_GRAPHIC_WIN.id = nullptr;
        }
        v6_remap_win(&ARTHUR_ROOM_GRAPHIC_WIN, wintype, nullptr);
    } else {
        v6_sizewin(&ARTHUR_ROOM_GRAPHIC_WIN);
    }
    win_setbgnd(ARTHUR_ROOM_GRAPHIC_WIN.id->peer, user_selected_background);
}

// Has the game fill in the upper area again for the current mode.
static void arthur_redraw_upper_area(void) {
    switch (screenmode) {
        case MODE_NORMAL:
            clear_image_buffer();
            call_routine(ar.RT_UPDATE_PICT_WINDOW, {1});
            draw_arthur_side_images(current_graphics_buf_win);
            if (showing_wide_arthur_room_image)
                arthur_draw_room_image(current_picture);
            flush_bitmap(current_graphics_buf_win);
            break;
        case MODE_INVENTORY:
            call_routine(ar.RT_UPDATE_INVT_WINDOW, {1});
            break;
        case MODE_STATUS:
            call_routine(ar.RT_UPDATE_STAT_WINDOW, {1});
            break;
        case MODE_MAP:
            set_global(ag.GL_MAP_GRID_Y, 0);
            call_routine(ar.RT_UPDATE_MAP_WINDOW, {1});
            flush_bitmap(current_graphics_buf_win);
            break;
        default:
            break;
    }
}

// Redraws the full-screen picture that is showing at the new window size.
static void arthur_resize_slideshow(void) {
    v6_define_window(mainwin, 1, 1, gscreenw, gscreenh);

    win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
    glk_window_clear(current_graphics_buf_win);
    if (last_slideshow_pic == K_PIC_SWORD_MERLIN) {
        float scale = draw_centered_title_image(K_PIC_SWORD);
        draw_centered_image(K_PIC_SWORD_MERLIN, scale, 0, 0);
    } else {
        draw_centered_title_image(last_slideshow_pic);
    }
}

// Master resize handler. Rebuilds the entire UI based on the current
// screen mode. Repositions and resizes all windows, redraws banner
// borders, room images, map, or text displays as appropriate.
//
// Window layout:
//   Window 0 (S-TEXT): Main text buffer (V6_TEXT_BUFFER_WINDOW)
//   Window 1 (S-WINDOW): One-line status bar (V6_STATUS_WINDOW)
//   Window 2: Mode-dependent upper area — room image, inventory grid,
//             status grid, or room description buffer (ARTHUR_ROOM_GRAPHIC_WIN)
//   Window 3: Inverted error/author message bar at bottom (ARTHUR_ERROR_WINDOW)
//   Windows 5, 6: Left/right banner erase regions (banners drawn to window 7)
//   Window 7 (S-FULL): Full-screen background for border/room graphics (V6_GRAPHICS_BG)
void arthur_update_on_resize(void) {
    arthur_apply_graphics_setting();
    image_needs_redraw = true;
    adjust_arthur_top_margin();

    v6_set_grid_colors(style_Normal);
    v6_set_grid_colors(style_Subheader);

    if (ARTHUR_ROOM_GRAPHIC_WIN.id && ARTHUR_ROOM_GRAPHIC_WIN.id->type == wintype_TextGrid)
        v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);

    int screenheight = gscreenh;
    if (ARTHUR_ERROR_WINDOW.id != nullptr) {
        int error_window_height = get_global(ag.GL_AUTHOR_SIZE);
        if (error_window_height == 0)
            error_window_height = 1;
        screenheight -= (gcellh * error_window_height + 2 * ggridmarginy);
    }

    if (screenmode == MODE_INITIAL_QUESTION)
        return;

    if (screenmode == MODE_SLIDESHOW) {
        arthur_resize_slideshow();
        return;
    }

    if (current_graphics_buf_win && screenmode != MODE_NORMAL) {
        glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
        glk_window_clear(current_graphics_buf_win);
    }

    if (screenmode == MODE_NO_GRAPHICS) {
        v6_define_window(&V6_STATUS_WINDOW, 1, 1, gscreenw, gcellh + 2 * ggridmarginy);
        v6_define_window(mainwin, 1, V6_STATUS_WINDOW.y_origin + V6_STATUS_WINDOW.y_size, gscreenw, screenheight - mainwin->y_origin - 1);
        v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
    } else {
        arthur_layout_text_windows(screenheight);

        if (screenmode == MODE_HINTS) {
            redraw_hint_screen_on_resize();
            glk_request_mouse_event(V6_TEXT_BUFFER_WINDOW.id);
            glk_request_mouse_event(V6_STATUS_WINDOW.id);
            return;
        }

        arthur_make_upper_window();
        v6_get_and_sync_upperwin_size();
        arthur_redraw_upper_area();
    }

    set_global(ag.UPDATE, 0);
    call_routine(ar.UPDATE_STATUS_LINE);

    if (ARTHUR_ERROR_WINDOW.id != nullptr) {
        v6_define_window(&ARTHUR_ERROR_WINDOW, V6_TEXT_BUFFER_WINDOW.x_origin, screenheight + 1, V6_TEXT_BUFFER_WINDOW.x_size, gscreenh - screenheight);
    }
}

// Moves the cursor in a grid window, converting from 1-based Z-machine
// coordinates to 0-based Glk coordinates. Note the (y, x) parameter order
// matches the Z-machine convention.
void arthur_move_cursor(int16_t y, int16_t x, winid_t win) {
    x--;
    y--;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    glk_window_move_cursor(win, x, y);
}

// Z-machine entry point: transitions to picture (room illustration) mode.
// If coming from slideshow or initial question, shrinks the foreground
// graphics window and switches to the background pixmap for room images.
void RT_UPDATE_PICT_WINDOW(void) {
    if (screenmode == MODE_SLIDESHOW || screenmode == MODE_INITIAL_QUESTION) {
        screenmode = MODE_NORMAL;
        win_sizewin(graphics_fg_glk->peer, 0, 0, 0, 0);
        current_graphics_buf_win = graphics_bg_glk;
        V6_GRAPHICS_BG.id = graphics_bg_glk;
        arthur_update_on_resize();
        v6_get_and_sync_upperwin_size();
    }
}

// Z-machine entry point: transitions to inventory display mode.
// Sets the room graphic window to a grid window with subheader style.
void RT_UPDATE_INVT_WINDOW(void) {
    screenmode = MODE_INVENTORY;
    if (ARTHUR_ROOM_GRAPHIC_WIN.id) {
        glk_set_window(ARTHUR_ROOM_GRAPHIC_WIN.id);
        glk_set_style(style_Subheader);
    }
}

// Z-machine entry point: transitions to character status display mode.
void RT_UPDATE_STAT_WINDOW(void) {
    screenmode = MODE_STATUS;
}

// Z-machine entry point: transitions to map display mode.
// Enables mouse events on the background graphics window for clickable map.
void RT_UPDATE_MAP_WINDOW(void) {
    screenmode = MODE_MAP;
    glk_request_mouse_event(graphics_bg_glk);
}

// Z-machine entry point: transitions to verbose room description mode.
void RT_UPDATE_DESC_WINDOW(void) {
    screenmode = MODE_ROOM_DESC;
}

// Makes the status window current and fills it with reverse-video spaces,
// so that no stray characters of earlier content are left. Reverse video
// stays on: the status line the game prints next is in reverse as well.
static void arthur_fill_status_bar(void) {
    glk_set_window(V6_STATUS_WINDOW.id);
    glk_window_move_cursor(V6_STATUS_WINDOW.id, 0, 0);
    glui32 width;
    glk_window_get_size(V6_STATUS_WINDOW.id, &width, nullptr);
    garglk_set_reversevideo(1);
    for (glui32 i = 0; i < width; i++)
        glk_put_char(' ');
}

// Resets the globals in which the game remembers what the status line
// shows, so that its incremental update repaints every field. The form
// is the one field the two callers reset to different values.
static void arthur_invalidate_status_line(uint16_t form) {
    set_global(ag.GL_SL_FORM, form);
    set_global(ag.GL_SL_VEH, 0);
    set_global(ag.GL_SL_HERE, 0);
    set_global(ag.GL_SL_HIDE, 0);
    set_global(ag.GL_SL_TIME, 0);
}

// Z-machine entry point: initializes the screen layout on game start.
// Measures the banner margin image to determine pillar widths, defines
// the left/right banner erase windows and the full-screen background,
// fills the status bar with reverse-video spaces, and resets all status
// line tracking globals so the Z-code will redraw everything.
void arthur_INIT_STATUS_LINE(void) {
    arthur_update_on_resize();

    int M;
    if (!get_image_size(K_PIC_BANNER_MARGIN, &M, nullptr)) { // <PICINF ,K-PIC-BANNER-MARGIN ,K-WIN-TBL>
        M = gcellw * 3;
    }
    int W = gscreenw - 2 * M;
    int L = M + 1;

    v6_define_window(&ARTHUR_LEFT_BANNER_WINDOW, 1, 1, M, gscreenh);
    v6_define_window(&ARTHUR_RIGHT_BANNER_WINDOW, L + W, 1, M, gscreenh);

    // Window 7 (S-FULL) is the standard V6 fullscreen window
    v6_define_window(&V6_GRAPHICS_BG, 1, 1, gscreenw, gscreenh);

    arthur_fill_status_bar();
    garglk_set_reversevideo(0);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);

    if (ag.GL_TIME_WIDTH != 0)
        set_global(ag.GL_TIME_WIDTH, 0);
    arthur_invalidate_status_line(0);
}

// Counts the number of lines in a length-prefixed string table. Each entry
// starts with a 16-bit character count followed by that many bytes of text.
// The table ends when a zero count is encountered. Used by RT_AUTHOR_OFF
// to size the error window.
static int arthur_count_lines(uint32_t table) {
    int lines = 0;
    for (uint16_t count = user_word(table); count != 0; count = user_word(table)) {
        lines++;
        table += 2 + count;
    }
    return lines;
}

// Z-machine entry point: displays the author/copyright message in an
// inverted (reverse-video) grid window at the bottom of the screen.
// Disables stream 3 (memory output), creates the error window sized to
// fit the message text, and prints the K_DIROUT_TBL contents. The text
// format changed between releases (length-prefixed lines in >41, single
// block in <=41). Also shrinks the main text buffer to make room.
void RT_AUTHOR_OFF(void) {
    output_stream(-3, 0);

    uint32_t addr = at.K_DIROUT_TBL;

    if (get_global(fg_global_idx) == DEFAULT_COLOUR) {
        glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_BackColor);
        glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_TextColor);
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_ReverseColor, 1);
    } else {
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_background);
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_ReverseColor, 0);
    }

    ARTHUR_ERROR_WINDOW.fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
    ARTHUR_ERROR_WINDOW.bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
    ARTHUR_ERROR_WINDOW.style.reset(STYLE_REVERSE);
    v6_delete_win(&ARTHUR_ERROR_WINDOW);

    if (ARTHUR_ERROR_WINDOW.id == nullptr && !gli_zmachine_no_err_win)
        ARTHUR_ERROR_WINDOW.id = gli_new_window(wintype_TextGrid, 0);

    int lines;

    if (header.release > 41) {
        lines = arthur_count_lines(addr);
    } else {
        int width_in_chars = (V6_TEXT_BUFFER_WINDOW.x_size - 2 * ggridmarginx) / gcellw;
        if (width_in_chars < 1)
            width_in_chars = 1;
        lines = user_word(addr) / width_in_chars + 1;
        if (user_word(addr) % width_in_chars == 0)
            lines--;
    }

    if (!gli_zmachine_no_err_win) {
        set_global(ag.GL_AUTHOR_SIZE, lines);

        int height = gcellh * lines + 2 * ggridmarginy;

        v6_define_window(&ARTHUR_ERROR_WINDOW, V6_TEXT_BUFFER_WINDOW.x_origin, gscreenh - height + 1, V6_TEXT_BUFFER_WINDOW.x_size, height);

        V6_TEXT_BUFFER_WINDOW.y_size = gscreenh - V6_TEXT_BUFFER_WINDOW.y_origin - height + 1;
        v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
        glk_window_clear(ARTHUR_ERROR_WINDOW.id);
        glk_set_window(ARTHUR_ERROR_WINDOW.id);
    } else {
        glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
    }

    if (header.release > 41) {
        for (uint16_t count = user_word(addr); count != 0; count = user_word(addr)) {
            for (uint16_t i = 0; i < count; i++) {
                put_char(user_byte(addr + 2 + i));
            }
            put_char(ZSCII_NEWLINE);

            addr += 2 + count;
        }
    } else {
        int number_of_letters = user_word(addr);
        for (uint16_t count = 0; count < number_of_letters; count++) {
            put_char(user_byte(addr + 2 + count));
        }
    }

    v6_set_grid_colors(style_Normal);
    glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_ReverseColor);

    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

// What VoiceOver says about a full-screen picture, which the player would
// otherwise have no way of knowing is there.
static const char *arthur_fullscreen_image_description(int picnum) {
    switch (picnum) {
        case K_PIC_TITLE:
            return "\n(Showing title image in a golden frame. The words 'Arthur: The Quest for Excalibur' where the 'L' is a glittering sword.)\n";
        case K_PIC_SWORD:
            return "\n(Showing a full screen image in a golden frame. A sword of gold, with a jewel-encrusted hilt, stuck in a rock, in front of a crude medieval stone church.)\n";
        case K_PIC_SWORD_MERLIN:
            return "\n(Showing a full screen image in a golden frame. Merlin has appeared behind the sword, wearing a cloak. He raises his right hand while stroking his long beard with his left.)\n";
        case K_PIC_ANGRY_DEMON:
            return "\n(Showing a full screen image of a screaming demon.)\n";
        case K_PIC_ENDGAME:
            return "\n(Showing a full screen image in a golden frame. Arthur is holding up Excalibur, smiling, while the face of Merlin is watching from the sky behind.)\n";
        default:
            return "\n(Showing a full screen image.)\n";
    }
}

// Central image dispatch: routes a picture to the appropriate drawing
// method based on its type. Handles four categories:
//   1. Full-screen images (title, sword/Merlin, endgame, demon): displayed
//      in a foreground graphics window covering the entire screen, with
//      VoiceOver descriptions for accessibility.
//   2. Room images and stamps: drawn to the offscreen pixmap via
//      arthur_draw_room_image().
//   3. Map images: markers drawn at (x, y) on the map scroll, or the
//      map background itself which enables mouse events.
//   4. Banner image: flagged for later drawing in flush_image_buffer().
// Returns true if the image was handled.
bool arthur_display_picture(glui32 picnum, glsi32 x, glsi32 y) {

    // Full-screen images (title sequence and special events)
    if ((picnum >= 1 && picnum <= 3) || picnum == K_PIC_ENDGAME || picnum == K_PIC_ANGRY_DEMON) {
        if (current_graphics_buf_win == nullptr || current_graphics_buf_win == graphics_bg_glk) {
            current_graphics_buf_win = graphics_fg_glk;
        }
        glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
        win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
        screenmode = MODE_SLIDESHOW;
        // Each of these is followed by a wait for a key. With graphics
        // off there is nothing to look at, so don't wait.
        if (v6_graphics_off())
            v6_skip_next_keypress();
        glk_request_mouse_event(current_graphics_buf_win);
        if (picnum != K_PIC_SWORD_MERLIN) {
            glk_window_clear(current_graphics_buf_win);
            draw_centered_title_image(picnum);
        } else {
            float scale = draw_centered_title_image(K_PIC_SWORD);
            draw_centered_image(K_PIC_SWORD_MERLIN, scale, 0, 0);
        }
        v6_describe_fullscreen_image(arthur_fullscreen_image_description(picnum));
        return true;
    } else if (current_graphics_buf_win == nullptr) {
        current_graphics_buf_win = graphics_bg_glk;
    }

    // Room images
    if (is_arthur_room_image(picnum) || is_arthur_stamp_image(picnum)) {
        screenmode = MODE_NORMAL;
        ensure_pixmap(current_graphics_buf_win);
        arthur_draw_room_image(picnum);
        return true;
    } else if (is_arthur_map_image(picnum)) {
        screenmode = MODE_MAP;
        if (picnum == K_MAP_SCROLL) {// map background
            glk_request_mouse_event(graphics_bg_glk);
        } else {
            arthur_draw_map_image(picnum, x, y);
            return true;
        }
    } else if (picnum == K_PIC_BANNER) { // Border image, drawn in flush_image_buffer()
        screenmode = MODE_NORMAL;
        return true;
    }

    draw_to_buffer(current_graphics_buf_win, picnum, x, y);
    return true;
}

// Handles window erase requests with Arthur-specific logic.
//   index -1: full screen erase — transitions between slideshow, initial
//     question, and normal modes. Clears graphics windows and adjusts
//     layout for the endgame and demon images.
//   index 2: clears the room image buffer.
//   index 3: deletes the error/author message window.
void arthur_erase_window(int16_t index) {
    if (!is_spatterlight_arthur)
        return;

    switch (index) {
        case -1:
            if (screenmode == MODE_SLIDESHOW) {
                clear_image_buffer();
                glk_window_set_background_color(graphics_bg_glk, user_selected_background);
                glk_window_clear(graphics_bg_glk);
                // With an Apple II interpreter number the game shows no
                // title pictures, so there may be no foreground window yet.
                if (graphics_fg_glk != nullptr) {
                    glk_window_set_background_color(graphics_fg_glk, user_selected_background);
                    glk_window_clear(graphics_fg_glk);
                }
                if (last_slideshow_pic == K_PIC_ANGRY_DEMON) {
                    screenmode = MODE_NORMAL;
                } else {
                    screenmode = MODE_INITIAL_QUESTION;
                    win_sizewin(V6_STATUS_WINDOW.id->peer, 0, 0, 0, 0);
                }
                current_graphics_buf_win = nullptr;
                if (graphics_fg_glk != nullptr)
                    win_sizewin(graphics_fg_glk->peer, 0, 0, 0, 0);
                if (last_slideshow_pic == K_PIC_ENDGAME) {
                    glk_window_clear(graphics_bg_glk);
                    v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
                    win_sizewin(V6_TEXT_BUFFER_WINDOW.id->peer, 0, 0, gscreenw, gscreenh);
                }

            } else if (screenmode == MODE_INITIAL_QUESTION) {
                screenmode = MODE_SLIDESHOW;
            }
            v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
            break;
        case 2:
            clear_image_buffer();
            break;
        case 3:
            v6_delete_win(&ARTHUR_ERROR_WINDOW);
            break;
        default:
            break;
    }
}

#pragma mark - Save/Restore State

// Saves Arthur-specific UI state into the autosave data structure:
// graphics window tags, foreground window tag, stored buffer window
// tag, and the last slideshow picture number.
void arthur_stash_state(library_state_data *dat) {

    if (!dat)
        return;

    v6_stash_graphics_windows(dat);
    dat->slideshow_pic = last_slideshow_pic;
}

// Restores Arthur-specific UI state from autosave data: looks up Glk
// windows by their saved tags and restores the last slideshow picture.
void arthur_recover_state(library_state_data *dat) {

    if (!dat)
        return;

    v6_recover_graphics_windows(dat);
    last_slideshow_pic = dat->slideshow_pic;
}

// Performs UI rebuild after a manual RESTORE command: syncs the screen
// mode from the Z-machine global and recolors the screen according to the
// colors set in the save file.
void arthur_update_after_restore(void) {
    arthur_sync_screenmode();
    after_V_COLOR();
}

// Performs a lighter UI rebuild after autorestore (app relaunch).
// Re-applies foreground/background colors to all windows, reopens the
// foreground graphics window, and triggers a window change event.
void arthur_update_after_autorestore(void) {
    update_user_defined_colours();
    v6_set_window_colors_from_globals();

    v6_close_and_reopen_front_graphics_window();

    window_change();
}

// Z-machine entry point: handles F-key presses to switch display modes.
// Variable 1 contains the ZSCII key code. If the requested mode differs
// from the current one, updates the Z-machine global and rebuilds the UI.
// Ignored during hints and slideshow modes.
void RT_HOT_KEY(void) {
    if (screenmode == MODE_HINTS || screenmode == MODE_SLIDESHOW)
        return;

    // No picture or map mode with graphics off: stay in text-only mode.
    if (v6_graphics_off() && (variable(1) == ZSCII_F1 || variable(1) == ZSCII_F2))
        store_variable(1, ZSCII_F6);

    for (const ArthurMode &m : arthur_modes) {
        if (m.key != variable(1))
            continue;
        if (get_global(ag.GL_WINDOW_TYPE) != m.wintype) {
            set_global(ag.GL_WINDOW_TYPE, m.wintype);
            screenmode = m.mode;
            arthur_update_on_resize();
        }
        return;
    }
}

// Z-machine entry point: forces a full status bar redraw. Fills the entire
// status bar with reverse-video spaces to prevent stray characters from
// previous content, then resets all status line tracking globals so the
// Z-code's incremental update logic will repaint everything.
void ARTHUR_UPDATE_STATUS_LINE(void) {
    arthur_fill_status_bar();
    arthur_invalidate_status_line(99);
}

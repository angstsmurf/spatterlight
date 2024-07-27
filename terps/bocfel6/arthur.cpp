//
//  arthur.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

extern "C" {
#include "glk.h"
#include "glkimp.h"
}

#include "draw_image.hpp"
#include "entrypoints.hpp"
#include "memory.h"
#include "screen.h"
#include "zterp.h"
#include "v6_shared.hpp"

#include "arthur.hpp"

extern float imagescalex, imagescaley;

extern uint32_t update_status_bar_address;
extern uint32_t update_picture_address;
extern uint32_t update_inventory_address;
extern uint32_t update_stats_address;
extern uint32_t update_map_address;
extern uint8_t update_global_idx;
extern uint8_t global_map_grid_y_idx;

bool showing_wide_arthur_room_image = false;

int arthur_text_top_margin = -1;
int arthur_pic_top_margin = 0;

bool is_arthur_stamp_image(int picnum) {
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

bool is_arthur_map_image(int picnum) {
    return (picnum >= 105 && picnum <= 153);
}


#define K_PIC_BANNER_MARGIN 100
#define K_MAP_SCROLL 137

void adjust_arthur_top_margin(void) {
    int mapheight, borderheight;
    get_image_size(K_MAP_SCROLL, nullptr, &mapheight); // Get height of map background image
    get_image_size(K_PIC_BANNER_MARGIN, nullptr, &borderheight); // Get height of border measuring dummy image
    float y_margin = mapheight * imagescaley;
    arthur_text_top_margin = ceil(y_margin / gcellh) * gcellh;
    arthur_pic_top_margin = arthur_text_top_margin / imagescaley - borderheight - 4;
    if (arthur_pic_top_margin < 0)
        arthur_pic_top_margin = 0;
    fprintf(stderr, "Arthur text top: %d\n", arthur_text_top_margin);
    fprintf(stderr, "Arthur pic top: %d\n", arthur_pic_top_margin);
}


void draw_arthur_room_image(int picnum) {
    int x, y, width, height;
    // K-PIC-CHURCHYARD=4
    // K-PIC-PARADE-AREA=17
    // K-PIC-AIR-SCENE=163

    if (picnum == 17 || picnum == 163) {
        get_image_size(picnum, &width, &height);
        showing_wide_arthur_room_image = true;
    } else {
        get_image_size(4, &width, &height);
        showing_wide_arthur_room_image = false;
    }

    x = ((hw_screenwidth - width) / 2.0);
    y = arthur_pic_top_margin;
    if (showing_wide_arthur_room_image)
        y--;

    if (graphics_type == kGraphicsTypeMacBW) {
        y += 11;
        x++;
    } else {
        y += 7;
    }

    if (is_arthur_stamp_image(picnum)) {
        //    K-PIC-ISLAND-DOOR=158
        if (picnum == 158)
            get_image_size(159, &width, &height);
        else
            get_image_size(picnum - 1, &width, &height);

        if (picnum == 9 && graphics_type == kGraphicsTypeAmiga)
            width -= 4;

        x += width;
        y += height;
    } else if (!is_arthur_room_image(picnum)) {
        return;
    }

    draw_to_pixmap_unscaled(picnum, x, y);
}


static void draw_arthur_map_image(int picnum, int x, int y) {
    int x_margin;
    get_image_size(K_PIC_BANNER_MARGIN, &x_margin, nullptr);
    x += x_margin;
    if (options.int_number == INTERP_MACINTOSH)
        y--;
    if (graphics_type != kGraphicsTypeApple2 && graphics_type != kGraphicsTypeMacBW){
        if (picnum >= 138)
            y--;
    }
    draw_to_pixmap_unscaled(picnum, x, y);
}

void ARTHUR_UPDATE_STATUS_LINE(void) {}

extern std::array<Window, 8> windows;
extern Window *mainwin, *curwin, *upperwin;

// Window 0 (S-TEXT) is the standard V6 text buffer window
// Window 1 (S-WINDOW) is the standard V6 status window

// Window 2 is the small room graphics window at top, not including banners
// Window 3 is the inverted error window at the bottom

// Windows 5 and 6 are left and right banners, but are only used when erasing
// banner graphics. The banners are drawn to window S-FULL (7)

// Window 7 (S-FULL) is the standard V6 fullscreen window

// Called on window_changed();
void arthur_update_on_resize(void) {
    adjust_arthur_top_margin();

    // We delete Arthur error window on resize
    if (windows[3].id && windows[3].id->type == wintype_TextGrid) {
        v6_delete_win(&windows[3]);
    }
    if (screenmode != MODE_SLIDESHOW) {
        if (screenmode != MODE_NO_GRAPHICS) {
            fprintf(stderr, "adjusting margins in window_change\n");
            int x_margin = 0, y_margin = 0;
            get_image_size(K_PIC_BANNER_MARGIN, &x_margin, &y_margin);

            x_margin *= imagescalex;
            y_margin *= imagescaley;

            if (screenmode != MODE_HINTS)
                upperwin->y_origin = arthur_text_top_margin;

            int width_in_chars = ((float)(gscreenw - 2 * x_margin) / gcellw);
            upperwin->x_size = width_in_chars * gcellw;
            upperwin->x_origin = (gscreenw - upperwin->x_size) / 2;
            if (graphics_type == kGraphicsTypeApple2)
                upperwin->x_size -= gcellw;
            mainwin->x_origin = upperwin->x_origin;
            mainwin->x_size = upperwin->x_size;

            if (screenmode == MODE_HINTS) {
                upperwin->y_size = 3 * gcellh + 2 * ggridmarginy;
                mainwin->y_origin = upperwin->y_origin + upperwin->y_size + gcellh;
            } else {
                upperwin->y_size = gcellh + 2 * ggridmarginy;
                mainwin->y_origin = upperwin->y_origin + upperwin->y_size;
            }

            mainwin->y_size = gscreenh - mainwin->y_origin;

            v6_sizewin(upperwin);
            v6_sizewin(mainwin);

            if (screenmode == MODE_HINTS) {
                glk_window_clear(current_graphics_buf_win);
                redraw_hint_screen_on_resize();
                return;
            }

            if (screenmode != MODE_MAP) {
                windows[2].y_size = arthur_text_top_margin;
                windows[2].x_size = upperwin->x_size;
                v6_sizewin(&windows[2]);
            }

            if (screenmode == MODE_NORMAL) {
                internal_call_with_arg(pack_routine(update_picture_address), 1);
                draw_arthur_side_images(current_graphics_buf_win);
                if (showing_wide_arthur_room_image)
                    draw_arthur_room_image(current_picture);
            } else if (screenmode == MODE_INVENTORY) {
                internal_call_with_arg(pack_routine(update_inventory_address), 1);
            } else if (screenmode == MODE_STATUS) {
                internal_call_with_arg(pack_routine(update_stats_address), 1);
            } else if (screenmode == MODE_MAP) {
                internal_call_with_arg(pack_routine(update_map_address), 1);
            }
        } else {
            upperwin->x_size = gscreenw;
            upperwin->y_size = gcellh + 2 * ggridmarginy;
            v6_sizewin(upperwin);
        }

        set_global(update_global_idx, 0);
        internal_call(pack_routine(update_status_bar_address));
    } else {
        v6_delete_glk_win(current_graphics_buf_win);
        current_graphics_buf_win = v6_new_glk_window(wintype_Graphics);
        clear_image_buffer();
        ensure_pixmap(current_graphics_buf_win);
        mainwin->x_size = gscreenw;
        mainwin->y_size = gscreenh;
        v6_sizewin(mainwin);
        if (last_slideshow_pic == 3) {
            draw_centered_title_image(2);
            draw_centered_title_image(3);
        } else {
            draw_centered_title_image(last_slideshow_pic);
        }
    }
    flush_bitmap(current_graphics_buf_win);
}

#define ROOM_GRAPHIC_WIN windows[2]


// Only called when set_current_window is called with 2,
// I when the room graphics window is set to current
void adjust_arthur_windows(void) {
    int x_margin;
    get_image_size(K_PIC_BANNER_MARGIN, &x_margin, nullptr); // Get width of dummy "margin" image
    if (screenmode != MODE_SLIDESHOW && screenmode != MODE_NO_GRAPHICS && screenmode != MODE_HINTS) {
        fprintf(stderr, "adjusting margins in adjust_arthur_windows\n");
        adjust_arthur_top_margin();
        upperwin->y_origin = arthur_text_top_margin;
        int width_in_chars = ((float)(gscreenw - 2 * x_margin * imagescalex) / gcellw);
        upperwin->x_size = width_in_chars * gcellw;
        upperwin->x_origin = (gscreenw - upperwin->x_size) / 2;
        if (graphics_type == kGraphicsTypeApple2)
            upperwin->x_origin += imagescalex;
        mainwin->x_size = upperwin->x_size;
        mainwin->x_origin = upperwin->x_origin;
        ROOM_GRAPHIC_WIN.x_origin = upperwin->x_origin;
        ROOM_GRAPHIC_WIN.x_size = upperwin->x_size;
        upperwin->y_size = gcellh + 2 * ggridmarginy;
        mainwin->y_origin = upperwin->y_origin + upperwin->y_size;
        mainwin->y_size = gscreenh - mainwin->y_origin - 1;
        v6_sizewin(upperwin);
        v6_sizewin(mainwin);
        ROOM_GRAPHIC_WIN.y_origin = 1;
        ROOM_GRAPHIC_WIN.y_size = arthur_text_top_margin;
    }
    if (screenmode == MODE_INVENTORY || screenmode == MODE_STATUS) {
        ROOM_GRAPHIC_WIN.y_origin = 1;
        ROOM_GRAPHIC_WIN.x_origin = upperwin->x_origin;
        if (ROOM_GRAPHIC_WIN.id && ROOM_GRAPHIC_WIN.id->type != wintype_TextGrid) {
            v6_delete_win(&ROOM_GRAPHIC_WIN);
        }

        if (ROOM_GRAPHIC_WIN.id == nullptr) {
            v6_remap_win_to_grid(&ROOM_GRAPHIC_WIN);
        }
        if (graphics_win_glk) {
            glk_window_set_background_color(graphics_win_glk, user_selected_background);
            glk_window_clear(graphics_win_glk);
        }
    } else if (screenmode == MODE_HINTS) {
        win_setbgnd(mainwin->id->peer, user_selected_background);
        mainwin->fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
        mainwin->bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
        return;
    } else if (screenmode == MODE_MAP) {
        int mapheight;
        get_image_size(K_MAP_SCROLL, nullptr, &mapheight); // Get height of map background image
        ROOM_GRAPHIC_WIN.y_origin = 0;
        ROOM_GRAPHIC_WIN.y_size = mapheight;
        ROOM_GRAPHIC_WIN.x_origin = x_margin;
        ROOM_GRAPHIC_WIN.x_size = hw_screenwidth - 2 * x_margin;
        v6_delete_win(&ROOM_GRAPHIC_WIN);
    } else if (screenmode == MODE_ROOM_DESC) {

        if (ROOM_GRAPHIC_WIN.id &&ROOM_GRAPHIC_WIN.id->type != wintype_TextBuffer) {
            v6_delete_win(&ROOM_GRAPHIC_WIN);
        }
        v6_remap_win_to_buffer(&ROOM_GRAPHIC_WIN);
        glk_window_clear(graphics_win_glk);
    } else if (screenmode == MODE_NO_GRAPHICS) {
    } else if (ROOM_GRAPHIC_WIN.id) {
        v6_delete_win(&ROOM_GRAPHIC_WIN);
    }
    v6_sizewin(&ROOM_GRAPHIC_WIN);
}


void RT_UPDATE_PICT_WINDOW(void) {
    fprintf(stderr, "RT_UPDATE_PICT_WINDOW\n");
}

void RT_UPDATE_INVT_WINDOW(void) {
    fprintf(stderr, "RT_UPDATE_INVT_WINDOW\n");
}

void RT_UPDATE_STAT_WINDOW(void) {
    fprintf(stderr, "RT_UPDATE_STAT_WINDOW\n");
}

void RT_UPDATE_MAP_WINDOW(void) {
    fprintf(stderr, "RT_UPDATE_MAP_WINDOW\n");
}

extern bool arthur_intro_timer;

bool arthur_display_picture(glsi32 x, glsi32 y) {
    // Skip Arthur "sidebars"
    // We draw them in flush_image_buffer() instead
    if (current_picture == 170 || current_picture == 171) {
        return true;
    }
    // Intro "slideshow"
    if ((current_picture >= 1 && current_picture <= 3) || current_picture == 84 || current_picture == 85) {
        if (current_graphics_buf_win == nullptr || current_graphics_buf_win == graphics_win_glk) {
            current_graphics_buf_win = v6_new_glk_window(wintype_Graphics);
            glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
            win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
        }
        screenmode = MODE_SLIDESHOW;
        glk_request_mouse_event(current_graphics_buf_win);
        ensure_pixmap(current_graphics_buf_win);
        if (current_picture == 3) {
            arthur_intro_timer = true;
            glk_request_timer_events(500);
            return true;
        }
        draw_centered_title_image(current_picture);
        return true;
    }

    else if (current_graphics_buf_win == nullptr) {
        current_graphics_buf_win = graphics_win_glk;
    }


    // Room images
    if (is_arthur_room_image(current_picture) || is_arthur_stamp_image(current_picture)) { // Pictures 106-149 are map images
        screenmode = MODE_NORMAL;
        ensure_pixmap(current_graphics_buf_win);
        draw_arthur_room_image(current_picture);
        return true;
    } else if (is_arthur_map_image(current_picture)) {
        screenmode = MODE_MAP;
        if (current_picture == K_MAP_SCROLL) {// map background
            glk_request_mouse_event(graphics_win_glk);
        } else {
            draw_arthur_map_image(current_picture, x, y);
            return true;
        }
    } else if (current_picture == 54) { // Border image, drawn in flush_image_buffer()
        screenmode = MODE_NORMAL;
        if (arthur_intro_timer) {
            arthur_intro_timer = 0;
            glk_request_timer_events(0);
        }
        return true;
    }

    draw_to_buffer(current_graphics_buf_win, current_picture, x, y);
    return true;
}

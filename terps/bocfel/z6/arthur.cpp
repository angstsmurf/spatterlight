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
#include "options.h"
#include "screen.h"
#include "zterp.h"
#include "unicode.h"
#include "v6_specific.h"
#include "v6_shared.hpp"

#include "arthur.hpp"

extern float imagescalex, imagescaley;

ArthurGlobals ag;
ArthurRoutines ar;
//ArthurObjects ao;
//ArthurAttributes aa;

extern int current_picture;

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


void arthur_draw_room_image(int picnum) {
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


static void arthur_draw_map_image(int picnum, int x, int y) {
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

enum kArthurWindowType {
    K_WIN_NONE = 0,
    K_WIN_INVT = 1,
    K_WIN_DESC = 2,
    K_WIN_STAT = 3,
    K_WIN_MAP = 4,
    K_WIN_PICT = 5
};

void arthur_sync_screenmode(void) {
    kArthurWindowType window_type = (kArthurWindowType)get_global(ag.GL_WINDOW_TYPE);
    switch (window_type) {
        case K_WIN_NONE:
            screenmode = MODE_NO_GRAPHICS;
            break;
        case K_WIN_INVT:
            screenmode = MODE_INVENTORY;
            break;
        case K_WIN_DESC:
            screenmode = MODE_ROOM_DESC;
            break;
        case K_WIN_STAT:
            screenmode = MODE_STATUS;
            break;
        case K_WIN_MAP:
            screenmode = MODE_MAP;
            break;
        case K_WIN_PICT:
            screenmode = MODE_NORMAL;
            break;
    }
}


void ARTHUR_UPDATE_STATUS_LINE(void) {

}

extern Window *mainwin, *curwin;

#define ARTHUR_GRAPHICS_BG windows[7]
#define ARTHUR_ROOM_GRAPHIC_WIN windows[2]

#define ARTHUR_ERROR_WINDOW windows[3]

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
    if (ARTHUR_ERROR_WINDOW.id && ARTHUR_ERROR_WINDOW.id->type == wintype_TextGrid) {
        v6_delete_win(&ARTHUR_ERROR_WINDOW);
    }
    if (screenmode != MODE_SLIDESHOW) {
        if (screenmode != MODE_NO_GRAPHICS) {
            fprintf(stderr, "adjusting margins in window_change\n");
            int x_margin = 0, y_margin = 0;
            get_image_size(K_PIC_BANNER_MARGIN, &x_margin, &y_margin);

            x_margin *= imagescalex;
            y_margin *= imagescaley;

            if (screenmode != MODE_HINTS)
                V6_STATUS_WINDOW.y_origin = arthur_text_top_margin;

            int width_in_chars = ((float)(gscreenw - 2 * x_margin) / gcellw);
            V6_STATUS_WINDOW.x_size = width_in_chars * gcellw;
            V6_STATUS_WINDOW.x_origin = (gscreenw - V6_STATUS_WINDOW.x_size) / 2;
            if (graphics_type == kGraphicsTypeApple2)
                V6_STATUS_WINDOW.x_size -= gcellw;
            mainwin->x_origin = V6_STATUS_WINDOW.x_origin;
            mainwin->x_size = V6_STATUS_WINDOW.x_size;

            if (screenmode == MODE_HINTS) {
                V6_STATUS_WINDOW.y_size = 3 * gcellh + 2 * ggridmarginy;
                mainwin->y_origin = V6_STATUS_WINDOW.y_origin + V6_STATUS_WINDOW.y_size + gcellh;
            } else {
                V6_STATUS_WINDOW.y_size = gcellh + 2 * ggridmarginy;
                mainwin->y_origin = V6_STATUS_WINDOW.y_origin + V6_STATUS_WINDOW.y_size;
            }

            mainwin->y_size = gscreenh - mainwin->y_origin;

            v6_sizewin(&V6_STATUS_WINDOW);
            v6_sizewin(mainwin);

            if (screenmode == MODE_HINTS) {
                glk_window_clear(current_graphics_buf_win);
                redraw_hint_screen_on_resize();
                return;
            }

            if (screenmode != MODE_MAP) {
                windows[2].y_size = arthur_text_top_margin;
                windows[2].x_size = V6_STATUS_WINDOW.x_size;
                v6_sizewin(&windows[2]);
            }

            if (screenmode == MODE_NORMAL) {
                internal_call_with_arg(pack_routine(ar.RT_UPDATE_PICT_WINDOW), 1);
                draw_arthur_side_images(current_graphics_buf_win);
                if (showing_wide_arthur_room_image)
                    arthur_draw_room_image(current_picture);
            } else if (screenmode == MODE_INVENTORY) {
                internal_call_with_arg(pack_routine(ar.RT_UPDATE_INVT_WINDOW), 1);
            } else if (screenmode == MODE_STATUS) {
                internal_call_with_arg(pack_routine(ar.RT_UPDATE_STAT_WINDOW), 1);
            } else if (screenmode == MODE_MAP) {
                internal_call_with_arg(pack_routine(ar.RT_UPDATE_MAP_WINDOW), 1);
            }
        } else {
            V6_STATUS_WINDOW.x_size = gscreenw;
            V6_STATUS_WINDOW.y_size = gcellh + 2 * ggridmarginy;
            v6_sizewin(&V6_STATUS_WINDOW);
        }

        set_global(ag.UPDATE, 0);
        internal_call(pack_routine(ar.UPDATE_STATUS_LINE));
    } else {
        v6_delete_glk_win(current_graphics_buf_win);
        current_graphics_buf_win = gli_new_window(wintype_Graphics, 0);
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



// Only called when set_current_window is called with 2,
// I when the room graphics window is set to current
// EDIT: Now called after INIT-STATUS-LINE
void arthur_adjust_windows(void) {
//    arthur_sync_screenmode();
    int x_margin;
    get_image_size(K_PIC_BANNER_MARGIN, &x_margin, nullptr); // Get width of dummy "margin" image
    if (screenmode != MODE_SLIDESHOW && screenmode != MODE_NO_GRAPHICS && screenmode != MODE_HINTS) {
        fprintf(stderr, "adjusting margins in adjust_arthur_windows\n");
        adjust_arthur_top_margin();
        V6_STATUS_WINDOW.y_origin = arthur_text_top_margin;
        int width_in_chars = ((float)(gscreenw - 2 * x_margin * imagescalex) / gcellw);
        V6_STATUS_WINDOW.x_size = width_in_chars * gcellw;
        V6_STATUS_WINDOW.x_origin = (gscreenw - V6_STATUS_WINDOW.x_size) / 2;
        if (graphics_type == kGraphicsTypeApple2)
            V6_STATUS_WINDOW.x_origin += imagescalex;
        mainwin->x_size = V6_STATUS_WINDOW.x_size;
        mainwin->x_origin = V6_STATUS_WINDOW.x_origin;
        ARTHUR_ROOM_GRAPHIC_WIN.x_origin = V6_STATUS_WINDOW.x_origin;
        ARTHUR_ROOM_GRAPHIC_WIN.x_size = V6_STATUS_WINDOW.x_size;
        V6_STATUS_WINDOW.y_size = gcellh + 2 * ggridmarginy;
        mainwin->y_origin = V6_STATUS_WINDOW.y_origin + V6_STATUS_WINDOW.y_size;
        mainwin->y_size = gscreenh - mainwin->y_origin - 1;
        v6_sizewin(&V6_STATUS_WINDOW);
        v6_sizewin(mainwin);
        ARTHUR_ROOM_GRAPHIC_WIN.y_origin = 1;
        ARTHUR_ROOM_GRAPHIC_WIN.y_size = arthur_text_top_margin;
    }
    if (screenmode == MODE_INVENTORY || screenmode == MODE_STATUS) {
        ARTHUR_ROOM_GRAPHIC_WIN.y_origin = 1;
        ARTHUR_ROOM_GRAPHIC_WIN.x_origin = V6_STATUS_WINDOW.x_origin;
        if (ARTHUR_ROOM_GRAPHIC_WIN.id && ARTHUR_ROOM_GRAPHIC_WIN.id->type != wintype_TextGrid) {
            v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
        }

        if (ARTHUR_ROOM_GRAPHIC_WIN.id == nullptr) {
            v6_remap_win_to_grid(&ARTHUR_ROOM_GRAPHIC_WIN);
        }
        if (graphics_bg_glk) {
            glk_window_set_background_color(graphics_bg_glk, user_selected_background);
            glk_window_clear(graphics_bg_glk);
        }
    } else if (screenmode == MODE_HINTS) {
        win_setbgnd(mainwin->id->peer, user_selected_background);
        mainwin->fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
        mainwin->bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
        return;
    } else if (screenmode == MODE_MAP) {
        int mapheight;
        get_image_size(K_MAP_SCROLL, nullptr, &mapheight); // Get height of map background image
        ARTHUR_ROOM_GRAPHIC_WIN.y_origin = 1;
        ARTHUR_ROOM_GRAPHIC_WIN.y_size = mapheight;
        ARTHUR_ROOM_GRAPHIC_WIN.x_origin = x_margin;
        ARTHUR_ROOM_GRAPHIC_WIN.x_size = hw_screenwidth - 2 * x_margin;
        v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
    } else if (screenmode == MODE_ROOM_DESC) {

        if (ARTHUR_ROOM_GRAPHIC_WIN.id &&ARTHUR_ROOM_GRAPHIC_WIN.id->type != wintype_TextBuffer) {
            v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
        }
        v6_remap_win_to_buffer(&ARTHUR_ROOM_GRAPHIC_WIN);
        glk_window_clear(graphics_bg_glk);
    } else if (screenmode == MODE_NO_GRAPHICS) {
        V6_STATUS_WINDOW.x_origin = 1;
        V6_STATUS_WINDOW.y_origin = 1;
        V6_STATUS_WINDOW.x_size = gscreenw;
        V6_STATUS_WINDOW.y_size = gcellh + 2 * ggridmarginy;
        mainwin->x_size = gscreenw;
        mainwin->x_origin = 1;
        mainwin->y_origin = V6_STATUS_WINDOW.y_origin + V6_STATUS_WINDOW.y_size;
        mainwin->y_size = gscreenh - mainwin->y_origin - 1;
        v6_sizewin(&V6_STATUS_WINDOW);
        v6_sizewin(mainwin);
        clear_image_buffer();
    } else if (ARTHUR_ROOM_GRAPHIC_WIN.id) {
        v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
    }
    v6_sizewin(&ARTHUR_ROOM_GRAPHIC_WIN);
}

void arthur_toggle_slideshow_windows(void) {
    if (current_graphics_buf_win == graphics_fg_glk) {
        current_graphics_buf_win = nullptr;
        win_sizewin(graphics_fg_glk->peer, 0, 0, 0, 0);
    } else if (current_graphics_buf_win == nullptr) {
        current_graphics_buf_win = graphics_fg_glk;
        win_sizewin(graphics_fg_glk->peer, 0, 0, gscreenw, gscreenh);
    } else {
        fprintf(stderr, "arthur_toggle_slideshow_windows: ERROR!\n");
    }
}

void arthur_change_current_window(void) {
    if (curwin == &windows[2]) {
        arthur_adjust_windows();

        // Handle Arthur bottom "error window"
    } else if (curwin == &windows[3]) {
        if (curwin->id && curwin->id->type != wintype_TextGrid && curwin != &windows[1]) {
            v6_delete_win(curwin);
        }
        curwin->y_origin = gscreenh - 2 * (gcellh + ggridmarginy);
        curwin->y_size = gcellh + 2 * ggridmarginy;
        if (curwin->id == nullptr) {
            v6_remap_win_to_grid(curwin);
        }
        //            curwin->fg_color = Color(Color::Mode::ANSI, get_global((fg_global_idx)));
        //            curwin->bg_color = Color(Color::Mode::ANSI, get_global((bg_global_idx)));

        fprintf(stderr, "internal fg:%d internal bg: %d\n", get_global(fg_global_idx), get_global(bg_global_idx));
        curwin->style.flip(STYLE_REVERSE);
//        glui32 bgcol = gsfgcol;
        glui32 zfgcol = get_global(fg_global_idx);
        curwin->fg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
        curwin->bg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
        if (zfgcol > 1) {
//            bgcol = zcolor_map[zfgcol];
            curwin->fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
            curwin->bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
        }
//        if (curwin->id->type == wintype_Graphics)
//            glk_window_set_background_color(curwin->id, bgcol);
        mainwin->y_size = curwin->y_origin - mainwin->y_origin;
        v6_sizewin(mainwin);
    }
}



void RT_UPDATE_PICT_WINDOW(void) {
    fprintf(stderr, "RT_UPDATE_PICT_WINDOW\n");
    if (screenmode == MODE_SLIDESHOW) {
        screenmode = MODE_NORMAL;
        arthur_toggle_slideshow_windows();
    }
}

void RT_UPDATE_INVT_WINDOW(void) {
    fprintf(stderr, "RT_UPDATE_INVT_WINDOW\n");
    screenmode = MODE_INVENTORY;
}

void RT_UPDATE_STAT_WINDOW(void) {
    fprintf(stderr, "RT_UPDATE_STAT_WINDOW\n");
    screenmode = MODE_STATUS;
}

void RT_UPDATE_MAP_WINDOW(void) {
    fprintf(stderr, "RT_UPDATE_MAP_WINDOW\n");
    screenmode = MODE_MAP;
    glk_request_mouse_event(graphics_bg_glk);
}

void RT_UPDATE_DESC_WINDOW(void) {
    fprintf(stderr, "RT_UPDATE_MAP_WINDOW\n");
    screenmode = MODE_MAP;
    glk_request_mouse_event(graphics_bg_glk);
}

void INIT_STATUS_LINE(void) {
    arthur_adjust_windows();
}

void after_INIT_STATUS_LINE(void) {
//    arthur_adjust_windows();
}


bool arthur_display_picture(glui32 picnum, glsi32 x, glsi32 y) {
    // Skip Arthur "sidebars"
    // We draw them in flush_image_buffer() instead
    if (picnum == 170 || picnum == 171) {
        return true;
    }
    // Intro "slideshow"
    if ((picnum >= 1 && picnum <= 3) || picnum == 84 || picnum == 85) {
        if (current_graphics_buf_win == nullptr || current_graphics_buf_win == graphics_bg_glk) {
            current_graphics_buf_win = graphics_fg_glk; //gli_new_window(wintype_Graphics, 0);
            glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
            win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
        }
        screenmode = MODE_SLIDESHOW;
        glk_request_mouse_event(current_graphics_buf_win);
        ensure_pixmap(current_graphics_buf_win);
        draw_centered_title_image(picnum);
        return true;
    }

    else if (current_graphics_buf_win == nullptr) {
        current_graphics_buf_win = graphics_bg_glk;
    }


    // Room images
    if (is_arthur_room_image(picnum) || is_arthur_stamp_image(picnum)) { // Pictures 106-149 are map images
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
    } else if (picnum == 54) { // Border image, drawn in flush_image_buffer()
        screenmode = MODE_NORMAL;
        return true;
    }

    draw_to_buffer(current_graphics_buf_win, picnum, x, y);
    return true;
}

void arthur_hotkeys(uint8_t key) {
    // Delete Arthur error window
    v6_delete_glk_win(windows[3].id);

    if (key != ZSCII_NEWLINE) {
        switch(key) {
            case ZSCII_F1:
                screenmode = MODE_NORMAL;
                break;
            case ZSCII_F2:
                screenmode = MODE_MAP;
                glk_request_mouse_event(graphics_bg_glk);
                break;
            case ZSCII_F3:
                screenmode = MODE_INVENTORY;
                break;
            case ZSCII_F4:
                screenmode = MODE_STATUS;
                break;
            case ZSCII_F5:
                screenmode = MODE_ROOM_DESC;
                break;
            case ZSCII_F6:
                screenmode = MODE_NO_GRAPHICS;
                break;
            default:
                break;
        }
    }
}

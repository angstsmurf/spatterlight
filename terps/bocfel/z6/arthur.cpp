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
#include "objects.h"
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
ArthurTables at;

extern Window *mainwin, *curwin;

#define ARTHUR_GRAPHICS_BG windows[7]
#define ARTHUR_ROOM_GRAPHIC_WIN windows[2]
#define ARTHUR_ERROR_WINDOW windows[3]

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
        if (picnum >= 138 || graphics_type == kGraphicsTypeCGA) {
            y--;
        }
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
                redraw_hint_screen_on_resize();
                glk_request_mouse_event(V6_TEXT_BUFFER_WINDOW.id);
                glk_request_mouse_event(V6_STATUS_WINDOW.id);
                return;
            }

            if (screenmode != MODE_MAP) {
                ARTHUR_ROOM_GRAPHIC_WIN.y_size = arthur_text_top_margin;
                ARTHUR_ROOM_GRAPHIC_WIN.x_size = V6_STATUS_WINDOW.x_size;
                v6_sizewin(&ARTHUR_ROOM_GRAPHIC_WIN);
            }

            v6_get_and_sync_upperwin_size();

            if (screenmode == MODE_NORMAL) {
                glk_window_fill_rect(current_graphics_buf_win, user_selected_background, 0, 0, gscreenw, gscreenh);
                clear_image_buffer();
                internal_call_with_arg(pack_routine(ar.RT_UPDATE_PICT_WINDOW), 1);
                draw_arthur_side_images(current_graphics_buf_win);
                if (showing_wide_arthur_room_image)
                    arthur_draw_room_image(current_picture);
            } else if (screenmode == MODE_INVENTORY) {
                internal_call_with_arg(pack_routine(ar.RT_UPDATE_INVT_WINDOW), 1);
            } else if (screenmode == MODE_STATUS) {
                internal_call_with_arg(pack_routine(ar.RT_UPDATE_STAT_WINDOW), 1);
            } else if (screenmode == MODE_MAP) {
                set_global(ag.GL_MAP_GRID_Y, 0);
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
        glk_window_clear(current_graphics_buf_win);
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
        v6_remap_win_to_grid(&ARTHUR_ROOM_GRAPHIC_WIN);

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
        if (ARTHUR_ROOM_GRAPHIC_WIN.id && ARTHUR_ROOM_GRAPHIC_WIN.id->type != wintype_TextBuffer) {
            v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
        }

        v6_remap_win_to_buffer(&ARTHUR_ROOM_GRAPHIC_WIN);
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
        v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
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
    if (curwin == &ARTHUR_ROOM_GRAPHIC_WIN) {
        arthur_adjust_windows();

        // Handle Arthur bottom "error window"
    } else if (curwin == &ARTHUR_ERROR_WINDOW && get_global(ag.GL_AUTHOR_SIZE) != 0) {
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

void arthur_move_cursor(int16_t y, int16_t x, winid_t win) {
    x--;
    y--;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    glk_window_move_cursor(win, x, y);
}

void RT_UPDATE_PICT_WINDOW(void) {
    if (screenmode == MODE_SLIDESHOW) {
        screenmode = MODE_NORMAL;
        win_sizewin(graphics_fg_glk->peer, 0, 0, 0, 0);
        current_graphics_buf_win = graphics_bg_glk;
        windows[7].id = graphics_bg_glk;
        v6_get_and_sync_upperwin_size();
    }
}

void RT_UPDATE_INVT_WINDOW(void) {
    fprintf(stderr, "RT_UPDATE_INVT_WINDOW\n");
    screenmode = MODE_INVENTORY;
}

void RT_UPDATE_STAT_WINDOW(void) {
    screenmode = MODE_STATUS;
}

void RT_UPDATE_MAP_WINDOW(void) {
    screenmode = MODE_MAP;
    glk_request_mouse_event(graphics_bg_glk);
}

void RT_UPDATE_DESC_WINDOW(void) {
    screenmode = MODE_ROOM_DESC;
}



void arthur_INIT_STATUS_LINE(void) {
    if (get_global(ag.GL_WINDOW_TYPE) == 0) { // <EQUAL? ,GL-WINDOW-TYPE ,K-WIN-NONE>
        V6_STATUS_WINDOW.y_size = gcellh + 2 * ggridmarginy;
        V6_STATUS_WINDOW.y_origin = 1;
        V6_STATUS_WINDOW.x_origin = 1;
        V6_STATUS_WINDOW.x_size = gscreenw;
        v6_sizewin(&V6_STATUS_WINDOW);
        V6_TEXT_BUFFER_WINDOW.y_origin = V6_STATUS_WINDOW.y_size + 1;
        V6_TEXT_BUFFER_WINDOW.x_origin = 1;
        V6_TEXT_BUFFER_WINDOW.x_size = gscreenw;
        V6_TEXT_BUFFER_WINDOW.y_size = gscreenh - V6_TEXT_BUFFER_WINDOW.y_origin;
        v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
    } else {
        int M;
        if (!get_image_size(0x64, &M, nullptr)) { // <PICINF ,K-PIC-BANNER-MARGIN ,K-WIN-TBL>
            M = gcellw * 3;
        }
        int W = gscreenw - 2 * M;
        int L = M + 1;
        int N = gscreenh / 2;
        N = floor(N / gcellh) * gcellh + 1;
        V6_TEXT_BUFFER_WINDOW.y_origin = N + gcellh ;
        V6_TEXT_BUFFER_WINDOW.x_origin = L;
        V6_TEXT_BUFFER_WINDOW.x_size = W;
        V6_TEXT_BUFFER_WINDOW.y_size = gscreenh - V6_TEXT_BUFFER_WINDOW.y_origin;
        v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
        V6_STATUS_WINDOW.y_origin = N;
        V6_STATUS_WINDOW.x_origin = L;
        V6_STATUS_WINDOW.x_size = W;
        V6_STATUS_WINDOW.y_size = gcellh + 2 * ggridmarginy;
        v6_sizewin(&V6_STATUS_WINDOW);

        // Window 2 is the small room graphics window at top, not including banners
        ARTHUR_ROOM_GRAPHIC_WIN.y_origin = 1;
        ARTHUR_ROOM_GRAPHIC_WIN.x_origin = L;
        if (header.release > 41) {
            set_global(ag.WINDOW_2_X, L - 1); // <SETG WINDOW-2-X <- .L 1>>
            set_global(ag.WINDOW_2_Y, 0); //  <SETG WINDOW-2-Y 0>
        }
        ARTHUR_ROOM_GRAPHIC_WIN.y_size = N - 1;
        ARTHUR_ROOM_GRAPHIC_WIN.x_size = W;

        // Windows 5 and 6 are left and right banners, but are only used when erasing
        // banner graphics. The actual banners are drawn to window 7 (S-FULL).
        windows[5].y_origin = 1;
        windows[5].x_origin = 1;
        windows[5].y_size = gscreenh;
        windows[5].x_size = M;

        windows[6].y_origin = 1;
        windows[6].x_origin = L + W;
        windows[6].y_size = gscreenh;
        windows[6].x_size = M;

        // Window 7 (S-FULL) is the standard V6 fullscreen window
        windows[7].y_origin = 1;
        windows[7].x_origin = 1;
        windows[7].y_size = gscreenh;
        windows[7].x_size = gscreenw;
    }
    glk_set_window(V6_STATUS_WINDOW.id);
    glk_window_move_cursor(V6_STATUS_WINDOW.id, 0, 0);

    glui32 width;
    glk_window_get_size(V6_STATUS_WINDOW.id, &width, nullptr);
    garglk_set_reversevideo(1);
    for (int i = 0; i < width; i++)
        glk_put_char(' ');
    garglk_set_reversevideo(0);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);

    if (header.release > 41)
        set_global(ag.GL_TIME_WIDTH, 0);
    set_global(ag.GL_SL_HERE, 0);
    set_global(ag.GL_SL_VEH, 0);
    set_global(ag.GL_SL_HIDE, 0);
    set_global(ag.GL_SL_TIME, 0);
    set_global(ag.GL_SL_FORM, 0);
    arthur_adjust_windows();
}

void ARTHUR_UPDATE_STATUS_LINE(void) {}

void UPDATE_STATUS_LINE(void) {}

int count_lines(uint32_t table) {
    int lines = 0;
    for (uint16_t count = user_word(table); count != 0; count = user_word(table)) {
        lines++;
        table += 2 + count;
    }
    return lines;
}

void RT_AUTHOR_OFF(void) {

    output_stream(-3, 0);

    uint32_t addr = at.K_DIROUT_TBL;

    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, user_selected_foreground);
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_background);

    if (ARTHUR_ERROR_WINDOW.id == nullptr && !gli_zmachine_no_err_win)
        ARTHUR_ERROR_WINDOW.id = gli_new_window(wintype_TextGrid, 0);

    int lines;

    if (header.release > 41) {
        lines = count_lines(addr);
    } else {
        int width_in_chars = (V6_TEXT_BUFFER_WINDOW.x_size - 2 * ggridmarginx) / gcellw;
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

        win_setbgnd(ARTHUR_ERROR_WINDOW.id->peer, user_selected_foreground);
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

    glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_BackColor);
    glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_TextColor);

    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

#define K_PIC_ENDGAME 84
#define K_PIC_ANGRY_DEMON 85

bool arthur_display_picture(glui32 picnum, glsi32 x, glsi32 y) {

    // Fullscreen images
    if ((picnum >= 1 && picnum <= 3) || picnum == K_PIC_ENDGAME || picnum == K_PIC_ANGRY_DEMON) {
        if (current_graphics_buf_win == nullptr || current_graphics_buf_win == graphics_bg_glk) {
            current_graphics_buf_win = graphics_fg_glk;
            glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
            win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
        }
        screenmode = MODE_SLIDESHOW;
        glk_request_mouse_event(current_graphics_buf_win);
        if (picnum != 3)
            clear_image_buffer();
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
    v6_delete_glk_win(ARTHUR_ERROR_WINDOW.id);

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

#pragma mark Restoring

void stash_arthur_state(library_state_data *dat) {

    if (!dat)
        return;

    if (current_graphics_buf_win)
        dat->current_graphics_win_tag = current_graphics_buf_win->tag;
    if (graphics_fg_glk)
        dat->graphics_fg_tag = graphics_fg_glk->tag;
    if (stored_gridwin)
        dat->stored_upper_tag = stored_gridwin->tag;
    if (stored_bufferwin)
        dat->stored_lower_tag = stored_bufferwin->tag;
    dat->slideshow_pic = last_slideshow_pic;
}

void recover_arthur_state(library_state_data *dat) {

    if (!dat)
        return;

    current_graphics_buf_win = gli_window_for_tag(dat->current_graphics_win_tag);
    graphics_fg_glk = gli_window_for_tag(dat->graphics_fg_tag);
    stored_gridwin = gli_window_for_tag(dat->stored_upper_tag);
    stored_bufferwin = gli_window_for_tag(dat->stored_lower_tag);
    last_slideshow_pic = dat->slideshow_pic;
}


void arthur_update_after_restore(void) {
    arthur_sync_screenmode();
    update_user_defined_colours();
    uint8_t fg = get_global(fg_global_idx);
    uint8_t bg = get_global(bg_global_idx);
    fprintf(stderr, "arthur_update_after_restore: fg:%d bg:%d\n", fg, bg);
    ARTHUR_ROOM_GRAPHIC_WIN.fg_color = Color(Color::Mode::ANSI, fg);
    ARTHUR_ROOM_GRAPHIC_WIN.bg_color = Color(Color::Mode::ANSI, bg);
    ARTHUR_ERROR_WINDOW.fg_color = Color(Color::Mode::ANSI, fg);
    ARTHUR_ERROR_WINDOW.bg_color = Color(Color::Mode::ANSI, bg);
    after_V_COLOR();
    glk_window_set_background_color(graphics_bg_glk, user_selected_background);
    glk_window_clear(graphics_bg_glk);
}

void arthur_close_and_reopen_front_graphics_window(void) {
    if (graphics_fg_glk)
        gli_delete_window(graphics_fg_glk);
    graphics_fg_glk = gli_new_window(wintype_Graphics, 0);
    if (screenmode == MODE_SLIDESHOW) {
        win_sizewin(graphics_fg_glk->peer, 0, 0, gscreenw, gscreenh);
        current_graphics_buf_win = graphics_fg_glk;
    } else {
        win_sizewin(graphics_fg_glk->peer, 0, 0, 0, 0);
        current_graphics_buf_win = graphics_bg_glk;
    }
}


void arthur_update_after_autorestore(void) {
    update_user_defined_colours();
    arthur_close_and_reopen_front_graphics_window();
    uint8_t fg = get_global(fg_global_idx);
    uint8_t bg = get_global(bg_global_idx);
    ARTHUR_ROOM_GRAPHIC_WIN.fg_color = Color(Color::Mode::ANSI, fg);
    ARTHUR_ROOM_GRAPHIC_WIN.bg_color = Color(Color::Mode::ANSI, bg);
    ARTHUR_ERROR_WINDOW.fg_color = Color(Color::Mode::ANSI, fg);
    ARTHUR_ERROR_WINDOW.bg_color = Color(Color::Mode::ANSI, bg);
    after_V_COLOR();
}

bool arthur_autorestore_internal_read_char_hacks(void) {
    if (screenmode == MODE_HINTS) {
        return true;
    }
    return false;
}

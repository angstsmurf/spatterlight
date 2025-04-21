//
//  arthur.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

#include "draw_image.hpp"
#include "memory.h"
#include "objects.h"
#include "options.h"
#include "screen.h"
#include "unicode.h"
#include "zterp.h"
#include "v6_specific.h"
#include "v6_shared.hpp"

#include "arthur.hpp"

extern float imagescalex, imagescaley;

ArthurGlobals ag;
ArthurRoutines ar;
ArthurTables at;

#define K_PIC_TITLE 1

#define K_PIC_SWORD 2
#define K_PIC_SWORD_MERLIN 3

#define K_PIC_ENDGAME 84
#define K_PIC_ANGRY_DEMON 85

#define K_PIC_BANNER 54
#define K_PIC_BANNER_MARGIN 100
#define K_MAP_SCROLL 137

#define K_PIC_CHURCHYARD 4
#define K_PIC_PARADE_AREA 17
#define K_PIC_AIR_SCENE 163

#define K_PIC_ISLAND_DOOR 158
#define K_PIC_ISLAND_DOOR_OFF 159

#define K_PIC_STONE_2 9


extern Window *mainwin, *curwin;

#define ARTHUR_ROOM_GRAPHIC_WIN windows[2]
#define ARTHUR_ERROR_WINDOW windows[3]

bool showing_wide_arthur_room_image = false;

static int arthur_text_top_margin = -1;
static int arthur_x_margin = 0;

int arthur_pic_top_margin = 0;

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

void adjust_arthur_top_margin(void) {
    int mapheight, borderheight, margin;
    get_image_size(K_MAP_SCROLL, &margin, &mapheight); // Get height of map background image
    get_image_size(K_PIC_BANNER_MARGIN, nullptr, &borderheight); // Get height of border measuring dummy image
    float y_margin = mapheight * imagescaley;
    arthur_text_top_margin = ceil(y_margin / gcellh) * gcellh;
    arthur_pic_top_margin = arthur_text_top_margin / imagescaley - borderheight - 4;
    if (arthur_pic_top_margin < 0)
        arthur_pic_top_margin = 0;
    arthur_x_margin = margin * imagescalex;
}


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
        y += 11;
        x++;
    } else {
        y += 7;
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

// Window 0 (S-TEXT) is the standard V6 text buffer window (V6_TEXT_BUFFER_WINDOW)
// Window 1 (S-WINDOW) is the standard V6 status window (V6_STATUS_WINDOW)

// Window 2 is the small room graphics window at top, not including banners (ARTHUR_ROOM_GRAPHIC_WIN)
// Window 3 is the inverted error window at the bottom (ARTHUR_ERROR_WINDOW)

// Windows 5 and 6 are left and right banners, but are only used when erasing
// banner graphics. The banners are drawn to window S-FULL (7)

// Window 7 (S-FULL) is the standard V6 fullscreen window (V6_GRAPHICS_BG)

// Called on window_changed();
void arthur_update_on_resize(void) {
    image_needs_redraw = true;
    adjust_arthur_top_margin();

    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, user_selected_background);
    glk_stylehint_set(wintype_TextGrid, style_Subheader, stylehint_TextColor, user_selected_foreground);
    glk_stylehint_set(wintype_TextGrid, style_Subheader, stylehint_BackColor, user_selected_background);

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

    if (screenmode != MODE_SLIDESHOW) {
        if (current_graphics_buf_win && screenmode != MODE_NORMAL) {
            glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
            glk_window_clear(current_graphics_buf_win);
        }
        if (screenmode != MODE_NO_GRAPHICS) {
            int x_margin = 0, y_margin = 0;
            get_image_size(K_PIC_BANNER_MARGIN, &x_margin, &y_margin);

            if (screenmode == MODE_MAP) {
                ARTHUR_ROOM_GRAPHIC_WIN.y_origin = 0;
                ARTHUR_ROOM_GRAPHIC_WIN.x_origin = x_margin;
                ARTHUR_ROOM_GRAPHIC_WIN.x_size = hw_screenwidth - 2 * x_margin;
            }

            x_margin *= imagescalex;
            y_margin *= imagescaley;

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

            if (screenmode == MODE_HINTS) {
                redraw_hint_screen_on_resize();
                glk_request_mouse_event(V6_TEXT_BUFFER_WINDOW.id);
                glk_request_mouse_event(V6_STATUS_WINDOW.id);
                return;
            }

            if (screenmode == MODE_STATUS || screenmode == MODE_INVENTORY || screenmode == MODE_ROOM_DESC) {
                ARTHUR_ROOM_GRAPHIC_WIN.x_origin = V6_STATUS_WINDOW.x_origin;
                ARTHUR_ROOM_GRAPHIC_WIN.x_size = V6_STATUS_WINDOW.x_size;
                ARTHUR_ROOM_GRAPHIC_WIN.y_size = arthur_text_top_margin - 1;
                glui32 wintype;
                if (screenmode == MODE_ROOM_DESC) {
                    wintype = wintype_TextBuffer;
                } else {
                    wintype = wintype_TextGrid;
                }
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
            } else {
                v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
            }

            v6_get_and_sync_upperwin_size();

            if (screenmode == MODE_NORMAL) {
                clear_image_buffer();
                internal_call_with_arg(pack_routine(ar.RT_UPDATE_PICT_WINDOW), 1);
                draw_arthur_side_images(current_graphics_buf_win);
                if (showing_wide_arthur_room_image)
                    arthur_draw_room_image(current_picture);
                flush_bitmap(current_graphics_buf_win);
            } else if (screenmode == MODE_INVENTORY) {
                internal_call_with_arg(pack_routine(ar.RT_UPDATE_INVT_WINDOW), 1);
            } else if (screenmode == MODE_STATUS) {
                internal_call_with_arg(pack_routine(ar.RT_UPDATE_STAT_WINDOW), 1);
            } else if (screenmode == MODE_MAP) {
                set_global(ag.GL_MAP_GRID_Y, 0);
                internal_call_with_arg(pack_routine(ar.RT_UPDATE_MAP_WINDOW), 1);
                flush_bitmap(current_graphics_buf_win);
            }
        } else {
            v6_define_window(&V6_STATUS_WINDOW, 1, 1, gscreenw, gcellh + 2 * ggridmarginy);
            v6_define_window(mainwin, 1, V6_STATUS_WINDOW.y_origin + V6_STATUS_WINDOW.y_size, gscreenw, screenheight - mainwin->y_origin - 1);
            v6_delete_win(&ARTHUR_ROOM_GRAPHIC_WIN);
        }

        set_global(ag.UPDATE, 0);
        internal_call(pack_routine(ar.UPDATE_STATUS_LINE));

        if (ARTHUR_ERROR_WINDOW.id != nullptr) {
            v6_define_window(&ARTHUR_ERROR_WINDOW, V6_TEXT_BUFFER_WINDOW.x_origin, screenheight + 1, V6_TEXT_BUFFER_WINDOW.x_size, gscreenh - screenheight);
        }
    } else {
        // We are showing a fullscreen image
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
}

void arthur_move_cursor(int16_t y, int16_t x, winid_t win) {
    x--;
    y--;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    glk_window_move_cursor(win, x, y);
}

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

void RT_UPDATE_INVT_WINDOW(void) {
    screenmode = MODE_INVENTORY;
    if (ARTHUR_ROOM_GRAPHIC_WIN.id) {
        glk_set_window(ARTHUR_ROOM_GRAPHIC_WIN.id);
        glk_set_style(style_Subheader);
    }
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
    arthur_update_on_resize();

    int M;
    if (!get_image_size(K_PIC_BANNER_MARGIN, &M, nullptr)) { // <PICINF ,K-PIC-BANNER-MARGIN ,K-WIN-TBL>
        M = gcellw * 3;
    }
    int W = gscreenw - 2 * M;
    int L = M + 1;

    // Windows 5 and 6 are left and right banners, but are only used when erasing
    // banner graphics. The actual banners are drawn to window 7 (S-FULL).
    v6_define_window(&windows[5], 1, 1, M, gscreenh);
    v6_define_window(&windows[6], L + W, 1, M, gscreenh);


    // Window 7 (S-FULL) is the standard V6 fullscreen window
    v6_define_window(&V6_GRAPHICS_BG, 1, 1, gscreenw, gscreenh);

    glk_set_window(V6_STATUS_WINDOW.id);
    glk_window_move_cursor(V6_STATUS_WINDOW.id, 0, 0);

    glui32 width;
    glk_window_get_size(V6_STATUS_WINDOW.id, &width, nullptr);
    garglk_set_reversevideo(1);
    for (int i = 0; i < width; i++)
        glk_put_char(' ');
    garglk_set_reversevideo(0);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);

    if (ag.GL_TIME_WIDTH != 0)
        set_global(ag.GL_TIME_WIDTH, 0);
    set_global(ag.GL_SL_HERE, 0);
    set_global(ag.GL_SL_VEH, 0);
    set_global(ag.GL_SL_HIDE, 0);
    set_global(ag.GL_SL_TIME, 0);
    set_global(ag.GL_SL_FORM, 0);
}

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

    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, user_selected_background);
    glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_ReverseColor);

    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

bool arthur_display_picture(glui32 picnum, glsi32 x, glsi32 y) {

    // Fullscreen images
    if ((picnum >= 1 && picnum <= 3) || picnum == K_PIC_ENDGAME || picnum == K_PIC_ANGRY_DEMON) {
        if (current_graphics_buf_win == nullptr || current_graphics_buf_win == graphics_bg_glk) {
            current_graphics_buf_win = graphics_fg_glk;
        }
        glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
        win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
        screenmode = MODE_SLIDESHOW;
        glk_request_mouse_event(current_graphics_buf_win);
        if (picnum != K_PIC_SWORD_MERLIN) {
            glk_window_clear(current_graphics_buf_win);
            draw_centered_title_image(picnum);
        } else {
            float scale = draw_centered_title_image(K_PIC_SWORD);
            draw_centered_image(K_PIC_SWORD_MERLIN, scale, 0, 0);
        }
        glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
        const char *message;
        switch (picnum) {
            case K_PIC_TITLE:
                message = "\n(Showing title image in a golden frame. The words 'Arthur: The Quest for Excalibur' where the 'L' is a glittering sword.)\n";
                break;
            case K_PIC_SWORD:
                message = "\n(Showing a full screen image in a golden frame. A sword of gold, with a jewel-encrusted hilt, stuck in a rock, in front of a crude medieval stone church.)\n";
                break;
            case K_PIC_SWORD_MERLIN:
                message = "\n(Showing a full screen image in a golden frame. Merlin has appeared behind the sword, wearing a cloak. He raises his right hand while stroking his long beard with his left.)\n";
                break;
            case K_PIC_ANGRY_DEMON:
                message = "\n(Showing a full screen image of a screaming demon.)\n";
                break;
            case K_PIC_ENDGAME:
                message = "\n(Showing a full screen image in a golden frame. Arthur is holding up Excalibur, smiling, while the face of Merlin is watching from the sky behind.)\n";
                break;
            default:
                message = "\n(Showing a full screen image.)\n";
                break;
        }
        glk_put_string_stream(glk_window_get_stream(V6_TEXT_BUFFER_WINDOW.id), const_cast<char*>(message));
        return true;
    } else if (current_graphics_buf_win == nullptr) {
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
    } else if (picnum == K_PIC_BANNER) { // Border image, drawn in flush_image_buffer()
        screenmode = MODE_NORMAL;
        return true;
    }

    draw_to_buffer(current_graphics_buf_win, picnum, x, y);
    return true;
}

void arthur_erase_window(int16_t index) {
    if (!is_spatterlight_arthur)
        return;

    switch (index) {
        case -1:
            if (screenmode == MODE_SLIDESHOW) {
                clear_image_buffer();
                glk_window_set_background_color(graphics_bg_glk, user_selected_background);
                glk_window_clear(graphics_bg_glk);
                glk_window_set_background_color(graphics_fg_glk, user_selected_background);
                glk_window_clear(graphics_fg_glk);
                if (last_slideshow_pic == K_PIC_ANGRY_DEMON) {
                    screenmode = MODE_NORMAL;
                } else {
                    screenmode = MODE_INITIAL_QUESTION;
                    win_sizewin(V6_STATUS_WINDOW.id->peer, 0, 0, 0, 0);
                }
                current_graphics_buf_win = nullptr;
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

#pragma mark Restoring

void stash_arthur_state(library_state_data *dat) {

    if (!dat)
        return;

    if (current_graphics_buf_win)
        dat->current_graphics_win_tag = current_graphics_buf_win->tag;
    if (graphics_fg_glk)
        dat->graphics_fg_tag = graphics_fg_glk->tag;
    if (stored_bufferwin)
        dat->stored_lower_tag = stored_bufferwin->tag;
    dat->slideshow_pic = last_slideshow_pic;
}

void recover_arthur_state(library_state_data *dat) {

    if (!dat)
        return;

    current_graphics_buf_win = gli_window_for_tag(dat->current_graphics_win_tag);
    graphics_fg_glk = gli_window_for_tag(dat->graphics_fg_tag);
    stored_bufferwin = gli_window_for_tag(dat->stored_lower_tag);
    last_slideshow_pic = dat->slideshow_pic;
}

void arthur_update_after_restore(void) {
    arthur_sync_screenmode();
    after_V_COLOR();
}

void arthur_update_after_autorestore(void) {
    update_user_defined_colours();
    uint8_t fg = get_global(fg_global_idx);
    uint8_t bg = get_global(bg_global_idx);

    for (auto &window : windows) {
        window.fg_color = Color(Color::Mode::ANSI, fg);
        window.bg_color = Color(Color::Mode::ANSI, bg);
    }

    v6_close_and_reopen_front_graphics_window();

    window_change();
}

struct HotKeyDict {
    kArthurWindowType wintype;
    V6ScreenMode mode;
};

static std::unordered_map<uint8_t, HotKeyDict> hotkeys = {
    { ZSCII_F1, {K_WIN_PICT, MODE_NORMAL}      },
    { ZSCII_F2, {K_WIN_MAP,  MODE_MAP}         },
    { ZSCII_F3, {K_WIN_INVT, MODE_INVENTORY}   },
    { ZSCII_F4, {K_WIN_STAT, MODE_STATUS}      },
    { ZSCII_F5, {K_WIN_DESC, MODE_ROOM_DESC}   },
    { ZSCII_F6, {K_WIN_NONE, MODE_NO_GRAPHICS} },
};

void RT_HOT_KEY(void) {
    if (screenmode == MODE_HINTS || screenmode == MODE_SLIDESHOW)
        return;

    HotKeyDict hotkey = hotkeys[variable(1)];

    kArthurWindowType oldwintype = (kArthurWindowType)get_global(ag.GL_WINDOW_TYPE);
    kArthurWindowType newwintype = hotkey.wintype;

    if (oldwintype != newwintype) {
        set_global(ag.GL_WINDOW_TYPE, newwintype);
        screenmode = hotkey.mode;
        arthur_update_on_resize();
    }
}

bool arthur_autorestore_internal_read_char_hacks(void) {
    if (screenmode == MODE_HINTS) {
        return true;
    }
    return false;
}

void ARTHUR_UPDATE_STATUS_LINE(void) {
    // We must always redraw the entire status bar,
    // or stray letters will be left behind

    glk_set_window(V6_STATUS_WINDOW.id);
    glk_window_move_cursor(V6_STATUS_WINDOW.id, 0, 0);
    glui32 width;
    glk_window_get_size(V6_STATUS_WINDOW.id, &width, nullptr);
    garglk_set_reversevideo(1);
    for (int i = 0; i < width; i++)
        glk_put_char(' ');
    set_global(ag.GL_SL_FORM, 99);
    set_global(ag.GL_SL_VEH, 0);
    set_global(ag.GL_SL_HERE, 0);
    set_global(ag.GL_SL_HIDE, 0);
    set_global(ag.GL_SL_TIME, 0);
}

void UPDATE_STATUS_LINE(void) {}
void RT_TH_EXCALIBUR(void) {}

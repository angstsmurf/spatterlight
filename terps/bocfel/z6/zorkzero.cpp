//
//  zorkzero.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-30.
//

#include "memory.h"
#include "objects.h"
#include "options.h"
#include "screen.h"
#include "unicode.h"
#include "v6_specific.h"
#include "v6_shared.hpp"
#include "zterp.h"

#include "entrypoints.hpp"
#include "draw_image.hpp"
#include "shogun.hpp"

#include "zorkzero.hpp"

#define z0_left_status_window windows[1].id

#define MAX(a,b) (a > b ? a : b)

winid_t z0_right_status_window = nullptr;
glui32 z0_right_status_width;

static bool has_made_peggleboz_move = false;

static int number_of_update_color_calls = 0;


ZorkGlobals zg;
ZorkRoutines zr;
ZorkTables zt;
ZorkObjects zo;
ZorkProperties zp;

static bool is_zorkzero_tower_image(int pic) {
    return (pic >= 42 && pic <= 48);
}

static bool is_zorkzero_rebus_image(int pic) {
    return (pic >= 34 && pic <= 40);
}

static bool is_zorkzero_encyclopedia_image(int pic) {
    return (pic >= 26 && pic <= 33);
}

static bool is_zorkzero_peggleboz_image(int pic) {
    return (pic >= 50 && pic <= 72);
}

static bool is_zorkzero_peggleboz_box_image(int pic) {
    uint16_t CURRENT_SPLIT = get_global(zg.CURRENT_SPLIT);

    if (CURRENT_SPLIT != PBOZ_SPLIT)
        return false;

    switch(pic) {
        case SHOW_MOVES_BOX:
        case RESTART_BOX:
            has_made_peggleboz_move = true;
            return true;
        case DIM_RESTART_BOX:
        case DIM_SHOW_MOVES_BOX:
            has_made_peggleboz_move = false;
            return true;
        case EXIT_BOX:
            return true;
        default:
            return false;
    }
}

void z0_hide_right_status(void) {
    if (z0_right_status_window == nullptr) {
        fprintf(stderr, "Error!\n");
    }
    glk_window_clear(z0_right_status_window);
    win_sizewin(z0_right_status_window->peer, 0, 0, 0, 0);
}

void z0_erase_screen(void) {
    clear_image_buffer();
    clear_margin_image_list();
    z0_hide_right_status();

    if (V6_TEXT_BUFFER_WINDOW.id != nullptr)
        glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
    if (V6_STATUS_WINDOW.id != nullptr)
        glk_window_clear(V6_STATUS_WINDOW.id);
    glk_window_clear(graphics_bg_glk);
}

void after_SPLIT_BY_PICTURE(void) {
    if (screenmode == MODE_NO_GRAPHICS) {
        V6_TEXT_BUFFER_WINDOW.x_size = gscreenw;
        v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
    }
}

void V_MODE(void) {
    if (screenmode != MODE_NO_GRAPHICS) {
        screenmode = MODE_NO_GRAPHICS;
        glk_window_clear(current_graphics_buf_win);
    } else {
        screenmode = MODE_NORMAL;
    }
}

void V_REFRESH(void) {
    fprintf(stderr, "V-REFRESH\n");
    uint16_t CURRENT_SPLIT = get_global(zg.CURRENT_SPLIT);
    if (CURRENT_SPLIT == TEXT_WINDOW_PIC_LOC) {
        v6_delete_win(&windows[3]);
    }
//    if (screenmode == MODE_DEFINE) {
//        screenmode = MODE_NORMAL;
//        v6_delete_win(&windows[2]);
//        v6_remap_win_to_buffer(&V6_TEXT_BUFFER_WINDOW);
//    }
}

//void V_CREDITS(void) {
////    if (!centeredText) {
////        centeredText = true;
////        set_current_style();
////    }
//}

//void after_V_CREDITS(void) {
////    if (centeredText) {
////        centeredText = false;
////        set_current_style();
////    }
//}

void CENTER(void) {
    V_CREDITS();
//    buffer_xpos = 0;
}


void SPLIT_BY_PICTURE(uint16_t id, bool clear_screen) {
    set_global(zg.CURRENT_SPLIT, id); // <SETG CURRENT-SPLIT .ID>
    if (clear_screen) {
        glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
        glk_window_clear(V6_STATUS_WINDOW.id);
    }
    int width, height;
    get_image_size(id, &width, &height);

    uint16_t x, x_size;
    uint16_t y = (height + 1) * imagescaley;

    if (get_global(zg.BORDER_ON) == 0 && id == TEXT_WINDOW_PIC_LOC) {
        x = 1;
        x_size = gscreenw;
    } else {
        x = width * imagescalex;
        x_size = gscreenw - x * 2;
    }

    v6_define_window(&V6_TEXT_BUFFER_WINDOW, x, y, x_size, gscreenh - y);
    v6_define_window(&V6_STATUS_WINDOW, 1, 1, gscreenw, y);
}

// ; "Make text window extend to bottom of screen (more or less) or to
// top of bottom edge of border, depending..."
void ADJUST_TEXT_WINDOW(int id) {
    int height = 0;
    int hw_screenheight = gscreenh;
    if (id != 0) {
        get_image_size(id, nullptr, &height);
        get_image_size(zorkzero_map_border, nullptr, &hw_screenheight);
        hw_screenheight *= imagescaley;
    }

    V6_TEXT_BUFFER_WINDOW.y_size = hw_screenheight - V6_TEXT_BUFFER_WINDOW.y_origin - height * imagescaley;
    v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
}

void DISPLAY_BORDER(BorderType border) {
    if (screenmode != MODE_HINTS)
        screenmode = MODE_NORMAL;
    if (current_graphics_buf_win == graphics_fg_glk) {
        win_sizewin(graphics_fg_glk->peer, 0, 0, 0, 0);
        glk_cancel_char_event(graphics_fg_glk);
        current_graphics_buf_win = graphics_bg_glk;
    }
//    if (current_graphics_buf_win != graphics_bg_glk && current_graphics_buf_win != nullptr) {
//        v6_delete_glk_win(current_graphics_buf_win);
//    }
    current_graphics_buf_win = graphics_bg_glk;
    glk_request_mouse_event(current_graphics_buf_win);

//    set_global(zg.NEW_COMPASS, 1);
    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(border, 0, 0);
    int X, Y;
    get_image_size(border, &X, &Y);
    int BL = -1, BR = -1;
    switch(border) {
        case OUTSIDE_BORDER:
            BL = OUTSIDE_BORDER_L;
            BR = OUTSIDE_BORDER_R;
            break;
        case CASTLE_BORDER:
            BL = CASTLE_BORDER_L;
            BR = CASTLE_BORDER_R;
            break;
        case UNDERGROUND_BORDER:
            BL = UNDERGROUND_BORDER_L;
            BR = UNDERGROUND_BORDER_R;
            break;
        case HINT_BORDER:
            BL = HINT_BORDER_L;
            BR = HINT_BORDER_R;
            break;
    }
    if (find_image(BL)) {        
        draw_to_pixmap_unscaled(BL, 0, Y);
    }

    if (find_image(BR)) {
        int width;
        get_image_size(BR, &width, nullptr);
        draw_to_pixmap_unscaled(BR, X - width, Y);
    }
}

void z0_autorestore_internal_read_char_hacks(void) {
}

static bool z0_init_status_line(bool DONT_CLEAR) {
//    set_global(zg.COMPASS_CHANGED, 1);  // <SETG COMPASS-CHANGED T>
//    set_global(zg.NEW_COMPASS, 1);  // <SETG NEW-COMPASS T>

    bool border_on = get_global(zg.BORDER_ON) == 1;
    if (!DONT_CLEAR) {
        // <CLEAR -1>
        if (V6_TEXT_BUFFER_WINDOW.id != nullptr) {
//            gli_delete_window(V6_TEXT_BUFFER_WINDOW.id);
//            V6_TEXT_BUFFER_WINDOW.id = gli_new_window(wintype_TextBuffer, 0);
            glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
        }
        clear_margin_image_list();
    }
    uint16_t CURRENT_SPLIT = get_global(zg.CURRENT_SPLIT);
    SPLIT_BY_PICTURE(CURRENT_SPLIT, false);

    if (CURRENT_SPLIT == TEXT_WINDOW_PIC_LOC) {
        ADJUST_TEXT_WINDOW(0);
    } else if (CURRENT_SPLIT == F_SPLIT) { // (<EQUAL? ,CURRENT-SPLIT ,F-SPLIT>
        ADJUST_TEXT_WINDOW(F_BOTTOM);
        return true;
    } else {
        ADJUST_TEXT_WINDOW(PBOZ_BOTTOM);
        return true;
    }

    z0_hide_right_status();

    if (border_on) { // <COND (,BORDER-ON

        glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_BackColor);

        if (graphics_type == kGraphicsTypeCGA || graphics_type == kGraphicsTypeMacBW) {
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, monochrome_black);
        } else {
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
        }

        set_current_window(&windows[7]); // <SCREEN ,S-FULL>
        if (zg.CURRENT_BORDER != 0) {
            set_global(zg.CURRENT_BORDER, internal_call(pack_routine(zr.SET_BORDER)));
            DISPLAY_BORDER((BorderType)get_global(zg.CURRENT_BORDER));
        } else {
            DISPLAY_BORDER((BorderType)internal_call(pack_routine(zr.SET_BORDER)));
        }
        for (int i = 1; i < 14; i++) {
            internal_clear_attr(0x166, i);
        }

    } else {
        set_current_window(&V6_STATUS_WINDOW); // <SCREEN ,S-WINDOW>
//        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_ReverseColor, 1);
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_background);
//        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, user_selected_foreground);
    }

    win_refresh(V6_STATUS_WINDOW.id->peer, 0, 0);
    win_refresh(z0_right_status_window->peer, 0, 0);

//    set_global(0x9a, 1); //  <SETG FONT-X <LOWCORE (FWRD 1)>>

    set_current_window(&V6_TEXT_BUFFER_WINDOW); // <SCREEN ,S-TEXT>

    return false;
}

void INIT_STATUS_LINE(void) {
    fprintf(stderr, "INIT-STATUS-LINE(DONT-CLEAR: %s)\n", variable(1) == 1 ? "true" : "false");
    z0_init_status_line(variable(1));
}


//FG_COLOR = 0->45; // Header byte 0x2d is default foreground color
//BG_COLOR = 0->44; // Header byte 0x2c is default background color

// Header byte 0x1e is interpreter number
// Header word 8 is flags 2, bit 6 means game wants to use colours

//if (0->30 == IBM && 0-->8 & 64 == 64) {
//    DEFAULT_FG = BLACK;
//    DEFAULT_BG = WHITE;
//    Default_Colors();
//} else if (0->30 == AMIGA) {
//    DEFAULT_FG = BLACK;
//    DEFAULT_BG = LIGHTGREY;
//    Default_Colors();
//}

static void draw_new_here(void) {
//    zo.PHIL_HALL = 0xc7;
//    zo.G_U_MOUNTAIN = 0xac;
//    zo.G_U_SAVANNAH = 0xc3;
//    zo.G_U_HIGHWAY = 0xce;

    V6_STATUS_WINDOW.x = 1;
    V6_STATUS_WINDOW.y = 1;

    int16_t HERE = get_global(zg.HERE);
    bool NARROW = options.int_number == INTERP_APPLE_IIE || options.int_number == INTERP_MSDOS;

    if (NARROW || HERE == zo.PHIL_HALL) {
        int16_t X = internal_get_prop(HERE, zp.P_APPLE_DESC);
        if (X) {
            print_handler(unpack_string(X), nullptr);
            return;
        }
    }

    if (HERE == zo.G_U_MOUNTAIN || HERE == zo.G_U_SAVANNAH || HERE == zo.G_U_HIGHWAY) {
        glk_put_string(const_cast<char*>("Great Undergd. "));
        //  <TELL "Great Undergd. ">
        if (HERE == zo.G_U_MOUNTAIN) {
            glk_put_string(const_cast<char*>("Mountain"));
            // <TELL "Mountain">
        } else if (HERE == zo.G_U_SAVANNAH) {
            glk_put_string(const_cast<char*>("Savannah"));
            // <TELL "Savannah">
        } else {
            glk_put_string(const_cast<char*>("Highway"));
            // <TELL "Highway">
        }
    } else {
        print_object(HERE, nullptr);
    }
}

static void draw_new_score(void) {
    int16_t SCORE = get_global(zg.SCORE);
    print_right_justified_number(SCORE);
}

static void z0_resize_status_windows(void) {

    bool BORDER_ON = (get_global(zg.BORDER_ON) == 1);

    if (z0_left_status_window == nullptr) {
        z0_left_status_window = gli_new_window(wintype_TextGrid, 0);
        fprintf(stderr, "z0_resize_status_windows: creating a new left status window with peer %d\n", z0_left_status_window->peer);
        if (BORDER_ON)
            win_maketransparent(z0_left_status_window->peer);
    }

    int16_t CURRENT_BORDER;
    if (zg.CURRENT_BORDER != 0)
        CURRENT_BORDER = get_global(zg.CURRENT_BORDER);
    else
        CURRENT_BORDER = internal_call(pack_routine(zr.SET_BORDER));

    int imgwidth, imgheight;
    get_image_size(CURRENT_BORDER, &imgwidth, &imgheight);

    fprintf(stderr, "z0_resize_status_windows: image width of CURRENT_BORDER (%d) is %d\n", CURRENT_BORDER, imgwidth);

    int x, y, width, height;

    V6_STATUS_WINDOW.x = 1;
    V6_STATUS_WINDOW.y = 1;

    int hereoffx, hereoffy;
    get_image_size(HERE_LOC, &hereoffx, &hereoffy);

    height = 2 * (gcellh + ggridmarginy);

    int text_area_height = 9;

    if (graphics_type == kGraphicsTypeMacBW) {
        text_area_height = 18;
    } else if (graphics_type == kGraphicsTypeCGA) {
        text_area_height = 10;
    } else if (graphics_type == kGraphicsTypeEGA) {
        text_area_height = 10;
    }

    int text_area_y_offset = text_area_height * imagescaley - gcellh;

    x = (hereoffx - 1) * imagescalex - ggridmarginx + text_area_y_offset;
    y = (hereoffy - 1) * imagescaley - ggridmarginy + text_area_y_offset;


    if (!BORDER_ON) {
        fprintf(stderr, "z0_resize_status_windows: border is off\n");
        z0_hide_right_status();

        v6_define_window(&V6_STATUS_WINDOW, 1, 1, gscreenw, height);
        glk_window_clear(z0_left_status_window);
        z0_right_status_width = (gscreenw - 2 * ggridmarginx) / gcellw;
        return;
    }

//    if (z0_right_status_window == nullptr) {
//        z0_right_status_window = gli_new_window(wintype_TextGrid, 0);
//        fprintf(stderr, "z0_resize_status_windows: creating a new right status window with peer %d\n", z0_right_status_window->peer);
//        win_maketransparent(z0_right_status_window->peer);
//    }

    uint16_t HERE = get_global(zg.HERE);
    int stringlength = count_characters_in_object(HERE);
    int width_in_chars = MAX(stringlength, 13); // 13 characters to make room for "MOVES:" plus 2 spaces and 10000 moves
    width = width_in_chars * gcellw + 2 * ggridmarginx;

    z0_left_status_window->bbox.x0 = x;
    win_sizewin(z0_left_status_window->peer, x, y, x + width, y + height);
    glk_window_clear(z0_left_status_window);

    int16_t REGION = internal_get_prop(HERE, zp.P_REGION);

    z0_right_status_width = MAX(count_characters_in_zstring(REGION), 10); // 10 characters to make room for "SCORE:" plus 1000 points

    width = z0_right_status_width * gcellw + 2 * ggridmarginx;

    x = gscreenw - x - width;
    win_sizewin(z0_right_status_window->peer, x, y, x + width, y + height);

    // We trick the text printing routine into
    // thinking that it is printing to the left
    // window when it prints to the right one,
    // so we have to pretend that the left one
    // is much wider than it really is
    z0_left_status_window->bbox.x1 = x + width;

    glk_window_clear(z0_right_status_window);
}

void z0_UPDATE_STATUS_LINE(void) {
    bool BORDER_ON = (get_global(zg.BORDER_ON) == 1);
//    bool COMPASS_CHANGED = (get_global(zg.COMPASS_CHANGED) == 1);
//    if (!COMPASS_CHANGED)
//        fprintf(stderr, "z0_UPDATE_STATUS_LINE: COMPASS_CHANGED == %d\n", get_global(zg.COMPASS_CHANGED));
    uint16_t HERE = get_global(zg.HERE);
    uint16_t REGION = internal_get_prop(HERE, zp.P_REGION);

    // if interpreter is IBM and color flag is unset
    // or border is off
    // set reverse video

    z0_resize_status_windows();

    set_current_window(&V6_STATUS_WINDOW);
    glk_window_move_cursor(z0_left_status_window, 0, 0);


    draw_new_here();

    glk_window_move_cursor(z0_left_status_window, 0, 1);
    glk_put_string(const_cast<char*>("Moves:  "));
    print_number(get_global(zg.MOVES));

    if (BORDER_ON) {
//        if (COMPASS_CHANGED) {
            if (zr.DRAW_NEW_COMP != 0) {
                internal_call(pack_routine(zr.DRAW_NEW_COMP));
            } else {
//                set_global(zg.COMPASS_CHANGED, 0);
                int width, height;
                get_image_size(COMPASS_PIC_LOC, &width, &height);

                store_word(zt.PICINF_TBL, (height + 1) * imagescaley);
                store_word(zt.PICINF_TBL + 2, (width + 1)  * imagescalex);

                fprintf(stderr, "COMPASS-PIC-LOC: width:%d height:%d\n", width, height);

                fprintf(stderr, "PICINF-TBL 0:0x%x\n", word(zt.PICINF_TBL));
                fprintf(stderr, "PICINF-TBL 1:0x%x\n", word(zt.PICINF_TBL + 2));

                fprintf(stderr, "pixelwidth:%f\n", pixelwidth);
                fprintf(stderr, "imagescalex:%f\n", imagescalex);
                fprintf(stderr, "imagescaley:%f\n", imagescaley);

                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_NORTH, N_HIGHLIGHTED, N_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_NE, NE_HIGHLIGHTED, NE_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_EAST, E_HIGHLIGHTED, E_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_SE, SE_HIGHLIGHTED, SE_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_SOUTH, S_HIGHLIGHTED, S_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_SW, SW_HIGHLIGHTED, SW_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_WEST, W_HIGHLIGHTED, W_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_NW, NW_HIGHLIGHTED, NW_UNHIGHLIGHTED});
        }
        glk_set_window(z0_right_status_window);
    }

    glk_window_move_cursor(BORDER_ON ? z0_right_status_window : z0_left_status_window, z0_right_status_width - count_characters_in_zstring(REGION), 0);
    print_handler(unpack_string(REGION), nullptr);

    glk_window_move_cursor(BORDER_ON ? z0_right_status_window : z0_left_status_window, z0_right_status_width - 10, 1);
    glk_put_string(const_cast<char*>("Score:"));
    draw_new_score();

    set_current_window(&V6_TEXT_BUFFER_WINDOW);
}


#pragma mark Update Colors

void z0_update_colors(void) {
    // Every system has its own default colors:
    // Amiga: black text on grey
    // IBM: EGA & VGA: black text on white. CGA: white text on black (on a real CGA monitor, "white" may be green or orange)
    // Macintosh: Black text on white
    // Apple 2: White text on black

    // In r393-s890714,
//    FG-COLOR   is global 0x1c
//    BG_COLOR   is global 0xcc
//    DEFAULT_FG is global 0x4f
//    DEFAULT_BG is global 0x81

    fprintf(stderr, "update_z0_colors: Called %d times\n", ++number_of_update_color_calls);

    uint16_t default_fg = get_global(zg.DEFAULT_FG); // 0x4f
    uint16_t default_bg = get_global(zg.DEFAULT_BG); // 0x81

    uint16_t current_fg = get_global(fg_global_idx);
    uint16_t current_bg = get_global(bg_global_idx);


    switch (options.int_number) {
        case INTERP_AMIGA:
            default_fg = BLACK_COLOUR;
            default_bg = LIGHTGREY_COLOUR;
            break;
        case INTERP_MSDOS:
            if (gli_z6_graphics == kGraphicsTypeCGA) {
                default_fg = WHITE_COLOUR;
                default_bg = BLACK_COLOUR;
            } else {
                default_fg = BLACK_COLOUR;
                default_bg = WHITE_COLOUR;
            }
            break;
        case INTERP_MACINTOSH:
            default_fg = BLACK_COLOUR;
            default_bg = WHITE_COLOUR;
            break;
        case INTERP_APPLE_IIE:
            if (gli_z6_graphics == kGraphicsTypeApple2) {
                default_fg = WHITE_COLOUR;
                default_bg = BLACK_COLOUR;
            } else {
                default_fg = BLACK_COLOUR;
                default_bg = WHITE_COLOUR;
            }
            break;
        default:
            fprintf(stderr, "ERROR! Illegal interpreter number!\n");
            break;
    }

    if (default_fg == 0)
        default_fg = 1;
    if (default_bg == 0)
        default_bg = 1;

    set_global(0x4f, default_fg);
    set_global(0x81, default_bg);

    if (graphics_type == kGraphicsTypeApple2) { // Only default colors are allowed with Apple 2 graphics
        current_fg = WHITE_COLOUR;
        current_bg = BLACK_COLOUR;
    } else {     // If machine is changed, the currently selected colors may not be available on the new machine. If so, reset them to default.
        if (options.int_number != INTERP_AMIGA) {
            if (current_fg == LIGHTGREY_COLOUR || current_fg == DARKGREY_COLOUR || current_bg == LIGHTGREY_COLOUR || current_bg == DARKGREY_COLOUR) {
                current_fg = default_fg;
                current_bg = default_bg;
            }
        }
    }

    if (number_of_update_color_calls == 1 && !(options.int_number != INTERP_MACINTOSH && graphics_type == kGraphicsTypeMacBW)) {
        current_fg = 1;
        current_bg = 1;
    }

    set_global(fg_global_idx, current_fg);
    set_global(bg_global_idx, current_bg);

    fprintf(stderr, "update_z0_colors: set FG-COLOR (global 0x1c) to %d\n", current_fg);
    fprintf(stderr, "update_z0_colors: set BG_COLOR (global 0xcc) to %d\n", current_bg);

//    if (current_fg == 1) {
//        current_fg = default_fg;
//    }
//
//    if (current_bg == 1) {
//        current_bg = default_bg;
//    }

    bool colorized_bw = (gli_z6_colorize &&
                         (gli_z6_graphics == kGraphicsTypeCGA || gli_z6_graphics == kGraphicsTypeMacBW));

    // When gli_enable_styles is true, all Spatterlight color settings (gfgcol, gbgcol) are ignored

    if (gli_enable_styles && !colorized_bw) {
        update_user_defined_colours();

        glk_stylehint_set(wintype_TextBuffer, style_Input, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_TextColor, user_selected_foreground);
        if (options.int_number == INTERP_MSDOS && gli_z6_graphics == kGraphicsTypeCGA)
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_background);
        else
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Preformatted, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Subheader, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Emphasized, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_BackColor, user_selected_background);
    } else {
        user_selected_foreground = gfgcol;
        user_selected_background = gbgcol;
    }

    if (colorized_bw) {
        update_monochrome_colours();
    }
}

#pragma mark ENCYCLOPEDIA FROBOZZICA

static glsi32 encyclopedia_background_color(void) {
    switch (graphics_type) {
        case kGraphicsTypeAmiga:
        case kGraphicsTypeVGA:
            return 0xE8CDAE;
        case kGraphicsTypeBlorb:
            return 0xEAD3B7;
        default:
            return monochrome_white;
    }
}

static void adjust_encyclopedia_text_window(void) {
    int width, height, x, y;

    get_image_size(zorkzero_encyclopedia_text_location, &x, &y);
    get_image_size(zorkzero_encyclopedia_text_size, &width, &height);
    if (graphics_type == kGraphicsTypeAmiga) {
        height -= 10;
    }

    v6_define_window(&windows[3], x * imagescalex, y * imagescaley, width * imagescalex, height * imagescaley);

    glsi32 encycl_bgnd = encyclopedia_background_color();
    win_setbgnd(windows[3].id->peer, encycl_bgnd);
    glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_BackColor, encycl_bgnd);
//    win_refresh(windows[3].id->peer, 0, 0);
}

static void draw_encyclopedia(void) {
    clear_image_buffer();
    current_graphics_buf_win = graphics_fg_glk;
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(zorkzero_encyclopedia_border, 0, 0);
    int width, height;
    get_image_size(zorkzero_encyclopedia_picture_location, &width, &height);
    draw_to_pixmap_unscaled(current_picture, width, height);
    flush_bitmap(graphics_fg_glk);
    adjust_encyclopedia_text_window();
}

static void adjust_text_window_by_split(uint16_t id) {
    int width, height;
    get_image_size(id, &width, &height);

    V6_TEXT_BUFFER_WINDOW.y_origin = (height + 1) * imagescaley;
    V6_TEXT_BUFFER_WINDOW.x_origin = width * imagescalex;
    V6_TEXT_BUFFER_WINDOW.x_size = gscreenw - V6_TEXT_BUFFER_WINDOW.x_origin * 2;
    V6_TEXT_BUFFER_WINDOW.y_size = gscreenh - V6_TEXT_BUFFER_WINDOW.y_origin;
    v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
}

static bool autosolve_visual_puzzle(void) {
    return skip_puzzle_prompt("\n\nWould you like to auto-solve this visual puzzle? (Y is affirmative): >");
}

#pragma mark DOUBLE FANUCCI

#define F_MENU_LOC    384
#define J_SCORE_LOC   385
#define F_DISCARD_LOC 419
#define F_CARD_1_LOC  420
#define F_CARD_SPACE  421

#define F_DISCARD_PIC_LOC 369

#define DRAW_CARDS_OFFSET 22

//#define NOT_HERE_OBJECT 0x0159
//#define F_CARD_TABLE 0x71c7

#define F_CARD_BACK 100
#define F_CARD 101
#define F_INKBLOTS 102
#define F_RV_INKBLOTS 117
#define F_RANK_PIC_LOC 374
#define F_REV_RANK_PIC_LOC 375
#define F_SUIT_PIC_LOC 376
#define F_REV_SUIT_PIC_LOC 377

#define F_0 132
#define F_RV_0 143

#define F_GRANOLA 154

#define F_CARD 101
#define F_1_PIC_LOC 370


// Window 1: Jester's Score
// Window 2: Your Score
//
// Windows 3 - 7 Card labels:
// 3: DISCARD
// 4: 1
// 5: 2
// 6: 3
// 7: 4
//
// Window 8 Command grid
// Window 0 Text buffer


static void fanucci_window(int index, int x, int y, int num_chars) {
    Window *win = &windows[index];
    win->x_origin = x - ggridmarginx;
    win->y_origin = y - ggridmarginy;
    win->x_size = num_chars * gcellw + 2 * ggridmarginx;
    win->y_size = gcellh + 2 * ggridmarginy;
    win->x = 1;
    win->y = 1;
    if (win->id != nullptr && win->id->type != wintype_TextGrid) {
        gli_delete_window(win->id);
    }

    if (win->id == nullptr) {
        win->id = gli_new_window(wintype_TextGrid, 0);
    }
    win_maketransparent(win->id->peer);
    v6_sizewin(win);
    glk_window_clear(win->id);
    glk_set_window(win->id);
}

static void update_score(winid_t win, uint16_t global, int offset) {
    glk_set_window(win);
    glk_window_move_cursor(win, offset, 0);
    uint16_t score = get_global(global);
    if (score < 10) {
        glk_put_string(
                       const_cast<char*>("00"));
    } else if (score < 100) {
        glk_put_char('0');
    }
    print_number(score);
}

static void update_scores(void) {
    update_score(V6_STATUS_WINDOW.id, 0x39, 17);
    update_score(z0_right_status_window, 0x3b, 13);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

static void draw_offset_image(int pic, int x, int y, int offpic) {
    int offsetx, offsety;
    get_image_size(offpic, &offsetx, &offsety);
    draw_to_pixmap_unscaled(pic, x + offsetx, y + offsety);
}

static void draw_card(int x, int y, int rank, int suit) {
    int rank_order[] = { -1, 4, 5, 6, 7, 8, 9, 0, 10, 1, 2, 3 };
    rank = user_byte(zt.F_CARD_TABLE + rank);
    suit = user_byte(zt.F_CARD_TABLE + suit);
    if (rank == 0) {
        draw_to_pixmap_unscaled(F_CARD_BACK, x, y);
    } else if (rank > 11) {
        int pic = F_GRANOLA + rank - 12;
        draw_to_pixmap_unscaled(pic, x, y);
    } else {
        draw_to_pixmap_unscaled(F_CARD, x, y);
        draw_offset_image(F_0 + rank_order[rank], x, y, F_RANK_PIC_LOC);
        draw_offset_image(F_RV_0 + rank_order[rank], x, y, F_REV_RANK_PIC_LOC);
        draw_offset_image(F_INKBLOTS + suit - 1, x, y, F_SUIT_PIC_LOC);
        draw_offset_image(F_RV_INKBLOTS + suit - 1, x, y, F_REV_SUIT_PIC_LOC);
    }

}

static void draw_cards(void) {
    zo.NOT_HERE_OBJECT = 0x159;

    int x, y;
    for (int cnt = 0; cnt < 5; cnt++) {
        get_image_size(F_DISCARD_PIC_LOC + cnt, &x, &y);
        uint16_t draw_cards_attribute = DRAW_CARDS_OFFSET + cnt;
        if (!internal_test_attr(zo.NOT_HERE_OBJECT, draw_cards_attribute))
            internal_set_attr(zo.NOT_HERE_OBJECT, draw_cards_attribute);
        draw_card(x, y, cnt * 2, cnt * 2 + 1);
    }
};

static winid_t fanucci_command_grid = nullptr;
static bool fanucci_window_is_narrow = false;

static void fanucci_select_command(int ptr, bool bold) {
    glk_set_window(fanucci_command_grid);
    glui32 menu_space = fanucci_window_is_narrow ? 8 : 14;
    glui32 line = ptr % 3;
    glui32 col = ptr / 3 * menu_space;
    glk_window_move_cursor(fanucci_command_grid, col, line);
    if (bold) {
        glk_set_style(style_Subheader);
        glk_put_char('>');
    } else {
        glk_put_char(UNICODE_SPACE);
    }
    uint16_t fanucci_command_table = fanucci_window_is_narrow ? zt.F_PLAY_TABLE_APPLE : zt.F_PLAY_TABLE;
    uint16_t string = user_word(fanucci_command_table + ptr * 2);
    print_handler(unpack_string(string), nullptr);
    glk_set_style(style_Normal);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

static int last_bold_move = 0;

static void bold_move(int ptr) {
    fanucci_select_command(ptr, true);
    last_bold_move = ptr;
}

static void unbold_move(int ptr) {
    fanucci_select_command(ptr, false);
    last_bold_move = -1;
}

static int mouse_ptr_in_grid(void) {
    uint16_t width = fanucci_window_is_narrow ?
        8 : 14;
    uint16_t mouse_click_addr = header.extension_table + 2;
    int16_t x = word(mouse_click_addr) - 1;
    int16_t y = word(mouse_click_addr + 2) - 1;

    if (y > 2)
        return -1;

    int column = x / width;
    if (column > 4)
        return -1;

    return column * 3 + y;
}

static bool pick_play() {
    uint16_t ptr = 0;
    bool clicked_in_grid = false;
    int top, left;
    get_image_size(F_MENU_LOC, &left, &top);
    bold_move(0);
    bool finished = false;
    bool resigned = false;
    while (!finished) {
        flush_image_buffer();
        uint16_t key = internal_read_char();

        if (key == ZSCII_CLICK_SINGLE || key == ZSCII_CLICK_DOUBLE) {
            clicked_in_grid = false;
            int new_ptr = mouse_ptr_in_grid();
            if (new_ptr != -1) {
                clicked_in_grid = true;
                if (ptr == new_ptr) {
                    key = ZSCII_CLICK_DOUBLE;
                } else {
                    unbold_move(ptr);
                    bold_move(new_ptr);
                    ptr = new_ptr;
                }
            }
            if (!clicked_in_grid) {
                glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
                win_beep(1);
                glk_put_string(const_cast<char*>("Click on one of the items in the menu to highlight a play; double-click on it to select that play."));
                //                <2CR-TO-PRINTER>
            }
        }

        int column = ptr / 3;
        int line = ptr % 3;
        switch(tolower(key)) {
            case ZSCII_CLICK_SINGLE:
                break;
            case ZSCII_CLICK_DOUBLE:
                if (!clicked_in_grid)
                    break;
                // Fallthrough
            case ZSCII_NEWLINE:
                finished = true;
                resigned = (internal_call(pack_routine(zr.PLAY_SELECTED), {ptr}) == 1); // <PLAY-SELECTED PTR>
                if (!resigned) {
                    unbold_move(ptr);
                }
                break;
            case ZSCII_UP:
            case ZSCII_KEY8:
            case 'u':
                unbold_move(ptr);
                if (line > 0) {
                    ptr--;
                }
                else {
                    ptr += 2;
                }
                bold_move(ptr);
                break;

            case ZSCII_DOWN:
            case ZSCII_KEY2:
            case 'd':
                unbold_move(ptr);
                if (line < 2) {
                    ptr++;
                }
                else {
                    ptr -= 2;
                }
                bold_move(ptr);
                break;

            case ZSCII_LEFT:
            case ZSCII_KEY4:
            case 'l':
                unbold_move(ptr);
                if (column > 0) {
                    ptr -= 3;
                } else {
                    ptr += 12;
                }
                bold_move(ptr);
                break;

            case ZSCII_RIGHT:
            case ZSCII_KEY6:
            case 'r':
            case ZSCII_SPACE:
                unbold_move(ptr);
                if (column < 4) {
                    ptr += 3;
                } else {
                    ptr -= 12;
                }
                bold_move(ptr);
                break;

            default:
                glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
                win_beep(1);
                glk_put_string(const_cast<char*>("Use the arrow keys to highlight a play, or hit RETURN/ENTER to select the currently highlighted play."));
                //                <2CR-TO-PRINTER>
                break;
        }
    }

    return resigned;
}

void J_PLAY(void) {
    internal_call(pack_routine(zr.J_PLAY));
}

static bool score_check(void) {
    int result = internal_call(pack_routine(zr.SCORE_CHECK));
    return (result == 1);
}

void FANUCCI(void) {
    bold_move(0);
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
    glk_put_string(const_cast<char*>("Use the arrow keys -- or the U, D, L and R keys -- to highlight a play, then hit the RETURN/ENTER key."));
    if (mouse_available()) {
        glk_put_string(const_cast<char*>(" Or, you can use your mouse"));
        if (options.int_number == INTERP_APPLE_IIE) {
            glk_put_string(const_cast<char*>("/joystick"));
        }
        glk_put_string(const_cast<char*>(" to select your play."));
    }
    bool finished = false;

    if (autosolve_visual_puzzle()) {
        internal_clear_attr(0x012f, 0x1e); // <FCLEAR ,BROOM ,TRYTAKEBIT>
        finished = true;
    }

    while(!finished) {
        if (pick_play()) {
            finished = true;
        } else {
            draw_cards();
            update_scores();
            uint16_t f_win_count = get_global(zg.F_WIN_COUNT);
            if (f_win_count == 3) {
                uint16_t your_score = get_global(zg.YOUR_SCORE);
                set_global(zg.YOUR_SCORE, your_score + 1000);
                internal_clear_attr(0x012f, 0x1e); // <FCLEAR ,BROOM ,TRYTAKEBIT>
                finished = true;
            } else if (score_check()) {
                finished = true;
            } else {
                glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
                J_PLAY();
                draw_cards();
                uint16_t f_plays = get_global(zg.F_PLAYS);
                set_global(zg.F_PLAYS, f_plays + 1);
                update_scores();
            }
            if (score_check())
                finished = true;
        }
    }
    gli_delete_window(fanucci_command_grid);
    fanucci_command_grid = nullptr;
    for (int i = 2; i < 7; i++)
        v6_delete_win(&windows[i]);
    screenmode = MODE_NORMAL;
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

static bool within(uint16_t left, uint16_t top, uint16_t width, uint16_t height) {
    uint16_t mouse_click_addr = header.extension_table + 2;
    int16_t x = word(mouse_click_addr);
    int16_t y = word(mouse_click_addr + 2);

    x = x / imagescalex - imagescalex;
    y = y / imagescaley - imagescaley;

    uint16_t right = left + width;
    uint16_t bottom = top + height;

    return (x >= left - 1 && x < right && y >= top - 1 && y < bottom);
}

uint16_t F_MOUSE_CARD_PICK(void) {
    int x, y, width, height;
    get_image_size(F_CARD, &width, &height);
    for (int i = 0; i < 4; i++) {
        get_image_size(F_1_PIC_LOC + i, &x, &y);
        if (within(x, y, width, height))
            return 49 + i;
    }
    return ZSCII_CLICK_SINGLE;
}



// Window 1: Jester's Score
// z0_right_status_window (no number): Your Score
//
// Windows 2 - 6 Card labels:
// 2: DISCARD
// 3: 1
// 4: 2
// 5: 3
// 6: 4
//
// fanucci_command_grid (no number): Commands / Plays
//
// Window 7 is S-FULL, the entire background
// Window 0 is S-TEXT, buffer text window

static void redraw_fanucci(void) {
    int x, y;

    get_image_size(J_SCORE_LOC, &x, &y);

    x *= imagescalex;
    if (graphics_type == kGraphicsTypeMacBW)
        y += 2;
    y *= imagescaley;
    fanucci_window(1, x, y, 20);
    glk_put_string(const_cast<char*>("Jester's Score:  000"));

//    if (z0_right_status_window == nullptr) {
////        z0_right_status_window = gli_new_window(wintype_TextGrid, 0);
//        fprintf(stderr, "Error!\n");
//    }

    win_sizewin(z0_right_status_window->peer, MAX(gscreenw - 21 * gcellw - 2 * ggridmarginx, 0), y, gscreenw - x, y + V6_STATUS_WINDOW.y_size);
    glk_set_window(z0_right_status_window);
    glk_window_clear(z0_right_status_window);
    glk_put_string(const_cast<char*>("Your Score:  000"));

    int card_num_y, discard_x, card_num_1_x, card_space, discard_width;

    get_image_size(F_DISCARD_LOC, nullptr, &card_num_y);

        card_num_y += 1; // Looks nicer with more vertical space

    get_image_size(F_DISCARD_PIC_LOC, &discard_x, nullptr);
    get_image_size(F_CARD, &discard_width, nullptr);

    discard_x *= imagescalex;
    discard_width *= imagescalex;
    discard_x += (discard_width - gcellw * 7) / 2;

    card_num_y *= imagescaley;
    get_image_size(F_DISCARD_PIC_LOC + 1, &card_num_1_x, nullptr);
    card_num_1_x *= imagescalex;
    card_num_1_x += discard_width / 2;
    get_image_size(F_CARD_SPACE, &card_space, nullptr);
    if (graphics_type != kGraphicsTypeApple2 && graphics_type != kGraphicsTypeMacBW && graphics_type != kGraphicsTypeCGA) {
        card_space += 1;
    }
    card_space *= imagescalex;

    update_scores();
    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(zorkzero_double_fanucci_border, 0, 0);
    draw_cards();

    // Make a separate grid window for each card label
    // (otherwise we can't center them properly)
    fanucci_window(2, discard_x, card_num_y, 7);
    glk_put_string(const_cast<char*>("DISCARD"));

    fanucci_window(3, card_num_1_x, card_num_y, 1);
    glk_put_char('1');
    fanucci_window(4, card_num_1_x + card_space, card_num_y, 1);
    glk_put_char('2');
    fanucci_window(5, card_num_1_x + card_space * 2, card_num_y, 1);
    glk_put_char('3');
    fanucci_window(6, card_num_1_x + card_space * 3, card_num_y, 1);
    glk_put_char('4');

    get_image_size(F_MENU_LOC, &x, &y);

    x = x * imagescalex; // We don't actually use this for the grid window
    y = (y + 3) * imagescaley; // Looks nicer with more vertical space

    // And a separate grid window for the commands
    if (fanucci_command_grid == nullptr)
        fanucci_command_grid = gli_new_window(wintype_TextGrid, 0);

    win_maketransparent(fanucci_command_grid->peer);

    int fanucci_command_window_width = 64 * gcellw + 2 * ggridmarginx;
    int xpos = (gscreenw - fanucci_command_window_width) / 2;

    // Use abbreviated Apple 2 commands and
    // narrow spacing if we are out of
    // horizontal space
    int min_left = x - ggridmarginx - 2 * gcellw;
    if (xpos < min_left) {
        fanucci_window_is_narrow = true;
        fanucci_command_window_width = 40 * gcellw + 2 * ggridmarginx;
        xpos = (gscreenw - fanucci_command_window_width) / 2 - gcellw;
    } else {
        fanucci_window_is_narrow = false;
    }

    if (options.int_number == INTERP_APPLE_IIE)
        fanucci_window_is_narrow = true;

    if (xpos < min_left)
        xpos = min_left;

    win_sizewin(fanucci_command_grid->peer, xpos, y, xpos + fanucci_command_window_width, y + 4 * gcellh + 2 * ggridmarginy);

    glk_window_clear(fanucci_command_grid);
    glk_set_window(fanucci_command_grid);

    int stored_last_bold_move = last_bold_move;
    // Print the commands inside the grid
    for (int i = 0; i < 15; i++) {
        unbold_move(i);
    }
    if (stored_last_bold_move != -1)
        bold_move(stored_last_bold_move);

    V6_TEXT_BUFFER_WINDOW.x_origin = x;
    V6_TEXT_BUFFER_WINDOW.y_origin = y + 4 * gcellh + 2 * ggridmarginy;
    V6_TEXT_BUFFER_WINDOW.x_size = gscreenw - 2 * x;
    ADJUST_TEXT_WINDOW(F_BOTTOM);
    flush_image_buffer();
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

void SETUP_FANUCCI(void) {
    last_bold_move = 0;
    redraw_fanucci();
    glk_request_mouse_event(fanucci_command_grid);
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
    screenmode = MODE_Z0_GAME;
}

#pragma mark SNARFEM

//#define PILE_TABLE 0x73f9

#define PILE_OF_0 74

#define L_FLOWERS_0 94
#define R_FLOWERS_0 84

#define PILE_1_PIC_LOC 357
#define L_FLOWERS_PIC_LOC 361
#define R_FLOWERS_PIC_LOC 362

#define BOX_1 445

#define BOX_1_LOC 470
#define SN_BOX_SPACE 471

#define DIMMED_SN_NUMBER 453
#define SN_NUMBER 444


static void snarfem_draw_numbered_boxes(int pile) {
    int x, y, space;

    get_image_size(BOX_1_LOC, &x, &y);
    get_image_size(SN_BOX_SPACE, &space, nullptr);
    for (int i = 1; i <= 9; i++) {
        bool dimmed = false;
        if (pile == 0) {
            if (i > 4 || user_word(zt.PILE_TABLE + i * 2) == 0) {
                dimmed = true;
            }
        } else if (user_word(zt.PILE_TABLE + pile * 2) < i) {
            dimmed = true;
        }

        draw_to_pixmap_unscaled(dimmed ? DIMMED_SN_NUMBER + i : SN_NUMBER + i, x, y);
        x += space;
    }
}

static void snarfem_draw_pile(int pile) {
    int num = user_word(zt.PILE_TABLE + pile * 2);
    int x, y;
    get_image_size(PILE_1_PIC_LOC + pile - 1, &x, &y);
    draw_to_pixmap_unscaled(PILE_OF_0 + num, x, y);
}

static bool snarfem_safe_number(uint16_t tbl[]) {
    int binary_table[10] = { 0, 1, 10, 11, 100, 101, 110, 111, 1000, 1001 };
    int x = 0;
    for (int i = 0; i < 4; i++) {
        x += binary_table[tbl[i]];
    }
    return (x % 2 == 0 && (x / 10) % 2 == 0 && (x / 100) % 2 == 0 && (x / 1000) % 2 == 0);
}


void DRAW_FLOWERS(void) {
    int num = 1;
    int pile = 0;
    int left, right;
    uint16_t temp_table[4], pile_table[4];
    // Copy ZIL table to C array for convenience
    for (int i = 0; i < 4; i++) {
        pile_table[i] = user_word(zt.PILE_TABLE + (i + 1) * 2);
    }
    if (snarfem_safe_number(pile_table)) {
        left = L_FLOWERS_0;
        right = R_FLOWERS_0;
    } else {
        bool found = false;
        while (!found) {
            memcpy(temp_table, pile_table, sizeof(temp_table));
            if (pile_table[pile] == 0) {
                pile++;
                if (pile > 3) {
                    fprintf(stderr, "draw_flowers: ERROR: Could not find safe number\n");
                }
            } else {
                if (pile_table[0] + pile_table[1] + pile_table[2] + pile_table[3] == pile_table[pile]) {
                    // "case where three piles are empty"
                    num = pile_table[pile];
                    found = true;
                } else {
                    if (num > temp_table[pile]) {
                        fprintf(stderr, "draw_flowers: ERROR: Could not find safe number\n");
                    }
                    temp_table[pile] -= num;
                    if (snarfem_safe_number(temp_table)) {
                        found = true;
                    } else if (pile_table[pile] == num) {
                        num = 1;
                        pile++;
                        if (pile > 3) {
                            fprintf(stderr, "draw_flowers: ERROR: Could not find safe number\n");
                        }
                    } else {
                        num++;
                    }
                }
            }
        }
        
        left = L_FLOWERS_0 + 1 + pile;
        right = R_FLOWERS_0 + num;
    }
    int x, y;
    get_image_size(L_FLOWERS_PIC_LOC, &x, &y);
    draw_to_pixmap_unscaled(left, x, y);
    get_image_size(R_FLOWERS_PIC_LOC, &x, &y);
    draw_to_pixmap_unscaled(right, x, y);
}

static uint16_t snarfem_click(bool already_picked_pile) {
    int width, height;
    get_image_size(BOX_1, &width, &height);
    int left, top;
    get_image_size(BOX_1_LOC, &left, &top);
    int box_space;
    get_image_size(SN_BOX_SPACE, &box_space, nullptr);
    for (int i = 1; i < 9; i++) {
        if (within(left, top, width, height)) {
            return i;
        }
        left += box_space;
    }
    if (already_picked_pile)
        return 0;
    for (int i = 1; i < 9; i++) {
        get_image_size(PILE_OF_0 + i, &width, &height);
        get_image_size(PILE_1_PIC_LOC + i - 1, &left, &top);
        if (within(left, top, width, height)) {
            return i;
        }
    }

    return 0;
}

static int last_pile = 0;

static void draw_snarfem(void) {
    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(zorkzero_snarfem_border, 0, 0);
    for(int i = 1; i <= 4; i++)
        snarfem_draw_pile(i);
    DRAW_FLOWERS();
    snarfem_draw_numbered_boxes(last_pile);

    int x, y, distance_from_bottom, height;
    get_image_size(SN_SPLIT, &x, &y);
    set_global(zg.CURRENT_SPLIT, SN_SPLIT); // <SETG CURRENT-SPLIT .ID>
    x *= imagescalex;
    y++;
    y *= imagescaley;
    get_image_size(zorkzero_snarfem_border, nullptr, &height);
    height *= imagescaley;
    get_image_size(SN_BOTTOM, nullptr, &distance_from_bottom);
    distance_from_bottom *= imagescaley;

    v6_define_window(&V6_TEXT_BUFFER_WINDOW, x + imagescalex, y, gscreenw - 2 * x, height - y - distance_from_bottom);

    flush_image_buffer();
}


void SETUP_SN(void) {
    z0_hide_right_status();

    v6_define_window(&V6_STATUS_WINDOW, 0, 0, 0, 0);

    draw_snarfem();
    win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, 0xffffff);
    set_current_window(&V6_TEXT_BUFFER_WINDOW);
}

void DRAW_SN_BOXES(void) {
    last_pile = last_pile;
    snarfem_draw_numbered_boxes(variable(1));
}

void DRAW_PILE(void) {
    snarfem_draw_pile(variable(1));
}

void SN_CLICK(void) {
    store_variable(2, snarfem_click(variable(1)));
}

void SNARFEM(void) {
    if (autosolve_visual_puzzle()) {
        transcribe_and_print_string("\n");
        store_variable(4, 1);
        internal_clear_attr(zo.FAN, zp.TRYTAKEBIT);
    }
}

#pragma mark PEGGLEBOZ

static void z0_draw_peggleboz_box_image(uint16_t pic, uint16_t x, uint16_t y) {
    int width, height;
    switch (pic) {
        case RESTART_BOX:
            has_made_peggleboz_move = true;
        case DIM_RESTART_BOX:
            get_image_size(PBOZ_RESTART_BOX_LOC, &width, &height);
            draw_to_pixmap_unscaled(pic, width, height);
            return;
        case SHOW_MOVES_BOX:
            has_made_peggleboz_move = true;
        case DIM_SHOW_MOVES_BOX:
            get_image_size(PBOZ_SHOW_MOVES_BOX_LOC, &width, &height);
            draw_to_pixmap_unscaled(pic, width, height);
            return;
        case EXIT_BOX:
            get_image_size(PBOZ_EXIT_BOX_LOC, &width, &height);
            draw_to_pixmap_unscaled(pic, width, height);
            return;
        default:
            fprintf(stderr, "Error! Unhandled peggleboz image!\n");
    }
}

static void draw_peggles(void) {
    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(zorkzero_peggleboz_border, 0, 0);

    int width, height;
    for (int i = 2; i < 43; i += 2) {
        uint16_t pic = user_word(zt.PBOZ_PIC_TABLE + i - 2);
        get_image_size(pic, &width, &height);
        store_word(zt.BOARD_TABLE + i * 2, height);
        store_word(zt.BOARD_TABLE + (i + 1) * 2, width);
    }

    internal_call(pack_routine(zr.DRAW_PEGS)); // <DRAW-PEGS>

    get_image_size(PBOZ_RESTART_BOX_LOC, &width, &height);
    if (has_made_peggleboz_move) {
        draw_to_pixmap_unscaled(RESTART_BOX, width, height);
    } else {
        draw_to_pixmap_unscaled(DIM_RESTART_BOX, width, height);
    }
    get_image_size(PBOZ_SHOW_MOVES_BOX_LOC, &width, &height);
    if (has_made_peggleboz_move) {
        draw_to_pixmap_unscaled(SHOW_MOVES_BOX, width, height);
    } else {
        draw_to_pixmap_unscaled(DIM_SHOW_MOVES_BOX, width, height);
    }
    get_image_size(PBOZ_EXIT_BOX_LOC, &width, &height);
    draw_to_pixmap_unscaled(EXIT_BOX, width, height);

    adjust_text_window_by_split(PBOZ_SPLIT);
    ADJUST_TEXT_WINDOW(PBOZ_BOTTOM);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

uint16_t selected_peg = 0;

bool skip_pboz = false;

void SETUP_PBOZ(void) {
    z0_hide_right_status();

    if (V6_STATUS_WINDOW.id != nullptr) {
        v6_define_window(&V6_STATUS_WINDOW, 0, 0, 0, 0);
    }

    // (Note that the below does not apply to release 242)
    // What does this do, exactly? Attribute 7 is checked in PBOZ-WIN-CHECK:
    // Game is considered won if the first 21 attributes (1-21) of NOT-HERE-OBJECT are set
    // except attribute 7, which must not be set
    internal_set_attr(zo.NOT_HERE_OBJECT, 7);
    internal_set_attr(zo.PBOZ_OBJECT, 0x20); // 0x20 is TOUCHBIT
    has_made_peggleboz_move = false;
    selected_peg = 0;
    draw_peggles();
    flush_image_buffer();

    if (autosolve_visual_puzzle()) {
        transcribe_and_print_string("\n");
        skip_pboz = true;
        transcribe_and_print_string("Press any key twice to win.\n");
    }

}

static uint16_t pboz_click(void) {
    int left, top, width, height;
    get_image_size(PBOZ_RESTART_BOX_LOC, &left, &top);
    get_image_size(RESTART_BOX, &width, &height);
    if (within(left, top, width, height)) {
        return 'Z';
    }

    get_image_size(PBOZ_SHOW_MOVES_BOX_LOC, &left, &top);
    get_image_size(SHOW_MOVES_BOX, &width, &height);
    if (within(left, top, width, height)) {
        return 'Y';
    }

    get_image_size(PBOZ_EXIT_BOX_LOC, &left, &top);
    get_image_size(EXIT_BOX, &width, &height);
    if (within(left, top, width, height)) {
        return 'X';
    }

    int expand_width, expand_height, peg_width, peg_height;
    get_image_size(EXPAND_HOT_SPOT, &expand_width, &expand_height);
    get_image_size(UNHL_PEG, &peg_width, &peg_height);

    for (int i = 2; i < 43; i += 2) {
        top = user_word(zt.BOARD_TABLE + i * 2);
        left = user_word(zt.BOARD_TABLE + (i + 1) * 2);
        if (within(left - expand_width, top, peg_width + 2 * expand_width, peg_height + expand_height)) {
            return i / 2 + 64; // "divide in half and convert to ASCII"
        }
    }
    win_beep(1);
    return ZSCII_CLICK_SINGLE;
}

void PBOZ_CLICK(void) {
    store_variable(2, pboz_click());
}

void PEG_GAME(void) {
    selected_peg = variable(2) * 2;
}

void PEG_GAME_READ_CHAR(void) {
    if (skip_pboz) {
        if (selected_peg == 0)
            store_variable(3, 'Q');
        else
            store_variable(3, 'G');
    }
}


void PBOZ_WIN_CHECK(void) {
    // Game is considered won if the first 21 attributes (1-21) of NOT-HERE-OBJECT are set
    // except attribute 7, which must not be set.
    if (skip_pboz) {
        for (int i = 1; i < 22; i++) {
            internal_set_attr(zo.NOT_HERE_OBJECT, i);
        }
        internal_clear_attr(zo.NOT_HERE_OBJECT, 7);
        skip_pboz = false;
    }
}

void DISPLAY_MOVES(void) {
    glk_put_char(UNICODE_LINEFEED);
}


#pragma mark TOWER OF BOZBAR

static void clear_peg_table(uint16_t table) {
    for (int i = 0; i < 6; i++) {
        store_word(table + i * 2, 0);
    }
}

static void draw_tower_of_bozbar(void);

static bool tower_solved = false;

static void solve_tower(uint8_t c) {
    uint16_t peg = 0;
    uint16_t table = 0;
    switch (c) {
        case 'l':
            peg = zo.LEFT_PEG;
            table = zt.LEFT_PEG_TABLE;
            break;
        case 'c':
            peg = zo.CENTER_PEG;
            table = zt.CENTER_PEG_TABLE;
            break;
        case 'r':
            peg = zo.RIGHT_PEG;
            table = zt.RIGHT_PEG_TABLE;
            break;
        default:
            fprintf(stderr, "ERROR!\n");
            return;
    }

    internal_insert(zo.SIX_WEIGHT, peg);
    internal_insert(zo.FIVE_WEIGHT, peg);
    internal_insert(zo.FOUR_WEIGHT, peg);
    internal_insert(zo.THREE_WEIGHT, peg);
    internal_insert(zo.TWO_WEIGHT, peg);
    internal_insert(zo.ONE_WEIGHT, peg);

    clear_peg_table(zt.LEFT_PEG_TABLE);
    clear_peg_table(zt.CENTER_PEG_TABLE);
    clear_peg_table(zt.RIGHT_PEG_TABLE);

    store_word(table, zo.SIX_WEIGHT);
    store_word(table + 2, zo.FIVE_WEIGHT);
    store_word(table + 4, zo.FOUR_WEIGHT);
    store_word(table + 6, zo.THREE_WEIGHT);
    store_word(table + 8, zo.TWO_WEIGHT);
    store_word(table + 10,zo.ONE_WEIGHT);

    draw_tower_of_bozbar();

    if (zg.TOWER_CHANGED != 0) // Does not exist in r242
        set_global(zg.TOWER_CHANGED, 1);
    else
        tower_solved = true;
}

static void ask_which_peg(void) {
    uint16_t result = internal_call(pack_routine(zr.TOWER_WIN_CHECK));
    if (result > 0) {
        transcribe_and_print_string("Currently, the weights are on the ");
        if (result == zo.PYRAMID_L) {
            transcribe_and_print_string("left");
        } else if (result == zo.PYRAMID_R) {
            transcribe_and_print_string("right");
        } else {
            transcribe_and_print_string("center");
        }
        transcribe_and_print_string(" peg. ");
    }
    transcribe_and_print_string("To which peg do you want to move the weights? (L/C/R) >");

    bool done = false;
    uint8_t c;
    while (!done) {
        c = internal_read_char();
        glk_put_char(c);
        transcribe(c);
        transcribe_and_print_string("\n");
        c = tolower(c);
        switch (c) {
            case 'l':
            case 'r':
            case 'c':
                done = true;
                break;
            default:
                transcribe_and_print_string("Please type L, C, or R. > ");
                break;
        }
    }

    solve_tower(c);
}

void SMALL_DOOR_F(void) {
    if (autosolve_visual_puzzle()) {
        ask_which_peg();
    } else if (gli_voiceover_on) {
        transcribe_and_print_string("[Note that you can get a description of the weights arrangement by pressing the D button during play.]");
    }
}

void TOWER_MODE(void) {
    if (tower_solved) {
        store_variable(5, 0);
        store_variable(6, 0);
        tower_solved = false;
    }
//    if (gli_voiceover_on) {
//        if (!dont_repeat_question_on_autorestore)
//            transcribe_and_print_string("Would you like to auto-solve this visual puzzle? (Y is affirmative): >");
//
//        dont_repeat_question_on_autorestore = false;
//
//        bool done = false;
//        while (!done) {
//            uint8_t c = internal_read_char();
//            glk_put_char(c);
//            transcribe(c);
//            transcribe_and_print_string("\n");
//            switch (c) {
//                case 'n':
//                case 'N':
//                    transcribe_and_print_string("\n");
//                    return;
//                case 'y':
//                case 'Y':
//                    done = true;
//                    break;
//                default:
//                    transcribe_and_print_string("(Y is affirmative): >");
//            }
//        }
//
//        transcribe_and_print_string("\n");
//        solve_tower();
//    }
}


static void draw_peg(uint16_t table, int offset) {
    uint16_t x = user_word(zt.B_X_TBL + offset * 2);
    for (int i = 0; i < 6; i++) {
        uint16_t wgt = user_word(table + i * 2);
        if (wgt == 0)
            return;
        uint16_t pic = internal_call(pack_routine(zr.SET_B_PIC), {wgt}); //  <SET-B-PIC WGT>
        draw_to_pixmap_unscaled(pic, x, user_word(zt.B_Y_TBL + i * 2));
    }
}

static bool undoing = true;

static void draw_tower_of_bozbar(void) {
    z0_hide_right_status();

    if (V6_STATUS_WINDOW.id != nullptr) {
        v6_define_window(&V6_STATUS_WINDOW, 0, 0, 0, 0);
    }

    int width, height;
    get_image_size(B_1_L_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL + 5 * 2, height);
    store_word(zt.B_X_TBL, width);

    get_image_size(B_2_C_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL + 4 * 2, height);
    store_word(zt.B_X_TBL + 2, width);

    get_image_size(B_3_R_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL + 3 * 2, height);
    store_word(zt.B_X_TBL + 2 * 2, width);

    get_image_size(B_4_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL + 2 * 2, height);

    get_image_size(B_5_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL + 2, height);

    get_image_size(B_6_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL,  height);

    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(zorkzero_tower_of_bozbar_border, 0, 0);
    draw_peg(zt.LEFT_PEG_TABLE, 0);
    draw_peg(zt.CENTER_PEG_TABLE, 1);
    draw_peg(zt.RIGHT_PEG_TABLE, 2);

    get_image_size(TOWER_UNDO_BOX_LOC, &width, &height);
    if (undoing || internal_call(pack_routine(zr.TOWER_WIN_CHECK))) {
        draw_to_pixmap_unscaled(DIM_UNDO_BOX, width, height);
    } else {
        draw_to_pixmap_unscaled(UNDO_BOX, width, height);
    }

    get_image_size(TOWER_EXIT_BOX_LOC, &width, &height);
    draw_to_pixmap_unscaled(EXIT_BOX, width, height);
    flush_image_buffer();
    adjust_text_window_by_split(zorkzero_tower_of_bozbar_split);
    ADJUST_TEXT_WINDOW(zorkzero_tower_of_bozbar_bottom);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

void DRAW_TOWER(void) {
    undoing = (variable(1) == 1);
    draw_tower_of_bozbar();
}

static uint8_t pick_peg(void) {
    int weight_width, left, top, peg_height;
    get_image_size(B_1_L_PIC_LOC, &left, &top);
    get_image_size(B_1_WEIGHT, &weight_width, nullptr);
    get_image_size(B_6_PIC_LOC, nullptr, &peg_height);
    if (within(left, top, weight_width, peg_height)) {
        return 'L';
    }
    get_image_size(B_2_C_PIC_LOC, &left, nullptr);
    if (within(left, top, weight_width, peg_height)) {
        return 'C';
    }
    get_image_size(B_3_R_PIC_LOC, &left, nullptr);
    if (within(left, top, weight_width, peg_height)) {
        return 'R';
    }
    win_beep(1);
    return ZSCII_CLICK_DOUBLE;
}

void B_MOUSE_PEG_PICK(void) {
    store_variable(2, pick_peg());
}

uint8_t pick_weight(void) {
    int left, top, width, height, weight_width, weight_height, num;
    get_image_size(TOWER_UNDO_BOX_LOC, &left, &top);
    get_image_size(UNDO_BOX, &width, &height);
    if (within(left, top, width, height))
        return 'U';
    get_image_size(TOWER_EXIT_BOX_LOC, &left, &top);
    get_image_size(EXIT_BOX, &width, &height);
    if (within(left, top, width, height))
        return 'X';
    get_image_size(B_1_WEIGHT, &weight_width, &weight_height);
    weight_height++;
    uint16_t tbl = zt.LEFT_PEG_TABLE;
    for (int cnt_x = 0; cnt_x < 3; cnt_x++) {
        for (int cnt_y = 0; cnt_y < 6; cnt_y++) {
            left = user_word(zt.B_X_TBL + cnt_x * 2);
            top = user_word(zt.B_Y_TBL + cnt_y * 2);
            if (within(left, top, weight_width, weight_height)) {
                num = user_word(tbl + cnt_y * 2);
                if (num != 0) {
                    num = internal_get_prop(num, 0x29);
                    if (num >= 1 && num <= 6)
                        return num + 48;
                }
                win_beep(1);
                return ZSCII_CLICK_SINGLE;
            }
        }
        if (cnt_x == 0) {
            tbl = zt.CENTER_PEG_TABLE;
        } else {
            tbl = zt.RIGHT_PEG_TABLE;
        }
    }
    win_beep(1);
    return ZSCII_CLICK_SINGLE;
}

void B_MOUSE_WEIGHT_PICK(void) {
    uint16_t result = pick_weight();
    store_variable(2, result);
}

#pragma mark Map stuff

static void update_blink_coordinates(uint16_t x, uint16_t y) {
    store_word(get_global(zg.BLINK_TBL) + 3 * 2, y);
    store_word(get_global(zg.BLINK_TBL) + 4 * 2, x);

    store_variable(3, y);
    store_variable(4, x);

    flush_image_buffer();
    glk_request_timer_events(1);
}

// If we resize the screen inside the loop of the V-MAP routine
// (i.e. the verb routine of the "map" command)
// and then make a mouse click which does not lead to map movement
// (i.e. we click outside of eligible rooms)
// the blinking image will be drawn in the wrong position,
// using the old coordinates.
// We fix this by updating the internal variables storing the
// coordinates upon every mouse click
void V_MAP_LOOP(void) {
    // Get the map location table of current room
    uint16_t TBL = internal_get_prop(get_global(zg.HERE), zp.P_MAP_LOC);
    // Get the coordinates of the map representation of current room
    uint16_t CY = internal_call(pack_routine(zr.MAP_Y), {user_word(TBL + 2)}); // <MAP-Y <ZGET .TBL 1>>
    uint16_t CX = internal_call(pack_routine(zr.MAP_X), {user_word(TBL + 4)}); // <MAP-X <ZGET .TBL 2>>

    store_variable(4, CX);
    store_variable(5, CY);
}

#pragma mark Other stuff

void z0_update_on_resize(void) {
    // Window 0 is S-TEXT, buffer text window
    // Window 1 is S-WINDOW, status text grid
    // Window 2 is SOFT-WINDOW, text grid used for DEFINE
    // Window 3 is used for encyclopedia image captions (on top of background image)
    // Window 7 is S-FULL, the entire background

//    if (current_graphics_buf_win)
//        glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
//    if (V6_TEXT_BUFFER_WINDOW.id)
//        win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);

    // Input text color should also be the same as the other text.

    // Global 0x2f is MACHINE


    int MACHINE = byte(0x1e);
    fprintf(stderr, "HW interpreter number (byte 0x1e): %d options.int_number:%lu\n", MACHINE, options.int_number);

    int FONT_X = byte(0x27);
    int FONT_Y = byte(0x26);

    fprintf(stderr, "HW font width (byte 0x1e): %d gcellw: %f HW font height (byte 0x1e): %d gcellh:%f\n", FONT_X, gcellw, FONT_Y, gcellh);

    int WIDTH = word(17 * 2);
    int HEIGHT = word(18 * 2);

    fprintf(stderr, "HW screen width (word 18): %d gscreenw: %d HW screen height (word 17): %d gscreenh:%d\n", WIDTH, gscreenw, HEIGHT, gscreenh);

    WIDTH = byte(33);

    fprintf(stderr, "WIDTH (Global 119) (byte 33): %d\n", WIDTH);

    int stored_margin_image_number = number_of_margin_images;

    bool NARROW = (MACHINE == INTERP_MSDOS || MACHINE == INTERP_APPLE_IIE);
    set_global(zg.NARROW, NARROW);

    glk_request_timer_events(0);

    // SETUP-SCREEN copies image metrics
    // for the header text and compass
    // into the SL-LOC-TBL
    if (zr.SETUP_SCREEN != 0)
        internal_call(pack_routine(zr.SETUP_SCREEN));

    if (screenmode == MODE_DEFINE) {
        adjust_definitions_window();
        return;
    } else if (screenmode == MODE_HINTS) {
        redraw_hint_screen_on_resize();
        return;
    } else if (screenmode == MODE_MAP) {
        z0_hide_right_status();

        // redraw the map
        internal_call(pack_routine(zr.V_REFRESH), {1}); // V-$REFRESH(DONT-CLEAR:true)
//        internal_call(pack_routine(0x16130)); // DO-MAP

        // We are in a loop inside the BLINK routine
        // and need to update coordinates in the BLINK-TBL
        // (which will be read by the TYPED? routine)
        // as well as the local Y and X variables
        // (which are used to redraw/unhighlight the
        // previous location icon when we move away)

        uint16_t TBL = internal_get_prop(get_global(zg.HERE), 0x26); //  <GETP ,HERE ,P?MAP-LOC>>
        uint16_t CY = internal_call(pack_routine(zr.MAP_Y), {user_word(TBL + 2)}); // <MAP-Y <ZGET .TBL 1>>
        uint16_t CX = internal_call(pack_routine(zr.MAP_X), {user_word(TBL + 4)}); // <MAP-X <ZGET .TBL 2>>

        update_blink_coordinates(CX, CY);
        return;
    } else if (screenmode == MODE_NORMAL || screenmode == MODE_Z0_GAME) {
        uint16_t CURRENT_SPLIT = get_global(zg.CURRENT_SPLIT);
        if (CURRENT_SPLIT == zorkzero_tower_of_bozbar_split) {
            draw_tower_of_bozbar();
            flush_image_buffer();
            return;
        } else if (CURRENT_SPLIT == PBOZ_SPLIT) { // Peggleboz
            draw_peggles();
            if (selected_peg != 0) {
                uint16_t CY = user_word(zt.BOARD_TABLE + selected_peg * 2);
                uint16_t CX = user_word(zt.BOARD_TABLE + (selected_peg + 1) * 2);
                update_blink_coordinates(CX, CY);
                return;
            }
            flush_image_buffer();
            return;
        } else if (CURRENT_SPLIT == SN_SPLIT) { // Snarfem
            draw_snarfem();
            return;
        } else if (CURRENT_SPLIT == F_SPLIT) { // Double Fanucci
            redraw_fanucci();
            return;
        }
        internal_call(pack_routine(zr.V_REFRESH), {1}); // V-$REFRESH(DONT-CLEAR:true)
        if (V6_TEXT_BUFFER_WINDOW.id) {
            if (graphics_type_changed) {
                refresh_margin_images();
                float yscalefactor = 2.0;
                if (graphics_type == kGraphicsTypeMacBW)
                    yscalefactor = imagescalex;
                float xscalefactor = yscalefactor * pixelwidth;
                win_refresh(V6_TEXT_BUFFER_WINDOW.id->peer, xscalefactor, yscalefactor);
            }
            win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);
        }
    } else if (screenmode == MODE_SLIDESHOW) {
        clear_image_buffer();
        if (is_zorkzero_encyclopedia_image(current_picture)) {
            draw_encyclopedia();
            return;
        }
        draw_to_buffer(current_graphics_buf_win, current_picture, 0, 0);
    }

    glui32 stored_bg = user_selected_background;

    if (current_picture == zorkzero_title_image) {
        user_selected_background = monochrome_black;
    }

    flush_image_buffer();

    user_selected_background = stored_bg;
    number_of_margin_images = stored_margin_image_number;
}

void z0_update_after_restore(void) {
    after_V_COLOR();
}

void z0_update_after_autorestore(void) {

}

extern bool pending_flowbreak;

bool z0_display_picture(int x, int y, Window *win) {

    if (is_zorkzero_tower_image(current_picture) || is_zorkzero_peggleboz_image(current_picture)) {
        draw_to_pixmap_unscaled(current_picture, x, y);
        return true;
    }

    if (is_zorkzero_peggleboz_box_image(current_picture)) {
        z0_draw_peggleboz_box_image(current_picture, x, y);
        return true;
    }

    if (current_picture == zorkzero_title_image || current_picture == zorkzero_encyclopedia_border || current_picture == zorkzero_map_border || (is_zorkzero_rebus_image(current_picture) && screenmode == MODE_NORMAL)) {

        // Removes extra graphics window that appears after resizing map?
//        if (win->id != nullptr && win->id != graphics_bg_glk) {
//            v6_delete_glk_win(win->id);
//        }
//
//        if (win->id == nullptr)
//            win->id = gli_new_window(wintype_Graphics, 0);
//        v6_define_window(win, 1, 1, gscreenw, gscreenh);

        z0_hide_right_status();

        current_graphics_buf_win = graphics_fg_glk;
        win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
        glk_request_mouse_event(graphics_fg_glk);

        if (current_picture == zorkzero_map_border) {
            screenmode = MODE_MAP;
        } else {
            screenmode = MODE_SLIDESHOW;
        }
        if (current_picture == zorkzero_encyclopedia_border) {
            windows[3].id = gli_new_window(wintype_TextBuffer, 0);
            draw_to_pixmap_unscaled(zorkzero_encyclopedia_border, 0, 0);
            image_needs_redraw = true;
            adjust_encyclopedia_text_window();
            return true;
        }
        draw_to_buffer(current_graphics_buf_win, current_picture, x, y);
        flush_bitmap(current_graphics_buf_win);
        return true;
    }

    if (win->id && win->id->type == wintype_TextBuffer) {
        pending_flowbreak = true;
        float inline_scale = 2.0;
        if (graphics_type == kGraphicsTypeMacBW)
            inline_scale = imagescalex;
        draw_inline_image(win->id, current_picture, imagealign_MarginLeft, current_picture, inline_scale, false);
        add_margin_image_to_list(current_picture);
        return true;
    } else if (is_zorkzero_encyclopedia_image(current_picture)) {
        win_sizewin(graphics_fg_glk->peer, 0, 0, gscreenw, gscreenh);
        draw_encyclopedia();
        return true;
    } else {
//        fprintf(stderr, "Current window is %d\n", win->index);
        if (current_graphics_buf_win == nullptr)
            current_graphics_buf_win = graphics_bg_glk;
        draw_to_buffer(current_graphics_buf_win, current_picture, x, y);
        return true;
    }

    return false;
}

void DEFAULT_COLORS(void) {
    set_global(zg.DEFAULT_FG, 1);
    set_global(zg.DEFAULT_BG, 1);
}

void z0_stash_state(library_state_data *dat) {
    if (!dat)
        return;

    if (current_graphics_buf_win)
        dat->current_graphics_win_tag = current_graphics_buf_win->tag;
    if (graphics_fg_glk)
        dat->graphics_fg_tag = graphics_fg_glk->tag;
    if (stored_bufferwin)
        dat->stored_lower_tag = stored_bufferwin->tag;
    if (z0_right_status_window)
        dat->z0_right_status_tag = z0_right_status_window->tag;
}

void z0_recover_state(library_state_data *dat) {
    if (!dat)
        return;

    current_graphics_buf_win = gli_window_for_tag(dat->current_graphics_win_tag);
    graphics_fg_glk = gli_window_for_tag(dat->graphics_fg_tag);
    stored_bufferwin = gli_window_for_tag(dat->stored_lower_tag);
    z0_right_status_window = gli_window_for_tag(dat->z0_right_status_tag);
}

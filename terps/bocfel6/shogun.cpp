//
//  shogun.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-21.
//

extern "C" {
#include "glk.h"
#include "glkimp.h"
}

#include "zterp.h"
#include "memory.h"
#include "screen.h"
#include "entrypoints.hpp"
#include "draw_image.hpp"

#include "shogun.hpp"

extern float imagescalex, imagescaley;

int shogun_graphical_banner_width_left = 0;
int shogun_graphical_banner_width_right = 0;

bool is_shogun_inline_image(int picnum) {
    return (picnum >= 7 && picnum <= 37);
}

bool is_shogun_map_image(int picnum) {
    return (picnum >= 38);
}

bool is_shogun_border_image(int picnum) {
    return ((picnum >= 3 && picnum <= 5) || picnum == 59 || picnum == 50);
}


void after_V_DEFINE(void) {
    screenmode = MODE_NORMAL;
    v6_delete_win(&windows[2]);
    v6_remap_win_to_buffer(&windows[0]);
}

void SCENE_SELECT(void) {
    v6_delete_win(&windows[0]);
    v6_delete_win(&windows[2]);
    windows[0].id = v6_new_glk_window(wintype_TextGrid, 0);
    win_setbgnd(windows[0].id->peer, user_selected_background);
    windows[0].x_origin = shogun_graphical_banner_width_left + letterwidth;
    if (windows[0].x_origin == 0)
        windows[0].x_origin = letterwidth * 2;
    windows[0].x_size = gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right - letterwidth * 2;
    v6_sizewin(&windows[0]);
    windows[2].id = v6_new_glk_window(wintype_TextGrid, 0);
}

void GOTO_SCENE(void) {
    screenmode = MODE_NORMAL;
    v6_delete_win(&windows[0]);
    windows[0].id = v6_new_glk_window(wintype_TextBuffer, 0);
    v6_delete_win(&windows[7]);
    adjust_shogun_window();
}

// Shogun
void after_V_HINT(void) {
    screenmode = MODE_NORMAL;
    v6_delete_win(&windows[0]);
    windows[0].id = v6_new_glk_window(wintype_TextBuffer, 0);
    adjust_shogun_window();
    //    if (graphics_type == kGraphicsTypeApple2)
    //        win_setbgnd(windows[1].id->peer, 0xffffff);
}

void after_SETUP_TEXT_AND_STATUS(void) {
    if (screenmode == MODE_NORMAL) {
        adjust_shogun_window();
        if (graphics_type == kGraphicsTypeApple2) {
            if (windows[1].id == nullptr) {
                windows[1].id = v6_new_glk_window(wintype_TextGrid, 0);
            }
            v6_sizewin(&windows[1]);
            glk_window_clear(windows[1].id);
        }
    } else if (screenmode == MODE_HINTS) {
        windows[1].y_size = 3 * gcellh + 2 * ggridmarginy;
        v6_delete_win(&windows[0]);
        v6_remap_win_to_grid(&windows[0]);
        win_setbgnd(windows[0].id->peer, user_selected_background);
        if (graphics_type == kGraphicsTypeApple2) {
            windows[7].y_origin = windows[1].y_size + 1;
            windows[0].y_origin = windows[7].y_origin + windows[7].y_size;
            windows[0].y_size = gscreenh - windows[0].y_origin;
        } else if (graphics_type != kGraphicsTypeMacBW && graphics_type != kGraphicsTypeCGA) {
            win_maketransparent(windows[1].id->peer);
        } else if (graphics_type == kGraphicsTypeCGA || graphics_type == kGraphicsTypeAmiga) {
            windows[1].x_origin = shogun_graphical_banner_width_left;
            windows[1].x_size = gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right;
            windows[0].x_origin = windows[1].x_origin;
            windows[0].x_size = windows[1].x_size;
            v6_sizewin(&windows[1]);
            v6_sizewin(&windows[0]);
        }
    }
}

void V_VERSION(void) {
    if (screenmode == MODE_SLIDESHOW) {
        if (windows[7].id != graphics_win_glk)
            v6_delete_win(&windows[7]);
        if (graphics_type == kGraphicsTypeApple2) {
            windows[7].id = v6_new_glk_window(wintype_TextGrid, 0);
            if (a2_graphical_banner_height == 0) {
                int width, height;
                get_image_size(3, &width, &height);
                a2_graphical_banner_height = height * (gscreenw / ((float)width * 2.0));
            }
            windows[7].y_origin = a2_graphical_banner_height + 1;
            windows[7].x_origin = 0;
            windows[7].x_size = gscreenw;
            windows[7].y_size = gscreenh - (2 * a2_graphical_banner_height + 4 * letterheight);
        } else {
            v6_delete_win(&windows[1]);
            windows[7].id = v6_new_glk_window(wintype_TextBuffer, 0);
            windows[7].y_origin = letterheight * 3;
            windows[7].x_origin = shogun_graphical_banner_width_left + imagescalex;
            windows[7].x_size = gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right - letterwidth;
            windows[7].y_size = gscreenh - 7 * letterheight;
        }
        v6_sizewin(&windows[7]);
//        windows[7].zpos = max_zpos;
        glk_set_window(windows[7].id);
        centeredText = true;
        set_current_style();
        screenmode = MODE_CREDITS;
    }
}

void MAC_II(void) {
    windows[7].x_size = 640;
}

void after_MAC_II(void) {
    windows[7].x_size = gscreenw;
}

void after_V_VERSION(void) {
    centeredText = false;
    set_current_style();
}

void adjust_shogun_window(void) {
    windows[0].x_cursor = 0;
    windows[0].y_cursor = 0;
    windows[0].x_size = gscreenw;

    if (graphics_type == kGraphicsTypeApple2) {
        screenmode = MODE_NORMAL;
        windows[1].y_size = 2 * letterheight;
        v6_sizewin(&windows[1]);
        windows[6].y_origin = windows[1].y_size + 1;
        windows[6].y_size = a2_graphical_banner_height;
        v6_sizewin(&windows[6]);
        windows[0].y_origin = windows[6].y_origin + windows[6].y_size;
        windows[0].y_size = gscreenh - windows[0].y_origin;
    } else {
        if (windows[1].id == nullptr)
            windows[1].id = v6_new_glk_window(wintype_TextGrid, 0);
        windows[1].x_origin = shogun_graphical_banner_width_left;
        windows[1].x_size = gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right;
        windows[1].fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
        windows[1].bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
        windows[1].style.reset(STYLE_REVERSE);
        glk_window_set_background_color(windows[0].id, user_selected_foreground);
        windows[0].fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
        windows[0].bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
        win_setbgnd(windows[0].id->peer, user_selected_background);
        windows[0].x_origin = windows[1].x_origin;
        windows[0].x_size = windows[1].x_size;
        v6_sizewin(&windows[1]);
    }
    v6_sizewin(&windows[0]);
    glk_set_window(windows[0].id);
}

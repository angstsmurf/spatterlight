// v6_windows.cpp
//
// Glk window primitives for V6 games: pixel placement and swapping the Glk
// window behind a Z-machine window.

#include <cassert>

#include "options.h"
#include "screen.h"
#include "zterp.h"

#include "v6_shared.hpp"
#include "v6_windows.hpp"

void v6_sizewin(Window *win) {
    if (win->id == nullptr)
        return;

    int x0 = (int16_t)win->x_origin <= 1 ? 0 : (int)win->x_origin - 1;
    int y0 = (int16_t)win->y_origin <= 1 ? 0 : (int)win->y_origin - 1;

    int x1 = x0 + win->x_size;
    if (x1 + gcellw > gscreenw)
        x1 = gscreenw;
    int y1 = y0 + win->y_size;
    if (y1 + gcellh > gscreenh && win->y_size > gcellh)
        y1 = gscreenh;

    win_sizewin(win->id->peer, x0, y0, x1, y1);
    win->id->bbox.x0 = x0;
    win->id->bbox.y0 = y0;
    win->id->bbox.x1 = x1;
    win->id->bbox.y1 = y1;

    if (win->id->type == wintype_TextGrid) {
        Window *stashedwin = curwin;
        curwin = win;
        set_cursor(win->y + 1, win->x + 1);
        curwin = stashedwin;
    }
}

void v6_define_window(Window *win, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    win->x_origin = x;
    win->y_origin = y;
    win->x_size = width;
    win->y_size = height;
    v6_sizewin(win);
}

void v6_delete_glk_win(winid_t glkwin) {
    if (glkwin == nullptr)
        return;

    if (graphics_bg_glk == glkwin) {
        graphics_bg_glk = nullptr;
    }

    if (current_graphics_buf_win == glkwin) {
        current_graphics_buf_win = nullptr;
    }

    for (auto &window : windows) {
        if (window.id == glkwin) {
            window.id = nullptr;
        }
    }

    gli_delete_window(glkwin);
}

void v6_delete_win(Window *win) {
    if (win == nullptr || win->id == nullptr)
        return;
    v6_delete_glk_win(win->id);
    win->id = nullptr;
}

void v6_remap_win(Window *win, int type, winid_t *stored_win) {
    if (win->id) {
        // Without somewhere to keep it, the old window would leak.
        assert(stored_win != nullptr);
        *stored_win = win->id;
        win_sizewin(win->id->peer, 0, 0, 0, 0);
    }

    win->id = gli_new_window(type, 0);

    if (win == curwin) {
        glk_set_window(win->id);
        set_current_style();
    }

    if (type == wintype_TextGrid) {
        if (is_spatterlight_arthur && win == upperwin) {
            win->style.set(STYLE_REVERSE);
        }
    } else if (type == wintype_TextBuffer) {
        glk_set_echo_line_event(win->id, 0);
    }
    v6_sizewin(win);
}

void v6_remap_win_to_type(Window *win, int type) {
    if (win->id) {
        if (win->id->type == type) {
            return;
        }
        gli_delete_window(win->id);
        win->id = nullptr;
    }
    v6_remap_win(win, type, nullptr);
}

void v6_remap_win_to_grid(Window *win) {
    v6_remap_win_to_type(win, wintype_TextGrid);
}

void v6_remap_win_to_buffer(Window *win) {
    v6_remap_win_to_type(win, wintype_TextBuffer);
}

void v6_apply_interpreter_number(void) {
    options.int_number = gli_zmachine_terp;
    if (!is_spatterlight_v6) {
        return;
    }
    switch (options.int_number) {
        case INTERP_CBM_64:
        case INTERP_DEC_20:
        case INTERP_CBM_128:
        case INTERP_TANDY:
            options.int_number = INTERP_MSDOS;
            break;
        case INTERP_DEFAULT:
        case INTERP_ATARI_ST:
            options.int_number = INTERP_AMIGA;
            break;
        case INTERP_APPLE_IIC:
        case INTERP_APPLE_IIGS:
            options.int_number = INTERP_APPLE_IIE;
            break;
        default:
            // Keeping the interpreter version selection
            break;
    }
}

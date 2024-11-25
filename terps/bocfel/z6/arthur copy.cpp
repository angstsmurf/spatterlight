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

// <ROUTINE RT-ROOM-NAME-MSG ("AUX" L)
// <COND
// (,LIT
//  <SET L <LOC ,PLAYER>>
//  <COND (<EQUAL? .L ,TH-BEHIND-G-STONE
//        ,TH-BEHIND-ROCK ,TH-BEHIND-DOOR>
//        <TELL "behind ">
//        <COND
//        (<EQUAL? .L ,TH-BEHIND-G-STONE>
//         <TELL "gravestone">)
//        (<EQUAL? .L ,TH-BEHIND-ROCK>
//         <TELL "rock">)
//        (T
//         <TELL "door">)>)
//  (ELSE
//   <TELL D ,HERE>)>)
// (T
//  <TELL "darkness">)>>

//Routine 1651c, 1 local ; <ROUTINE RT-ROOM-NAME-MSG ("AUX" L)
//
//1651d:  a0 93 f4                JZ              G83 [TRUE] 16552
//16520:  83 01 21 01             GET_PARENT      "yourself" -> L00
//16524:  c1 94 01 88 66 01 0b 61 JE              L00,#88,#66,#010b [FALSE] 1654b
//1652c:  b2 ...                  PRINT           "behind "
//1652f:  41 01 88 4a             JE              L00,#88 [FALSE] 1653b
//16533:  b2 ...                  PRINT           "gravestone"
//1653a:  b0                      RTRUE
//1653b:  41 01 66 48             JE              L00,#66 [FALSE] 16545
//1653f:  b2 ...                  PRINT           "rock"
//16544:  b0                      RTRUE
//16545:  b2 ...                  PRINT           "door"
//1654a:  b0                      RTRUE
//1654b:  d9 2f 1a 3a 17 00       CALL_2S         10458 (G07) -> -(SP)
//16551:  b8                      RET_POPPED
//16552:  b2 ...                  PRINT           "darkness"
//16559:  b0                      RTRUE

void RT_ROOM_NAME_MSG(void) {
    if (get_global(0x83) == 0) { // LIT
        // PRINT           "darkness"
    } else {
        int L = internal_get_parent(0x21);
        switch (L) {
            case 0x88: // TH-BEHIND-G-STONE
                    // PRINT  "behind gravestone"
                break;
            case 0x66: // TH-BEHIND-ROCK
                break;
                // PRINT  "behind rock"

            case 0x10b: // TH-BEHIND-DOOR
                        // PRINT  "behind door"

        }
    }
}

void ARTHUR_INIT_STATUS_LINE(void) {
    if (get_global(0x32) == 0) { // <EQUAL? ,GL-WINDOW-TYPE ,K-WIN-NONE>
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
        windows[2].y_origin = 1;
        windows[2].x_origin = L;
        set_global(0x88, L - 1);
        set_global(0x59, 0);
        windows[2].y_size = N - 1;
        windows[2].x_size = W;

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

    set_global(0x46, 0); // <SETG GL-TIME-WIDTH 0>
    set_global(0x8c, 0); // <SETG GL-SL-HERE <>>
    set_global(0x70, 0); // <SETG GL-SL-VEH <>>
    set_global(0x95, 0); // <SETG GL-SL-HIDE <>>
    set_global(0x4f, 0); // <SETG GL-SL-TIME 0>
    set_global(0x4c, 0); // <SETG GL-SL-FORM 0>
}

void INIT_STATUS_LINE(void) {
//    arthur_adjust_windows();
}

void after_INIT_STATUS_LINE(void) {
//    arthur_adjust_windows();
}


void ARTHUR_UPDATE_STATUS_LINE(void) {}

void UPDATE_STATUS_LINE(void) {}
//    bool REF = false;
//    glk_set_window(V6_STATUS_WINDOW.id);
//    garglk_set_reversevideo(1);
//    int L = internal_get_parent(0x21);
//    if (get_global(0x07) != get_global(0x8c)) // <EQUAL? ,HERE ,GL-SL-HERE>
//        goto addr9e4c;
//    if (get_global(0x47) != get_global(0x95)) // <EQUAL? ,GL-HIDING ,GL-SL-HIDE>
//        goto addr9e4c;
//    if (internal_get_parent(L) != 0xba) // <IN? .L ,ROOMS>
//        goto addr9e42;
//    if (get_global(0x70) != 0)
//        goto addr9e4c;
//
//    addr9e42:
//    if (internal_get_parent(L) == 0xba) // <IN? .L ,ROOMS>
//        goto addr9ee1;
//    if (L == get_global(0x70)) // <EQUAL? .L ,GL-SL-VEH>
//        goto addr9ee1;
//
//        addr9e4c:
//    REF = true; // <SET REF T>
//    glk_window_clear(V6_STATUS_WINDOW.id);
//    //  <CURSET 1 1>
//    //  <XERASE 1> ; Erase first character in status bar
//    //  <DIROUT ,K-D-TBL-ON ,K-DIROUT-TBL> ; Direct text output to table K-DIROUT-TBL
//    //  <TELL " "> ; Print a space character (to the table)
//    //  <RT-ROOM-NAME-MSG>  ; Print room name
//    //  <DIROUT ,K-D-TBL-OFF> ; Stop directing output to K-DIROUT-TBL
//
//    //  <COND
//    //  (<AND <G=? <SET C <GETB ,K-DIROUT-TBL 3>> !\a>
//    //   <L=? .C !\z>>
//    //   <PUTB ,K-DIROUT-TBL 3 <- .C 32>>)>
//
//addr9ee1:

//    9e4c:  0d 05 01                STORE           L04,#01
//    9e4f:  ef 5f 01 01             SET_CURSOR      #01,#01
//    9e53:  da 1f 01 31 01          CALL_2N         a034 (#01) ; <XERASE 1>
//    9e58:  f3 4f 03 41 29          OUTPUT_STREAM   REDIRECT_ENABLE,#4129
//    9e5d:  e5 7f 20                PRINT_CHAR      ' '
//    9e60:  8f 32 6b                CALL_1N         1651c
//    9e63:  f3 3f ff fd             OUTPUT_STREAM   #fffd


//    9e67:  d0 1f 41 29 03 01       LOADB           #4129,#03 -> L00 ; Set C to byte 3 of table K-DIROUT-TBL
//    9e6d:  42 01 61 d0             JL              L00,#61 [TRUE] 9e7f ; if C < 0x61 (97), skip this
//    9e71:  43 01 7a cc             JG              L00,#7a [TRUE] 9e7f ; if C > 0x7a (122), skip this
//    9e75:  55 01 20 00             SUB             L00,#20 -> -(SP) ; L -= 32
//    9e79:  e2 1b 41 29 03 00       STOREB          #4129,#03,(SP)+

//    9e7f:  ef 5f 01 01             SET_CURSOR      #01,#01
//    9e83:  d4 1f 41 29 02 07       ADD             #4129,#02 -> L06
//    9e89:  cf 1f 41 29 00 00       LOADW           #4129,#00 -> -(SP)
//    9e8f:  fe af 07 00             PRINT_TABLE     L06,(SP)+




// <DEFINE UPDATE-STATUS-LINE ("AUX" C N L CW (REF <>) TMP)
// <SCREEN 1>
// <HLIGHT ,H-INVERSE>
// <SET L <LOC ,CH-PLAYER>>
// <COND
// (<OR <NOT <EQUAL? ,HERE ,GL-SL-HERE>>
//  <NOT <EQUAL? ,GL-HIDING ,GL-SL-HIDE>>
//  <AND
//  <IN? .L ,ROOMS>
//  ,GL-SL-VEH>
//  <AND
//  <NOT <IN? .L ,ROOMS>>
//  <NOT <EQUAL? .L ,GL-SL-VEH>>>>
//  <SET REF T>
//  <CURSET 1 1>
//  <XERASE 1>
//  <DIROUT ,K-D-TBL-ON ,K-DIROUT-TBL>
//  <TELL " ">
//  <RT-ROOM-NAME-MSG>
//  <DIROUT ,K-D-TBL-OFF>
//  <COND
//  (<AND <G=? <SET C <GETB ,K-DIROUT-TBL 3>> !\a>
//   <L=? .C !\z>>
//   <PUTB ,K-DIROUT-TBL 3 <- .C 32>>)>
//  <CURSET 1 1>
//  <PRINTT <ZREST ,K-DIROUT-TBL 2> <ZGET ,K-DIROUT-TBL 0>>
//  <COND
//  (<OR <IN? .L ,ROOMS>
//   <EQUAL? .L ,TH-BEHIND-G-STONE ,TH-BEHIND-ROCK ,TH-BEHIND-DOOR>>)
//  (<EQUAL? .L ;,TH-BEHIND-G-STONE ,TH-BEHIND-ROCK ,TH-BEHIND-DOOR>
//   <TELL ", behind ">
//   <COND
//   ;(<EQUAL? .L ,TH-BEHIND-G-STONE>
//     <TELL "gravestone">)
//   (<EQUAL? .L ,TH-BEHIND-ROCK>
//    <TELL "rock">)
//   (T
//    <TELL "door">)>)
//  (,GL-HIDING
//   <TELL ", behind " D ,GL-HIDING>)
//  (T
//   <TELL "," in .L " " D .L>)>)>
// <COND
// (<OR .REF
//  <NOT <EQUAL? ,GL-PLAYER-FORM ,GL-SL-FORM>>>
//  <COND (<AND <NOT .REF>
//         <NOT <EQUAL? ,GL-SL-FORM ,K-FORM-ARTHUR>>>
//         <SET TMP ,GL-PLAYER-FORM>
//         <SETG GL-PLAYER-FORM ,GL-SL-FORM>
//         <DIROUT ,K-D-TBL-ON ,K-DIROUT-TBL>
//         <TELL Form>
//         <DIROUT ,K-D-TBL-OFF>
//         <SETG GL-PLAYER-FORM .TMP>
//         <SET N <+ 1 </ <- <WINGET 1 ,K-W-XSIZE> <LOWCORE TWID>> 2>>>
//         <CURSET 1 .N>
//         <XERASE <LOWCORE TWID>>)>
//  <COND
//  (<NOT <EQUAL? ,GL-PLAYER-FORM ,K-FORM-ARTHUR>>
//   <DIROUT ,K-D-TBL-ON ,K-DIROUT-TBL>
//   <TELL Form>
//   <DIROUT ,K-D-TBL-OFF>
//   <SET N <+ 1 </ <- <WINGET 1 ,K-W-XSIZE> <LOWCORE TWID>> 2>>>
//   <CURSET 1 .N>
//   <TELL Form>)>)>
// <COND
// (<OR .REF
//  <NOT <EQUAL? </ ,GL-SL-TIME 180> </ ,GL-MOVES 180>>>>
//  <COND (<AND <NOT .REF> ,GL-TIME-WIDTH>
//         <CURSET 1 <- <WINGET 1 ,K-W-XSIZE> ,GL-TIME-WIDTH>>
//         <XERASE 1>)>
//  <SET REF T>
//  <DIROUT ,K-D-TBL-ON ,K-DIROUT-TBL>
//  <SET N </ ,GL-MOVES 1440>>
//  <COND
//  (<EQUAL? .N 0>
//   <TELL "St Anne's Day">)
//  (<EQUAL? .N 1>
//   <TELL "St John's Day">)
//  (<EQUAL? .N 2>
//   <TELL "Christmas Eve">)
//  (T
//   <TELL "Christmas Day">)>
//  <TELL ", ">
//  <RT-HOUR-NAME-MSG ,GL-MOVES>
//  <TELL " ">
//  <DIROUT ,K-D-TBL-OFF>
//  <SETG GL-TIME-WIDTH <LOWCORE TWID>>
//  <CURSET 1 <- <WINGET 1 ,K-W-XSIZE> <LOWCORE TWID>>>
//  <PRINTT <ZREST ,K-DIROUT-TBL 2> <ZGET ,K-DIROUT-TBL 0>>)>
// <HLIGHT ,H-NORMAL>
// <CURSET 1 1>
// <SCREEN 0>
// <SETG GL-SL-HERE ,HERE>
// <SETG GL-SL-HIDE ,GL-HIDING>
// <COND
// (<NOT <IN? .L ,ROOMS>>
//  <SETG GL-SL-VEH .L>)
// (T
//  <SETG GL-SL-VEH <>>)>
// <SETG GL-SL-FORM ,GL-PLAYER-FORM>
// <SETG GL-SL-TIME ,GL-MOVES>
// <RTRUE>>


//9e29:  eb 7f 01                SET_WINDOW      #01
//9e2c:  f1 7f 01                SET_TEXT_STYLE  REVERSE
//9e2f:  83 01 21 03             GET_PARENT      "yourself" -> L02
//9e33:  61 17 9c 57             JE              G07,G8c [FALSE] 9e4c
//9e37:  61 57 a5 53             JE              G47,G95 [FALSE] 9e4c
//9e3b:  46 03 ba 45             JIN             L02,"that" [FALSE] 9e42
//9e3f:  a0 80 4c                JZ              G70 [FALSE] 9e4c
//9e42:  46 03 ba 80 9c          JIN             L02,"that" [TRUE] 9ee1
//9e47:  61 03 80 80 97          JE              L02,G70 [TRUE] 9ee1
//9e4c:  0d 05 01                STORE           L04,#01
//9e4f:  ef 5f 01 01             SET_CURSOR      #01,#01
//9e53:  da 1f 01 31 01          CALL_2N         a034 (#01) ; <XERASE 1>
//9e58:  f3 4f 03 41 29          OUTPUT_STREAM   REDIRECT_ENABLE,#4129
//9e5d:  e5 7f 20                PRINT_CHAR      ' '
//9e60:  8f 32 6b                CALL_1N         1651c
//9e63:  f3 3f ff fd             OUTPUT_STREAM   #fffd
//9e67:  d0 1f 41 29 03 01       LOADB           #4129,#03 -> L00
//9e6d:  42 01 61 d0             JL              L00,#61 [TRUE] 9e7f
//9e71:  43 01 7a cc             JG              L00,#7a [TRUE] 9e7f
//9e75:  55 01 20 00             SUB             L00,#20 -> -(SP)
//9e79:  e2 1b 41 29 03 00       STOREB          #4129,#03,(SP)+
//9e7f:  ef 5f 01 01             SET_CURSOR      #01,#01
//9e83:  d4 1f 41 29 02 07       ADD             #4129,#02 -> L06
//9e89:  cf 1f 41 29 00 00       LOADW           #4129,#00 -> -(SP)
//9e8f:  fe af 07 00             PRINT_TABLE     L06,(SP)+
//9e93:  46 03 ba 80 4b          JIN             L02,"that" [TRUE] 9ee1
//9e98:  c1 94 03 88 66 01 0b 80 42
//JE              L02,#88,#66,#010b [TRUE] 9ee1
//9ea1:  c1 93 03 66 01 0b 5b    JE              L02,#66,#010b [FALSE] 9ec1
//9ea8:  b2 ...                  PRINT           ", behind "
//9ead:  41 03 66 4a             JE              L02,#66 [FALSE] 9eb9
//9eb1:  b2 ...                  PRINT           "rock"
//9eb6:  8c 00 2a                JUMP            9ee1
//9eb9:  b2 ...                  PRINT           "door"
//9ebe:  8c 00 22                JUMP            9ee1
//9ec1:  a0 57 cf                JZ              G47 [TRUE] 9ed1
//9ec4:  b2 ...                  PRINT           ", behind "
//9ec9:  da 2f 1a 3a 57          CALL_2N         10458 (G47)
//9ece:  8c 00 12                JUMP            9ee1
//9ed1:  e5 7f 2c                PRINT_CHAR      ','
//9ed4:  da 2f 1a f1 03          CALL_2N         10734 (L02)
//9ed9:  e5 7f 20                PRINT_CHAR      ' '
//9edc:  da 2f 1a 3a 03          CALL_2N         10458 (L02)
//9ee1:  a0 05 47                JZ              L04 [FALSE] 9ee9
//9ee4:  61 50 5c 80 78          JE              G40,G4c [TRUE] 9f5f
//9ee9:  a0 05 00 41             JZ              L04 [FALSE] 9f2c
//9eed:  41 5c 00 fd             JE              G4c,#00 [TRUE] 9f2c
//9ef1:  2d 06 50                STORE           L05,G40
//9ef4:  2d 50 5c                STORE           G40,G4c
//9ef7:  f3 4f 03 41 29          OUTPUT_STREAM   REDIRECT_ENABLE,#4129
//9efc:  f9 17 33 0a 00 01       CALL_VN         16798 (#00,#01)
//9f02:  f3 3f ff fd             OUTPUT_STREAM   #fffd
//9f06:  2d 50 06                STORE           G40,L05
//9f09:  be 13 5f 01 03 07       GET_WIND_PROP   #01,#03 -> L06
//9f0f:  0f 00 18 00             LOADW           #00,#18 -> -(SP)
//9f13:  75 07 00 00             SUB             L06,(SP)+ -> -(SP)
//9f17:  57 00 02 00             DIV             (SP)+,#02 -> -(SP)
//9f1b:  34 01 00 02             ADD             #01,(SP)+ -> L01
//9f1f:  ef 6f 01 02             SET_CURSOR      #01,L01
//9f23:  0f 00 18 00             LOADW           #00,#18 -> -(SP)
//9f27:  da 2f 01 31 00          CALL_2N         a034 ((SP)+)
//9f2c:  41 50 00 f1             JE              G40,#00 [TRUE] 9f5f
//9f30:  f3 4f 03 41 29          OUTPUT_STREAM   REDIRECT_ENABLE,#4129
//9f35:  f9 17 33 0a 00 01       CALL_VN         16798 (#00,#01)
//9f3b:  f3 3f ff fd             OUTPUT_STREAM   #fffd
//9f3f:  be 13 5f 01 03 07       GET_WIND_PROP   #01,#03 -> L06
//9f45:  0f 00 18 00             LOADW           #00,#18 -> -(SP)
//9f49:  75 07 00 00             SUB             L06,(SP)+ -> -(SP)
//9f4d:  57 00 02 00             DIV             (SP)+,#02 -> -(SP)
//9f51:  34 01 00 02             ADD             #01,(SP)+ -> L01
//9f55:  ef 6f 01 02             SET_CURSOR      #01,L01
//9f59:  f9 17 33 0a 00 01       CALL_VN         16798 (#00,#01)
//9f5f:  a0 05 4f                JZ              L04 [FALSE] 9f6f
//9f62:  57 5f b4 07             DIV             G4f,#b4 -> L06
//9f66:  57 11 b4 00             DIV             G01,#b4 -> -(SP)
//9f6a:  61 07 00 80 a2          JE              L06,(SP)+ [TRUE] a00f
//9f6f:  a0 05 58                JZ              L04 [FALSE] 9f88
//9f72:  a0 56 d5                JZ              G46 [TRUE] 9f88
//9f75:  be 13 5f 01 03 00       GET_WIND_PROP   #01,#03 -> -(SP)
//9f7b:  75 00 56 00             SUB             (SP)+,G46 -> -(SP)
//9f7f:  ef 6f 01 00             SET_CURSOR      #01,(SP)+
//9f83:  da 1f 01 31 01          CALL_2N         a034 (#01)
//9f88:  0d 05 01                STORE           L04,#01
//9f8b:  f3 4f 03 41 29          OUTPUT_STREAM   REDIRECT_ENABLE,#4129
//9f90:  d7 8f 11 05 a0 02       DIV             G01,#05a0 -> L01
//9f96:  a0 02 52                JZ              L01 [FALSE] 9fa9
//9f99:  b2 ...                  PRINT           "St Anne's Day"
//9fa6:  8c 00 33                JUMP            9fda
//9fa9:  41 02 01 52             JE              L01,#01 [FALSE] 9fbd
//9fad:  b2 ...                  PRINT           "St John's Day"
//9fba:  8c 00 1f                JUMP            9fda
//9fbd:  41 02 02 50             JE              L01,#02 [FALSE] 9fcf
//9fc1:  b2 ...                  PRINT           "Christmas Eve"
//9fcc:  8c 00 0d                JUMP            9fda
//9fcf:  b2 ...                  PRINT           "Christmas Day"
//9fda:  b2 ...                  PRINT           ", "
//9fdd:  da 2f 01 49 11          CALL_2N         a094 (G01)
//9fe2:  e5 7f 20                PRINT_CHAR      ' '
//9fe5:  f3 3f ff fd             OUTPUT_STREAM   #fffd
//9fe9:  0f 00 18 56             LOADW           #00,#18 -> G46
//9fed:  be 13 5f 01 03 07       GET_WIND_PROP   #01,#03 -> L06
//9ff3:  0f 00 18 00             LOADW           #00,#18 -> -(SP)
//9ff7:  75 07 00 00             SUB             L06,(SP)+ -> -(SP)
//9ffb:  ef 6f 01 00             SET_CURSOR      #01,(SP)+
//9fff:  d4 1f 41 29 02 07       ADD             #4129,#02 -> L06
//a005:  cf 1f 41 29 00 00       LOADW           #4129,#00 -> -(SP)
//a00b:  fe af 07 00             PRINT_TABLE     L06,(SP)+
//a00f:  f1 7f 00                SET_TEXT_STYLE  ROMAN
//a012:  ef 5f 01 01             SET_CURSOR      #01,#01
//a016:  eb 7f 00                SET_WINDOW      #00
//a019:  2d 9c 17                STORE           G8c,G07
//a01c:  2d a5 57                STORE           G95,G47
//a01f:  46 03 ba c8             JIN             L02,"that" [TRUE] a029
//a023:  2d 80 03                STORE           G70,L02
//a026:  8c 00 05                JUMP            a02c
//a029:  0d 80 00                STORE           G70,#00
//a02c:  2d 5c 50                STORE           G4c,G40
//a02f:  2d 5f 11                STORE           G4f,G01
//a032:  b0                      RTRUE


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

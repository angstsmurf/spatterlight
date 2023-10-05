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

#include "screen.h"
#include "zterp.h"
#include "memory.h"

#include "draw_image.hpp"
#include "arthur.hpp"

extern float imagescalex, imagescaley;

extern uint32_t update_status_bar_address;
extern uint32_t update_picture_address;
extern uint32_t update_inventory_address;
extern uint32_t update_stats_address;
extern uint32_t update_map_address;
extern uint8_t update_global_idx;
extern uint8_t global_map_grid_y_idx;

static bool showing_wide_arthur_room_image = false;
static int arthur_diffx = 0, arthur_diffy = 0;

extern int arthur_text_top_margin;
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

bool is_arthur_room_image(int picnum) {
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
    int mapheight, borderheight;
    get_image_size(137, nullptr, &mapheight); // Get height of map background image
    get_image_size(100, nullptr, &borderheight); // Get height of border measuring dummy image
    float y_margin = mapheight * imagescaley;
//    arthur_text_top_margin = (ceil(y_margin / gcellh) + 1) * gcellh;
//    if (graphics_type == kGraphicsTypeApple2)
//        arthur_text_top_margin -= gcellh;
    arthur_text_top_margin = ceil(y_margin / gcellh) * gcellh;
    arthur_pic_top_margin = arthur_text_top_margin / imagescaley - borderheight - 4;
    if (arthur_pic_top_margin < 0)
        arthur_pic_top_margin = 0;
    fprintf(stderr, "Arthur text top: %d\n", arthur_text_top_margin);
    fprintf(stderr, "Arthur pic top: %d\n", arthur_pic_top_margin);
}

void adjust_arthur_room_image(int picnum, int width, int height, int *x, int *y) {
    int newx = *x, newy = *y;
    if (is_arthur_room_image(picnum)) {
        newx = ceil(((float)gscreenw - (float)width * imagescalex) / 2.0);
        newy = arthur_pic_top_margin;

        if (graphics_type == kGraphicsTypeMacBW) {
            newy = (newy + 12) * imagescaley;
            newx++;
        } else {
            newy = (newy + 8) * imagescaley;
        }
        arthur_diffx = newx - *x;
        arthur_diffy = newy - *y;
    } else if (is_arthur_stamp_image(picnum)) {
        newx = *x + arthur_diffx;
        newy = *y + arthur_diffy;
    }

    *x = newx;
    *y = newy;
}

void draw_arthur_room_image(int picnum) {
    int x, y, width, height;
//    K-PIC-CHURCHYARD=4

    //     K-PIC-PARADE-AREA=17
    // K-PIC-AIR-SCENE=163


    if (picnum == 17 || picnum == 163) {
        get_image_size(picnum, &width, &height);
        showing_wide_arthur_room_image = true;
    } else {
        get_image_size(4, &width, &height);
        showing_wide_arthur_room_image = false;
    }

    //    if (is_arthur_room_image(picnum)) {
    x = ((hw_screenwidth - width) / 2.0);
    y = arthur_pic_top_margin;

    if (graphics_type == kGraphicsTypeMacBW) {
        y += 11;
        x++;
    } else {
        y += 7;
    }
    //    }
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


void draw_arthur_map_image(int picnum, int x, int y) {
    int x_margin;
    get_image_size(100, &x_margin, nullptr);
    x += x_margin;
    if (options.int_number == INTERP_MACINTOSH)
        y--;
    if (graphics_type != kGraphicsTypeApple2 && graphics_type != kGraphicsTypeMacBW){
        if (picnum >= 138)
            y--;
    }
    draw_to_pixmap_unscaled(picnum, x, y);
}

extern std::array<Window, 8> windows;
extern Window *mainwin, *curwin, *upperwin;

// Called on window_changed();
void arthur_window_adjustments(void) {
    adjust_arthur_top_margin();

    // We delete Arthur error window on resize
    if (windows[3].id && windows[3].id->type == wintype_TextGrid) {
        v6_delete_win(&windows[3]);
    }
    if (screenmode != MODE_SLIDESHOW) {
        if (screenmode != MODE_NOGRAPHICS) {
            fprintf(stderr, "adjusting margins in window_change\n");
            int x_margin = 0, y_margin = 0;
            get_image_size(100, &x_margin, &y_margin);

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
                return;
            }

            if (screenmode != MODE_MAP) {
                windows[2].y_size = arthur_text_top_margin;
                windows[2].x_size = upperwin->x_size;
                v6_sizewin(&windows[2]);
            }

            if (screenmode == MODE_NORMAL) {
                internal_call_with_arg(pack_routine(update_picture_address), 1);
                if (!showing_wide_arthur_room_image)
                    draw_arthur_side_images(current_graphics_buf_win);
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

        store_word(header.globals + (update_global_idx * 2), 0);
        internal_call(pack_routine(update_status_bar_address));
    } else {
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

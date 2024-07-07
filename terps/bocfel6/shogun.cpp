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

#include "draw_image.hpp"
#include "entrypoints.hpp"
#include "memory.h"
#include "objects.h"
#include "screen.h"
#include "unicode.h"
#include "v6_shared.hpp"
#include "zterp.h"

#include "shogun.hpp"

extern float imagescalex, imagescaley;

int shogun_graphical_banner_width_left = 0;
int shogun_graphical_banner_width_right = 0;

#define SHOGUN_MAZE_WINDOW windows[3]
#define SHOGUN_MENU_WINDOW windows[2]

//#define P_BORDER 3
//#define P_BORDER2 4

#define P_HINT_LOC 49
//#define P_HINT_BORDER 50

#define P_BORDER_R 59
#define P_BORDER2_R 60

#define P_HINT_BORDER_L 61
#define P_HINT_BORDER_R 62


bool is_shogun_inline_image(int picnum) {
    return (picnum >= 7 && picnum <= 37);
}

bool is_shogun_map_image(int picnum) {
    return (picnum >= 38);
}

bool is_shogun_border_image(int picnum) {
    return ((picnum >= 3 && picnum <= 4) || picnum == 59 || picnum == 50);
}


//void after_V_DEFINE(void) {
//    screenmode = MODE_NORMAL;
//    v6_delete_win(&SHOGUN_MENU_WINDOW);
//    v6_remap_win_to_buffer(&windows[0]);
//}



void GOTO_SCENE(void) {
//    screenmode = MODE_NORMAL;
//    v6_delete_win(&windows[0]);
//    windows[0].id = v6_new_glk_window(wintype_TextBuffer, 0);
//    v6_delete_win(&windows[7]);
//    adjust_shogun_window();
}

// Shogun
//void after_V_HINT(void) {
//    screenmode = MODE_NORMAL;
//    v6_delete_win(&windows[0]);
//    windows[0].id = v6_new_glk_window(wintype_TextBuffer, 0);
//    adjust_shogun_window();
//    //    if (graphics_type == kGraphicsTypeApple2)
//    //        win_setbgnd(V6_STATUS_WINDOW.id->peer, 0xffffff);
//}

#define P_BORDER_LOC 2
#define STATUS_LINES 2

//void after_SETUP_TEXT_AND_STATUS(void) {
//    if (screenmode == MODE_NORMAL) {
////        adjust_shogun_window();
////        if (graphics_type == kGraphicsTypeApple2) {
////            if (V6_STATUS_WINDOW.id == nullptr) {
////                V6_STATUS_WINDOW.id = v6_new_glk_window(wintype_TextGrid);
////            }
////            v6_sizewin(&V6_STATUS_WINDOW);
////            glk_window_clear(V6_STATUS_WINDOW.id);
////        }
//    } else if (screenmode == MODE_HINTS) {
////        V6_STATUS_WINDOW.y_size = 3 * gcellh + 2 * ggridmarginy;
////        v6_delete_win(&windows[0]);
////        v6_remap_win_to_grid(&windows[0]);
////        win_setbgnd(windows[0].id->peer, user_selected_background);
////        if (graphics_type == kGraphicsTypeApple2) {
////            windows[7].y_origin = V6_STATUS_WINDOW.y_size + 1;
////            windows[0].y_origin = windows[7].y_origin + windows[7].y_size;
////            windows[0].y_size = gscreenh - windows[0].y_origin;
////        } else if (graphics_type != kGraphicsTypeMacBW && graphics_type != kGraphicsTypeCGA) {
////            win_maketransparent(V6_STATUS_WINDOW.id->peer);
////        } else if (graphics_type == kGraphicsTypeCGA || graphics_type == kGraphicsTypeAmiga) {
////            V6_STATUS_WINDOW.x_origin = shogun_graphical_banner_width_left;
////            V6_STATUS_WINDOW.x_size = gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right;
////            windows[0].x_origin = V6_STATUS_WINDOW.x_origin;
////            windows[0].x_size = V6_STATUS_WINDOW.x_size;
////            v6_sizewin(&V6_STATUS_WINDOW);
////            v6_sizewin(&windows[0]);
////        }
//    }
//}

void setup_text_and_status(int P) {
    if (P == 0)
        P = P_BORDER_LOC;
    int X, HIGH = gscreenh, WIDE = gscreenw, SLEFT = 0, SHIGH = STATUS_LINES * (gcellh + ggridmarginy);
    if (graphics_type != kGraphicsTypeApple2) {
//        get_image_size(P, &X, nullptr);
//        WIDE -= X * 2;
//        SLEFT += X;
//        v6_define_window(&V6_STATUS_WINDOW, SLEFT * imagescalex, 1, WIDE * imagescalex,  SHIGH);

        v6_define_window(&V6_STATUS_WINDOW, shogun_graphical_banner_width_left, 1, gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right + imagescalex, 2 * (gcellh + ggridmarginy));
    } else { // Apple 2
        int border_height;
        if (P == P_HINT_LOC) {
            get_image_size(P_HINT_BORDER, &X, &border_height);
            set_global(0x37, border_height);
            SHIGH = border_height * imagescaley + 3 * gcellh + 2 * ggridmarginy;
            v6_define_window(&V6_STATUS_WINDOW, SLEFT * imagescalex, 1, WIDE * imagescalex,  SHIGH);
        } else {
            get_image_size(P_BORDER, &X, &border_height);
            set_global(0x37, border_height);
            // Graphics banner window

            v6_define_window(&windows[6], 1, SHIGH + 1, WIDE * imagescalex, border_height * imagescaley);
            v6_define_window(&V6_STATUS_WINDOW, SLEFT * imagescalex, 1, WIDE * imagescalex,  SHIGH);

            SHIGH += border_height * imagescaley;
        }
    }

    if (SHIGH < 0)
        SHIGH = 0;

    v6_define_window(&windows[0], V6_STATUS_WINDOW.x_origin, 1 + SHIGH, V6_STATUS_WINDOW.x_size, HIGH - SHIGH);
}

void SETUP_TEXT_AND_STATUS(void) {
    setup_text_and_status(variable(1));
}


// <ROUTINE SETUP-TEXT-AND-STATUS ("OPT" (P ,P-BORDER-LOC) // Optional argument P, default value 2 (P-BORDER-LOC)
//                                "AUX" X (HIGH <LOWCORE VWRD>) // Local variables X, HIGH (vertical screen height), WIDE, SLEFT, SHIGH
//                                (WIDE <LOWCORE HWRD>) (SLEFT 1) // WIDE is set to screen width
//                                (SHIGH <* ,STATUS-LINES ,FONT-Y>))
// <COND (<NOT <APPLE?>> ;"only apples have no borders" // Account for side images
//       <PICINF .P ,YX-TBL>
//       <SET X <GET ,YX-TBL 1>> // X = width of img P (usually img 2)
//       <SET SLEFT <+ .X .SLEFT>> // SLEF += X
//       <COND (<NOT <EQUAL? .P ,P-BORDER-LOC>> // if (P != 2) SHIGH = height of img P
//              <SET SHIGH <GET ,YX-TBL 0>>)>
//       <SET WIDE <- .WIDE <* .X 2>>> // WIDE -= X * 2
//       <WINDEF ,S-STATUS 1 .SLEFT .SHIGH .WIDE>) // window S-STATUS: top 1 left SLEFT height SHIGH width WIDE
//(ELSE // if APPLE
// <COND (<EQUAL? .P ,P-HINT-LOC> if P == P-HINT-LOC (49) (We are showing hints)
//        <COND (<PICINF ,P-HINT-BORDER ,YX-TBL>
//               <SETG BORDER-HEIGHT <GET ,YX-TBL 0>>)>
//        <SET SHIGH <+ ,BORDER-HEIGHT <* 3 ,FONT-Y>>>
//        <WINDEF ,S-STATUS 1 .SLEFT .SHIGH .WIDE>)
// (ELSE (We are not showing hints)
//  <COND (<PICINF ,P-BORDER ,YX-TBL> // P-BORDER = 3
//         <SETG BORDER-HEIGHT <GET ,YX-TBL 0>>)> BORDER-HEIGHT = height of image 3
//  <WINDEF ,S-BORDER
//  <+ 1 .SHIGH> 1
//  <- .HIGH .SHIGH> .WIDE> // window S-BORDER: top: SHIGH + 1 left: 1 height: HIGH - SHIGH width WIDE
//  <WINDEF ,S-STATUS 1 .SLEFT .SHIGH .WIDE>
//  <SET SHIGH <+ .SHIGH ,BORDER-HEIGHT>>)>)>
// <WINDEF ,S-TEXT
// <+ 1 .SHIGH> .SLEFT
// <- .HIGH .SHIGH> .WIDE>>

#define S_ERASMUS 1
#define S_VOYAGE 6

#define BRIDGE_OF_ERASMUS 0x39
#define GALLEY 0x2b

#define WHEEL 0x010d
#define GALLEY_WHEEL 0x0166

#define SUPPORTERBIT 0x08
#define ONBIT 0x13
#define VEHICLEBIT 0x2c

#define SCORE_FACTOR 5

bool last_was_interlude = false;

void update_status_line(bool interlude) {

    last_was_interlude = interlude;

    set_global(0x85, gcellw); //  Set global DIGIT-WIDTH to width of 0 in pixels
    set_global(0x43, 10); // Set global SCORE-START to width in pixels of string "Score: " + 4 digits

    V6_STATUS_WINDOW.x_cursor = 1;
    V6_STATUS_WINDOW.y_cursor = 1;
    glui32 width;
    winid_t gwin = V6_STATUS_WINDOW.id;
    if (gwin == nullptr) {
        V6_STATUS_WINDOW.id = v6_new_glk_window(wintype_TextGrid);
        gwin = V6_STATUS_WINDOW.id;
        v6_sizewin(&V6_STATUS_WINDOW);
    }
    set_current_window(&V6_STATUS_WINDOW);
    glk_window_get_size(gwin, &width, nullptr);

    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, user_selected_background);
    win_setbgnd(gwin->peer, user_selected_background);
//    win_refresh(gwin->peer, 0, 0);

    garglk_set_reversevideo(1);
    glk_window_clear(gwin);
    glk_window_move_cursor(gwin, 0, 0);
    uint16_t scene = get_global(0x7c);
    uint16_t here = get_global(0x0a);
    if (scene != 0) {
        print_handler(unpack_string(user_word(0xb94e + scene * 2)), nullptr); // table SCENE-NAMES
        glk_put_char(':');
    }
    if (options.int_number != INTERP_MACINTOSH) {
        glk_window_move_cursor(gwin, width / 2 - 3, 0);
        glk_put_string(const_cast<char*>("SHOGUN"));
    }
//    store_word(0x8, word(0x8) & 0xfffb); // set refresh flag to false;
    glk_window_move_cursor(gwin, 0, 1);
    if (here != 0 && !interlude) {
        set_global(0x15, here); // <SETG SHERE ,HERE>
        if (here == GALLEY && (graphics_type == kGraphicsTypeApple2 || graphics_type == kGraphicsTypeAmiga)) {
            glk_put_string(const_cast<char*>("Galley"));
        } else {
            print_object(here, nullptr);
        }
        uint16_t player_object = get_global(0xbf);
        int16_t tmp = internal_get_parent(player_object);
        if (internal_test_attr(tmp, VEHICLEBIT)) {
            set_global(0x15, tmp);
            if (internal_test_attr(tmp, SUPPORTERBIT)) {
                glk_put_string(const_cast<char*>(", on "));
            } else {
                glk_put_string(const_cast<char*>(", in "));
            }
            internal_call_with_arg(pack_routine(0x10ba8), tmp); //  <TELL THE .TMP>
        }

        if ((here == BRIDGE_OF_ERASMUS && internal_test_attr(WHEEL, ONBIT)) || (here == GALLEY && internal_test_attr(GALLEY_WHEEL, ONBIT))) {
            glk_put_string(const_cast<char*>("; course "));
            internal_call_with_arg(pack_routine(0x19d1c), get_global(0x44));
            glk_put_string(const_cast<char*>("; wheel "));
            internal_call_with_arg(pack_routine(0x19d1c), get_global(0xcc));
        }

    } else if (interlude) {
        glk_put_string(const_cast<char*>("Interlude"));
    }

    glk_window_move_cursor(gwin, width - get_global(0x43), 0);
    glk_put_string(const_cast<char*>("Score:"));
    int16_t tmp = get_global(0x66) * SCORE_FACTOR;
    print_right_justified_number(tmp);
    glk_window_move_cursor(gwin, width - get_global(0x43), 1);
    glk_put_string(const_cast<char*>("Moves:"));
    print_right_justified_number(get_global(0xe5));
    set_current_window(&windows[0]);
}

void shogun_UPDATE_STATUS_LINE(void) {
    update_status_line(false);
}

//void init_status_line(bool no_sts) {
//    glk_window_clear(windows[0].id);
//    if (no_sts) {
//        setup_text_and_status(0);
//    }
//    set_current_window(&V6_STATUS_WINDOW);
//    set_global(0x85, gcellw); //  Set global DIGIT-WIDTH to width of 0 in pixels
//    set_global(0x43, 11); // Set global SCORE-START to width in pixels of string "Score: " + 4 digits
//    garglk_set_reversevideo(1);
//    glk_window_clear(V6_STATUS_WINDOW.id);
//    update_status_line();
//}

void interlude_status_line(void) {
    if (graphics_type != kGraphicsTypeApple2) {
        if (find_image(P_BORDER2)) {
            shogun_display_border(P_BORDER2);
        } else if (find_image(P_BORDER)) {
            shogun_display_border(P_BORDER);
        }
    }
//    int left = (graphics_type == kGraphicsTypeMacBW) ? 0 : 2;
//    set_global(0x15, 0);
//    set_current_window(&V6_STATUS_WINDOW);
//    garglk_set_reversevideo(1);
//    glk_window_move_cursor(V6_STATUS_WINDOW.id, left, 0);
    update_status_line(true);
//    glk_window_move_cursor(V6_STATUS_WINDOW.id, 0, 1);
//    set_current_window(&V6_STATUS_WINDOW);
//    glk_put_string(const_cast<char*>("Interlude"));
//    set_current_window(&windows[0]);
}


void INTERLUDE_STATUS_LINE(void) {
    if (graphics_type != kGraphicsTypeApple2) {
        if (find_image(P_BORDER2)) {
            shogun_display_border(P_BORDER2);
        } else if (find_image(P_BORDER)) {
            shogun_display_border(P_BORDER);
        }
    }
    update_status_line(true);
}

#pragma mark SHOGUN MENU

void display_menu_line(uint16_t menu, uint16_t line, bool reverse) {
    if (line < 1)
        line = 1;
    glk_window_move_cursor(SHOGUN_MENU_WINDOW.id, 0, line - 1);
    SHOGUN_MENU_WINDOW.x_cursor = 1;
    SHOGUN_MENU_WINDOW.y_cursor = (line - 1) * gcellh + 1;

    if (reverse)
        garglk_set_reversevideo(1);
    uint16_t string_address = user_word(menu + line * 2);
    uint8_t length = user_byte(string_address);
    uint16_t chr = string_address + 1;
    for (int i = 0; i < length; i++)
        put_char(user_byte(chr++));
    if (reverse)
        garglk_set_reversevideo(0);
}

void select_line(uint16_t menu, uint16_t line) {
    display_menu_line(menu, line, true);
}

void unselect_line(uint16_t menu, uint16_t line) {
    display_menu_line(menu, line, false);
}

// Returns width of the longest menu entry in characters
uint16_t menu_width(uint16_t MENU) {
    uint16_t num_items = user_word(MENU);
    uint16_t string_start;
    uint16_t longest = 0;
    for (int i = 1; i <= num_items; i++) {
        string_start = user_word(MENU + i * 2);
        uint16_t string_length = user_byte(string_start);
        if (string_length > longest)
            longest = string_length;
    }
    return longest;
}

#define PART_MENU 0x4abe
#define CONTINUE_MENU 0x4e26
#define CONTINUE_AND_HINT_MENU 0x4e2e

uint16_t current_menu_selection;
uint16_t current_menu = PART_MENU;

#define MAX_QUICKSEARCH 25

void print_quicksearch(char *typed) {
    glk_set_window(windows[5].id);
    glk_window_move_cursor(windows[5].id, 0, 1);
    for (int i = 0; i < MAX_QUICKSEARCH; i++)
        glk_put_char(UNICODE_SPACE);
    glk_window_move_cursor(windows[5].id, 0, 1);
    glk_put_string(typed);
    glk_set_window(SHOGUN_MENU_WINDOW.id);
}

// Quick search function
int quicksearch(uint16_t chr_to_find, char *typed_letters, int type_pos) {
    chr_to_find = tolower(chr_to_find);
    if (type_pos < 0 || type_pos >= MAX_QUICKSEARCH || chr_to_find > 'z' || chr_to_find < 'a')
        return 0;
    typed_letters[type_pos] = chr_to_find;
    typed_letters[type_pos + 1] = 0;
    uint16_t cnt = user_word(current_menu); // first word of table is number of elements
    for (int i = 1; i <= cnt; i++) {
        uint16_t string_address = user_word(current_menu + i * 2);
        uint8_t length = user_byte(string_address);
        if (type_pos < length && type_pos >= 0) {
            bool found = true;
            for (int j = type_pos; j >= 0; j--) {
                uint8_t chr_in_string = tolower(user_byte(string_address + j + 1));
                if (chr_in_string != typed_letters[j]) {
                    found = false;
                    break;
                }
            }
            if (found) {
                print_quicksearch(typed_letters);
                return i;
            }
        }
    }
    return 0;
}

void update_menu(void) {
    uint16_t cnt = user_word(current_menu); // first word of table is number of elements
    uint16_t width = menu_width(current_menu) * gcellw + 2 * ggridmarginx;
    uint16_t height = cnt * gcellh + 2 * ggridmarginy;
    v6_define_window(&windows[0], shogun_graphical_banner_width_left, windows[0].y_origin, gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right,  gscreenh - windows[0].y_origin - height);
    int menutop = gscreenh - height;
    if (graphics_type == kGraphicsTypeApple2 && current_menu == PART_MENU) {
        menutop -= a2_graphical_banner_height;
    }
    v6_define_window(&SHOGUN_MENU_WINDOW, (gscreenw - width) / 2, menutop, width, height);

    int left_margin;
    get_image_size(P_BORDER_LOC, &left_margin, nullptr);

    v6_define_window(&windows[5], round(left_margin * imagescalex), SHOGUN_MENU_WINDOW.y_origin, gscreenw - ceil(left_margin * 2.0 * imagescalex) + 2, height);

    set_current_window(&SHOGUN_MENU_WINDOW);

    for (int i = 1; i <= cnt; i++) {
        if (i == current_menu_selection)
            select_line(current_menu, i);
        else
            unselect_line(current_menu, i);
    }
}

int menu_select(uint16_t menu, uint16_t ypos, uint16_t selection) {
    if (selection == 0)
        selection = 1;
    current_menu = menu;
    current_menu_selection = selection;
    uint16_t cnt = user_word(menu); // first word of table is number of elements

    v6_delete_win(&SHOGUN_MENU_WINDOW);
    SHOGUN_MENU_WINDOW.id = v6_new_glk_window(wintype_TextGrid);
    
    update_menu();
    set_current_window(&SHOGUN_MENU_WINDOW);
    glk_request_mouse_event(SHOGUN_MENU_WINDOW.id);
    SHOGUN_MENU_WINDOW.style.reset(STYLE_REVERSE);

    uint16_t mouse_click_addr = header.extension_table + 2;
    int16_t y, new_selection;
    int type_pos = 0;
    char typed_letters[MAX_QUICKSEARCH + 1];
    flush_image_buffer();
    bool finished = false;
    while (!finished) {
        uint16_t chr = read_char();
        int width = menu_width(current_menu);
        int x;
        switch (chr) {
            case ZSCII_CLICK_SINGLE:
            case ZSCII_CLICK_DOUBLE:
                x = word(mouse_click_addr) - 1;
                y = word(mouse_click_addr + 2);
                if (x > 0 && y > 0 && x < width && y <= cnt) {
                    unselect_line(menu,
                                  selection);
//                    if (y == selection)
//                        chr = ZSCII_CLICK_DOUBLE;
                    selection = y;
                    if (chr == ZSCII_CLICK_DOUBLE) {
                        finished = true;
                    }
                    select_line(menu, selection);
                }
                break;
            case ZSCII_BACKSPACE:
                if (type_pos > 0) {
                    type_pos--;
                    typed_letters[type_pos] = 0;
                    print_quicksearch(typed_letters);
                } else {
                    win_beep(1);
                }
                break;
            case ZSCII_UP:
            case ZSCII_LEFT:
            case ZSCII_KEY8:
            case ZSCII_KEY4:
                unselect_line(menu,
                              selection);
                if (selection > 1)
                    selection--;
                else
                    selection = cnt;
                select_line(menu,
                            selection);
                break;
            case ZSCII_DOWN:
            case ZSCII_RIGHT:
            case ZSCII_KEY2:
            case ZSCII_KEY6:
            case ZSCII_SPACE:
                unselect_line(menu,
                              selection);
                if (selection < cnt)
                    selection++;
                else
                    selection = 1;
                select_line(menu,
                            selection);
                break;
            case ZSCII_NEWLINE:
            case UNICODE_LINEFEED:
                finished = true;
                break;
            default:
                if (type_pos < MAX_QUICKSEARCH && ((chr >= 'a' && chr <= 'z') ||
                    (chr >= 'A' && chr <= 'Z'))) {
                    new_selection = quicksearch(chr, typed_letters, type_pos);
                    if (new_selection != 0) {
                        unselect_line(menu,
                                      selection);
                        selection = new_selection;
                        select_line(menu,
                                    selection);
                        type_pos++;
                        break;
                    }
                }
                win_beep(1);
                break;
        }
        current_menu_selection = selection;
    }
    v6_delete_win(&SHOGUN_MENU_WINDOW);
    set_current_window(&V6_TEXT_BUFFER_WINDOW);
    return selection;
}

uint16_t current_menu_message;
#define CURRENT_BORDER 0x14

int get_from_menu(uint16_t MSG, uint16_t MENU, uint16_t FCN, int DEF) {
    screenmode = MODE_SHOGUN_MENU;
    if (DEF == 0)
        DEF = 1;
    if (MSG == 0) {
        fprintf(stderr, "Wut");
    }
    current_menu_message = MSG;
    current_menu = MENU;
    uint16_t L = user_word(MENU); // Number of entries
    set_global(0x9e, 1); // Set RESTORED? to true
    uint16_t menuheight = L * gcellh + 2 * ggridmarginy;
    shogun_display_border((ShogunBorderType)get_global(CURRENT_BORDER));
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_ReverseColor, 0);
    v6_delete_win(&windows[5]);
    windows[5].id = v6_new_glk_window(wintype_TextGrid);

    int left_margin;
    get_image_size(P_BORDER_LOC, &left_margin, nullptr);

    v6_define_window(&windows[5], round(left_margin * imagescalex), SHOGUN_MENU_WINDOW.y_origin, gscreenw - ceil(left_margin * 2.0 * imagescalex) + 2, menuheight);
    windows[5].x_cursor = 1;
    windows[5].y_cursor = 1;

    uint16_t result = 0;
    while (result == 0) {
        set_current_window(&windows[5]);
        glk_window_clear(windows[5].id);
        print_handler(unpack_string(MSG), nullptr);
        result = menu_select(MENU, gscreenh - menuheight, DEF);
        if (result < 0)
            return result;
        if (user_word(0x8) & 1) { // Is transcripting on?
            output_stream(-1, 0);
            put_char(ZSCII_SPACE);
            display_menu_line(MENU, result, false);
            put_char(ZSCII_NEWLINE);
            put_char(ZSCII_NEWLINE);
            output_stream(1, 0);
        }

        uint16_t string_2 = user_word(MENU + 4);
        if (result == 2 && user_byte(string_2) == 24) {
           bool success = super_hacky_shogun_menu_save(SaveType::Normal, SaveOpcode::None);
            if (success == false) {
                fprintf(stderr, "Save failed\n");
            }
            result = 0;
        } else {
            result = internal_call_with_arg(FCN, result);
        }
        set_global(0x9e, 0); // Set RESTORED? to false
    }
    v6_delete_win(&windows[5]);
    v6_define_window(&V6_TEXT_BUFFER_WINDOW, round(left_margin * imagescalex), V6_TEXT_BUFFER_WINDOW.y_origin, gscreenw - ceil(left_margin * 2.0 * imagescalex) + 2, gscreenh - V6_TEXT_BUFFER_WINDOW.y_origin);
    screenmode = MODE_NORMAL;
    if (graphics_type == kGraphicsTypeApple2) {
        shogun_display_border((ShogunBorderType)get_global(CURRENT_BORDER));
    }
    return result;
}

void GET_FROM_MENU(void) {
    store_variable(1, get_from_menu(variable(1), variable(2), variable(3), variable(4)));
}

void SCENE_SELECT(void) {
    uint16_t result = 0;
    while (result == 0) {
        result = get_from_menu(0x48, 0x4abe, 0x1277, 1);
    }
}

void V_VERSION(void) {
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
    centeredText = true;
    set_current_style();
    screenmode = MODE_CREDITS;
}

// Checks whether we are running on an original
// black and white Macintosh or a later one with
// color (Mac II.)
// The original returns true if interpreter version is Macintosh
// and screen width is 640.
// Used in V-COLOR and DO-COLOR.
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

void shogun_display_border(ShogunBorderType border) {
    if (border == 0) {
        border = (ShogunBorderType)get_global(CURRENT_BORDER);
        if (border == 0) {
            border = P_BORDER;
            set_global(CURRENT_BORDER, P_BORDER);
        }
    } else if (border != P_HINT_BORDER) {
        set_global(CURRENT_BORDER, border);
    }

    // Delete covering graphics window (which would show the title screen)
    if (current_graphics_buf_win != graphics_win_glk && current_graphics_buf_win != nullptr) {
        v6_delete_glk_win(current_graphics_buf_win);
    }

    current_graphics_buf_win = graphics_win_glk;
    if (windows[7].id != current_graphics_buf_win) {
        v6_delete_win(&windows[7]);
        windows[7].id = current_graphics_buf_win;
        v6_define_window(&windows[7], 1, 1, gscreenw, gscreenh);
    }
    glk_request_mouse_event(current_graphics_buf_win);

    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    int border_top = 0;
    if (graphics_type == kGraphicsTypeApple2 && !(screenmode == MODE_SHOGUN_MENU && current_menu == PART_MENU)) {
        border_top = V6_STATUS_WINDOW.y_size / imagescaley;
    }
    draw_to_pixmap_unscaled(border, 0, border_top);

    int16_t BR = -1;
    int16_t BL = -1;
    switch(border) {
        case kShogunHintBorder:
            BL = P_HINT_BORDER_L;
            BR = P_HINT_BORDER_R;
            break;
        case P_BORDER:
            BR = P_BORDER_R;
            break;
        case P_BORDER2:
            BR = P_BORDER2_R;
            break;
    }

    int Y = 0;
    int width, height;

    if (graphics_type != kGraphicsTypeApple2) {
        if (border == kShogunHintBorder) {
            get_image_size(P_HINT_LOC, &width, &height);
        } else {
            get_image_size(P_BORDER_LOC, &width, &height);
        }
        shogun_graphical_banner_width_left = shogun_graphical_banner_width_right = round(width * imagescalex);
        a2_graphical_banner_height = 0;
    } else { // Using Apple 2 graphics
        get_image_size(border, nullptr, &a2_graphical_banner_height);
        if (screenmode == MODE_SHOGUN_MENU && current_menu == PART_MENU) {
            draw_to_pixmap_unscaled(border, 0, gscreenh / imagescaley - a2_graphical_banner_height);
        }
        a2_graphical_banner_height *= imagescaley;
    }
//        else if (graphics_type == kGraphicsTypeMacBW) {
//            shogun_graphical_banner_width_left = imagescalex * 44;
//            shogun_graphical_banner_width_right = imagescalex * 45;
//        } else {
//            shogun_graphical_banner_width_left = imagescalex * 23.4;
//            shogun_graphical_banner_width_right = imagescalex * 22.6;
//        }
//    }


    if (find_image(BL)) {
        get_image_size(BL, &width, &height);
        draw_to_pixmap_unscaled(BL, 0, Y);
        draw_to_pixmap_unscaled(BL, 0, Y + height);
        draw_to_pixmap_unscaled(BR, hw_screenwidth - width, Y);
        draw_to_pixmap_unscaled(BR, hw_screenwidth - width, Y + height);
        return;
    } else if (graphics_type == kGraphicsTypeAmiga || graphics_type == kGraphicsTypeMacBW) {
        get_image_size(border, &width, &height);
        if (border == P_BORDER) {
            if (graphics_type == kGraphicsTypeMacBW) {
                draw_to_pixmap_unscaled_flipped(border, 0, height);
            } else {
                draw_to_pixmap_unscaled_flipped(border, 0, height - 2);
            }
        } else if (border == P_BORDER2) {
            if (graphics_type == kGraphicsTypeMacBW) {
                draw_to_pixmap_unscaled(border, 0, height - 35);
            } else {
                draw_to_pixmap_unscaled(border, 0, height - 22);
            }
        }
    }

    if (find_image(BR)) {
        get_image_size(BR, &width, &height);
        if (graphics_type == kGraphicsTypeCGA) {
            height -= 7;
        }
        draw_to_pixmap_unscaled_flipped(border, hw_screenwidth - width, Y + height);
        draw_to_pixmap_unscaled(BR, hw_screenwidth - width, Y);
        draw_to_pixmap_unscaled_flipped(BR, 0, Y + height);
        if (graphics_type == kGraphicsTypeCGA) {
            draw_to_pixmap_unscaled(border, 0, 0);
        }
    }
}


void shogun_DISPLAY_BORDER(void) {
    if (screenmode != MODE_HINTS && screenmode != MODE_SHOGUN_MENU)
        screenmode = MODE_NORMAL;
    shogun_display_border((ShogunBorderType)variable(1));
}

void CENTER_PIC_X(void) {
    float inline_scale = 2.0;
    if (graphics_type == kGraphicsTypeMacBW)
        inline_scale = imagescalex;
    glk_set_style(style_User1);
    draw_inline_image(V6_TEXT_BUFFER_WINDOW.id, variable(1), imagealign_InlineCenter, variable(1), inline_scale, false);
    glk_set_style(style_Normal);
    glk_put_char(UNICODE_LINEFEED);
    add_margin_image_to_list(variable(1));
}

#define P_CREST 31

// Only used for final P-CREST image
void CENTER_PIC(void) {
    if (windows[4].id != nullptr && windows[4].id->type != wintype_Graphics) {
        v6_delete_win(&windows[4]);
    }
    if (windows[4].id == nullptr) {
        windows[4].id = v6_new_glk_window(wintype_Graphics);
    }
    int width, height;
    get_image_size(P_CREST, &width, &height);
    windows[4].y_size = height * imagescaley + 2 * gcellh;
    windows[4].x_size = V6_TEXT_BUFFER_WINDOW.x_size;
    windows[4].x_origin = V6_TEXT_BUFFER_WINDOW.x_origin;
    V6_TEXT_BUFFER_WINDOW.y_origin = V6_STATUS_WINDOW.y_size + windows[4].y_size;
    V6_TEXT_BUFFER_WINDOW.y_size = gscreenh - V6_TEXT_BUFFER_WINDOW.y_origin;
    glk_window_set_background_color(windows[4].id, user_selected_background);
    glk_window_clear(windows[4].id);
    v6_sizewin(&windows[4]);
    width *= imagescalex;
    height *= imagescaley;

    draw_inline_image(windows[4].id, P_CREST, (windows[4].x_size - width) / 2, (windows[4].y_size - height) / 2, imagescaley, false);
    v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
}

void MARGINAL_PIC(void) {
    if (variable(1) == 0)
        return;
    bool right = true;
    int picnum = variable(1);
    if (internal_arg_count() > 1) {
        right = (variable(2) == 1);
    }

    int width;
    get_image_size(picnum, &width, nullptr);
    float inline_scale = 2.0;
    if (graphics_type == kGraphicsTypeMacBW)
        inline_scale = imagescalex;

    if (width * inline_scale > V6_TEXT_BUFFER_WINDOW.x_size) {
        inline_scale = (float)V6_TEXT_BUFFER_WINDOW.x_size / width;
    }

    draw_inline_image(V6_TEXT_BUFFER_WINDOW.id, picnum, right ? imagealign_MarginRight : imagealign_MarginLeft, picnum, inline_scale, false);
    add_margin_image_to_list(picnum);

}

#define P_MAZE_BACKGROUND 44
#define P_MAZE_BOX 45

#define XOFFSET 0xad
#define YOFFSET 0x62

#define MAZE_WIDTH 37
#define MAZE_HEIGHT 17

#define MAZE_BOX_TABLE 0x4f86

void display_maze_pic(int pic, int x, int y) {
    if (pic != 0) {
        int bx = user_word(MAZE_BOX_TABLE + 2);
        int by = user_word(MAZE_BOX_TABLE);
        int offx = get_global(XOFFSET);
        int offy = get_global(YOFFSET);
        draw_to_pixmap_unscaled(pic, offx + x * bx, offy + y * by);
    }
 }

void DISPLAY_MAZE_PIC(void) {
    display_maze_pic(variable(1), variable(3), variable(2));
}

void print_maze(void) {
    int offs = 0;
    uint16_t MAZE_MAP = get_global(0x53);
    int offx = get_global(XOFFSET);
    int offy = get_global(YOFFSET);
    draw_to_pixmap_unscaled(P_MAZE_BACKGROUND, offx, offy);
    for (int y = 0; y < MAZE_HEIGHT; y++) {
        for (int x = 0; x < MAZE_WIDTH; x++, offs++) {
            int M = user_byte(MAZE_MAP + offs) & 0x7f;
            display_maze_pic(M, x, y);
        }
    }
}

void display_maze(bool clear) {
    screenmode = MODE_SHOGUN_MAZE;
    int height;
    get_image_size(P_MAZE_BACKGROUND, nullptr, &height);
    int maze_height = height * imagescaley;

    long status_height = V6_STATUS_WINDOW.y_size;
    long y = status_height + V6_STATUS_WINDOW.y_origin;

    long textwin_height = gscreenh - status_height;
    v6_define_window(&V6_TEXT_BUFFER_WINDOW, V6_TEXT_BUFFER_WINDOW.x_origin, y + maze_height, V6_TEXT_BUFFER_WINDOW.x_size, textwin_height - maze_height);

    if (clear)
        glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);

    int BY, BX;
    get_image_size(P_MAZE_BOX, &BX, &BY);
    store_word(MAZE_BOX_TABLE, BY);
    store_word(MAZE_BOX_TABLE + 2, BX);

    int border_width;
    get_image_size(P_BORDER_LOC, &border_width, nullptr);

    set_global(XOFFSET, border_width);
    set_global(YOFFSET, status_height / imagescaley);

    SHOGUN_MAZE_WINDOW.x_origin = 0;
    SHOGUN_MAZE_WINDOW.y_origin = 0;

    print_maze();
}

void DISPLAY_MAZE(void) {
    display_maze(true);
}

#define NORTH 0x4f64
#define SOUTH 0x4f6a
#define EAST 0x4f70
#define WEST 0x4f75

int maze_mouse_f(void) {
    int16_t WX, WY;
    uint16_t DIR = 0;
    int BX, BY;
    get_image_size(P_MAZE_BOX, &BX, &BY);
    uint16_t X = get_global(0x0b);
    uint16_t Y = get_global(0xb1);
    WY = get_global(YOFFSET) + Y * BY + BY / 2;
    WX = get_global(XOFFSET) + X * BX + BX / 2;

    uint16_t mouse_click_addr = header.extension_table + 2;
    int16_t mouse_x = word(mouse_click_addr) - 1;
    int16_t mouse_y = word(mouse_click_addr + 2) - 1;

    WY = mouse_y - WY;
    WX = mouse_x - WX;

    if (WX >= 0) { // right side
        if (WY < 0) { // top right
            WY = -WY;
            if (WX > WY) {
                DIR = EAST;
            } else {
                DIR = NORTH;
            }
        } else { // bottom right
            if (WX > WY) {
                DIR = EAST;
            } else {
                DIR = SOUTH;
            }
        }
    } else if (WY < 0) { // top left
        WY = -WY;
        WX = -WX;
        if (WX > WY) {
            DIR = WEST;
        } else {
            DIR = NORTH;
        }
    } else { // bottom left
        WX = -WX;
        if (WX > WY) {
            DIR = WEST;
        } else {
            DIR = SOUTH;
        }
    }
    if (WX <= BX / 2 && WY <= BY / 2)
        return 0;
    if (DIR != 0) {
        internal_call_with_arg(pack_routine(0x11698), DIR);
        glk_put_char(UNICODE_LINEFEED);
        return 13;
    }
    return 0;
}

void MAZE_MOUSE_F(void) {
    store_variable(1, maze_mouse_f());
}

void adjust_shogun_window(void) {
    V6_TEXT_BUFFER_WINDOW.x_cursor = 0;
    V6_TEXT_BUFFER_WINDOW.y_cursor = 0;
    V6_TEXT_BUFFER_WINDOW.x_size = gscreenw;

    if (graphics_type == kGraphicsTypeApple2) {
//        screenmode = MODE_NORMAL;
        V6_STATUS_WINDOW.y_size = 2 * (gcellh + ggridmarginy);
        v6_sizewin(&V6_STATUS_WINDOW);
        windows[6].y_origin = V6_STATUS_WINDOW.y_size + 1;
        windows[6].y_size = a2_graphical_banner_height;
        v6_sizewin(&windows[6]);
        V6_TEXT_BUFFER_WINDOW.y_origin = windows[6].y_origin + windows[6].y_size;
        V6_TEXT_BUFFER_WINDOW.y_size = gscreenh - V6_TEXT_BUFFER_WINDOW.y_origin;
    } else {
        if (V6_STATUS_WINDOW.id == nullptr)
            V6_STATUS_WINDOW.id = v6_new_glk_window(wintype_TextGrid);
        v6_define_window(&V6_STATUS_WINDOW, shogun_graphical_banner_width_left, 1, gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right + imagescalex, 2 * (gcellh + ggridmarginy));
        V6_STATUS_WINDOW.fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
        V6_STATUS_WINDOW.bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
        V6_STATUS_WINDOW.style.reset(STYLE_REVERSE);
        glk_window_set_background_color(V6_TEXT_BUFFER_WINDOW.id, user_selected_foreground);
        V6_TEXT_BUFFER_WINDOW.fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
        V6_TEXT_BUFFER_WINDOW.bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
        win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);
        V6_TEXT_BUFFER_WINDOW.x_origin = V6_STATUS_WINDOW.x_origin;
        V6_TEXT_BUFFER_WINDOW.x_size = V6_STATUS_WINDOW.x_size;
    }
    v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

void shogun_draw_title_image(void) {
    Window *win = &windows[7];
    winid_t gwin = win->id;
    if (gwin == nullptr) {
        win->id = v6_new_glk_window(wintype_Graphics);
        gwin = win->id;
        glk_request_mouse_event(gwin);
    }
    v6_define_window(win, 1, 1, gscreenw, gscreenh);
    int width, height;
    get_image_size(1, &width, &height);
    float scale = gscreenw / ((float)width * pixelwidth);
    int ypos = gscreenh - height * scale + 1;
    if (graphics_type == kGraphicsTypeMacBW)
        ypos += 2;
    glk_window_set_background_color(gwin, monochrome_white);
    glk_window_clear(gwin);
    draw_inline_image(gwin, kShogunTitleImage, 0, ypos, scale, false);
    screenmode = MODE_SLIDESHOW;
}

void shogun_update_after_restore(void) {
    v6_delete_win(&windows[5]);
    v6_delete_win(&SHOGUN_MENU_WINDOW);
    adjust_shogun_window();
}

void shogun_update_on_resize(void) {
    // Window 0: (S-TEXT) Text buffer, "main" window
    // Window 1: (S-STATUS) Status
    // Window 2: (MENU-WINDOW) Menu grid, centered on screen. We put it at the bottom, as
    // we can't line windows up with text in a buffer window
    // Window 3: (MAZE-WINDOW) Maze. This has no corresponding Glk window.
    // All maze graphics are actually drawn to the "full screen" background window (7).
    // Window 4: Top graphics window used for ending P-CREST image.
    // Window 5: Menu background, used for "You may choose to:" messages
    // Window 6: (S-BORDER) Horizontal Apple 2 graphic border
    // Window 7: (S-FULL) Full screen background, used by us for border graphics and maze

    set_global(0x33, options.int_number); // GLOBAL MACHINE

    set_global(0x1f, byte(0x2d)); // GLOBAL FG-COLOR
    set_global(0xd7, byte(0x2c)); // GLOBAL BG-COLOR

    set_global(0xa2, gcellw); // GLOBAL FONT-X
    set_global(0xe9, gcellh); // GLOBAL FONT-Y

    if (screenmode == MODE_DEFINE) {
        resize_definitions_window();
        return;
    } else if (screenmode == MODE_HINTS) {
        setup_text_and_status(P_HINT_LOC);
        redraw_hint_screen_on_resize();
        return;
    } else if (screenmode == MODE_SLIDESHOW) {
        if (current_picture == kShogunTitleImage) {
            shogun_draw_title_image();
            return;
        }
        if (SHOGUN_MAZE_WINDOW.id == nullptr) {
            SHOGUN_MAZE_WINDOW.id = v6_new_glk_window(wintype_Graphics);
        }
        SHOGUN_MAZE_WINDOW.x_size = gscreenw;
        SHOGUN_MAZE_WINDOW.y_size = gscreenh;

        v6_sizewin(&SHOGUN_MAZE_WINDOW);
    } else {
        shogun_display_border((ShogunBorderType)get_global(CURRENT_BORDER));
        if (V6_TEXT_BUFFER_WINDOW.id) {
            refresh_margin_images();
            float yscalefactor = 2.0;
            if (graphics_type == kGraphicsTypeMacBW)
                yscalefactor = imagescalex;
            float xscalefactor = yscalefactor * pixelwidth;
            win_refresh(V6_TEXT_BUFFER_WINDOW.id->peer, xscalefactor, yscalefactor);
            win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);
        }
        if (V6_TEXT_BUFFER_WINDOW.id != nullptr && !(current_menu == PART_MENU && screenmode == MODE_SHOGUN_MENU)) {
            setup_text_and_status(P_BORDER_LOC);
            update_status_line(last_was_interlude);
        }
        if (screenmode == MODE_SHOGUN_MENU) {
            update_menu();
        } else {
            adjust_shogun_window();
            if (screenmode == MODE_SHOGUN_MAZE) {
                display_maze(false);
            }
        }
        if (windows[4].id != nullptr) {
            CENTER_PIC();
        }
        flush_image_buffer();
    }
}

void shogun_display_inline_image(glui32 align) {
    float inline_scale = 2.0;
    if (graphics_type == kGraphicsTypeMacBW)
        inline_scale = imagescaley;
    draw_inline_image(V6_TEXT_BUFFER_WINDOW.id, current_picture, align, current_picture, inline_scale, false);
    add_margin_image_to_list(current_picture);
}


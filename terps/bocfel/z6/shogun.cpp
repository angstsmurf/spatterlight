//
//  shogun.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-21.
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

#include "shogun.hpp"

extern float imagescalex, imagescaley;

static int shogun_banner_width_left = 0;


uint16_t current_menu_selection = 0;
uint16_t current_menu;
ShogunBorderType current_border;

static int maze_offset_x = 0;
static int maze_offset_y = 0;
static int maze_box_width = 0;
static int maze_box_height = 0;

ShogunGlobals sg;
ShogunRoutines sr;
ShogunTables st;
ShogunMenus sm;
ShogunObjects so;

#define SHOGUN_MAZE_WINDOW windows[3]
#define SHOGUN_MENU_WINDOW windows[2]
#define SHOGUN_MENU_BG_WIN windows[5]
#define SHOGUN_CREST_WINDOW windows[4]
#define SHOGUN_A2_BORDER_WIN windows[6]

#define P_BORDER_R 59
#define P_BORDER2_R 60

#define P_HINT_BORDER_L 61
#define P_HINT_BORDER_R 62

#define P_BORDER_LOC 2
#define STATUS_LINES 2

#define S_ERASMUS 1
#define S_VOYAGE 6

#define SUPPORTERBIT 0x08
#define ONBIT 0x13
#define VEHICLEBIT 0x2c

#define SCORE_FACTOR 5

#define P_MAZE_BACKGROUND 44
#define P_MAZE_BOX 45

// This should just call shogun_update_on_resize()
static void setup_text_and_status(int P) {

    if (screenmode == MODE_SHOGUN_MAZE && P != P_BORDER_LOC) {
        screenmode = MODE_NORMAL;
        image_needs_redraw = true;
        shogun_display_border(current_border);
    }

    if (P == 0)
        P = P_BORDER_LOC;

    int status_height = get_global(sg.SCENE) == 0 ? 0 : 2 * (gcellh + ggridmarginy);

    int SHIGH = status_height;

    int X, HIGH = gscreenh, WIDE = gscreenw, SLEFT = 0, border_height;

    if (graphics_type != kGraphicsTypeApple2) {
        v6_define_window(&V6_STATUS_WINDOW, shogun_banner_width_left, 1, gscreenw - shogun_banner_width_left * 2, status_height);
    } else { // Apple 2
        shogun_banner_width_left = 0;
        if (P == P_HINT_LOC) {
            get_image_size(P_HINT_BORDER, &X, &border_height);
            SHIGH = border_height * imagescaley + 3 * gcellh + 2 * ggridmarginy;
            v6_define_window(&V6_STATUS_WINDOW, SLEFT * imagescalex, 1, WIDE * imagescalex,  SHIGH);
        } else {
            get_image_size(P_BORDER, &X, &border_height);
            if (get_global(sg.SCENE) == 0)
                border_height = 0;
            // Graphics banner window
            v6_define_window(&SHOGUN_A2_BORDER_WIN, 1, status_height + 1, WIDE * imagescalex, border_height * imagescaley);
            v6_define_window(&V6_STATUS_WINDOW, SLEFT * imagescalex, 1, WIDE * imagescalex,  status_height);

            SHIGH += border_height * imagescaley;
        }
    }

    if (SHIGH < 0)
        SHIGH = 0;

    if (SHOGUN_MENU_BG_WIN.id)
        HIGH -= SHOGUN_MENU_BG_WIN.x_size;

    v6_define_window(&V6_TEXT_BUFFER_WINDOW, V6_STATUS_WINDOW.x_origin, imagescaley * 2 + SHIGH, V6_STATUS_WINDOW.x_size, HIGH - SHIGH);
}

void SETUP_TEXT_AND_STATUS(void) {
    setup_text_and_status(variable(1));
}


static bool last_was_interlude = false;
static int a2_graphical_banner_height = 0;

static void update_status_line(bool interlude) {
    last_was_interlude = interlude;

    int status_height = (gcellh + ggridmarginy) * 2;

    uint16_t scene = get_global(sg.SCENE);
    if (scene == 0) {
        V6_TEXT_BUFFER_WINDOW.y_origin = a2_graphical_banner_height;
        V6_TEXT_BUFFER_WINDOW.y_size = gscreenh - 2 * a2_graphical_banner_height;
        if (SHOGUN_MENU_BG_WIN.id != 0)
            V6_TEXT_BUFFER_WINDOW.y_size -= SHOGUN_MENU_BG_WIN.y_size;
        v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
        v6_define_window(&V6_STATUS_WINDOW, 0, 0, 0, 0);
        fprintf(stderr, "Scene is zero!\n");
        return;
    }

    V6_STATUS_WINDOW.x = 1;
    V6_STATUS_WINDOW.y = 1;
    glui32 width;

    update_user_defined_colours();

    winid_t gwin = V6_STATUS_WINDOW.id;
    v6_define_window(&V6_STATUS_WINDOW, shogun_banner_width_left, 0, gscreenw - 2 * shogun_banner_width_left, status_height);
    v6_get_and_sync_upperwin_size();
    set_current_window(&V6_STATUS_WINDOW);
    glk_window_get_size(gwin, &width, nullptr);
    garglk_set_reversevideo(0);
    V6_STATUS_WINDOW.style.reset(STYLE_REVERSE);
    garglk_set_zcolors(user_selected_background, user_selected_foreground);
    win_setbgnd(gwin->peer, user_selected_foreground);
    glk_window_clear(V6_STATUS_WINDOW.id);

    uint16_t here = get_global(sg.HERE);
    if (scene != 0) {
        uint32_t addr = user_word(st.SCENE_NAMES + scene * 2);
        if (header.release > 278) {
            print_handler(unpack_string(addr), nullptr); // table SCENE-NAMES
        } else {
            uint8_t number_of_characters = byte(addr++);
            for (uint16_t j = 0; j < number_of_characters; j++) {
                put_char(user_byte(addr + j));
            }
        }
        glk_put_char(':');
    } else if (scene == 0) {
        fprintf(stderr, "Scene is zero! (%d)\n", scene);
    }
    if (options.int_number != INTERP_MACINTOSH) {
        glk_window_move_cursor(gwin, width / 2 - 3, 0);
        glk_put_string(const_cast<char*>("SHOGUN"));
    }
    glk_window_move_cursor(gwin, 0, 1);
    if (here != 0 && !interlude) {
        if (here == so.GALLEY && (graphics_type == kGraphicsTypeApple2 || graphics_type == kGraphicsTypeAmiga)) {
            glk_put_string(const_cast<char*>("Galley"));
        } else {
            print_object(here, nullptr);
        }
        uint16_t player_object = get_global(sg.WINNER);
        uint16_t tmp = internal_get_parent(player_object);
        if (internal_test_attr(tmp, VEHICLEBIT)) {
            if (internal_test_attr(tmp, SUPPORTERBIT)) {
                glk_put_string(const_cast<char*>(", on "));
            } else {
                glk_put_string(const_cast<char*>(", in "));
            }
            internal_call(pack_routine(sr.TELL_THE), {tmp});
        }

        if ((here == so.BRIDGE_OF_ERASMUS && internal_test_attr(so.WHEEL, ONBIT)) || (here == so.GALLEY && internal_test_attr(so.GALLEY_WHEEL, ONBIT))) {
            glk_put_string(const_cast<char*>("; course "));
            internal_call(pack_routine(sr.TELL_DIRECTION), {get_global(sg.SHIP_DIRECTION)});

            glk_put_string(const_cast<char*>("; wheel "));
            internal_call(pack_routine(sr.TELL_DIRECTION), {get_global(sg.SHIP_COURSE)});
        }

    } else if (interlude) {
        glk_put_string(const_cast<char*>("Interlude"));
    }

    glk_window_move_cursor(gwin, width - 10, 0);
    glk_put_string(const_cast<char*>("Score:"));
    int16_t tmp = get_global(sg.SCORE) * SCORE_FACTOR;
    print_right_justified_number(tmp);
    glk_window_move_cursor(gwin, width - 10, 1);
    glk_put_string(const_cast<char*>("Moves:"));
    print_right_justified_number(get_global(sg.MOVES));
    set_current_window(&windows[0]);

    if (graphics_type == kGraphicsTypeApple2 && current_border == P_BORDER2 && scene != S_ERASMUS) {
        shogun_display_border(P_BORDER);
    }
}

void shogun_UPDATE_STATUS_LINE(void) {
    update_status_line(false);
}

void INTERLUDE_STATUS_LINE(void) {
    if (graphics_type != kGraphicsTypeApple2) {
        shogun_display_border(P_BORDER2);
    }
    update_status_line(true);
}

#pragma mark SHOGUN MENU

static uint16_t current_menu_message;

static void print_menu_line_to_transcript(uint16_t menu, uint16_t result) {
    output_stream(-1, 0);
    put_char(ZSCII_SPACE);
    uint16_t string_address = user_word(menu + result * 2);
    uint8_t length = user_byte(string_address);
    uint16_t chr = string_address + 1;
    for (int i = 0; i < length; i++) {
        uint16_t c = user_byte(chr++);
        put_char(c);
    }
    put_char(ZSCII_NEWLINE);
    put_char(ZSCII_NEWLINE);
    output_stream(1, 0);
}

static void print_zstring_to_transcript(uint16_t string) {
    output_stream(-1, 0);
    print_handler(unpack_string(string), nullptr);
    output_stream(1, 0);
}


static void display_menu_line(uint16_t menu, uint16_t line, bool reverse, bool send_menu) {

    if (SHOGUN_MENU_WINDOW.id == nullptr) {
        return;
    }

    if (line < 1)
        line = 1;
    glk_window_move_cursor(SHOGUN_MENU_WINDOW.id, 0, line - 1);
    SHOGUN_MENU_WINDOW.x = 1;
    SHOGUN_MENU_WINDOW.y = (line - 1) * gcellh + 1;
    strid_t stream = glk_window_get_stream(SHOGUN_MENU_WINDOW.id);
    if (reverse)
        garglk_set_reversevideo_stream(stream, 1);
    uint16_t string_address = user_word(menu + line * 2);
    uint8_t length = user_byte(string_address);
    uint16_t chr = string_address + 1;
    char str[40];
    for (int i = 0; i < length; i++) {
        uint16_t c = user_byte(chr++);
        glk_put_char_stream(stream, c);
        str[i] = c;
    }
    if (reverse)
        garglk_set_reversevideo_stream(stream, 0);

    if (send_menu)
        win_menuitem(kV6MenuTypeShogun, line - 1, user_word(menu), 0, str, length);
}

static void select_line(uint16_t menu, uint16_t line) {
    display_menu_line(menu, line, true, false);
}

static void unselect_line(uint16_t menu, uint16_t line) {
    display_menu_line(menu, line, false, false);
}

// Returns width of the longest menu entry in characters
static uint16_t menu_width(uint16_t MENU) {
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

#define MAX_QUICKSEARCH 25

static void print_quicksearch(char *typed) {
    glk_window_clear(SHOGUN_MENU_BG_WIN.id);
    strid_t stream = glk_window_get_stream(SHOGUN_MENU_BG_WIN.id);
    char menu_message[100];
    print_long_zstr_to_cstr(current_menu_message, menu_message, 100);
    glk_put_string_stream(stream, menu_message);
    glk_put_char_stream(stream, '\n');
    glk_put_string_stream(stream, typed);
}

// Quick search function
static int quicksearch(uint16_t chr_to_find, char *typed_letters, int type_pos) {
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

static void update_menu(void) {
    uint16_t cnt = user_word(current_menu); // first word of table is number of elements
    uint16_t width = menu_width(current_menu) * gcellw + 2 * ggridmarginx;
    uint16_t height = cnt * gcellh + 2 * ggridmarginy;
    if (screenmode == MODE_SHOGUN_MENU && current_menu == sm.PART_MENU) {
        windows[0].y_origin = a2_graphical_banner_height;
    }
    v6_define_window(&V6_TEXT_BUFFER_WINDOW, shogun_banner_width_left, windows[0].y_origin, gscreenw - shogun_banner_width_left * 2,  gscreenh - windows[0].y_origin - height);
    int menutop = gscreenh - height;
    if (graphics_type == kGraphicsTypeApple2 && current_menu == sm.PART_MENU) {
        menutop -= a2_graphical_banner_height;
    }
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, user_selected_background);
    if (SHOGUN_MENU_BG_WIN.id == nullptr) {
        SHOGUN_MENU_BG_WIN.id = gli_new_window(wintype_TextBuffer, 0);
        v6_delete_win(&SHOGUN_MENU_WINDOW);
    }
    v6_define_window(&SHOGUN_MENU_BG_WIN, shogun_banner_width_left, menutop, (gscreenw - width) / 2  - shogun_banner_width_left, height);

    strid_t menubg = glk_window_get_stream(SHOGUN_MENU_BG_WIN.id);
    win_refresh(SHOGUN_MENU_BG_WIN.id->peer, 0, 0);
    win_setbgnd(SHOGUN_MENU_BG_WIN.id->peer, user_selected_background);
    glk_window_clear(SHOGUN_MENU_BG_WIN.id);
    char str[100];
    print_long_zstr_to_cstr(current_menu_message, str, 100);
    glk_put_string_stream(menubg, str);

    if (SHOGUN_MENU_WINDOW.id == nullptr) {
        SHOGUN_MENU_WINDOW.id = gli_new_window(wintype_TextGrid, 0);
    }

    v6_define_window(&SHOGUN_MENU_WINDOW, (gscreenw - width) / 2, menutop, width, height);
    win_refresh(SHOGUN_MENU_WINDOW.id->peer, 0, 0);
    win_setbgnd(SHOGUN_MENU_WINDOW.id->peer, user_selected_background);
    strid_t menuwin = glk_window_get_stream(SHOGUN_MENU_WINDOW.id);
    garglk_set_reversevideo_stream(menuwin, 0);
    glk_window_clear(SHOGUN_MENU_WINDOW.id);
    for (int i = 1; i <= cnt; i++) {
        display_menu_line(current_menu, i, (i == current_menu_selection), true);
    }
}

static int menu_select(uint16_t menu, uint16_t selection) {
    if (selection == 0)
        selection = 1;
    current_menu = menu;
    current_menu_selection = selection;
    uint16_t cnt = user_word(menu); // first word of table is number of elements
    update_menu();
    SHOGUN_MENU_WINDOW.style.reset(STYLE_REVERSE);

    uint16_t mouse_click_addr = header.extension_table + 2;
    int16_t y, new_selection;
    int type_pos = 0;
    char typed_letters[MAX_QUICKSEARCH + 1];
    bool finished = false;
    win_menuitem(kV6MenuSelectionChanged, selection - 1, 0, 0, nullptr, 0);
    set_current_window(&V6_TEXT_BUFFER_WINDOW);

    while (!finished) {
        glk_request_mouse_event(SHOGUN_MENU_WINDOW.id);
        uint16_t chr = internal_read_char();
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
        if (current_menu_selection != selection)
            win_menuitem(kV6MenuSelectionChanged, selection - 1, 0, 0, nullptr, 0);
        current_menu_selection = selection;
    }
    v6_delete_win(&SHOGUN_MENU_WINDOW);
    return selection;
}

static int get_from_menu(uint16_t MSG, uint16_t MENU, uint16_t FCN, int default_selection) {
    screenmode = MODE_SHOGUN_MENU;
    if (default_selection == 0)
        default_selection = 1;
    if (MSG == 0) {
        fprintf(stderr, "get_from_menu() called without a menu message.\n");
    }

    current_menu_message = MSG;
    current_menu = MENU;
    shogun_display_border(current_border);

    if (gli_voiceover_on)
        internal_read_char();

    update_menu();

    uint16_t result = 0;
    while (result == 0) {
        glk_window_clear(SHOGUN_MENU_BG_WIN.id);
        char message[40];
        int length = print_long_zstr_to_cstr(MSG, message, 40);
        garglk_set_zcolors_stream(glk_window_get_stream(SHOGUN_MENU_BG_WIN.id), user_selected_foreground, zcolor_Current);
        glk_put_string_stream(glk_window_get_stream(SHOGUN_MENU_BG_WIN.id), message);
        print_zstring_to_transcript(MSG);
        win_menuitem(kV6MenuTitle, 0, kV6MenuTypeShogun, 0, const_cast<char *>(message), length);

        result = menu_select(MENU, default_selection);

        win_menuitem(kV6MenuExited, 0, kV6MenuTypeShogun, 0, nullptr, 0);

        if ((word(0x10) & FLAGS2_TRANSCRIPT) == FLAGS2_TRANSCRIPT) { // Is transcripting on?
            print_menu_line_to_transcript(MENU, result);
        }

        glk_window_clear(SHOGUN_MENU_BG_WIN.id);
    }

    return result;
}

void GET_FROM_MENU(void) {
    // On autorestore, we want to re-select the saved selection

    if (current_menu_selection == 0) {
        if (internal_arg_count() < 4) {
            current_menu_selection = 1;
        } else {
            current_menu_selection = variable(4);
        }
    }
    uint16_t result = get_from_menu(variable(1), variable(2), variable(3), current_menu_selection);

    // When we return to our hacked Zcode,
    // L02 (var 3) must contain the address to the menu function,  L08 (var 9) must contain the index of selected line, L01 (var 2) must contain address of the menu.
    store_variable(9, result);
    current_menu_selection = 0;
    v6_delete_win(&SHOGUN_MENU_BG_WIN);
    v6_define_window(&V6_TEXT_BUFFER_WINDOW, shogun_banner_width_left, V6_TEXT_BUFFER_WINDOW.y_origin, gscreenw - shogun_banner_width_left * 2.0, gscreenh - V6_TEXT_BUFFER_WINDOW.y_origin);
    screenmode = MODE_NORMAL;
    glk_put_char_stream(glk_window_get_stream(V6_TEXT_BUFFER_WINDOW.id), '\n');
    glk_put_char_stream(glk_window_get_stream(V6_TEXT_BUFFER_WINDOW.id), '\n');
}

void after_GET_FROM_MENU(void) {
    glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_BackColor);
    if (sg.CURRENT_BORDER != 0) {
        current_border = (ShogunBorderType)get_global(sg.CURRENT_BORDER);
    }

    if (graphics_type != kGraphicsTypeApple2 && current_border == P_BORDER2) {
        current_border = P_BORDER;
        if (sg.CURRENT_BORDER != 0) {
            set_global(sg.CURRENT_BORDER, P_BORDER);
        }
    }

    shogun_display_border(current_border);
}

void SCENE_SELECT(void) {
    uint16_t menu = sm.PART_MENU;
    if (internal_arg_count() > 0 && variable(1) == 1) {
        menu = st.SCENE_NAMES;
    }
    store_variable(5, menu);
}

void V_VERSION(void) {
    V_CREDITS();
}

void after_V_VERSION(void) {
    after_V_CREDITS();
}


// Checks whether we are running on an original
// black and white Macintosh or a later one with
// color (Mac II.)
// The original returns true if interpreter version is Macintosh
// and screen width is 640.
// Used in V-COLOR and DO-COLOR.
void MAC_II(void) {
    V6_GRAPHICS_BG.x_size = 640;
}

void after_MAC_II(void) {
    V6_GRAPHICS_BG.x_size = gscreenw;
}

#pragma mark Display border

void shogun_display_apple_ii_border(ShogunBorderType border, bool start_menu_mode) {
    // With Apple 2 graphics, the border is right below the status window
    // (except at start menu screen)
    int border_top = V6_STATUS_WINDOW.y_size / imagescaley + 1;
    int width, height;

    // On Apple 2: P_BORDER is crests, P_BORDER2 is waves

    get_image_size(border, &width, &height);

    a2_graphical_banner_height = (height + 1) * imagescaley;
    // Draw banner at bottom and top if we are at start menu screen
    // and using Apple 2 graphics
    if (start_menu_mode) {
        draw_to_pixmap_unscaled(border, 0, 0);
        draw_to_pixmap_unscaled(border, 0, gscreenh / imagescaley - height);
        flush_bitmap(current_graphics_buf_win);
        return;
    }

    draw_rectangle_on_bitmap(user_selected_foreground, 0, 0, hw_screenwidth, border_top);
    draw_rectangle_on_bitmap(0xffffff, 0, border_top - 1, hw_screenwidth, height + 2);
    draw_to_pixmap_unscaled(border, 0, border_top);
    flush_bitmap(current_graphics_buf_win);
}

extern int margin_images[100];
extern int number_of_margin_images;

void shogun_display_border(ShogunBorderType border) {
    if (border != NO_BORDER  && border != P_BORDER && border != P_BORDER2 && border != P_HINT_BORDER) {
        border = P_BORDER;
    }
    bool start_menu_mode = (get_global(sg.SCENE) == 0);

    if (get_global(sg.SCENE) == S_ERASMUS && current_menu == sm.PART_MENU) {
        border = (graphics_type == kGraphicsTypeApple2) ? P_BORDER2 : P_BORDER;
    }

    if (border == 0) {
        border = current_border;
    }

    if (border == 0) {
        border = P_BORDER;
    }

    if (screenmode == MODE_HINTS)
        border = P_HINT_BORDER;

    if (border != P_HINT_BORDER) {
        if (sg.CURRENT_BORDER != 0) {
            set_global(sg.CURRENT_BORDER, border);
        }
        current_border = border;
    }

    // Delete covering graphics window (which would show the title screen)
    if (current_graphics_buf_win != graphics_bg_glk && current_graphics_buf_win != nullptr) {
        if (current_graphics_buf_win == graphics_fg_glk)
            graphics_fg_glk = nullptr;
        // Hack to avoid border changing color before image gets removed
        image_needs_redraw = false;
        glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
        glk_window_clear(current_graphics_buf_win);
        v6_delete_glk_win(current_graphics_buf_win);
    }

    current_graphics_buf_win = graphics_bg_glk;
    if (V6_GRAPHICS_BG.id != current_graphics_buf_win) {
        v6_delete_win(&V6_GRAPHICS_BG);
        V6_GRAPHICS_BG.id = current_graphics_buf_win;
        v6_define_window(&V6_GRAPHICS_BG, 1, 1, gscreenw, gscreenh);
    }

    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    int border_top = 0;

    win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);

    if (graphics_type == kGraphicsTypeApple2) {
        shogun_display_apple_ii_border(border, start_menu_mode);
        return;
    }

    int width, height;
    if (!get_image_size(border, &width, &height)) {
        if (border == P_BORDER2) {
            border = P_BORDER;
            if (!get_image_size(border, &width, &height)) {
                return;
            }
        }
    }

    if (number_of_margin_images > 0 && border != P_HINT_BORDER) {
        extract_palette_from_picnum(margin_images[number_of_margin_images - 1]);
    } else {
        extract_palette_from_picnum(border);
    }

    int16_t BR = -1;
    int16_t BL = -1;
    switch(border) {
        case P_HINT_BORDER:
            BL = P_HINT_BORDER_L;
            BR = P_HINT_BORDER_R;
            break;
        case P_BORDER:
            BR = P_BORDER_R;
            break;
        case P_BORDER2:
            BR = P_BORDER2_R;
            break;
        case NO_BORDER:
            fprintf(stderr, "Error: No border!\n");
            break;
    }

    int left_margin = 0;
    int pillar_top = 0;

    if (border == P_HINT_BORDER) {
        if (graphics_type != kGraphicsTypeAmiga && graphics_type != kGraphicsTypeMacBW) {
            border_top = height;
            pillar_top = border_top;
            get_image_size(P_HINT_LOC, &width, nullptr);
        } else {
            get_image_size(P_HINT_LOC, &width, &pillar_top);
        }
    } else {
        get_image_size(P_BORDER_LOC, &width, nullptr);
    }
    left_margin = width;
    shogun_banner_width_left = ceil(width * imagescalex) + 1;
    a2_graphical_banner_height = 0;

    // Draw a line to mask background at rightmost pixels in CGA
    if (border == P_HINT_BORDER && graphics_type == kGraphicsTypeCGA) {
        draw_rectangle_on_bitmap(monochrome_black, hw_screenwidth - 1, 0, 1, gscreenh / imagescaley);
    }

    draw_to_pixmap_unscaled_using_current_palette(border, 0, 0);

    float factor = (float)gscreenw / hw_screenwidth / pixelwidth;
    int desired_height = ceil(gscreenh / factor);

    int lowest_drawn_line = height + border_top;
    bool must_extend = (desired_height > lowest_drawn_line);

    // BL won't be found
    // unless we are drawing the hints border
    // and graphics type is not Amiga or Mac B/W
    if (find_image(BL)) {
        get_image_size(BL, &width, &height);
        draw_to_pixmap_unscaled(BL, 0, border_top);
        draw_to_pixmap_unscaled(BR, hw_screenwidth - width, border_top);
        lowest_drawn_line = height + border_top;
    } else {
        // Amiga och Mac border graphics are a single image with everything,
        // not separated into top and sides

        if (must_extend && (graphics_type == kGraphicsTypeAmiga || graphics_type == kGraphicsTypeMacBW)) {
            if (border == P_BORDER) {
                if (graphics_type == kGraphicsTypeMacBW) {
                    draw_to_pixmap_unscaled_flipped_using_current_palette(border, 0, height);
                    lowest_drawn_line = height * 2;
                } else {
                    draw_to_pixmap_unscaled_flipped_using_current_palette(border, 0, height - 2);
                    lowest_drawn_line = height * 2 - 2;
                }
            } else if (border == P_BORDER2) {
                if (graphics_type == kGraphicsTypeMacBW) {
                    draw_to_pixmap_unscaled_using_current_palette(border, 0, height - 35);
                    lowest_drawn_line = height * 2 - 35;
                } else {
                    draw_to_pixmap_unscaled_using_current_palette(border, 0, height - 22);
                    lowest_drawn_line = height * 2 - 22;
                }
            } else if (border == P_HINT_BORDER && graphics_type == kGraphicsTypeMacBW) {
                extend_mac_bw_hint_border(desired_height);
                desired_height = 0;
            }
        }
        
        if (find_image(BR)) {
            get_image_size(BR, &width, &height);
            if (graphics_type == kGraphicsTypeCGA) {
                height -= 7;
            }
            if (must_extend)
                draw_to_pixmap_unscaled_flipped_using_current_palette(border, hw_screenwidth - width, border_top + height);
            draw_to_pixmap_unscaled_using_current_palette(BR, hw_screenwidth - width, border_top);
            lowest_drawn_line = border_top + height;
            if (must_extend) {
                draw_to_pixmap_unscaled_flipped_using_current_palette(BR, 0, border_top + height);
                lowest_drawn_line += height;
            }
            if (graphics_type == kGraphicsTypeCGA) {
                draw_to_pixmap_unscaled(border, 0, 0);
            }
        }
    }
    extend_shogun_border(desired_height, lowest_drawn_line, pillar_top);

    // We draw a rectangle of status window color at the top to avoid
    // visible gaps at the edges. (Except at the start menu, where
    // there is no status window.)
    bool should_draw_covering_rectangle = false;

    glui32 rectangle_color = user_selected_foreground;
    if (!start_menu_mode && border != P_HINT_BORDER) {
        should_draw_covering_rectangle = true;
    }

    if (border == P_HINT_BORDER ) {
        if (graphics_type == kGraphicsTypeCGA) {
            should_draw_covering_rectangle = true;
        } else {
            left_margin = 0;
            if (graphics_type == kGraphicsTypeMacBW) {
                should_draw_covering_rectangle = true;
            } else if (options.int_number == INTERP_MACINTOSH && graphics_type == kGraphicsTypeAmiga) {
                should_draw_covering_rectangle = true;
                rectangle_color = ROSE_TAUPE;
            }
        }
    }

    if (should_draw_covering_rectangle) {
        draw_rectangle_on_bitmap(rectangle_color, left_margin, 0, hw_screenwidth - left_margin * 2, V6_STATUS_WINDOW.y_size / imagescaley + 1);
    }
    flush_bitmap(current_graphics_buf_win);
}


void shogun_DISPLAY_BORDER(void) {
    if (screenmode == MODE_SLIDESHOW)
        screenmode = MODE_SHOGUN_MENU;
    if (screenmode != MODE_HINTS && screenmode != MODE_SHOGUN_MENU)
        screenmode = MODE_NORMAL;
    if (internal_arg_count() > 0) {
        shogun_display_border((ShogunBorderType)variable(1));
    } else {
        shogun_display_border(P_BORDER);
    }
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
    if (SHOGUN_CREST_WINDOW.id != nullptr && SHOGUN_CREST_WINDOW.id->type != wintype_Graphics) {
        v6_delete_win(&SHOGUN_CREST_WINDOW);
    }
    if (SHOGUN_CREST_WINDOW.id == nullptr) {
        SHOGUN_CREST_WINDOW.id = gli_new_window(wintype_Graphics, 0);
    }
    int width, height;
    get_image_size(P_CREST, &width, &height);
    v6_define_window(&SHOGUN_CREST_WINDOW, V6_STATUS_WINDOW.x_origin, V6_STATUS_WINDOW.y_size + imagescaley * 2, V6_STATUS_WINDOW.x_size, height * imagescaley + 2 * gcellh);
    V6_TEXT_BUFFER_WINDOW.y_origin = V6_STATUS_WINDOW.y_size + SHOGUN_CREST_WINDOW.y_size + 2;
    V6_TEXT_BUFFER_WINDOW.y_size = gscreenh - V6_TEXT_BUFFER_WINDOW.y_origin;
    if (screenmode == MODE_SHOGUN_MENU) {
        V6_TEXT_BUFFER_WINDOW.y_size -= SHOGUN_MENU_WINDOW.y_size;
    }
    glk_window_set_background_color(SHOGUN_CREST_WINDOW.id, user_selected_background);
    glk_window_clear(SHOGUN_CREST_WINDOW.id);
    width *= imagescalex;
    height *= imagescaley;

    draw_inline_image(SHOGUN_CREST_WINDOW.id, P_CREST, (SHOGUN_CREST_WINDOW.x_size - width) / 2, (SHOGUN_CREST_WINDOW.y_size - height) / 2, imagescaley, false);
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

    int margin;
    get_image_size(P_BORDER_LOC, &margin, nullptr);

    if (width >= hw_screenwidth - 2 * margin - 10) {
        // We guess that this is a center image
        glk_set_style(style_User1);
        glk_window_flow_break(V6_TEXT_BUFFER_WINDOW.id);
        int x_size = V6_TEXT_BUFFER_WINDOW.x_size - 2 * gbuffermarginx;
        draw_inline_image(V6_TEXT_BUFFER_WINDOW.id, picnum, imagealign_InlineCenter, picnum, (float)x_size / (width * pixelwidth), false);
        glk_set_style(style_Normal);
        glk_put_char(UNICODE_LINEFEED);
        glk_put_char(UNICODE_LINEFEED);
    } else {
        draw_inline_image(V6_TEXT_BUFFER_WINDOW.id, picnum, right ? imagealign_MarginRight : imagealign_MarginLeft, picnum, inline_scale, false);
    }
    add_margin_image_to_list(picnum);
    shogun_display_border(current_border);
}

#pragma mark Maze

static void display_maze_pic(int pic, int x, int y) {
    if (pic != 0) {
        draw_to_pixmap_unscaled(pic, maze_offset_x + x * maze_box_width, maze_offset_y + y * maze_box_height);
        image_needs_redraw = true;
    }
 }

void DISPLAY_MAZE_PIC(void) {
    display_maze_pic(variable(1), variable(3), variable(2));
}

static void get_maze_width_and_height(int *width, int *height) {
    if (sg.MAZE_WIDTH == 0) {
        if (graphics_type == kGraphicsTypeApple2) {
            *width = 0x12;
        } else {
            *width = 0x25;
        }
    } else {
        *width = get_global(sg.MAZE_WIDTH);
    }
    if (sg.MAZE_HEIGHT == 0) {
        *height = 0x11;
    } else {
        *height = get_global(sg.MAZE_HEIGHT);
    }
}

static void print_maze(void) {
    int offs = 0;
    uint16_t maze_map_table = get_global(sg.MAZE_MAP);
    int width, height;
    if (graphics_type == kGraphicsTypeBlorb) {
        get_image_size(P_MAZE_BACKGROUND, &width, &height);
        draw_rectangle_on_bitmap(0x717171, maze_offset_x, maze_offset_y, width, height);
    } else {
        draw_to_pixmap_unscaled(P_MAZE_BACKGROUND, maze_offset_x, maze_offset_y);
    }

    //    if (graphics_type == kGraphicsTypeApple2 && maze_width != 19) {
    //        get_image_size(P_MAZE_BACKGROUND, &width, &height);
    //        draw_to_pixmap_unscaled(P_MAZE_BACKGROUND, maze_offset_x + width, maze_offset_y);
    //    }

    int maze_height, maze_width;

    get_maze_width_and_height(&maze_width, &maze_height);

    for (int y = 0; y < maze_height; y++) {
        for (int x = 0; x < maze_width; x++, offs++) {
            int maze_tile = user_byte(maze_map_table + offs) & 0x7f;
            display_maze_pic(maze_tile, x, y);
        }
    }
}

static void display_maze(bool clear) {
    screenmode = MODE_SHOGUN_MAZE;
    int height;
    get_image_size(P_MAZE_BACKGROUND, nullptr, &height);
//    fudge_for_apple_2_maze(1);
    int maze_height = height * imagescaley;

    long status_height = V6_STATUS_WINDOW.y_size;
    long y = status_height + V6_STATUS_WINDOW.y_origin;

    long textwin_height = gscreenh - status_height;
    v6_define_window(&V6_TEXT_BUFFER_WINDOW, V6_TEXT_BUFFER_WINDOW.x_origin, y + maze_height, V6_TEXT_BUFFER_WINDOW.x_size, textwin_height - maze_height);

    if (clear)
        glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);

    get_image_size(P_MAZE_BOX, &maze_box_width, &maze_box_height);

    int border_width;
    get_image_size(P_BORDER_LOC, &border_width, nullptr);

    maze_offset_x = border_width;
    maze_offset_y = status_height / imagescaley;

    SHOGUN_MAZE_WINDOW.x_origin = 0;
    SHOGUN_MAZE_WINDOW.y_origin = 0;

    print_maze();
}

void DISPLAY_MAZE(void) {
    display_maze(true);
}

// The original source lets you move diagonally as
// well, but as the maze has no diagonal passages,
// it only means a lot of bumping into walls.
// I think this way is more user-friendly.

static int maze_mouse_f(void) {
    int16_t WX, WY;
    uint16_t DIR = 0;
    int BX, BY;
    get_image_size(P_MAZE_BOX, &BX, &BY);
    uint16_t X = get_global(sg.MAZE_X);
    uint16_t Y = get_global(sg.MAZE_Y);
    WY = maze_offset_y + Y * BY + BY / 2;
    WX = maze_offset_x + X * BX + BX / 2;

    uint16_t mouse_click_addr = header.extension_table + 2;
    int16_t mouse_x = word(mouse_click_addr) - 1;
    int16_t mouse_y = word(mouse_click_addr + 2) - 1;

    WY = mouse_y - WY;
    WX = mouse_x - WX;

    if (WX >= 0) { // right side
        if (WY < 0) { // top right
            WY = -WY;
            if (WX > WY) {
                DIR = so.EAST;
            } else {
                DIR = so.NORTH;
            }
        } else { // bottom right
            if (WX > WY) {
                DIR = so.EAST;
            } else {
                DIR = so.SOUTH;
            }
        }
    } else if (WY < 0) { // top left
        WY = -WY;
        WX = -WX;
        if (WX > WY) {
            DIR = so.WEST;
        } else {
            DIR = so.NORTH;
        }
    } else { // bottom left
        WX = -WX;
        if (WX > WY) {
            DIR = so.WEST;
        } else {
            DIR = so.SOUTH;
        }
    }
    if (WX <= BX / 2 && WY <= BY / 2)
        return 0;
    if (DIR != 0) {
        internal_call(pack_routine(sr.ADD_TO_INPUT), {DIR});
        glk_put_char(UNICODE_LINEFEED);
        return 13;
    }
    return 0;
}

void MAZE_MOUSE_F(void) {
    store_variable(1, maze_mouse_f());
}

# pragma mark Skip maze stuff

#define P_MAZE_WALL 0x80
#define P_MAZE_STREET 0


typedef struct MazeExits {
    uint8_t NORTH;
    uint8_t EAST;
    uint8_t SOUTH;
    uint8_t WEST;
} MazeExits;

bool deadend(int i, MazeExits *me) {
    int MAZE_WIDTH, MAZE_HEIGHT;

    get_maze_width_and_height(&MAZE_WIDTH, &MAZE_HEIGHT);

    int MAZE_MAP = get_global(sg.MAZE_MAP);
    int mazesize = MAZE_WIDTH * MAZE_HEIGHT;

    int start = get_global(sg.MAZE_XSTART) + get_global(sg.MAZE_YSTART) * MAZE_WIDTH;

    int exits = 0;

    if (i == start || memory[MAZE_MAP + i] != P_MAZE_STREET || i % MAZE_WIDTH == 0 || (i + 1) % MAZE_WIDTH == 0) { // skip left and right borders
        return false;
    }

    me->NORTH = P_MAZE_WALL;
    me->EAST = P_MAZE_WALL;
    me->SOUTH = P_MAZE_WALL;
    me->WEST = P_MAZE_WALL;

    if (i > MAZE_WIDTH && memory[MAZE_MAP + (i - MAZE_WIDTH)] == P_MAZE_STREET) {
        me->NORTH = P_MAZE_STREET;
        exits++;
    }
    if (i < mazesize - 1 && memory[MAZE_MAP + i + 1] == P_MAZE_STREET) {
        me->EAST = P_MAZE_STREET;
        exits++;
    }
    if (i < mazesize - MAZE_WIDTH && memory[MAZE_MAP + i + MAZE_WIDTH] == P_MAZE_STREET) {
        me->SOUTH = P_MAZE_STREET;
        exits++;
    }
    if (i > 1 && memory[MAZE_MAP + i - 1] == P_MAZE_STREET) {
        me->WEST = P_MAZE_STREET;
        exits++;
    }

    return (exits == 1);
}

void debug_draw_maze(void) {
    int MAZE_HEIGHT, MAZE_WIDTH;
    get_maze_width_and_height(&MAZE_WIDTH, &MAZE_HEIGHT);

    int MAZE_MAP = get_global(sg.MAZE_MAP);

    fprintf(stderr, "sg.MAZE_HEIGHT:%d\n", MAZE_HEIGHT);
    fprintf(stderr, "sg.MAZE_WIDTH:%d\n", MAZE_WIDTH);
    fprintf(stderr, "sg.MAZE_MAP:0x%x\n", MAZE_MAP);

    int size = MAZE_WIDTH * MAZE_HEIGHT;

    fprintf(stderr, "MAZE-MAP start address: 0x%x end: 0x%x\n", MAZE_MAP, MAZE_MAP + size);
    for (int i = 0; i < size; i++) {
        if (i % MAZE_WIDTH == 0)
            fprintf(stderr, "\n");
        uint8_t c = memory[MAZE_MAP + i];
        if (c == 0)
            fprintf(stderr, "0");
        else if (c == 0x80) {
            fprintf(stderr, "X");
        } else {
            fprintf(stderr, "?");
        }
    }
    fprintf(stderr, "\n");
}

void simplify_maze(void) {
    MazeExits me = {};

    int MAZE_WIDTH, MAZE_HEIGHT;
    get_maze_width_and_height(&MAZE_WIDTH, &MAZE_HEIGHT);

    int MAZE_MAP = get_global(sg.MAZE_MAP);

    int mazesize = MAZE_WIDTH * (MAZE_HEIGHT - 1);
    for (int i = MAZE_WIDTH + 1; i < mazesize; i++) { // Skip bottom and top borders
        int pos = i;
        while (deadend(pos, &me)) {
            memory[MAZE_MAP + pos] =  P_MAZE_WALL;
            if (me.EAST == P_MAZE_STREET)
                pos++;
            else if (me.WEST == P_MAZE_STREET)
                pos--;
            else if (me.NORTH == P_MAZE_STREET)
                pos -= MAZE_WIDTH;
            else if (me.SOUTH == P_MAZE_STREET)
                pos += MAZE_WIDTH;
        }
    }
}

bool dont_repeat_question_on_autorestore = false;

void after_BUILDMAZE(void) {
    if (skip_puzzle_prompt("Would you like me to simplify the city maze, to make it easier to traverse without seeing the graphics? (Y is affirmative): >")) {
        simplify_maze();
    }
}

#pragma mark Adjust windows

static void adjust_shogun_window(void) {
    V6_TEXT_BUFFER_WINDOW.x = 0;
    V6_TEXT_BUFFER_WINDOW.y = 0;

    int status_window_height = (get_global(sg.SCENE) == 0) ? 0 : 2 * (gcellh + ggridmarginy);

    if (graphics_type == kGraphicsTypeApple2) {
        V6_STATUS_WINDOW.y_size = status_window_height;
        V6_STATUS_WINDOW.x_origin = 1;
        V6_STATUS_WINDOW.x_size = gscreenw;
        v6_sizewin(&V6_STATUS_WINDOW);
        SHOGUN_A2_BORDER_WIN.y_origin = V6_STATUS_WINDOW.y_size + 1;
        SHOGUN_A2_BORDER_WIN.y_size = a2_graphical_banner_height;
        v6_sizewin(&SHOGUN_A2_BORDER_WIN);
        V6_TEXT_BUFFER_WINDOW.y_origin = SHOGUN_A2_BORDER_WIN.y_origin + SHOGUN_A2_BORDER_WIN.y_size;
        V6_TEXT_BUFFER_WINDOW.x_size = gscreenw;
        V6_TEXT_BUFFER_WINDOW.y_size = gscreenh - V6_TEXT_BUFFER_WINDOW.y_origin;
    } else {
        v6_define_window(&V6_STATUS_WINDOW, shogun_banner_width_left, 1, gscreenw - shogun_banner_width_left * 2, status_window_height);
        V6_STATUS_WINDOW.fg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
        V6_STATUS_WINDOW.bg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
        V6_STATUS_WINDOW.style.reset(STYLE_REVERSE);

        V6_TEXT_BUFFER_WINDOW.x_origin = V6_STATUS_WINDOW.x_origin;
        V6_TEXT_BUFFER_WINDOW.x_size = V6_STATUS_WINDOW.x_size;
    }

    V6_TEXT_BUFFER_WINDOW.fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
    V6_TEXT_BUFFER_WINDOW.bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
    win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);
    
    v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

void shogun_draw_title_image(void) {
    if (graphics_fg_glk == nullptr)
        graphics_fg_glk = gli_new_window(wintype_Graphics, 0);
    win_sizewin(graphics_fg_glk->peer, 0, 0, gscreenw, gscreenh);
    current_graphics_buf_win = graphics_fg_glk;
    glk_request_mouse_event(graphics_fg_glk);
    int width, height;
    get_image_size(1, &width, &height);
    float scale = gscreenw / ((float)width * pixelwidth);
    int ypos = gscreenh - height * scale + 1;
    if (graphics_type == kGraphicsTypeMacBW)
        ypos += 2;
    glk_window_set_background_color(graphics_fg_glk, monochrome_white);
    glk_window_clear(graphics_fg_glk);
    draw_inline_image(graphics_fg_glk, kShogunTitleImage, 0, ypos, scale, false);
    screenmode = MODE_SLIDESHOW;
    win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, monochrome_white);
}

void shogun_erase_screen(void) {
    v6_delete_win(&SHOGUN_MENU_WINDOW);
    v6_delete_win(&SHOGUN_MENU_BG_WIN);
    v6_delete_win(&SHOGUN_CREST_WINDOW);
    if (graphics_type != kGraphicsTypeApple2) {
        v6_delete_win(&SHOGUN_A2_BORDER_WIN);
    }
}

void shogun_update_on_resize(void) {
    // Window 0: (S-TEXT) Text buffer, "main" window
    // Window 1: (S-STATUS) Status
    // Window 2: (MENU-WINDOW) Menu grid, centered on screen. We put it at the bottom, as
    // we can't line windows up with text in a buffer window
    // Window 3: (MAZE-WINDOW) Maze. This has no corresponding Glk window.
    // All maze graphics are actually drawn to the "full screen" background window (7).
    // Window 4: (SHOGUN_CREST_WINDOW) Top graphics window only used for P-CREST image at the end of the game.
    // Window 5: Menu background, used for "You may choose to:" messages
    // Window 6: (S-BORDER) Horizontal Apple 2 graphic border
    // Window 7: (S-FULL) Full screen background, used by us for border graphics and maze

    set_global(sg.MACHINE, options.int_number); // GLOBAL MACHINE

    set_global(sg.FONT_X, 1); // GLOBAL FONT-X
    set_global(sg.FONT_Y, 1); // GLOBAL FONT-Y

    if (screenmode == MODE_DEFINE) {
        adjust_definitions_window();
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
            SHOGUN_MAZE_WINDOW.id = gli_new_window(wintype_Graphics, 0);
        }
        SHOGUN_MAZE_WINDOW.x_size = gscreenw;
        SHOGUN_MAZE_WINDOW.y_size = gscreenh;

        v6_sizewin(&SHOGUN_MAZE_WINDOW);
    } else {
        shogun_display_border(current_border);
        if (V6_TEXT_BUFFER_WINDOW.id) {
            float xscalefactor = 0;
            float yscalefactor = 0;
            if (graphics_type_changed) {
                refresh_margin_images();
                yscalefactor = 2.0;
                if (graphics_type == kGraphicsTypeMacBW)
                    yscalefactor = imagescalex;
                xscalefactor = yscalefactor * pixelwidth;
            }
            win_refresh(V6_TEXT_BUFFER_WINDOW.id->peer, xscalefactor, yscalefactor);
            win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);
        }
        if (V6_TEXT_BUFFER_WINDOW.id != nullptr && !(current_menu == sm.PART_MENU && screenmode == MODE_SHOGUN_MENU)) {
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
        if (SHOGUN_CREST_WINDOW.id != nullptr) {
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

#pragma mark Restoring

//TODO: Merge with stash_arthur_state and recover_arthur_state

void shogun_stash_state(library_state_data *dat) {
    if (!dat)
        return;

    if (current_graphics_buf_win)
        dat->current_graphics_win_tag = current_graphics_buf_win->tag;
    if (graphics_fg_glk)
        dat->graphics_fg_tag = graphics_fg_glk->tag;
    if (stored_bufferwin)
        dat->stored_lower_tag = stored_bufferwin->tag;
    dat->slideshow_pic = current_border;

    dat->shogun_menu = current_menu;
    dat->shogun_menu_selection = current_menu_selection;
}

void shogun_recover_state(library_state_data *dat) {
    if (!dat)
        return;

    current_graphics_buf_win = gli_window_for_tag(dat->current_graphics_win_tag);
    graphics_fg_glk = gli_window_for_tag(dat->graphics_fg_tag);
    stored_bufferwin = gli_window_for_tag(dat->stored_lower_tag);
    current_border = (ShogunBorderType)dat->slideshow_pic;

    current_menu = dat->shogun_menu;
    current_menu_selection = dat->shogun_menu_selection;
}

void shogun_update_after_restore(void) {
    screenmode = MODE_NORMAL;
    if (sg.CURRENT_BORDER != 0)
        current_border = (ShogunBorderType)get_global(sg.CURRENT_BORDER);
    update_status_line(last_was_interlude);
    current_menu_selection = 0;
    shogun_erase_screen();
    after_V_COLOR();
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
    uint16_t look = internal_call(pack_routine(sr.DESCRIBE_ROOM));
    if (look) {
        internal_call(pack_routine(sr.DESCRIBE_OBJECTS));
    }
}

//TODO: Merge with arthur_update_after_autorestore
void shogun_update_after_autorestore(void) {
    update_user_defined_colours();
    uint8_t fg = get_global(fg_global_idx);
    uint8_t bg = get_global(bg_global_idx);

    for (auto &window : windows) {
        window.fg_color = Color(Color::Mode::ANSI, fg);
        window.bg_color = Color(Color::Mode::ANSI, bg);
    }

    if (current_graphics_buf_win) {
        glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
        glk_window_clear(current_graphics_buf_win);
    }

    if (V6_TEXT_BUFFER_WINDOW.id != nullptr)
        set_current_window(&V6_TEXT_BUFFER_WINDOW);

    v6_delete_win(&SHOGUN_MENU_WINDOW);

    if (screenmode == MODE_SLIDESHOW) {
        v6_close_and_reopen_front_graphics_window();
    }
    window_change();
}

void shogun_update_after_restart(void) {
    // Get rid of ending crest graphics window
    v6_delete_win(&SHOGUN_CREST_WINDOW);
    current_menu_selection = 0;
}

bool shogun_autorestore_internal_read_char_hacks(void) {
    if (screenmode == MODE_HINTS || screenmode == MODE_SHOGUN_MENU || screenmode == MODE_DEFINE) {
        return true;
    }
    if (screenmode == MODE_NORMAL) { // We can assume that we are answering the maze simplification question;
        dont_repeat_question_on_autorestore = true;
        return true;
    }
    return false;
}

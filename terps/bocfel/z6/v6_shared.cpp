//
//  v6_shared.cpp
//  bocfel6
//
//  Created by Administrator on 2024-06-09.
//

#include <sstream>

#include "draw_image.hpp"
#include "memory.h"
#include "objects.h"
#include "screen.h"
#include "stack.h"
#include "unicode.h"
#include "zterp.h"
#include "options.h"
#include "v6_specific.h"

#include "arthur.hpp"
#include "shogun.hpp"
#include "zorkzero.hpp"

#include "v6_shared.hpp"

#define DEFINITIONS_WINDOW windows[2]

int margin_images[100];
int number_of_margin_images = 0;

void shift_margin_image_list(void) {
    fprintf(stderr, "More than 100 margin images! Shifting list, forgetting the first margin image (%d)\n", margin_images[0]);
    for (int i = 1; i < number_of_margin_images; i++) {
       margin_images[i] = margin_images[i - 1];
    }
    number_of_margin_images = 99;
}

void add_margin_image_to_list(int image) {
    if (number_of_margin_images >= 100) {
        shift_margin_image_list();
    }
    for (int i = 0; i < number_of_margin_images; i++) {
        if (margin_images[i] == image) {
            // We have already added this image to the list
            return;
        }
    }
    margin_images[number_of_margin_images] = image;
    number_of_margin_images++;
}

void clear_margin_image_list(void) {
    number_of_margin_images = 0;
}

void refresh_margin_images(void) {
    // We uncache all used margin images and create new ones

    for (int i = 0; i < number_of_margin_images; i++) {
        recreate_image(margin_images[i], false);
    }
}

#pragma mark PRINT NUMBER

void print_number(int number) {
    std::ostringstream ss;
    ss << number;
    for (const auto &c : ss.str()) {
        glk_put_char(c);
    }
}

void print_right_justified_number(int number) {

    int spaces;
    if (number > 999 || number < -99) {
        spaces = 0;
    } else if (number > 99 || number < -9) {
        spaces = 1;
    } else if (number > 9 || number < 0) {
        spaces = 2;
    } else {
        spaces = 3;
    }

    for (int i = 0; i < spaces; i++)
        glk_put_char(' ');

    print_number(number);
}

#pragma mark Print Z string to C string

static char *string_buf_ptr = nullptr;
static int string_buf_pos = 0;
static int string_maxlen = 0;

static void print_to_string_buffer(uint8_t c) {
    if (string_buf_pos < string_maxlen)
        string_buf_ptr[string_buf_pos++] = c;
}

int print_long_zstr_to_cstr(uint16_t addr, char *str, int maxlen) {
    int length = count_characters_in_zstring(addr);
    if (length < 2)
        return 0;
    string_buf_ptr = str;
    string_buf_pos = 0;
    string_maxlen = maxlen;
    print_handler(unpack_string(addr), print_to_string_buffer);
    str[length] = 0;
    return length;
}

int print_zstr_to_cstr(uint16_t addr, char *str) {
    return print_long_zstr_to_cstr(addr, str, STRING_BUFFER_SIZE);
}

void v6_close_and_reopen_front_graphics_window(void) {
    if (graphics_fg_glk) {
        if (current_graphics_buf_win == graphics_fg_glk) {
            current_graphics_buf_win = nullptr;
        }
        gli_delete_window(graphics_fg_glk);
    }
    graphics_fg_glk = gli_new_window(wintype_Graphics, 0);
    if (screenmode == MODE_SLIDESHOW) {
        win_sizewin(graphics_fg_glk->peer, 0, 0, gscreenw, gscreenh);
        current_graphics_buf_win = graphics_fg_glk;
        glk_request_mouse_event(graphics_fg_glk);
    } else {
        win_sizewin(graphics_fg_glk->peer, 0, 0, 0, 0);
        if (screenmode == MODE_INITIAL_QUESTION) {
            current_graphics_buf_win = nullptr;
        } else {
            current_graphics_buf_win = graphics_bg_glk;
        }
    }
}

void transcribe_and_print_string(const char *str) {
    glk_put_string(const_cast<char*>(str));
    int i = 0;
    while (i++ != 0) {
        transcribe(str[i]);
    }
}

#pragma mark DEFINITIONS SCREEN

#define DEFINITIONS_WIDTH 30 // FLEN in original source
#define ARTHUR_LAST_OBJECT 0x137

static void print_reverse_video_space(void) {
    garglk_set_reversevideo(1);
    glk_put_char(UNICODE_SPACE);
    garglk_set_reversevideo(0);
}

uint16_t fkeys_table_addr = 0xce2a;
uint16_t fnames_table_addr = 0x4b57;

// Return width of the widest user-defined command
static int soft_commands_width(void) {
    int widest = 0;
    int num_commands = user_word(fkeys_table_addr) / 2;
    int fkey = fkeys_table_addr + 2;
    int16_t fdef;
    for (int i = 0; i < num_commands; i++) {
        int16_t temp = user_word(fkey);
        if (temp < 0) {
            fdef = user_word(fkey + 2);
            if (fdef != 0) {
                int length = count_characters_in_zstring(user_word(fdef)) + 4;
                if (length > widest)
                    widest = length;
            }
        }
        fkey += 4;
    }
    return widest;
}

int define_window_width = 0;

static void print_center_table(int y, uint16_t string) {
    if (define_window_width == 0)
        define_window_width = soft_commands_width();

    int length = count_characters_in_zstring(string);
    Window win = DEFINITIONS_WINDOW;
    winid_t gwin = win.id;
    glui32 width;
    glk_window_get_size(gwin, &width, nullptr);

    if (define_window_width != 0) {
        glk_window_move_cursor(gwin, (width - define_window_width) / 2, y);
        for (int i = 0; i < define_window_width; i++)
            glk_put_char(UNICODE_SPACE);
    }

    glk_window_move_cursor(gwin, (width - length) / 2, y);
    print_handler(unpack_string(string), nullptr);
}

static uint16_t scan_table(uint16_t value, uint16_t address, uint16_t length, bool bytewise) {
    for (uint16_t i = 0; i < length; i++) {
        if ((bytewise && user_byte(address) == value) ||
            (!bytewise && user_word(address) == value)) {
            return address;
        }
        address++;
        if (!bytewise)
            address++;
    }
    return 0;
}

// Print a single define menu line.
// Either a function key name + an editable command
// or a hard-coded menu item such as Save Definitions.
static void display_soft(int function_key, int index, bool inverse, bool send_menu) {
    int fdef, y;
    char str[60];
    int len = 0;
    fdef = user_word(function_key + 2);
    y = index + 1;
    if ((int16_t)user_word(function_key) < 0) { // hard-coded menu item
        if (fdef != 0) {
            DEFINITIONS_WINDOW.x = 1;
            DEFINITIONS_WINDOW.y = y * gcellh;
            if (inverse) {
                garglk_set_reversevideo(1);
            }
            print_center_table(y, user_word(fdef));
            if (send_menu) {
                len = print_long_zstr_to_cstr(user_word(fdef), str, 40);
                win_menuitem(kV6MenuTypeDefine, index - 1, 18, 0, str, len);
            }
        }
    } else {
        glk_window_move_cursor(DEFINITIONS_WINDOW.id, 0, y);
        DEFINITIONS_WINDOW.x = 1;
        DEFINITIONS_WINDOW.y = y * gcellh + 1;

        uint16_t key_name_string = scan_table(user_word(function_key), fnames_table_addr, user_word(fnames_table_addr - 2), false);
        if (key_name_string != 0) {
            if (inverse) {
                garglk_set_reversevideo(0);
            } else {
                garglk_set_reversevideo(1);
            }
            len = print_long_zstr_to_cstr(user_word(key_name_string + 2), str, 60);
            glk_put_string(str);
            garglk_set_reversevideo(0);
            glk_put_char(UNICODE_SPACE);
            if (inverse) {
                garglk_set_reversevideo(1);
            }
        }
        int string_length = user_byte(fdef);

        // For VoiceOver
        str[len++] = ':';
        str[len++] = ' ';

        for (int i = 0; i < string_length; i++) {
            uint8_t c = user_byte(fdef + 2 + i);
            if (c == ZSCII_NEWLINE)
                c = '|';
            glk_put_char(c);
            str[len++] = c; // For VoiceOver
        }

        // For VoiceOver
        if (send_menu)
            win_menuitem(kV6MenuTypeDefine, index, 18, 0, str, user_byte(fdef + 1) + 5);

        glsi32 x = user_byte(fdef + 1) + 4;

        // Print cursor if this is the selected line
        if (!inverse) {
            glk_window_move_cursor(DEFINITIONS_WINDOW.id, x, y);
            print_reverse_video_space();
            glk_window_move_cursor(DEFINITIONS_WINDOW.id, x, y);
        }
    }
    garglk_set_reversevideo(0);
}

int global_define_line = 0;

static void display_softs(void) {
    fprintf(stderr, "display_softs\n");
    uint16_t number_of_lines = user_word(fkeys_table_addr) / 2;
    winid_t win = DEFINITIONS_WINDOW.id;
    glk_set_window(win);
    glui32 width;
    glk_window_get_size(win, &width, nullptr);
    int16_t xpos = (width - 14) / 2;
    if (xpos < 0)
        xpos = 0;
    glk_window_move_cursor(win, xpos, 0);
    char *function_keys_string = const_cast<char*>("Function Keys");
    glk_put_string(function_keys_string);
    win_menuitem(kV6MenuTitle, 0, kV6MenuTypeDefine, 0, function_keys_string, 13);
    uint16_t function_key = fkeys_table_addr + 2;
    for (int i = 0; i < number_of_lines; i++) {
        display_soft(function_key, i, (i != global_define_line), true);
        function_key = function_key + 4;
    }
}

void send_edited_menu_line(uint8_t chr) {
    win_menuitem(kV6MenuCurrentItemChanged, global_define_line, chr, 0, nullptr, 0);
}

void adjust_definitions_window(void) {

    uint16_t fkey = fkeys_table_addr + 2 + 4 * global_define_line;

    int x_size = (DEFINITIONS_WIDTH + 5) * gcellw + 1 + 2 * ggridmarginx;
    if (x_size > gscreenw)
        x_size = gscreenw;

    int left = (gscreenw - x_size) / 2;
    int16_t linmax = user_word(fkeys_table_addr) / 2;
    int y_size = (linmax + 1) * gcellh + 2 * ggridmarginy;
    if (y_size > gscreenh)
        x_size = gscreenh;
    int top = (gscreenh - y_size) / 2;


    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, user_selected_background);

    if (DEFINITIONS_WINDOW.id == nullptr) {
        DEFINITIONS_WINDOW.id = gli_new_window(wintype_TextGrid, 0);
    } else {
        win_refresh(DEFINITIONS_WINDOW.id->peer, 0, 0);
    }

    v6_define_window( &DEFINITIONS_WINDOW, left, top, x_size, y_size);

    set_current_window(&DEFINITIONS_WINDOW);
    win_setbgnd(DEFINITIONS_WINDOW.id->peer, user_selected_background);
    glk_request_mouse_event(DEFINITIONS_WINDOW.id);

    display_softs();

    // Print selected line a second time
    // just to move the cursor into position
    display_soft(fkey, global_define_line, false, false);
    win_menuitem(kV6MenuSelectionChanged, global_define_line < 15 ? global_define_line : global_define_line - 1, 18, 0, nullptr, 0);
    glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_BackColor);
}

// Shared between Zork Zero and Shogun
void V_DEFINE(void) {
    int linmax;
    uint16_t fkey, fdef, clicked_line, pressed_fkey, length;

    if (is_spatterlight_zork0) {
        update_user_defined_colours();
    }

    glk_cancel_line_event(V6_TEXT_BUFFER_WINDOW.id, nullptr);

    clear_image_buffer();
    win_sizewin(graphics_bg_glk->peer, 0, 0, gscreenw, gscreenh);
    glk_window_clear(graphics_bg_glk);

    v6_define_window(&V6_TEXT_BUFFER_WINDOW, 0, 0, 0, 0);
    v6_define_window(&V6_STATUS_WINDOW, 0, 0, 0, 0);
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_ReverseColor, 0);
    screenmode = MODE_DEFINE;

    fkey = fkeys_table_addr + 2 + 4 * global_define_line;
    fdef = user_word(fkey + 2);

    linmax = user_word(fkeys_table_addr) / 2;

    adjust_definitions_window();

//    z0_erase_screen();

    winid_t gwin = DEFINITIONS_WINDOW.id;

    bool finished = false;

    while (!finished)  {
        int16_t new_line = global_define_line;
        uint16_t chr = internal_read_char();
        switch (chr) {
            case ZSCII_CLICK_SINGLE:
                // fallthrough
            case ZSCII_CLICK_DOUBLE:
                glk_request_mouse_event(gwin);
                clicked_line = word(header.extension_table + 4);
                if (clicked_line <= 1) {
                    break;
                }
                new_line = clicked_line - 2;

                // Deselect the old line and select the clicked one.
                // This is identical to the code at the end of the input loop,
                // but must be repeated here in order to make the double click
                // apply to the right line
                if (global_define_line != new_line) {
                    display_soft(fkey, global_define_line, true, false);
                    fkey = fkeys_table_addr + 2 + 4 * new_line;
                    display_soft(fkey, new_line, false, false);
                    global_define_line = new_line;
                    fdef = user_word(fkey + 2);
                }

                // if we double clicked a command (as opposed to a definition)
                // we fall through and treat it as ZSCII_NEWLINE.
                // Otherwise we break.
                if (!(chr == ZSCII_CLICK_DOUBLE && (int16_t)user_word(fkey) < 0)) {
                    break;
                }
                // fallthrough
            case ZSCII_NEWLINE:
                // if we are on a static command, run the associated routine
                if ((int16_t)user_word(fkey) < 0) {
                    if (internal_call(user_word(fdef + 2))) {
                        // Exit menu if the call returns true
                        finished = true;
                    } else {
                        // Otherwise select EXIT (bottom line).
                        // (We send a kV6MenuExited here to make sure that the VoiceOver menu is up-to-date)
                        win_menuitem(kV6MenuExited, 0, 0, 0, nullptr, 0);
                        display_softs();
                        new_line = linmax - 1;
                    }
                    break;
                }

                // If we are not on a static command, we fall through
                // and treat ZSCII_NEWLINE as ZSCII_DOWN.

                // fallthrough
            case ZSCII_DOWN:
            case ZSCII_KEY2:
                new_line++;
                if (new_line < linmax) {
                    // Skip the blank line between definitions
                    // and commands
                    if (user_word(fkeys_table_addr + 4 + 4 * new_line) == 0)
                        new_line++;
                } else {
                    new_line = 0;
                }
                break;

            case ZSCII_UP:
            case ZSCII_KEY8:
                new_line--;
                if (new_line >= 0) {
                    // Skip the blank line between definitions
                    // and commands
                    if (user_word(fkeys_table_addr + 4 + 4 * new_line) == 0)
                        new_line--;
                } else {
                    new_line = linmax - 1;
                }
                break;

            default:
                // If the user pressed a function key, move to the corresponding line
                pressed_fkey = scan_table(chr, fkeys_table_addr + 2, user_word(fkeys_table_addr), false);
                if (pressed_fkey) {
                    new_line = (pressed_fkey - fkeys_table_addr) / 4;
                    // skip text editing if we are on a static command line
                } else if ((int16_t)user_word(fkey) >= 0) {
                    if (chr == ZSCII_BACKSPACE || chr == 127) {
                        length = user_byte(fdef + 1);
                        if (length != 0) {
                            length--;
                            store_byte(fdef + 1, length);
                            store_byte(fdef + length + 2, ZSCII_SPACE);
                            // print cursor at new position
                            glk_window_move_cursor(gwin, length + 4, global_define_line + 1);
                            if (length + 1 < user_byte(fdef)) {
                                print_reverse_video_space();
                            }
                            glk_put_char(UNICODE_SPACE);
                            glk_window_move_cursor(gwin, length + 4, global_define_line + 1);
                            send_edited_menu_line(0);
                        } else {
                            win_beep(1);
                        }
                    } else if (chr >= ZSCII_SPACE && chr < 127) {
                        length = user_byte(fdef + 1);
                        if (length + 1 >= user_byte(fdef)) {
                            win_beep(1);
                            // If the command has ZSCII_NEWLINE at the end, allow no more characters
                        } else if (scan_table(ZSCII_NEWLINE, fdef + 2, user_byte(fdef + 1), true) != 0) {
                            win_beep(1);
                        } else {
                            if (chr == '|' || chr == '!') {
                                chr = ZSCII_NEWLINE;
                            }
                            // store new length
                            store_byte(fdef + 1, length + 1);

                            // make lowercase
                            if (chr >= 'A' && chr <= 'Z')
                                chr += 32;

                            // add the typed character to the end of the definition string
                            store_byte(fdef + length + 2, chr);
                            // and print it
                            if (chr == ZSCII_NEWLINE) {
                                chr = '|';
                            }
                            glk_put_char(chr);
                            send_edited_menu_line(chr);
                            // print cursor at the new position if we are not at max
                            if (length + 2 < user_byte(fdef)) {
                                print_reverse_video_space();
                                glk_window_move_cursor(gwin, length + 5, global_define_line + 1);
                            } else {
                                // print cursor at end of line if we are at max
                                glk_window_move_cursor(gwin, user_byte(fdef) + 3, global_define_line + 1);
                                print_reverse_video_space();
                            }
                        }
                    } else {
                        win_beep(1);
                    }
                } else {
                    win_beep(1);
                }
                break;
        }

        // Deselect the old line
        // and select the new one
        if (global_define_line != new_line) {
            display_soft(fkey, global_define_line, true, true);
            display_soft(fkeys_table_addr + 2 + 4 * new_line, new_line, false, false);
            global_define_line = new_line;
            win_menuitem(kV6MenuSelectionChanged, global_define_line < 15 ? global_define_line : global_define_line - 1, 18, 0, nullptr, 0);
            fkey = fkeys_table_addr + 2 + 4 * global_define_line;
            fdef = user_word(fkey + 2);
        }
    };

    v6_delete_win(&DEFINITIONS_WINDOW);
    screenmode = MODE_NORMAL;
    global_define_line = 0;

    win_menuitem(kV6MenuExited, 0, 0, 0, nullptr, 0);

    if (is_spatterlight_zork0) {
        z0_update_on_resize();
    } else {
        // Game is Shogun
        internal_call(pack_routine(sr.V_REFRESH));
        shogun_update_on_resize();
    }
}

#pragma mark HINTS SCREEN

// Shared between Zork Zero, Shogun, and Arthur

// To keep track of the main buffer window while
// using a grid for the hints menu
winid_t stored_bufferwin = nullptr;

uint8_t hint_chapter_global_idx = 0;
uint8_t hint_quest_global_idx = 0;

uint16_t hints_table_addr = 0;

static glui32 upperwin_foreground = zcolor_Default; // black
static glui32 upperwin_background = zcolor_Default; // white

InfocomV6MenuType hints_depth = kV6MenuTypeTopic;

static uint16_t h_chapt_num = 1;
static uint16_t h_quest_num = 1;

// HINT-COUNTS in original source
// 0xe9d2 is value for Zork Zero release 393
// It is changed in entrypoints.cpp
uint16_t seen_hints_table_addr = 0xe9d2;

static int16_t select_hint_by_mouse(int16_t *chr) {
    glk_cancel_char_event(V6_STATUS_WINDOW.id);
    set_current_window(&V6_TEXT_BUFFER_WINDOW);
    glk_request_mouse_event(V6_STATUS_WINDOW.id);
    glk_request_mouse_event(V6_TEXT_BUFFER_WINDOW.id);

    *chr = internal_read_char();

    if (*chr != ZSCII_CLICK_SINGLE && *chr != ZSCII_CLICK_DOUBLE)
        return 0;

    uint16_t mouse_click_addr = header.extension_table + 2;
    int16_t x = word(mouse_click_addr) - 1;
    int16_t y = word(mouse_click_addr + 2) - 1;

    glui32 upperwidth, lowerwidth, lowerheight;
    glk_window_get_size(V6_STATUS_WINDOW.id, &upperwidth, nullptr);

    int left = (upperwidth - 15) / 2;
    if (left < 0)
        left = upperwidth / 3;
    int right = upperwidth - left;
    if (y == 1) {
        if (x <= left) {
            *chr = 'N';
        } else if (x > right) {
            *chr = ZSCII_NEWLINE;
        } else {
            *chr = 'M';
        }
    } else if (y == 2) {
        if (x <= left) {
            *chr = 'P';
        } else if (x > right) {
            *chr = 'Q';
        }
    }

    y -= 2;

    if (y <= 0) {
        return 0;
    }

    glk_window_get_size(V6_TEXT_BUFFER_WINDOW.id, &lowerwidth, &lowerheight);

    if (x >= lowerwidth / 2) { // in right column
        y += lowerheight;
    }

    return y;
}


//  Arthur has contextual hints that
//  might not be shown, so the internal index of
//  a hint might not match its line on-screen.
//  If the game is not Arthur, these two functions
//  return the argument unchanged

static uint16_t index_to_line(uint16_t index) {
    if (is_spatterlight_arthur) {
        uint16_t max = user_word(at.K_HINT_ITEMS);
        for (int i = 1; i <= max; i++) {
            if (user_word(at.K_HINT_ITEMS + i * 2) == index)
                return i;
        }
        return user_word(at.K_HINT_ITEMS + 2);
    }
    return index;
}

static uint16_t line_to_index(uint16_t line) {
    if (is_spatterlight_arthur) {
        uint16_t max = user_word(at.K_HINT_ITEMS);
        if (line > 1 && line <= max)
            return (user_word(at.K_HINT_ITEMS + line * 2));
        return 1;
    }
    return line;
}


static void draw_hints_windows(void) {

    update_user_defined_colours();

    upperwin_foreground = user_selected_foreground;
    upperwin_background = user_selected_background;

    bool is_macintosh = (graphics_type == kGraphicsTypeMacBW || (options.int_number == INTERP_MACINTOSH && graphics_type == kGraphicsTypeAmiga));

    if (!is_spatterlight_arthur) {
        switch (graphics_type) {
            case kGraphicsTypeVGA:
            case kGraphicsTypeBlorb:
            case kGraphicsTypeAmiga:
                upperwin_background = zcolor_Default;
                break;
            case kGraphicsTypeEGA:
                upperwin_background = BROWN;
                break;
            case kGraphicsTypeMacBW:
            case kGraphicsTypeCGA:
            case kGraphicsTypeApple2:
                upperwin_foreground = user_selected_background;
                upperwin_background = user_selected_foreground;
            case kGraphicsTypeNoGraphics:
                break;
        }
        if (options.int_number == INTERP_MACINTOSH && graphics_type == kGraphicsTypeAmiga) {
            upperwin_foreground = 0;
        } else if (graphics_type == kGraphicsTypeVGA || graphics_type == kGraphicsTypeEGA || graphics_type == kGraphicsTypeBlorb) {
            upperwin_foreground = 0xffffff;
            if (upperwin_background == 0xffffff) {
                upperwin_foreground = user_selected_background;
                if (upperwin_foreground == 0xffffff)
                    upperwin_foreground = 0;
            }
        }
        if (is_spatterlight_zork0) {
            z0_erase_screen();
            garglk_set_zcolors_stream(glk_window_get_stream(V6_STATUS_WINDOW.id), upperwin_foreground, upperwin_background);
        }
    }

    if (is_spatterlight_arthur || (upperwin_background != zcolor_Default && upperwin_background != BROWN))
        win_setbgnd(V6_STATUS_WINDOW.id->peer, upperwin_background);

    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, user_selected_background);

    if (hints_depth != kV6MenuTypeHint && V6_TEXT_BUFFER_WINDOW.id->type == wintype_TextBuffer) {
        v6_remap_win(&V6_TEXT_BUFFER_WINDOW, wintype_TextGrid, &stored_bufferwin);
    } else if (V6_TEXT_BUFFER_WINDOW.id->type == wintype_TextGrid) {
        v6_delete_win(&V6_TEXT_BUFFER_WINDOW);
        V6_TEXT_BUFFER_WINDOW.id = stored_bufferwin;
        v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
    }

    win_refresh(V6_TEXT_BUFFER_WINDOW.id->peer, 0, 0);
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);

    int width = 1;
    int status_x = 0;

    V6_STATUS_WINDOW.x = 0;
    V6_STATUS_WINDOW.y = 0;

    int height = gcellh * 4 + ggridmarginy * 2;

    if (is_spatterlight_zork0) {
        // Global BORDER-ON, false if in text-only mode
        bool text_only_mode = (get_global(zg.BORDER_ON) == 0);
        if (!text_only_mode) {
            DISPLAY_BORDER(HINT_BORDER);
            get_image_size(TEXT_WINDOW_PIC_LOC, &width, &height);
            width = width * imagescalex;
            height = height * imagescaley;
        }
    }

    if (is_spatterlight_arthur) {
        win_refresh(V6_STATUS_WINDOW.id->peer, 0, 0);
    } else if (is_spatterlight_shogun) {
        if (graphics_type == kGraphicsTypeApple2) {
            int bannerheight;
            get_image_size(P_HINT_BORDER, nullptr, &bannerheight);
            height += (bannerheight + 2) * imagescaley - gcellh;
        } else {
            get_image_size(P_HINT_LOC, &width, &height);
            if (graphics_type == kGraphicsTypeCGA) {
                width += 3;
            } else if (graphics_type == kGraphicsTypeAmiga) {
                height += 1;
            }
            width *= imagescalex;
            height *= imagescaley;
            if (!is_macintosh) {
                status_x = width;
            }
        }
    } else if (is_spatterlight_zork0 && graphics_type == kGraphicsTypeApple2) {
        // Just for the lulz we try to use the Apple II hint screen header in Zork 0
        // that the actual game did not use (presumably because it was impossible to
        // fit the text in its low resolution.) We cut off 23 "pixels" on each side of
        //  the status text view to make it fit between the question marks in the image.
        status_x = 23 * imagescalex;
    }
    v6_define_window(&V6_STATUS_WINDOW, status_x, 1, gscreenw - 2 * status_x, gcellh * 3 + 2 * ggridmarginy);

    if (height < V6_STATUS_WINDOW.y_size + 1) {
        height = V6_STATUS_WINDOW.y_size + 1;
    }

    v6_define_window(&V6_TEXT_BUFFER_WINDOW, width, height, gscreenw - 2 * width, gscreenh - height);

    flush_bitmap(current_graphics_buf_win);

    win_refresh(V6_TEXT_BUFFER_WINDOW.id->peer, 0, 0);

    glk_window_clear(V6_STATUS_WINDOW.id);
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);

    set_current_window(&V6_TEXT_BUFFER_WINDOW);
    glk_request_char_event(V6_TEXT_BUFFER_WINDOW.id);

    if (is_spatterlight_shogun) {
        shogun_display_border(P_HINT_BORDER);
    } else if  (is_spatterlight_zork0) {
        z0_display_border(Z0_HINT_BORDER);
    }
}

static void center_line(const char *str, int line, int length, bool reverse) {
    glui32 width;
    winid_t win = V6_STATUS_WINDOW.id;
    glk_window_get_size(win, &width, nullptr);
    if (length > width)
        width = length;
    glk_window_move_cursor(win, (width - length) / 2, line - 1);
    if (reverse) {
        if (is_spatterlight_shogun) {
            garglk_set_zcolors(upperwin_foreground, upperwin_background);
        }
        garglk_set_reversevideo(1);
    }
    glk_put_string(const_cast<char *>(str));
    garglk_set_reversevideo(0);
    if (is_spatterlight_shogun) {
        garglk_set_zcolors(upperwin_foreground, zcolor_Default);
    }
}

static void right_line(const char *str, int line, int length) {
    glui32 width;
    winid_t win = V6_STATUS_WINDOW.id;
    glk_window_get_size(win, &width, nullptr);
    if (length > width)
        width = length;
    glk_set_window(win);
    glk_window_move_cursor(win, width - length, line - 1);
    glk_put_string(const_cast<char *>(str));
}

static void left_line(const char *str, int line) {
    winid_t win = V6_STATUS_WINDOW.id;
    glk_window_move_cursor(win, 0, line - 1);
    glk_put_string(const_cast<char *>(str));
}

static void hint_title(const char *title, int length) {
    if (hints_depth != kV6MenuTypeHint)
        win_menuitem(kV6MenuExited, 0, 0, 0, nullptr, 0);
    winid_t win = V6_STATUS_WINDOW.id;
    glk_set_window(win);
    glk_window_clear(win);
    center_line(title, 1, length, true);

    if (hints_depth != kV6MenuTypeHint) {
        left_line("N for next item.", 2);
        left_line("P for previous item.", 3);
    }

    if (hints_depth != kV6MenuTypeTopic) {
        center_line("M for hint menu.", 2, 16, false);
    }
    if (mouse_available()) {
        center_line("(Or use mouse.)", 3, 16, false);
    }

    if (hints_depth == kV6MenuTypeHint)
        right_line("Return for a hint.", 2, 18);
    else
        right_line("Return for hints.", 2, 17);

    right_line("Q to resume story.", 3, 18);

    win_menuitem(kV6MenuTitle, 0, hints_depth, 0, const_cast<char *>(title), length);
}

static bool rt_see_qst(uint16_t obj) {
    return (internal_call(pack_routine(ar.RT_SEE_QST), {obj}) == 1);
}

static uint16_t hint_question_name(uint16_t question) {
    uint16_t result = user_word(hints_table_addr + h_chapt_num * 2);
    uint16_t base_address = user_word(result + (question + 1) * 2);

    if (is_game(Game::Arthur)) {
        base_address += 2;
        if (!rt_see_qst(user_word(base_address)))
            return 0;
    }

    return user_word(base_address + 2);
}

static bool arthur_is_topic_in_context(uint16_t topic) {
    int16_t max = user_word(topic);
    for (uint16_t i = 2; i <= max; i++) {
        if (rt_see_qst(user_word(user_word(topic + i * 2) + 2)))
            return true;
    }
    return false;
}

static uint16_t hint_topic_name(uint16_t chapter) {
    uint16_t topic = user_word(hints_table_addr + chapter * 2);

    if (is_spatterlight_arthur && !arthur_is_topic_in_context(topic))
        return 0;

    return user_word(topic + 2);
}

static int16_t get_seen_hints(void) {

    // seen_hints_table_addr points to a byte table that keeps track of
    // how many hints have been shown for a particular question.
    // It is actually a nibble table where the high four bits of each byte store
    // odd question numbers; the low four bits store even question numbers.

    int16_t cv = user_word(seen_hints_table_addr + (h_chapt_num - 1) * 2);
    int16_t address = cv + (h_quest_num - 1) / 2;
    int16_t seen = user_byte(address);
    bool odd = ((h_quest_num & 1) == 1);
    if (odd) {
        seen = seen >> 4;
    }
    return seen & 0xf;
}


static void print_and_send_menuitem(uint16_t str, int max) {
    char string[4000];
    int length = print_long_zstr_to_cstr(str, string, 4000);
    glk_put_string(string);
    int index;
    switch (hints_depth) {
        case kV6MenuTypeQuestion:
            index = h_quest_num - 1;
            break;
        case kV6MenuTypeTopic:
            index = h_chapt_num - 1;
            break;
        case kV6MenuTypeHint:
            index = get_seen_hints() - 1;
            break;
        default:
            fprintf(stderr, "send_menuitem: Illegal menu type!\n");
            return;
    }
    if (index >= max)
        index = max - 1;
    win_menuitem(hints_depth, index, max, 0, string, length);
}



static int hint_put_up_frobs(uint16_t max, uint16_t start) {
    uint16_t x = 0;
    uint16_t y = 0;
    glui32 width, height;

    if (V6_TEXT_BUFFER_WINDOW.id->type == wintype_TextBuffer) {
        glk_cancel_char_event(V6_TEXT_BUFFER_WINDOW.id);
        v6_remap_win(&V6_TEXT_BUFFER_WINDOW, wintype_TextGrid, &stored_bufferwin);
        win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);
        glk_request_char_event(V6_TEXT_BUFFER_WINDOW.id);
    }
    glk_window_get_size(V6_TEXT_BUFFER_WINDOW.id, &width, &height);
    set_current_window(&V6_TEXT_BUFFER_WINDOW);

    uint16_t str;
    int number_of_entries = 0;
    for (int i = start; i <= max; i++) {
        if (hints_depth == kV6MenuTypeTopic) {
            str = hint_topic_name(i);
        } else {
            str = hint_question_name(i);
        }
        if (str == 0)
            continue;
        number_of_entries++;
        if (is_spatterlight_arthur) {
            store_word(at.K_HINT_ITEMS + number_of_entries * 2, i);
        }
        glk_window_move_cursor(V6_TEXT_BUFFER_WINDOW.id, x, y);
        print_and_send_menuitem(str, number_of_entries);

        y++;
        V6_TEXT_BUFFER_WINDOW.x = 1;
        if (y == height) {
            y = 0;
            x = width / 2;
        }
    }
    if (is_spatterlight_arthur) {
        store_word(at.K_HINT_ITEMS, number_of_entries); // the first word of a table contains the length
    }
    return number_of_entries;
}

static int hint_new_cursor(uint16_t pos, bool reverse) {
    uint16_t x = 0;
    if ((int16_t)pos < 1)
        pos = 1;
    uint16_t y = pos - 1;
    glui32 width, height;
    glk_window_get_size(V6_TEXT_BUFFER_WINDOW.id, &width, &height);
    if (pos > height) {
        x = width / 2;
        y -= height;
    }
    glk_window_move_cursor(V6_TEXT_BUFFER_WINDOW.id, x, y);
    V6_TEXT_BUFFER_WINDOW.x = 1;
    if (reverse)
        garglk_set_reversevideo(1);

    uint16_t index = line_to_index(pos);
    uint16_t string_address;
    if (hints_depth == kV6MenuTypeTopic) {
        string_address = hint_topic_name(index);
    } else {
        string_address = hint_question_name(index);
    }
    print_handler(unpack_string(string_address), nullptr);
    garglk_set_reversevideo(0);
    return x;
}

// Store the index of the last shown
// hint in the nibble table
static void store_hints_seen(uint8_t value) {
    int16_t cv = user_word(seen_hints_table_addr + (h_chapt_num - 1) * 2);
    int16_t address = cv + (h_quest_num - 1) / 2;
    uint8_t seen = user_byte(address);
    bool odd = ((h_quest_num & 1) == 1);
    if (odd) {
        seen = seen & 0x0f;
        value = value << 4;
    } else {
        seen = seen & 0xf0;
        value = value & 0x0f;
    }
    value = seen | value;
    store_byte(address, value);
}

static int16_t init_hints(int16_t *hints_base_address) {

    int16_t h = user_word(user_word(hints_table_addr + h_chapt_num * 2) + (h_quest_num + 1) * 2);
    char str[64];

    int offset = is_spatterlight_arthur ? 4 : 2;

    int length = print_long_zstr_to_cstr(user_word(h + offset), str, 64);
    hint_title(str, length);

    int16_t seen = get_seen_hints();
    int16_t max = user_word(h) - 1;

    if (seen == max) {
        // Remove the text "Return for a hint" if there are no more hints.
        right_line("                  ", 2, 18);
    }
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);

    if (hints_base_address != nullptr)
        *hints_base_address = h;
    return max;
}

static bool display_hints(bool only_refresh) {
    hints_depth = kV6MenuTypeHint;

    win_menuitem(kV6MenuExited, 0, 0, 0, nullptr, 0);

    int16_t hints_base_address;

    if (V6_TEXT_BUFFER_WINDOW.id && V6_TEXT_BUFFER_WINDOW.id->type == wintype_TextGrid) {
        v6_delete_win(&V6_TEXT_BUFFER_WINDOW);
        V6_TEXT_BUFFER_WINDOW.id = gli_new_window(wintype_TextBuffer, 0);
        v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
    }
    int16_t max = init_hints(&hints_base_address);
    set_current_window(&V6_TEXT_BUFFER_WINDOW);
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);

    int16_t seen = get_seen_hints();

    if (seen <= 1) {
        win_menuitem(kV6MenuTypeHint, 0, max - 1, 0, nullptr, 0);
    }

    bool done = false;
    bool result = false;

    int16_t cnt;

    for (cnt = is_game(Game::Arthur) ? 1 : 0; cnt <= max; cnt++) {
        store_hints_seen(cnt);
        if (cnt == max) {
            char *str = const_cast<char *>("[No more hints.]\n");
            glk_put_string(str);
            win_menuitem(kV6MenuTypeHint, max, max - 1, 0, str, 17);

            // Remove "Return for a hint."
            right_line("                  ", 2, 18);
        } else {
            print_number(max - cnt);
            glk_put_char('>');
            glk_put_char(UNICODE_SPACE);
        }
        set_current_window(&V6_TEXT_BUFFER_WINDOW);
        if (cnt >= seen) {
            if (only_refresh) {
                return false;
            }
            bool loop = true;
            while (loop) {
                int16_t chr;
                select_hint_by_mouse(&chr);
                switch (chr) {
                    case ZSCII_NEWLINE:
                    case UNICODE_LINEFEED:
                        if (cnt < max) {
                            loop = false;
                        } else {
                            win_beep(1);
                        }
                        break;
                    case 'm':
                    case 'M':
                        result = true;
                    case 'q':
                    case 'Q':
                        done = true;
                        loop = false;
                        break;
                    default:
                        win_beep(1);
                        break;
                }
            }

            if (done)
                break;
        }

        if (cnt < max) {
            int16_t str = user_word(hints_base_address + (cnt + 2) * 2);
            print_and_send_menuitem(str, max - 1);
            glk_put_char(UNICODE_LINEFEED);
        }
    }

    if (V6_TEXT_BUFFER_WINDOW.id->type == wintype_TextBuffer) {
        stored_bufferwin = V6_TEXT_BUFFER_WINDOW.id;
        win_sizewin(stored_bufferwin->peer, 0, 0, 0, 0);
        V6_TEXT_BUFFER_WINDOW.id = nullptr;
    }
    v6_remap_win_to_grid(&V6_TEXT_BUFFER_WINDOW);
    glk_request_char_event(V6_TEXT_BUFFER_WINDOW.id);

    hints_depth = kV6MenuTypeQuestion;

    return result;
}

static bool hint_inner_menu_loop(uint16_t *index, uint16_t num_entries, InfocomV6MenuType name_routine, bool *exit_hint_menu);

static int display_questions(void) {
    char str[30];
    int length = print_long_zstr_to_cstr(user_word(user_word(hints_table_addr + h_chapt_num * 2) + 2), str, 30);
    hint_title(str, length);
    uint16_t max = user_word(user_word(hints_table_addr + h_chapt_num * 2)) - 1;
    int number_of_entries = hint_put_up_frobs(max, 1);
    hint_new_cursor(index_to_line(h_quest_num), true);
    return number_of_entries;
}

static bool hint_pick_question(void) {
    bool result = true;

    bool outer_loop = true;

    while (outer_loop) {
        hints_depth = kV6MenuTypeQuestion;

        glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
        uint16_t max = display_questions();
        outer_loop = hint_inner_menu_loop(&h_quest_num, max, kV6MenuTypeQuestion, &result);
    }
    hints_depth = kV6MenuTypeTopic;
    return result;
}

static bool hint_inner_menu_loop(uint16_t *index, uint16_t num_entries, InfocomV6MenuType local_menu_depth, bool *exit_hint_menu) {
    bool loop = true;
    bool outer_loop = true;
    bool done = true;
    uint16_t selected_line = index_to_line(*index);
    glui32 column_height;

    while (loop) {
        uint16_t new_selection = selected_line;
        *index = line_to_index(selected_line);
        int16_t chr;
        int16_t mouse = select_hint_by_mouse(&chr);
        glk_window_get_size(V6_TEXT_BUFFER_WINDOW.id, nullptr, &column_height);
        switch (chr) {
            case 'Q':
            case 'q':
                done = false;
                outer_loop = false;
                loop = false;
                break;
            case 'm':
            case 'M':
                if (local_menu_depth == kV6MenuTypeQuestion) {
                    loop = false;
                    outer_loop = false;
                } else {
                    win_beep(1);
                }
                break;
            case ZSCII_LEFT:
                if (selected_line > column_height)
                    selected_line -= column_height;
                else
                    win_beep(1);
                break;
            case ZSCII_RIGHT:
                if (num_entries >= selected_line + column_height)
                    selected_line += column_height;
                else
                    win_beep(1);
                break;
            case 'n':
            case 'N':
            case ZSCII_DOWN:
                if (num_entries == 1) {
                    win_beep(1);
                } else if (selected_line == num_entries)
                    selected_line = 1;
                else
                    selected_line++;
                break;
            case 'p':
            case 'P':
            case ZSCII_UP:
                if (num_entries == 1) {
                    win_beep(1);
                } else if (selected_line == 1)
                    selected_line = num_entries;
                else
                    selected_line--;
                break;
            case ZSCII_CLICK_SINGLE:
            case ZSCII_CLICK_DOUBLE:
                if (mouse >= 1 && mouse <= num_entries) {
                    selected_line = mouse;
                    if (chr == ZSCII_CLICK_SINGLE) {
                        break;
                    }
                } else {
                    win_beep(1);
                    break;
                }
            case ZSCII_NEWLINE:
            case UNICODE_LINEFEED:
            case ZSCII_SPACE:
                if (local_menu_depth == kV6MenuTypeTopic) {
                    outer_loop = hint_pick_question();
                } else {
                    outer_loop = display_hints(false);
                    done = outer_loop;
                }
                loop = false;
                break;
            default:
                win_beep(1);
        }
        if (selected_line != new_selection) {
            hint_new_cursor(new_selection, false);
            hint_new_cursor(selected_line, true);

            win_menuitem(kV6MenuSelectionChanged, selected_line - 1, 0, 0, nullptr, 0);

            if (local_menu_depth == kV6MenuTypeTopic) {
                h_quest_num = 1;
                set_global(hint_quest_global_idx, 1);
                set_global(hint_chapter_global_idx, line_to_index(selected_line));
            } else {
                set_global(hint_quest_global_idx, line_to_index(selected_line));
            }
        }
    }
    if (exit_hint_menu != nullptr) {
        *exit_hint_menu = done;
    }

    return outer_loop;
}

static int display_topics(void) {
    hint_title(const_cast<char *>(" InvisiClues (tm) "), 18);
    int number_of_entries = hint_put_up_frobs(user_word(hints_table_addr), 1);
    hint_new_cursor(index_to_line(h_chapt_num), true);
    return number_of_entries;
}

void redraw_hint_screen_on_resize(void) {
    draw_hints_windows();
    switch (hints_depth) {
        case kV6MenuTypeHint:
            display_hints(true);
            break;
        case kV6MenuTypeQuestion:
            display_questions();
            break;
        default:
            display_topics();
            break;
    }
}

void DO_HINTS(void) {
    // Screenmode will be MODE_HINTS on autorestore,
    // and we need to store the mode we were in before
    // entering hint screen.
    if (is_spatterlight_arthur)
        arthur_sync_screenmode();

    V6ScreenMode stored_mode = screenmode;

    glk_cancel_line_event(V6_TEXT_BUFFER_WINDOW.id, nullptr);
    // Disallow transcript in main buffer window
    V6_TEXT_BUFFER_WINDOW.attribute &= ~WINATTR_TRANSCRIPTING_BIT;

    if (is_spatterlight_shogun || is_spatterlight_arthur) {
        v6_delete_win(&windows[2]);
        if (is_spatterlight_arthur) {
            clear_image_buffer();
            if (current_graphics_buf_win)
                glk_window_clear(current_graphics_buf_win);

            // Delete the topmost window used by Arthur in its status,
            // inventory, and room description modes.
        } else {
            v6_delete_win(&windows[4]);
            v6_delete_win(&windows[5]);
        }
    }

    h_chapt_num = get_global(hint_chapter_global_idx);
    h_quest_num = get_global(hint_quest_global_idx);

    screenmode = MODE_HINTS;
    draw_hints_windows();

    int number_of_entries;
    bool outer_loop = true;

    while (outer_loop) {
        glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);

        // Unless we are autorestoring (and this is the first iteration of the loop),
        // hints_depth will be kV6MenuTypeTopic
        switch (hints_depth) {
            case kV6MenuTypeQuestion:
                display_questions();
                outer_loop = hint_pick_question();
                break;
            case kV6MenuTypeHint:
                outer_loop = display_hints(false);
                break;
            case kV6MenuTypeTopic:
                number_of_entries = display_topics();
                outer_loop = hint_inner_menu_loop(&h_chapt_num, number_of_entries, kV6MenuTypeTopic, nullptr);
                break;
            default:
                fprintf(stderr, "DO_HINTS: Illegal hints_depth value!\n");
                return;
        }
    }

    glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_TextColor);
    glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_BackColor);

    v6_sizewin(&V6_STATUS_WINDOW);

    if (V6_TEXT_BUFFER_WINDOW.id->type != wintype_TextBuffer) {
        v6_delete_win(&V6_TEXT_BUFFER_WINDOW);
        V6_TEXT_BUFFER_WINDOW.id = stored_bufferwin;
    }

    v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);

    screenmode = stored_mode;

    if (screenmode == MODE_HINTS)
        screenmode = MODE_NORMAL;

    stored_bufferwin = nullptr;

    hints_depth = kV6MenuTypeTopic;

    win_menuitem(kV6MenuExited, 0, 0, 0, nullptr, 0);

    win_refresh(V6_STATUS_WINDOW.id->peer, 0, 0);
    
    if (is_spatterlight_zork0) {
        z0_update_on_resize();
    } else if (is_spatterlight_shogun) {
        internal_call(pack_routine(sr.V_REFRESH));
        shogun_update_on_resize();
    }

    // Allow transcript in main buffer window
    V6_TEXT_BUFFER_WINDOW.attribute |= WINATTR_TRANSCRIPTING_BIT;
    glk_put_string(const_cast<char*>("Back to the story...\n"));
    glk_set_echo_line_event(V6_TEXT_BUFFER_WINDOW.id, 0);
}

#pragma mark Credits

void V_CREDITS(void) {
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

void after_V_CREDITS(void) {
    V6_TEXT_BUFFER_WINDOW.style.reset(STYLE_BOLD);
    V6_TEXT_BUFFER_WINDOW.style.reset(STYLE_ITALIC);
    V6_TEXT_BUFFER_WINDOW.style.reset(STYLE_FIXED);
}

void after_V_COLOR(void) {
    uint8_t fg = get_global(fg_global_idx);
    uint8_t bg = get_global(bg_global_idx);

    update_user_defined_colours();

    for (auto &window : windows) {
        // These will already be correctly set unless we are called from the after restore routine
        window.fg_color = Color(Color::Mode::ANSI, fg);
        if (&window == &V6_STATUS_WINDOW) {
            window.bg_color = Color();
        } else {
            window.bg_color = Color(Color::Mode::ANSI, bg);
        }
        winid_t glkwin = window.id;
        if (glkwin != nullptr) {
            if (glkwin->type == wintype_Graphics) {
                glk_window_set_background_color(glkwin, user_selected_background);
                glk_window_clear(glkwin);
            } else {
                if (glkwin->type == wintype_TextBuffer) {
                    win_setbgnd(glkwin->peer, user_selected_background);
                }

                glk_set_window(glkwin);

                // Colours may be set to default (1) if this is called from the after restore routine
                glsi32 zcolfg, zcolbg;
                if (fg == DEFAULT_COLOUR)
                    zcolfg = zcolor_Default;
                else
                    zcolfg = user_selected_foreground;

                if (bg == DEFAULT_COLOUR)
                    zcolbg = zcolor_Default;
                else
                    zcolbg = user_selected_background;

                garglk_set_zcolors(zcolfg, zcolbg);
            }
        }
    }
    update_monochrome_colours();
    if (is_spatterlight_arthur) {
        arthur_update_on_resize();
    } else if (is_spatterlight_shogun) {
        shogun_update_on_resize();
    } else if (is_spatterlight_zork0) {
        z0_update_on_resize();
    }
}


#pragma mark Skip puzzles prompt

bool skip_puzzle_prompt(const char *str) {
    // We skip asking about skipping the puzzle
    // if VoiceOver is off and we did not just
    // autorestore to this prompt.
    if (gli_voiceover_on || dont_repeat_question_on_autorestore) {
        set_current_window(&V6_TEXT_BUFFER_WINDOW);
        if (!dont_repeat_question_on_autorestore)
            transcribe_and_print_string(str);
        dont_repeat_question_on_autorestore = false;
        while (1) {
            uint8_t c = internal_read_char();
            glk_put_char(c);
            transcribe(c);
            transcribe_and_print_string("\n");
            c = tolower(c);
            switch (c) {
                case 'n':
                    transcribe_and_print_string("\n");
                    return false;
                case 'y':
                    transcribe_and_print_string("\n");
                    return true;
                default:
                    transcribe_and_print_string("(Y is affirmative): >");
            }
        }
    }
    return false;
}


#pragma mark Empty functions used by entrypoints code

void DISPLAY_HINT(void) {}
void RT_SEE_QST(void) {}
void V_COLOR(void) {}
void V_BOW(void) {}
void MAZE_F(void) {}
void DESCRIBE_ROOM(void) {}
void DESCRIBE_OBJECTS(void) {}
void WINPROP(void) {}
void SET_BORDER(void) {}
void DRAW_NEW_HERE(void) {}
void DRAW_NEW_COMP(void) {}
void DRAW_COMPASS_ROSE(void) {}
void SETUP_SCREEN(void) {}
void MAP_X(void) {}
void PLAY_SELECTED(void) {}
void SCORE_CHECK(void) {}
void TOWER_WIN_CHECK(void) {}
void DRAW_PEGS(void) {}
void SET_B_PIC(void) {}
void BLINK(void) {}

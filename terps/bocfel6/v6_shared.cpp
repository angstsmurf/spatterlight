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
#include "unicode.h"
#include "zterp.h"

#include "shogun.hpp"
#include "zorkzero.hpp"

#include "v6_shared.hpp"

#define DEFINE_GRID_WINDOW windows[2]

enum EntryNameType {
    HINT_TOPIC,
    HINT_QUESTION,
    HINT_HINT
};

int margin_images[100];
int number_of_margin_images = 0;

void add_margin_image_to_list(int image) {
    for (int i = 0; i < number_of_margin_images; i++) {
        if (margin_images[i] == image) {
            // We have already added this image to the list
            return;
        }
    }
    fprintf(stderr, "z0_add_margin_image: Adding margin image %d to the update list\n", image);
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


#pragma mark DEFINITIONS SCREEN

#define DEFINITIONS_WIDTH 30 // FLEN in original source
#define ARTHUR_K_HINT_ITEMS 0x432e
#define ARTHUR_LAST_OBJECT 0x137

int line_clicked(void) {
    uint16_t mouse_click_addr = header.extension_table + 2;
    int y = word(mouse_click_addr + 2);
    return y;
}

void print_reverse_video_space(void) {
    garglk_set_reversevideo(1);
    glk_put_char(UNICODE_SPACE);
    garglk_set_reversevideo(0);
}

// <CONSTANT FKEYS <PLTABLE !.L>>>
uint16_t fkeys = 0xce2a;

// Return width of the widest user-defined command
int soft_commands_width(void) {
    int widest = 0;
    int num_commands = user_word(fkeys) / 2;
    int fkey = fkeys + 2;
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

void print_center_table(int y, uint16_t string) {
    if (define_window_width == 0)
        define_window_width = soft_commands_width();

    int length = count_characters_in_zstring(string);
    Window win = DEFINE_GRID_WINDOW;
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

uint16_t scan_table(uint16_t value, uint16_t address, uint16_t length, bool bytewise) {
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
void display_soft(int function_key, int index, bool inverse) {

    uint16_t fnames;

    if (is_game(Game::Shogun)) {
        fnames = 0x4b57;
    } else {
        fnames = 0xcd55;
    }

    int fdef, y;
    fdef = user_word(function_key + 2);
    y = index + 1;
    if ((int16_t)user_word(function_key) < 0) { // hard-coded menu item
        if (fdef != 0) {
            DEFINE_GRID_WINDOW.x_cursor = 1;
            DEFINE_GRID_WINDOW.y_cursor = y * gcellh;
            if (inverse) {
                garglk_set_reversevideo(1);
            }
            print_center_table(y, user_word(fdef));
        }
    } else {
        glk_window_move_cursor(DEFINE_GRID_WINDOW.id, 0, y);
        DEFINE_GRID_WINDOW.x_cursor = 1;
        DEFINE_GRID_WINDOW.y_cursor = y * gcellh + 1;

        uint16_t key_name_string = scan_table(user_word(function_key), fnames, user_word(fnames - 2), false);
        if (key_name_string != 0) {
            if (inverse) {
                garglk_set_reversevideo(0);
            } else {
                garglk_set_reversevideo(1);
            }
            print_handler(unpack_string(user_word(key_name_string + 2)), nullptr);
            garglk_set_reversevideo(0);
            glk_put_char(UNICODE_SPACE);
            if (inverse) {
                garglk_set_reversevideo(1);
            }
        }
        int string_length = user_byte(fdef);
        for (int i = 0; i < string_length; i++) {
            uint8_t c = user_byte(fdef + 2 + i);
            if (c == ZSCII_NEWLINE)
                glk_put_char('|');
            else
                glk_put_char(c);
        }
        glsi32 x = user_byte(fdef + 1) + 4;

        // Print cursor if this is the selected line
        if (!inverse) {
            glk_window_move_cursor(DEFINE_GRID_WINDOW.id, x, y);
            print_reverse_video_space();
            glk_window_move_cursor(DEFINE_GRID_WINDOW.id, x, y);
        }
    }
    garglk_set_reversevideo(0);
}

int global_define_line = 0;

void display_softs(void) {
    uint16_t number_of_lines = user_word(fkeys) / 2;
    winid_t win = DEFINE_GRID_WINDOW.id;
    glk_set_window(win);
    glui32 width;
    glk_window_get_size(win, &width, nullptr);
    int16_t xpos = (width - 14) / 2;
    if (xpos < 0)
        xpos = 0;
    glk_window_move_cursor(win, xpos, 0);
    glk_put_string(const_cast<char*>("Function Keys"));
    uint16_t function_key = fkeys + 2;
    for (int i = 0; i < number_of_lines; i++) {
        display_soft(function_key, i, (i != global_define_line));
        function_key = function_key + 4;
    }
}

void z0_erase_screen(void) {
    clear_image_buffer();
    clear_margin_image_list();
    if (z0_right_status_window != nullptr) {
        gli_delete_window(z0_right_status_window);
        z0_right_status_window = nullptr;
    }
    glk_window_clear(windows[0].id);
    glk_window_clear(windows[1].id);
    glk_window_clear(graphics_win_glk);
}


void resize_definitions_window(void) {
    uint8_t SCRH = byte(0x21);
    uint8_t SCRV = byte(0x20);

    uint16_t fkey = fkeys + 2 + 4 * global_define_line;
    uint16_t fdef = user_word(fkey + 2);

    int16_t left = (SCRH - user_byte(fdef)) / 2;
    int16_t linmax = user_word(fkeys) / 2;


    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, gfgcol);
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, gbgcol);

    v6_define_window( &DEFINE_GRID_WINDOW, gcellw * left - ggridmarginx, gcellh * (SCRV - linmax) / 2 - ggridmarginy, (DEFINITIONS_WIDTH + 5) * gcellw + 1 + 2 * ggridmarginx, (linmax + 1) * gcellh + 2 * ggridmarginy);

    winid_t win = DEFINE_GRID_WINDOW.id;
  
    glk_set_window(win);

    garglk_set_zcolors(gfgcol, gbgcol);
    win_setbgnd(win->peer, gbgcol);

    glk_window_clear(win);
    glk_request_mouse_event(win);

    display_softs();

    // Print selected line a second time
    // just to move the cursor into position
    display_soft(fkey, global_define_line, false);
}

// Shared between Zork Zero and Shogun
void V_DEFINE(void) {
    int linmax;
    uint16_t fkey, fdef, clicked_line, pressed_fkey, length;

    if (is_game(Game::ZorkZero)) {
        fkeys = 0xce2a;
        update_user_defined_colors();
        win_sizewin(graphics_win_glk->peer, 0, 0, gscreenw, gscreenh);
        glk_window_set_background_color(graphics_win_glk, user_selected_background);
        v6_delete_win(&windows[1]);
        //        v6_delete_win(&windows[0]);

    } else {
        fkeys = 0x4dc8;
        v6_delete_win(&windows[1]);
    }

    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_ReverseColor, 0);
    glk_window_clear(graphics_win_glk);
    screenmode = MODE_DEFINE;

    fkey = fkeys + 2;
    fdef = user_word(fkey + 2);

    z0_erase_screen();

    Window *win = &DEFINE_GRID_WINDOW;

    if (win->id == nullptr) {
        win->id = v6_new_glk_window(wintype_TextGrid);
    }

    winid_t gwin = win->id;

    linmax = user_word(fkeys) / 2;

    global_define_line = 0;

    resize_definitions_window();

    bool finished = false;

    while (!finished)  {
        int16_t new_line = global_define_line;
        uint16_t chr = read_char();
        switch (chr) {
            case ZSCII_CLICK_SINGLE:
                // fallthrough
            case ZSCII_CLICK_DOUBLE:
                glk_request_mouse_event(gwin);
                clicked_line = line_clicked();
                if (clicked_line <= 1) {
                    break;
                }
                new_line = clicked_line - 2;

                // Deselect the old line and select the clicked one.
                // This is identical to the code at the end of the input loop
                // but must be repeated here in order to make the double click
                // below apply to the right line
                if (global_define_line != new_line) {
                    display_soft(fkey, global_define_line, true);
                    fkey = fkeys + 2 + 4 * new_line;
                    display_soft(fkey, new_line, false);
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
                        // Otherwise select EXIT (bottom line)
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
                    if (user_word(fkeys + 4 + 4 * new_line) == 0)
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
                    if (user_word(fkeys + 4 + 4 * new_line) == 0)
                        new_line--;
                } else {
                    new_line = linmax - 1;
                }
                break;

            default:
                // If the user pressed a function key, move to the corresponding line
                pressed_fkey = scan_table(chr, fkeys + 2, user_word(fkeys), false);
                if (pressed_fkey) {
                    new_line = (pressed_fkey - fkeys) / 4;
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
                                glk_put_char('|');
                            } else {
                                glk_put_char(chr);
                            }
                            // print cursor at the new position
                            if (length + 2 < user_byte(fdef)) {
                                print_reverse_video_space();
                                glk_window_move_cursor(gwin, length + 5, global_define_line + 1);
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
            display_soft(fkey, global_define_line, true);
            display_soft(fkeys + 2 + 4 * new_line, new_line, false);
            global_define_line = new_line;
            fkey = fkeys + 2 + 4 * global_define_line;
            fdef = user_word(fkey + 2);
        }
    };

    v6_delete_win(win);
    screenmode = MODE_NORMAL;
    if (is_game(Game::ZorkZero)) {
        z0_update_on_resize();
    } else {
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_ReverseColor, 1);
        internal_call(pack_routine(0x183a4)); // V-REFRESH
    }
}

#pragma mark HINTS SCREEN

void DISPLAY_BORDER(BorderType border);

int16_t select_hint_by_mouse(int16_t *chr) {

    *chr = read_char();

    if (*chr != ZSCII_CLICK_SINGLE && *chr != ZSCII_CLICK_DOUBLE)
        return 0;

    uint16_t mouse_click_addr = header.extension_table + 2;
    int16_t x = word(mouse_click_addr) - 1;
    int16_t y = word(mouse_click_addr + 2) - 1;

    glui32 upperwidth, lowerwidth, lowerheight;
    glk_window_get_size(windows[1].id, &upperwidth, nullptr);

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

    glk_window_get_size(windows[0].id, &lowerwidth, &lowerheight);

    if (x >= lowerwidth / 2) { // in right column
        y += lowerheight;
    }

    return y;
}

glsi32 upperwin_foreground = zcolor_Default; // black
glsi32 upperwin_background = zcolor_Default; // white

// Returns the argument unchanged
// unless the game is Arthur
uint16_t index_to_line(uint16_t index) {
    if (is_game(Game::Arthur)) {
        uint16_t max = user_word(ARTHUR_K_HINT_ITEMS);
        for (int i = 1; i <= max; i++) {
            if (user_word(ARTHUR_K_HINT_ITEMS + i * 2) == index)
                return i;
        }
        return user_word(ARTHUR_K_HINT_ITEMS + 2);
    }
    return index;
}

// Feturns the argument unchanged
// unless the game is Arthur
uint16_t line_to_index(uint16_t line) {
    if (is_game(Game::Arthur)) {
        uint16_t max = user_word(ARTHUR_K_HINT_ITEMS);
        if (line > 1 && line <= max)
            return (user_word(ARTHUR_K_HINT_ITEMS + line * 2));
        return 1;
    }
    return line;
}

void redraw_hints_windows(void) {

    upperwin_background = monochrome_white;
    upperwin_foreground = monochrome_black;

    if (graphics_type == kGraphicsTypeBlorb || graphics_type == kGraphicsTypeVGA || graphics_type == kGraphicsTypeAmiga) {
        upperwin_background = 0x826766;
    } else if (graphics_type == kGraphicsTypeEGA) {
        upperwin_background = 0xd47fd4;
    } else if ((graphics_type == kGraphicsTypeMacBW || graphics_type == kGraphicsTypeCGA) && is_game(Game::Arthur)) {
        upperwin_background = monochrome_black;
        upperwin_foreground = monochrome_white;
    }
    if (graphics_type == kGraphicsTypeVGA || graphics_type == kGraphicsTypeEGA || graphics_type == kGraphicsTypeApple2) {
        upperwin_foreground = 0xffffff;
        if (upperwin_background == 0xffffff)
            upperwin_foreground = user_selected_background;
        if (upperwin_foreground == 0xffffff)
            upperwin_foreground = 0;
    }

    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, upperwin_foreground);
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, upperwin_background);

    win_setbgnd(windows[1].id->peer, upperwin_background);
    win_refresh(windows[1].id->peer, 0, 0);

    v6_define_window(&windows[1], 1, 1, gscreenw, gcellh * 3 + 2 * ggridmarginy);

    windows[0].bg_color = windows[1].bg_color;

    glk_set_window(windows[0].id);
    garglk_set_zcolors(monochrome_black, monochrome_white);

    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, monochrome_black);
    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, monochrome_white);
    win_refresh(windows[0].id->peer, 0, 0);

    int width = 1;
    int height = windows[1].y_size + gcellh;

    if (is_game(Game::ZorkZero)) {
        bool showing_borders = (get_global(0x83) == 1);
        if (showing_borders) {
            DISPLAY_BORDER(HINT_BORDER);
            get_image_size(TEXT_WINDOW_PIC_LOC, &width, &height);
            width = width * imagescalex;
            height = height * imagescaley;
            if (height < windows[1].y_size + 1) {
                height = windows[1].y_size + 1;
            }
        }
    } else if (is_game(Game::Shogun)) {
        shogun_display_border(P_HINT_BORDER);
        width = shogun_graphical_banner_width_left;
        height += a2_graphical_banner_height;
    }

    v6_define_window(&windows[0], width, height, gscreenw - width * 2, gscreenh - windows[0].y_origin);

    flush_bitmap(current_graphics_buf_win);

    glk_set_window(windows[1].id);
}

void init_hint_screen(void) {
    z0_erase_screen();
    v6_delete_win(&windows[0]);
    v6_remap_win_to_grid(&windows[0]);
    windows[0].attribute &= ~1; // remove any wrapping

    gli_delete_window(windows[1].id);
    windows[1].id = gli_new_window(wintype_TextGrid, 0);

    redraw_hints_windows();
}

void center_line(const char *str, int line, int length, bool reverse) {
    glui32 width;
    winid_t win = windows[1].id;
    glk_window_get_size(win, &width, nullptr);
    if (length > width)
        width = length;
    glk_window_move_cursor(win, (width - length) / 2, line - 1);
    if (reverse)
        garglk_set_reversevideo(1);
    glk_put_string(const_cast<char *>(str));
    garglk_set_reversevideo(0);
}

void right_line(const char *str, int line, int length) {
    glui32 width;
    winid_t win = windows[1].id;
    glk_window_get_size(win, &width, nullptr);
    if (length > width)
        width = length;
    glk_set_window(win);
    glk_window_move_cursor(win, width - length, line - 1);
    glk_put_string(const_cast<char *>(str));
}

void left_line(const char *str, int line) {
    winid_t win = windows[1].id;
    glk_window_move_cursor(win, 0, line - 1);
    glk_put_string(const_cast<char *>(str));
}

EntryNameType hints_depth = HINT_TOPIC;

void hint_title(const char *title, int length) {
    winid_t win = windows[1].id;
    glk_set_window(win);
    garglk_set_zcolors(upperwin_foreground, upperwin_background);
    glk_window_clear(win);
    center_line(title, 1, length, true);

    if (hints_depth != HINT_HINT) {
        left_line("N for next item.", 2);
        left_line("P for previous item", 3);
    }

    if (hints_depth != HINT_TOPIC) {
        center_line("M for hint menu.", 2, 16, false);
    }
    if (mouse_available()) {
        center_line("(Or use mouse.)", 3, 16, false);
    }

    if (hints_depth == HINT_HINT)
        right_line("Return for a hint.", 2, 18);
    else
        right_line("Return for hints.", 2, 17);

    right_line("Q to resume story.", 3, 18);
}

uint16_t hints = 0xe9fe;

uint16_t h_chapt_num = 1;
uint16_t h_quest_num = 1;

bool rt_see_qst(int16_t obj) {
    if (obj == 0)
        return true;
    if (obj < 0 || obj > ARTHUR_LAST_OBJECT)
        return (internal_call(obj) == 1);
    if (internal_get_parent(obj) == 0xba) // obj in ROOMS
        return (internal_test_attr(obj, 0x27)); // touchbit
    return (internal_test_attr(obj, 0x26));  // seenbit
}

uint16_t hint_question_name(uint16_t question) {
    uint16_t result = user_word(hints + h_chapt_num * 2);
    uint16_t base_address = user_word(result + (question + 1) * 2);

    if (is_game(Game::Arthur)) {
        base_address += 2;
        if (!rt_see_qst(user_word(base_address)))
            return 0;
    }

    return user_word(base_address + 2);
}

bool arthur_is_topic_in_context(uint16_t topic) {
    int16_t max = user_word(topic);
    for (uint16_t i = 2; i < max; i++) {
        if (rt_see_qst(user_word(user_word(topic + i * 2) + 2)))
            return true;
    }
    return false;
}

uint16_t hint_topic_name(uint16_t chapter) {
    uint16_t topic = user_word(hints + chapter * 2);

    if (is_game(Game::Arthur) && !arthur_is_topic_in_context(topic))
        return 0;

    return user_word(topic + 2);
}

int hint_put_up_frobs(uint16_t mx, uint16_t st) {
    uint16_t x = 0;
    uint16_t y = 0;
    glui32 width, height;
    glk_window_get_size(windows[0].id, &width, &height);
    glk_window_clear(windows[0].id);
    glk_set_window(windows[0].id);
    uint16_t str;
    int number_of_entries = 0;
    for (int i = st; i <= mx; i++) {
        if (hints_depth == HINT_TOPIC) {
            str = hint_topic_name(i);
        } else {
            str = hint_question_name(i);
        }
        if (str == 0)
            continue;
        number_of_entries++;
        if (is_game(Game::Arthur)) {
            store_word(ARTHUR_K_HINT_ITEMS + number_of_entries * 2, i);
        }
        glk_window_move_cursor(windows[0].id, x, y);
        print_handler(unpack_string(str), nullptr);
        y++;
        windows[0].x_cursor = 1;
        if (y == height) {
            y = 0;
            x = width / 2;
        }
    }
    if (is_game(Game::Arthur)) {
        store_word(ARTHUR_K_HINT_ITEMS, number_of_entries); // the first word of a table contains the length
    }
    return number_of_entries;
}

int hint_new_cursor(uint16_t pos, bool reverse) {
    uint16_t x = 0;
    if ((int16_t)pos < 1)
        pos = 1;
    uint16_t y = pos - 1;
    glui32 width, height;
    glk_window_get_size(windows[0].id, &width, &height);
    if (pos > height) {
        x = width / 2;
        y -= height;
    }
    glk_window_move_cursor(windows[0].id, x, y);
    windows[0].x_cursor = 1;
    if (reverse)
        garglk_set_reversevideo(1);

    uint16_t index = line_to_index(pos);
    uint16_t string_address;
    if (hints_depth == HINT_TOPIC) {
        string_address = hint_topic_name(index);
    } else {
        string_address = hint_question_name(index);
    }
    print_handler(unpack_string(string_address), nullptr);
    garglk_set_reversevideo(0);
    return x;
}


void print_number(int number);

int print_long_zstr_to_cstr(uint16_t addr, char *str, int maxlen);

uint16_t seen_hints_table = 0xe9d2;

int16_t get_seen_hints(void) {
    //    Byte table to use for showing questions already seen.
    //    Actually a nibble table. The high four bits of each byte are for
    //    odd question numbers; the low four bits are for even question numbers.
    int16_t cv = user_word(seen_hints_table + (h_chapt_num - 1) * 2);
    int16_t address = cv + (h_quest_num - 1) / 2;
    int16_t seen = user_byte(address);
    bool odd = ((h_quest_num & 1) == 1);
    if (odd) {
        seen = seen >> 4;
    }
    return seen & 0xf;
}

// Store the index of the last seen hint
// into nibble table
void store_hints_seen(uint8_t value) {
    int16_t cv = user_word(seen_hints_table + (h_chapt_num - 1) * 2);
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

int16_t display_actual_hints(int16_t *hints_base_address) {

    int16_t h = user_word(user_word(hints + h_chapt_num * 2) + (h_quest_num + 1) * 2);
    char str[64];

    int offset = is_game(Game::Arthur) ? 4 : 2;

    int length = print_long_zstr_to_cstr(user_word(h + offset), str, 64);
    hint_title(str, length);

    int16_t seen = get_seen_hints();
    int16_t max = user_word(h) - 1;

    if (seen == max) {
        // Remove "Return for a hint." if there are no more hints.
        right_line("                  ", 2, 18);
    }
    glk_set_window(windows[0].id);

    if (hints_base_address != nullptr)
        *hints_base_address = h;
    return max;
}

bool display_hints(void) {

    hints_depth = HINT_HINT;

    if (is_game(Game::Shogun)) {
        seen_hints_table = 0xbe6d;
    } else if (is_game(Game::Arthur)) {
        seen_hints_table = 0x973a;
    }

    int16_t hints_base_address;
    int16_t max = display_actual_hints(&hints_base_address);

    v6_delete_win(&windows[0]);
    v6_remap_win_to_buffer(&windows[0]);

    int16_t seen = get_seen_hints();

    bool done = false;
    bool result = false;

    int16_t cnt;

    for (cnt = is_game(Game::Arthur) ? 1 : 0; cnt <= max; cnt++) {
        store_hints_seen(cnt);
        if (cnt == max) {
            glk_put_string(const_cast<char *>("[No more hints.]\n"));
            // Remove "Return for a hint."
            right_line("                  ", 2, 18);
        } else {
            print_number(max - cnt);
            glk_put_char('>');
            glk_put_char(UNICODE_SPACE);
        }
        if (cnt >= seen) {
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
            print_handler(unpack_string(user_word(hints_base_address + (cnt + 2) * 2)), nullptr);
            glk_put_char(UNICODE_LINEFEED);
        }
    }

    v6_delete_win(&windows[0]);
    v6_remap_win_to_grid(&windows[0]);

    return result;
}

bool hint_inner_menu_loop(uint16_t *index, uint16_t num_entries, EntryNameType name_routine, bool *exit_hint_menu);

int display_questions(void) {
    char str[30];
    int length = print_long_zstr_to_cstr(user_word(user_word(hints + h_chapt_num * 2) + 2), str, 30);
    hint_title(str, length);
    uint16_t max = user_word(user_word(hints + h_chapt_num * 2)) - 1;
    int number_of_entries = hint_put_up_frobs(max, 1);
    hint_new_cursor(index_to_line(h_quest_num), true);
    return number_of_entries;
}

bool hint_pick_question(void) {
    bool result = true;

    bool outer_loop = true;

    while (outer_loop) {

        hints_depth = HINT_QUESTION;

        uint16_t max = display_questions();

        outer_loop = hint_inner_menu_loop(&h_quest_num, max, HINT_QUESTION, &result);
    }
    hints_depth = HINT_TOPIC;
    return result;
}

bool hint_inner_menu_loop(uint16_t *index, uint16_t num_entries, EntryNameType local_menu_depth, bool *exit_hint_menu) {
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
        glk_window_get_size(windows[0].id, nullptr, &column_height);
        switch (chr) {
            case 'Q':
            case 'q':
                done = false;
                outer_loop = false;
                loop = false;
                break;
            case 'm':
            case 'M':
                if (local_menu_depth == HINT_QUESTION) {
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
                if (local_menu_depth == HINT_TOPIC) {
                    outer_loop = hint_pick_question();
                } else {
                    outer_loop = display_hints();
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
            if (local_menu_depth == HINT_TOPIC) {
                h_quest_num = line_to_index(1);
            }
        }
    }
    if (exit_hint_menu != nullptr) {
        *exit_hint_menu = done;
    }

    return outer_loop;
}

int display_topics(void) {
    hint_title(const_cast<char *>(" InvisiClues (tm) "), 18);
    int number_of_entries = hint_put_up_frobs(user_word(hints), 1);
    hint_new_cursor(index_to_line(h_chapt_num), true);
    return number_of_entries;
}

void redraw_hint_screen_on_resize(void) {
    redraw_hints_windows();
    switch (hints_depth) {
        case HINT_HINT:
            display_actual_hints(nullptr);
            break;
        case HINT_QUESTION:
            display_questions();
            break;
        default:
            display_topics();
            break;
    }
}

// Shared between Zork Zero, Shogun, and Arthur
void DO_HINTS(void) {
    if (is_game(Game::Shogun)) {
        hints = 0xbe99;
    } else if (is_game(Game::Arthur)) {
        hints = 0x9778;
    }

    glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_ReverseColor, 0);

    screenmode = MODE_HINTS;
    hints_depth = HINT_TOPIC;
    garglk_set_reversevideo(0);
    init_hint_screen();

    bool outer_loop = true;

    while (outer_loop) {
        int number_of_entries = display_topics();
        outer_loop = hint_inner_menu_loop(&h_chapt_num, number_of_entries, HINT_TOPIC, nullptr);
    }

    v6_delete_win(&windows[0]);
    v6_remap_win_to_buffer(&windows[0]);
    screenmode = MODE_NORMAL;
    if (is_game(Game::ZorkZero)) {
        z0_update_on_resize();
    } else if (is_game(Game::Shogun)) {
//        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_ReverseColor, 1);
        internal_call(pack_routine(0x183a4)); // V-REFRESH
    } else {
        return;
    }

    glk_put_string(const_cast<char*>("Back to the story...\n"));
}

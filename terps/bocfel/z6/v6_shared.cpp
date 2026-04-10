// v6_shared.cpp
//
// Shared UI functionality for all Infocom Z-machine version 6 games
// (Zork Zero, Shogun, Arthur).
//
// This file contains code used by two or more of the V6 games:
//
// - Margin image tracking: Maintains a list of images drawn as margin
//   (inline) images in text buffer windows, so they can be recreated
//   when the graphics type changes.
// - Number printing: Outputs integers to Glk windows character by character.
// - Z-string to C-string conversion: Decodes Z-machine encoded strings
//   into plain C strings for use with Glk and VoiceOver accessibility.
// - Definitions screen (DEFINE command): A shared function-key editor
//   that lets players assign text macros to function keys. Used by both
//   Zork Zero and Shogun.
// - Hints screen (InvisiClues): A three-level hint menu (topics ->
//   questions -> progressive hints) shared by all three games. Supports
//   keyboard and mouse navigation, with VoiceOver menu integration.
// - Credits display and color management after V-COLOR.
// - Visual puzzle skip prompt for accessibility.
// - Empty stub functions for entry points that only some games implement.

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
//#include "zorkzero.hpp"

#include "v6_shared.hpp"

// Window 2 (SOFT-WINDOW) is used as the definitions text grid
#define DEFINITIONS_WINDOW windows[2]

#pragma mark - Margin Image Tracking

// List of image numbers currently displayed as margin (inline) images in
// text buffer windows. When the graphics type changes, these images need
// to be recreated at the new resolution.
int margin_images[100];
int number_of_margin_images = 0;

// Shifts the margin image list when full, discarding the oldest entry.
void shift_margin_image_list(void) {
    fprintf(stderr, "More than 100 margin images! Shifting list, forgetting the first margin image (%d)\n", margin_images[0]);
    for (int i = 1; i < number_of_margin_images; i++) {
       margin_images[i] = margin_images[i - 1];
    }
    number_of_margin_images = 99;
}

// Adds an image to the margin image list if not already present.
// Shifts the list if full (maximum 100 entries).
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

// Resets the margin image list (e.g., when clearing the screen).
void clear_margin_image_list(void) {
    number_of_margin_images = 0;
}

// Recreates all tracked margin images at the current graphics resolution.
// Called when the graphics type changes (e.g., switching from Amiga to VGA).
void refresh_margin_images(void) {
    for (int i = 0; i < number_of_margin_images; i++) {
        recreate_image(margin_images[i], false);
    }
}

#pragma mark - Print Number

// Prints an integer to the current Glk window, one character at a time.
void print_number(int number) {
    std::ostringstream ss;
    ss << number;
    for (const auto &c : ss.str()) {
        glk_put_char(c);
    }
}

// Prints a number right-justified in a 4-character field, padding with
// leading spaces. Used for score and move count display in status lines.
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

#pragma mark - Z-String to C-String Conversion

// Buffer state for print_to_string_buffer callback
static char *string_buf_ptr = nullptr;
static int string_buf_pos = 0;
static int string_maxlen = 0;

// Callback for print_handler that appends characters to a C string buffer
// instead of displaying them.
static void print_to_string_buffer(uint8_t c) {
    if (string_buf_pos < string_maxlen)
        string_buf_ptr[string_buf_pos++] = c;
}

// Decodes a Z-machine encoded string at `addr` into a C string buffer `str`
// with a maximum length of `maxlen`. Returns the string length, or 0 if
// the string is shorter than 2 characters.
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

// Convenience wrapper: decodes a Z-string with the default buffer size.
int print_zstr_to_cstr(uint16_t addr, char *str) {
    return print_long_zstr_to_cstr(addr, str, STRING_BUFFER_SIZE);
}

// Destroys and recreates the foreground graphics window. Used when the
// graphics type changes and the window needs fresh configuration.
// In slideshow mode, the new window is sized to fill the screen;
// otherwise it's hidden (zero size) and the background window is used.
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

// Prints a C string to the current Glk window and also sends it to the
// transcript stream (if active).
void transcribe_and_print_string(const char *str) {
    glk_put_string(const_cast<char*>(str));
    int i = 0;
    while (i++ != 0) {
        transcribe(str[i]);
    }
}

#pragma mark - Definitions Screen (DEFINE Command)

// The Definitions screen lets players assign text macros to function keys.
// Shared between Zork Zero and Shogun. The screen displays a centered
// "Function Keys" title, a list of editable key definitions, and built-in
// commands (Save Definitions, Restore Definitions, Exit).

// Maximum width of a function key definition string
#define DEFINITIONS_WIDTH 30 // FLEN in original source
#define ARTHUR_LAST_OBJECT 0x137

// Prints a single reverse-video space character, used as a text cursor
// indicator in the definitions editor.
static void print_reverse_video_space(void) {
    garglk_set_reversevideo(1);
    glk_put_char(UNICODE_SPACE);
    garglk_set_reversevideo(0);
}

// Address of the function key definitions table in Z-machine memory.
// Each entry is 4 bytes: 2-byte key code + 2-byte pointer to definition data.
// Default values are for Zork Zero; updated by entrypoints.cpp for other games.
uint16_t fkeys_table_addr = 0xce2a;
// Address of the function key names table (maps key codes to display names).
uint16_t fnames_table_addr = 0x4b57;

// Returns the width of the widest user-defined command string.
// Used to center the command text in the definitions window.
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

// Cached width of the widest command (0 = not yet computed)
int define_window_width = 0;

// Prints a Z-string centered on line `y` of the definitions window.
// First clears the line area to the width of the widest command,
// then prints the string centered.
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

// Scans a Z-machine table for a value. Searches `length` entries starting
// at `address`, comparing either bytes (bytewise=true) or words (bytewise=false).
// Returns the address of the match, or 0 if not found.
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

// Displays a single line in the definitions menu. There are two types:
// - Hard-coded commands (negative key code): centered text items like
//   "Save Definitions" or "Exit" that run a Z-machine routine when selected.
// - Editable definitions (non-negative key code): shows the function key
//   name (reverse video) followed by the user's command text with a cursor.
// When `inverse` is true, the line is shown as selected (highlighted).
// When `send_menu` is true, sends VoiceOver menu item notifications.
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

// Currently selected line index in the definitions menu
int global_define_line = 0;

// Renders the entire definitions menu: title, all function key definitions,
// and built-in commands. Sends VoiceOver menu items for each line.
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

// Notifies VoiceOver that the current menu line's text has changed.
void send_edited_menu_line(uint8_t chr) {
    win_menuitem(kV6MenuCurrentItemChanged, global_define_line, chr, 0, nullptr, 0);
}

// Creates or resizes the definitions window to be centered on screen,
// sized to fit all menu lines. Sets up text styles, displays the menu,
// and positions the cursor on the currently selected line.
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

// Z-machine entry point: main loop for the DEFINE command (function key editor).
// Shared between Zork Zero and Shogun. Handles:
// - Arrow key / mouse navigation between lines
// - Enter on hard-coded commands (Save/Restore/Exit)
// - Text editing (typing characters, backspace) for editable definitions
// - Function key shortcuts to jump to the corresponding line
// - Pipe character '|' is converted to ZSCII_NEWLINE (command separator)
void V_DEFINE(void) {
    int linmax;
    uint16_t fkey, fdef, clicked_line, pressed_fkey, length;

//    if (is_spatterlight_zork0) {
//        update_user_defined_colours();
//    }

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
//
//    if (is_spatterlight_zork0) {
//        z0_update_on_resize();
//    } else {
        // Game is Shogun
        internal_call(pack_routine(sr.V_REFRESH));
        shogun_update_on_resize();
//    }
}

#pragma mark - Hints Screen (InvisiClues)

// The hints system provides a three-level progressive hint menu:
//   Level 1 (Topics): Categories of hints (e.g., "In the Dungeon")
//   Level 2 (Questions): Specific puzzles within a topic
//   Level 3 (Hints): Progressive hints revealed one at a time with Enter
// Shared between Zork Zero, Shogun, and Arthur.

// Saves the main text buffer window while the hints system temporarily
// replaces it with a text grid for menu display.
winid_t stored_bufferwin = nullptr;

// Z-machine global indices for the current hint chapter and question,
// so the game can persist hint state across save/restore.
uint8_t hint_chapter_global_idx = 0;
uint8_t hint_quest_global_idx = 0;

// Address of the master hints table in Z-machine memory.
// First word is the number of topics; subsequent words point to topic tables.
uint16_t hints_table_addr = 0;

// Colors for the hints status bar (upper window). Set per-platform
// in draw_hints_windows.
static glui32 upperwin_foreground = zcolor_Default;
static glui32 upperwin_background = zcolor_Default;

// Current depth in the hint menu hierarchy
InfocomV6MenuType hints_depth = kV6MenuTypeTopic;

// Current chapter (topic) and question indices within the hints table
static uint16_t h_chapt_num = 1;
static uint16_t h_quest_num = 1;

// Address of the "seen hints" nibble table (HINT-COUNTS in original source).
// Tracks how many hints have been revealed for each question. Each byte
// stores two questions: high nibble for odd-numbered, low nibble for even.
// Default is for Zork Zero r393; changed in entrypoints.cpp for other games.
uint16_t seen_hints_table_addr = 0xe9d2;

// Waits for user input (keyboard or mouse) in the hints screen.
// If a mouse click occurs, maps it to a command key (N/P/M/Q/Enter)
// based on position in the status bar, or returns a line number if
// clicked in the menu area. Left/right columns are supported for
// two-column layouts.
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


// Arthur has contextual hints that may be hidden based on game state,
// so the internal table index may not match the visible line number.
// These two functions map between them. For non-Arthur games, they
// return the argument unchanged.

// Converts an internal hint table index to a visible line number.
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

// Converts a visible line number back to an internal hint table index.
static uint16_t line_to_index(uint16_t line) {
    if (is_spatterlight_arthur) {
        uint16_t max = user_word(at.K_HINT_ITEMS);
        if (line > 1 && line <= max)
            return (user_word(at.K_HINT_ITEMS + line * 2));
        return 1;
    }
    return line;
}


// Sets up the window layout for the hints screen. Configures colors
// based on graphics type and platform, positions the status bar (with
// navigation instructions) and the menu/hint text area. For topic and
// question levels, the text buffer is remapped to a text grid for
// fixed-position menu display. Handles game-specific border drawing.
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
//        if (is_spatterlight_zork0) {
//            z0_erase_screen();
//            garglk_set_zcolors_stream(glk_window_get_stream(V6_STATUS_WINDOW.id), upperwin_foreground, upperwin_background);
//        }
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

//    if (is_spatterlight_zork0) {
//        // Global BORDER-ON, false if in text-only mode
//        bool text_only_mode = (get_global(zg.BORDER_ON) == 0);
//        if (!text_only_mode) {
//            DISPLAY_BORDER(HINT_BORDER);
//            get_image_size(TEXT_WINDOW_PIC_LOC, &width, &height);
//            width = width * imagescalex;
//            height = height * imagescaley;
//        }
//    }

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
//    } else if (is_spatterlight_zork0 && graphics_type == kGraphicsTypeApple2) {
//        // Just for the lulz we try to use the Apple II hint screen header in Zork 0
//        // that the actual game did not use (presumably because it was impossible to
//        // fit the text in its low resolution.) We cut off 23 "pixels" on each side of
//        //  the status text view to make it fit between the question marks in the image.
//        status_x = 23 * imagescalex;
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
//    } else if  (is_spatterlight_zork0) {
//        z0_display_border(Z0_HINT_BORDER);
    }
}

// Prints a string centered on a given line of the status window.
// If `reverse` is true, displays in reverse video.
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

// Prints a string right-aligned on a given line of the status window.
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

// Prints a string left-aligned on a given line of the status window.
static void left_line(const char *str, int line) {
    winid_t win = V6_STATUS_WINDOW.id;
    glk_window_move_cursor(win, 0, line - 1);
    glk_put_string(const_cast<char *>(str));
}

// Displays the hint screen's status bar with a centered title and
// navigation instructions. Content varies by depth level:
// - Topics: N/P to navigate, Return for hints, Q to quit
// - Questions: adds M for hint menu
// - Hints: Return for next hint, no N/P navigation
// Also sends VoiceOver title notification.
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

// Arthur-specific: checks if a hint question is relevant to the current
// game state by calling the game's RT-SEE-QST routine.
static bool rt_see_qst(uint16_t obj) {
    return (internal_call(pack_routine(ar.RT_SEE_QST), {obj}) == 1);
}

// Returns the Z-string address of a question's display name within the
// current chapter. For Arthur, also checks context visibility.
// Returns 0 if the question should be hidden.
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

// Arthur-specific: checks if any question in a topic is currently relevant,
// hiding entire topics that aren't applicable to the player's game state.
static bool arthur_is_topic_in_context(uint16_t topic) {
    int16_t max = user_word(topic);
    for (uint16_t i = 2; i <= max; i++) {
        if (rt_see_qst(user_word(user_word(topic + i * 2) + 2)))
            return true;
    }
    return false;
}

// Returns the Z-string address of a topic's display name.
// For Arthur, returns 0 if no questions in the topic are contextually relevant.
static uint16_t hint_topic_name(uint16_t chapter) {
    uint16_t topic = user_word(hints_table_addr + chapter * 2);

    if (is_spatterlight_arthur && !arthur_is_topic_in_context(topic))
        return 0;

    return user_word(topic + 2);
}

// Returns the number of hints already revealed for the current question.
// The seen-hints table is a nibble table: the high 4 bits of each byte
// store the count for odd-numbered questions; the low 4 bits store even.
static int16_t get_seen_hints(void) {
    int16_t cv = user_word(seen_hints_table_addr + (h_chapt_num - 1) * 2);
    int16_t address = cv + (h_quest_num - 1) / 2;
    int16_t seen = user_byte(address);
    bool odd = ((h_quest_num & 1) == 1);
    if (odd) {
        seen = seen >> 4;
    }
    return seen & 0xf;
}


// Decodes and prints a Z-string, then sends it as a VoiceOver menu item
// at the appropriate index for the current hint depth level.
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



// Populates the hint menu with topic or question entries. Displays items
// in a two-column grid layout (wrapping to the right column when the
// left is full). For Arthur, builds the K_HINT_ITEMS mapping table
// and skips contextually irrelevant items. Returns the total number of
// visible entries.
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

// Moves the cursor to a menu line and optionally highlights it with
// reverse video. Handles two-column layout by checking if the position
// exceeds the column height. Returns the x coordinate of the line.
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

// Stores the number of hints revealed for the current question in the
// nibble table. Preserves the other nibble in the same byte.
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

// Initializes the hint display for the current question: shows the title,
// checks how many hints have been seen, and removes the "Return for a hint"
// prompt if all hints are already revealed. Returns the maximum hint count
// and optionally stores the base address for hint text lookup.
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

// Displays progressive hints for the current question. Hints are revealed
// one at a time when the user presses Enter, with a countdown showing
// remaining hints. Already-seen hints are shown immediately. The user
// can press M to return to the menu or Q to quit. If `only_refresh` is
// true, only redraws already-seen hints without waiting for input.
// Returns true if the user chose to return to the topic menu.
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

// Displays the question list for the current topic. Shows the topic
// title and all questions, with the current selection highlighted.
// Returns the number of visible questions.
static int display_questions(void) {
    char str[30];
    int length = print_long_zstr_to_cstr(user_word(user_word(hints_table_addr + h_chapt_num * 2) + 2), str, 30);
    hint_title(str, length);
    uint16_t max = user_word(user_word(hints_table_addr + h_chapt_num * 2)) - 1;
    int number_of_entries = hint_put_up_frobs(max, 1);
    hint_new_cursor(index_to_line(h_quest_num), true);
    return number_of_entries;
}

// Outer loop for question selection. Displays questions and enters the
// navigation loop. Returns true if the user wants to continue browsing
// topics, false if they want to exit hints entirely.
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

// Core navigation loop for topic and question menus. Handles:
// - N/P/arrow keys: move selection up/down (with wrapping)
// - Left/right arrows: switch columns in two-column layout
// - M: return to topic menu (from question level only)
// - Q: exit hints entirely
// - Enter/click: drill into the selected item (questions or hints)
// - Mouse: single-click selects, double-click enters
// Updates the `index` to the selected item and notifies VoiceOver.
// Returns true if the outer loop should continue (re-display needed).
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

// Displays the top-level topic list with the "InvisiClues (tm)" title.
// Returns the number of visible topics.
static int display_topics(void) {
    hint_title(const_cast<char *>(" InvisiClues (tm) "), 18);
    int number_of_entries = hint_put_up_frobs(user_word(hints_table_addr), 1);
    hint_new_cursor(index_to_line(h_chapt_num), true);
    return number_of_entries;
}

// Redraws the hint screen after a window resize, restoring whichever
// level (topics, questions, or hints) was being displayed.
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

// Z-machine entry point: main hints screen controller. Saves the current
// screen mode, sets up the hints windows, and enters the topic selection
// loop. The loop can recurse into questions and then individual hints.
// On exit, restores the original screen mode, cleans up windows, and
// refreshes the game display. Supports autorestore (can resume at any
// hint depth level). Temporarily disables transcript in the buffer window
// to avoid logging hint text.
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
    
//    if (is_spatterlight_zork0) {
//        z0_update_on_resize();
//    } else
        if (is_spatterlight_shogun) {
        internal_call(pack_routine(sr.V_REFRESH));
        shogun_update_on_resize();
    }

    // Allow transcript in main buffer window
    V6_TEXT_BUFFER_WINDOW.attribute |= WINATTR_TRANSCRIPTING_BIT;
    glk_put_string(const_cast<char*>("Back to the story...\n"));
    glk_set_echo_line_event(V6_TEXT_BUFFER_WINDOW.id, 0);
}

#pragma mark - Credits

// Z-machine entry point: called before credits text is printed.
// Ensures output goes to the main text buffer window.
void V_CREDITS(void) {
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

// Z-machine entry point: called after credits text. Resets bold, italic,
// and fixed-width text styles that may have been set during credit display.
void after_V_CREDITS(void) {
    V6_TEXT_BUFFER_WINDOW.style.reset(STYLE_BOLD);
    V6_TEXT_BUFFER_WINDOW.style.reset(STYLE_ITALIC);
    V6_TEXT_BUFFER_WINDOW.style.reset(STYLE_FIXED);
}

// Z-machine entry point: called after the game's V-COLOR routine changes
// colors. Updates all windows to use the new foreground and background
// colors, including Glk style hints, window backgrounds, and z-colors.
// Handles the special case where colors are set to "default" (1) after
// a restore. Also updates monochrome palette and triggers a game-specific
// screen refresh.
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
//    } else if (is_spatterlight_zork0) {
//        z0_update_on_resize();
    }
}


#pragma mark - Skip Puzzles Prompt

// Prompts the user to skip a visual puzzle for accessibility. Only shows
// the prompt when VoiceOver is active or when autorestoring to a point
// where the prompt was previously shown. Returns true if the user
// accepts (types 'Y'), false if they decline ('N') or if the prompt
// was not shown.
bool skip_puzzle_prompt(const char *str) {
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


#pragma mark - Entry Point Stubs

// Empty stub functions for Z-machine entry points that are only implemented
// by specific games. The entrypoints system requires all entry point functions
// to exist, so games that don't use a particular entry point get these no-ops.
// The actual implementations live in the game-specific files (zorkzero.cpp,
// shogun.cpp, arthur.cpp).
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
void V_REFRESH(void) {}

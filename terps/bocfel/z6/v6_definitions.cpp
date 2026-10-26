// v6_definitions.cpp
//
// The Definitions screen (DEFINE command) shared by Zork Zero and Shogun: a
// function-key editor that lets players assign text macros to function keys.

#include <cmath>

#include "draw_image.hpp"
#include "memory.h"
#include "objects.h"
#include "process.h"
#include "screen.h"
#include "stack.h"
#include "unicode.h"
#include "zterp.h"
#include "options.h"
#include "v6_specific.h"

#include "arthur.hpp"
#include "journey.hpp"
#include "shogun.hpp"
#include "zorkzero.hpp"

#include "v6_shared.hpp"

// Window 2 (SOFT-WINDOW) is used as the definitions text grid
#define DEFINITIONS_WINDOW windows[2]

#pragma mark - Definitions Screen (DEFINE Command)

// The Definitions screen lets players assign text macros to function keys.
// Shared between Zork Zero and Shogun. The screen displays a centered
// "Function Keys" title, a list of editable key definitions, and built-in
// commands (Save Definitions, Restore Definitions, Exit).

// Maximum width of a function key definition string
constexpr int DEFINITIONS_WIDTH = 30; // FLEN in original source

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

// Address of the entry for line `line` of the definitions screen.
static uint16_t fkey_entry(int line) {
    return fkeys_table_addr + 2 + 4 * line;
}

// The definitions screen as VoiceOver sees it. The front end takes the third
// win_menuitem() argument of a kV6MenuTypeDefine item as the total number of
// items in the menu ("item 3 of 18"). Both games have 19 rows: 14 definitions,
// a blank separator and 4 commands. The separator is not a menu item, so the
// rows below it are numbered one lower.
enum {
    kDefineMenuItemCount = 18,
    kDefineMenuBlankRow = 14
};

static int define_menu_item_index(int line) {
    return line > kDefineMenuBlankRow ? line - 1 : line;
}

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
static int define_window_width = 0;

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

// A DEFINE menu row is either a function key the player can redefine or a
// fixed command (Save Definitions, Exit and so on). The game marks the
// commands with a negative first word.
static bool is_command_entry(uint16_t function_key) {
    return (int16_t)user_word(function_key) < 0;
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
    // Every piece of this line comes from the story file (the key name, the
    // user's definition and its game-supplied length byte), so build it through
    // MenuString: appends truncate at str's capacity rather than past it.
    MenuString menu_string(str, sizeof(str));
    fdef = user_word(function_key + 2);
    y = index + 1;
    if (is_command_entry(function_key)) {
        if (fdef != 0) {
            DEFINITIONS_WINDOW.x = 1;
            DEFINITIONS_WINDOW.y = y * gcellh;
            if (inverse) {
                garglk_set_reversevideo(1);
            }
            print_center_table(y, user_word(fdef));
            if (send_menu) {
                menu_string.append_zstr(user_word(fdef));
                win_menuitem(kV6MenuTypeDefine, define_menu_item_index(index), kDefineMenuItemCount, 0, str, menu_string.length());
            }
        }
    } else {
        glk_window_move_cursor(DEFINITIONS_WINDOW.id, 0, y);
        DEFINITIONS_WINDOW.x = 1;
        DEFINITIONS_WINDOW.y = y * gcellh + 1;

        uint16_t key_name_string = scan_table(user_word(function_key), fnames_table_addr, user_word(fnames_table_addr - 2), false);
        if (key_name_string != 0) {
            garglk_set_reversevideo(!inverse);
            menu_string.append_zstr(user_word(key_name_string + 2));
            glk_put_string(str);
            garglk_set_reversevideo(0);
            glk_put_char(UNICODE_SPACE);
            if (inverse) {
                garglk_set_reversevideo(1);
            }
        }
        int string_length = user_byte(fdef);

        menu_string.append(": ");

        for (int i = 0; i < string_length; i++) {
            uint8_t c = user_byte(fdef + 2 + i);
            if (c == ZSCII_NEWLINE)
                c = '|';
            glk_put_char(c);
            menu_string.append_char(c); // For VoiceOver
        }

        // For VoiceOver. Pass the builder's length -- the number of bytes
        // actually written into str above -- not user_byte(fdef + 1) + 5, which
        // is a screen column (0..260) unrelated to the fill and would make the
        // front-end read uninitialized or past-buffer bytes of str[60].
        if (send_menu)
            win_menuitem(kV6MenuTypeDefine, define_menu_item_index(index), kDefineMenuItemCount, 0, str, menu_string.length());

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
static void send_edited_menu_line(uint8_t chr) {
    win_menuitem(kV6MenuCurrentItemChanged, global_define_line, chr, 0, nullptr, 0);
}

// Creates or resizes the definitions window to be centered on screen,
// sized to fit all menu lines. Sets up text styles, displays the menu,
// and positions the cursor on the currently selected line.
void adjust_definitions_window(void) {

    uint16_t fkey = fkey_entry(global_define_line);

    int x_size = (DEFINITIONS_WIDTH + 5) * gcellw + 1 + 2 * ggridmarginx;
    if (x_size > gscreenw)
        x_size = gscreenw;

    int left = (gscreenw - x_size) / 2;
    int16_t linmax = user_word(fkeys_table_addr) / 2;
    int y_size = (linmax + 1) * gcellh + 2 * ggridmarginy;
    if (y_size > gscreenh)
        y_size = gscreenh;
    int top = (gscreenh - y_size) / 2;


    v6_set_grid_colors(style_Normal);

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
    win_menuitem(kV6MenuSelectionChanged, define_menu_item_index(global_define_line), kDefineMenuItemCount, 0, nullptr, 0);
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

    fkey = fkey_entry(global_define_line);
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
                // Ignore clicks below the last line and on the blank line
                // between definitions and commands: neither is an entry
                // that can be selected, edited or run.
                if (new_line >= linmax || user_word(fkey_entry(new_line) + 2) == 0) {
                    new_line = global_define_line;
                    break;
                }

                // Deselect the old line and select the clicked one.
                // This is identical to the code at the end of the input loop,
                // but must be repeated here in order to make the double click
                // apply to the right line
                if (global_define_line != new_line) {
                    display_soft(fkey, global_define_line, true, false);
                    fkey = fkey_entry(new_line);
                    display_soft(fkey, new_line, false, false);
                    global_define_line = new_line;
                    fdef = user_word(fkey + 2);
                }

                // if we double clicked a command (as opposed to a definition)
                // we fall through and treat it as ZSCII_NEWLINE.
                // Otherwise we break.
                if (!(chr == ZSCII_CLICK_DOUBLE && is_command_entry(fkey))) {
                    break;
                }
                // fallthrough
            case ZSCII_NEWLINE:
                // if we are on a static command, run the associated routine
                if (is_command_entry(fkey)) {
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
                } else if (!is_command_entry(fkey)) {
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
        // and select the new one.
        // (No menu item is sent for the redrawn lines: the front end already
        // has all of them, and would add this one to its list a second time.)
        if (global_define_line != new_line) {
            display_soft(fkey, global_define_line, true, false);
            display_soft(fkey_entry(new_line), new_line, false, false);
            global_define_line = new_line;
            win_menuitem(kV6MenuSelectionChanged, define_menu_item_index(global_define_line), kDefineMenuItemCount, 0, nullptr, 0);
            fkey = fkey_entry(global_define_line);
            fdef = user_word(fkey + 2);
        }
    };

    v6_delete_win(&DEFINITIONS_WINDOW);
    screenmode = MODE_NORMAL;
    global_define_line = 0;

    win_menuitem(kV6MenuExited, 0, 0, 0, nullptr, 0);

    v6_redraw_game_screen();
}

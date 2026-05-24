//
//  shogun.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-21.
//
//  Implements the custom UI for Infocom's "Shogun" (1989), a Z-machine
//  version 6 adaptation of James Clavell's novel. Shogun is largely a standard text adventure, apart from some custom menu functionality, graphical borders, inline illustrations, and a maze mini-game.
//
//  Key UI elements:
//    - Decorative border graphics framing the text area (crests, waves,
//      or hint-specific borders depending on context and platform)
//    - A two-line status bar showing scene name, location, score, and moves
//    - A pop-up menu system for story choices ("You may choose to: ...")
//      with keyboard navigation and type-ahead quick search
//    - Inline and marginal illustrations within the text buffer
//    - A graphical maze mini-game with mouse/keyboard navigation
//    - A title screen displayed at game start
//
//  The game supports multiple graphics modes: Amiga, Macintosh (B/W and
//  color), Apple II, CGA, and modern Blorb resources. Border layout and
//  image scaling vary significantly across these modes.
//

#include <queue>
#include <vector>
#include <algorithm>

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

// Width in pixels of the left border pillar graphic. Used to inset the
// status bar and text buffer so they don't overlap the decorative frame.
static int shogun_banner_width_left = 0;

// Menu state: the currently highlighted line (1-based) and the Z-machine
// address of the active menu table. Preserved across autosave/restore.
uint16_t current_menu_selection = 0;
uint16_t current_menu;
// The currently active border style (crests, waves, hint border, or none).
ShogunBorderType current_border;

// Pixel offset and tile dimensions for the maze mini-game display.
// The maze is rendered as a grid of small tile images drawn to the
// background pixmap.
static int maze_offset_x = 0;
static int maze_offset_y = 0;
static int maze_box_width = 0;
static int maze_box_height = 0;

// Global lookup tables mapping Z-machine global variable indices, routine
// addresses, table addresses, menu identifiers, and object numbers to
// symbolic names. Populated at startup by scanning the game's data.
ShogunGlobals sg;
ShogunRoutines sr;
ShogunTables st;
ShogunMenus sm;
ShogunObjects so;

// Shogun uses several Glk windows for different UI regions.
#define SHOGUN_MAZE_WINDOW windows[3]   // Maze graphics (no actual Glk window; draws to bg pixmap)
#define SHOGUN_MENU_WINDOW windows[2]   // Grid window for the menu choice list
#define SHOGUN_MENU_BG_WIN windows[5]   // Text buffer beside the menu for prompt messages
#define SHOGUN_CREST_WINDOW windows[4]  // Graphics window for the final P-CREST image
#define SHOGUN_A2_BORDER_WIN windows[6] // Apple II horizontal border banner

// Picture numbers for the right-side border pillar graphics.
// The left pillar is part of the main border image; right pillars are separate.
#define P_BORDER_R 59       // Right pillar for the standard crest border
#define P_BORDER2_R 60      // Right pillar for the wave border

// Picture numbers for left and right hint-mode border pillars.
#define P_HINT_BORDER_L 61
#define P_HINT_BORDER_R 62

#define P_BORDER_LOC 2      // Picture number used to measure the left border/pillar width
#define STATUS_LINES 2      // Height of the status bar in text lines

// Scene constants used for special-case behaviour.
#define S_ERASMUS 1         // Scene 1: aboard the Erasmus (affects border choice)
#define S_VOYAGE 6          // Scene 6: the sea voyage

// Z-machine object attribute bit numbers used in status line display.
#define SUPPORTERBIT 0x08   // Object is a supporter (player is "on" it)
#define ONBIT 0x13          // Object is switched on (wheel is active)
#define VEHICLEBIT 0x2c     // Object is a vehicle (player is "in" it)

#define SCORE_FACTOR 5      // Multiplier applied to raw score for display

// Picture numbers for the maze mini-game.
#define P_MAZE_BACKGROUND 44  // Background grid image for the maze
#define P_MAZE_BOX 45         // A single maze tile (used for sizing)

// Configures the status bar and text buffer window positions based on the
// current border type. 'P' is a picture number indicating which border is
// active (P_BORDER_LOC for normal, P_HINT_LOC for hints). On Apple II,
// a separate graphical banner window is created below the status bar.
// Exits maze mode if we're switching to a non-maze border.
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

// Z-machine entry point. Variable 1 is the border picture number.
void SETUP_TEXT_AND_STATUS(void) {
    setup_text_and_status(variable(1));
}


static bool last_was_interlude = false;     // Whether the last status update was for an interlude
static int a2_graphical_banner_height = 0;  // Height of Apple II graphical banner in pixels

// Redraws the two-line status bar. Line 1 shows the scene name (from
// SCENE_NAMES table), centered "SHOGUN" title (non-Mac), and score.
// Line 2 shows the current room name, vehicle info, ship navigation
// details (course/wheel direction), and move count. When 'interlude' is
// true, shows "Interlude" instead of a room name.
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
    if (get_global(sg.MOVES) == 278)
        fprintf(stderr, "here\n");
    print_right_justified_number(get_global(sg.MOVES));
    set_current_window(&windows[0]);

    if (graphics_type == kGraphicsTypeApple2 && current_border == P_BORDER2 && scene != S_ERASMUS) {
        shogun_display_border(P_BORDER);
    }
}

// Z-machine entry point for the normal (non-interlude) status line update.
void shogun_UPDATE_STATUS_LINE(void) {
    update_status_line(false);
}

// Z-machine entry point for interlude status line (shows "Interlude"
// instead of a location name, and uses the wave border on Apple II).
void INTERLUDE_STATUS_LINE(void) {
    if (graphics_type != kGraphicsTypeApple2) {
        shogun_display_border(P_BORDER2);
    }
    update_status_line(true);
}

#pragma mark - Menu System

// Z-machine address of the current menu prompt string (e.g. "You may choose to:").
static uint16_t current_menu_message;

// Writes the selected menu choice to the transcript stream. Called after
// the player confirms a selection so the transcript records their choice.
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

// Prints a Z-string to the transcript only (not to screen).
static void print_zstring_to_transcript(uint16_t string) {
    output_stream(-1, 0);
    print_handler(unpack_string(string), nullptr);
    output_stream(1, 0);
}


// Renders a single menu line in the menu grid window. If 'reverse' is true,
// the line is drawn with reverse video (highlighted). Menu entries are stored
// as length-prefixed byte strings in Z-machine memory. If 'send_menu' is true,
// also sends the item to the front-end via win_menuitem() for accessibility.
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

// Highlights a menu line (draws it in reverse video).
static void select_line(uint16_t menu, uint16_t line) {
    display_menu_line(menu, line, true, false);
}

// Unhighlights a menu line (draws it in normal video).
static void unselect_line(uint16_t menu, uint16_t line) {
    display_menu_line(menu, line, false, false);
}

// Returns the width of the longest menu entry in characters. Used to
// size the menu grid window so all entries fit without truncation.
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

#define MAX_QUICKSEARCH 25  // Maximum characters for type-ahead search

// Updates the menu background window to show the prompt message and the
// characters typed so far for quick search.
static void print_quicksearch(char *typed) {
    glk_window_clear(SHOGUN_MENU_BG_WIN.id);
    strid_t stream = glk_window_get_stream(SHOGUN_MENU_BG_WIN.id);
    char menu_message[100];
    print_long_zstr_to_cstr(current_menu_message, menu_message, 100);
    glk_put_string_stream(stream, menu_message);
    glk_put_char_stream(stream, '\n');
    glk_put_string_stream(stream, typed);
}

// Type-ahead quick search: as the player types letters, finds the first
// menu entry whose prefix matches. Returns the 1-based index of the
// matching entry, or 0 if no match. Case-insensitive.
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

// Creates or updates the menu display: sizes and positions the menu grid
// and background windows, renders all menu lines, and highlights the
// current selection. The menu is placed at the bottom of the screen,
// centered horizontally, with the prompt message in the background window
// to its left.
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

// Main menu interaction loop. Handles keyboard navigation (arrow keys,
// Enter to confirm), mouse clicks (single-click to highlight, double-click
// to confirm), and type-ahead letter search. Returns the 1-based index of
// the selected menu entry when the player presses Enter or double-clicks.
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

// High-level menu presentation function. Displays the border, shows the
// prompt message (MSG), presents the menu (MENU), and loops until the
// player makes a valid selection. If VoiceOver is active, waits for an
// extra keypress first. Also handles transcripting of the chosen entry.
// FCN is the Z-machine callback routine (not used directly here).
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

// Z-machine entry point for the menu system. Variables: 1=message,
// 2=menu table, 3=callback routine, 4=default selection (optional).
// Stores the result in variable 9 for the Z-code to process.
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

// Cleanup after a menu selection: resets text grid style hints and
// restores the appropriate border. On non-Apple II, downgrades
// P_BORDER2 (crests) to P_BORDER (waves) since waves are Apple II only.
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

// Z-machine entry point: determines which menu to use for scene selection.
// Variable 1=1 means use SCENE_NAMES (debug jump to any scene in the game),
// otherwise use PART_MENU.
// Result stored in variable 5.
void SCENE_SELECT(void) {
    uint16_t menu = sm.PART_MENU;
    if (internal_arg_count() > 0 && variable(1) == 1) {
        menu = st.SCENE_NAMES;
    }
    store_variable(5, menu);
}

// Z-machine entry point for the VERSION verb. Delegates to the shared
// V_CREDITS implementation used across V6 games.
void V_VERSION(void) {
    V_CREDITS();
}

void after_V_VERSION(void) {
    after_V_CREDITS();
}


// Z-machine entry point: checks whether we are running on a color-capable
// Macintosh (Mac II) vs. a black-and-white original Mac. The original game
// detects this by checking if the screen is exactly 640px wide. Here we
// temporarily set the graphics background width to 640 so the Z-code's
// check succeeds, allowing it to enable color. Used by V-COLOR and DO-COLOR.
void MAC_II(void) {
    V6_GRAPHICS_BG.x_size = 640;
}

// Restores the graphics background width to the actual screen width
// after the MAC_II check completes.
void after_MAC_II(void) {
    V6_GRAPHICS_BG.x_size = gscreenw;
}

#pragma mark - Display Border

// Draws the decorative border for Apple II graphics mode. Apple II uses
// a single horizontal banner strip (crests or waves) placed below the
// status bar. At the start menu, banners are drawn at both the top and
// bottom of the screen. The banner is rendered to the offscreen pixmap
// and then flushed to the screen.
void shogun_display_apple_ii_border(ShogunBorderType border, bool start_menu_mode) {
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

extern int margin_images[100];       // List of inline/margin images in the text buffer
extern int number_of_margin_images;  // Count of tracked margin images

// True if `b` is a real border picture (P_BORDER, P_BORDER2, P_HINT_BORDER).
// NO_BORDER is treated as "not specified" and falls through to the lookup
// fallbacks in shogun_display_border.
static bool is_real_shogun_border(ShogunBorderType b) {
    return b == P_BORDER || b == P_BORDER2 || b == P_HINT_BORDER;
}

// Resolve which border picture to actually draw. Precedence:
//   1. Hints mode always uses P_HINT_BORDER.
//   2. The Erasmus part-menu special case picks a graphics-dependent border.
//   3. Otherwise prefer the caller's argument, falling back to the
//      most-recently-drawn border, and finally to P_BORDER.
static ShogunBorderType resolve_shogun_border(ShogunBorderType requested) {
    if (screenmode == MODE_HINTS)
        return P_HINT_BORDER;
    if (get_global(sg.SCENE) == S_ERASMUS && current_menu == sm.PART_MENU)
        return (graphics_type == kGraphicsTypeApple2) ? P_BORDER2 : P_BORDER;
    if (is_real_shogun_border(requested))
        return requested;
    if (is_real_shogun_border(current_border))
        return current_border;
    return P_BORDER;
}

// If the foreground graphics window currently overlays the background
// (because we were showing the title screen or another fullscreen
// image), tear it down so the new border can be drawn against a clean
// background.
static void shogun_clear_covering_graphics_window(void) {
    if (current_graphics_buf_win == graphics_bg_glk || current_graphics_buf_win == nullptr)
        return;
    if (current_graphics_buf_win == graphics_fg_glk)
        graphics_fg_glk = nullptr;
    // Suppress the redraw that would otherwise flash the new border in the
    // wrong color while the old image is still on screen.
    image_needs_redraw = false;
    glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
    glk_window_clear(current_graphics_buf_win);
    v6_delete_glk_win(current_graphics_buf_win);
}

// Re-attach the background graphics window so subsequent drawing goes
// into it (sizing it to fill the screen if it isn't already).
static void shogun_attach_background_graphics_window(void) {
    current_graphics_buf_win = graphics_bg_glk;
    if (V6_GRAPHICS_BG.id != current_graphics_buf_win) {
        v6_delete_win(&V6_GRAPHICS_BG);
        V6_GRAPHICS_BG.id = current_graphics_buf_win;
        v6_define_window(&V6_GRAPHICS_BG, 1, 1, gscreenw, gscreenh);
    }
}

// Picks the BL/BR pillar images that go with `border`. Returns by writing
// out-params; BL is -1 for borders that don't have a separate left pillar.
static void shogun_pillar_images_for(ShogunBorderType border, int16_t *BL, int16_t *BR) {
    *BL = -1;
    *BR = -1;
    switch (border) {
        case P_HINT_BORDER:
            *BL = P_HINT_BORDER_L;
            *BR = P_HINT_BORDER_R;
            break;
        case P_BORDER:
            *BR = P_BORDER_R;
            break;
        case P_BORDER2:
            *BR = P_BORDER2_R;
            break;
        case NO_BORDER:
            break; // unreachable after resolve_shogun_border
    }
}

// Master border drawing routine for all non-Apple II graphics modes.
// Draws the top banner, left/right pillar graphics, and extends them
// downward to fill the screen. The layout varies significantly by
// graphics mode:
//   - Amiga/Mac B/W: single combined image with all border elements
//   - CGA: separated top + pillar images with special edge handling
//   - Blorb: modern resources, similar to CGA layout
// Also handles the hint-mode border which has different pillar images.
// Draws a status-bar-colored rectangle at the top.
// (The raw graphics have a top bar that
 // won't fit all the header text, so the original interpreters also cover it
 // with a solid color rectangle.)
void shogun_display_border(ShogunBorderType border) {
    const bool start_menu_mode = (get_global(sg.SCENE) == 0);

    border = resolve_shogun_border(border);

    // Remember the resolved border (except hint borders, which are
    // transient and shouldn't displace the last gameplay choice).
    if (border != P_HINT_BORDER) {
        if (sg.CURRENT_BORDER != 0)
            set_global(sg.CURRENT_BORDER, border);
        current_border = border;
    }

    shogun_clear_covering_graphics_window();
    shogun_attach_background_graphics_window();

    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);

    if (graphics_type == kGraphicsTypeApple2) {
        shogun_display_apple_ii_border(border, start_menu_mode);
        return;
    }

    int width, height;
    if (!get_image_size(border, &width, &height)) {
        // P_BORDER2 isn't present in every release; fall back to P_BORDER.
        if (border != P_BORDER2)
            return;
        border = P_BORDER;
        if (!get_image_size(border, &width, &height))
            return;
    }

    if (number_of_margin_images > 0 && border != P_HINT_BORDER)
        extract_palette_from_picnum(margin_images[number_of_margin_images - 1]);
    else
        extract_palette_from_picnum(border);

    int16_t BL, BR;
    shogun_pillar_images_for(border, &BL, &BR);

    // Layout: where the pillar artwork starts (border_top), where to begin
    // copying for vertical extension (pillar_top), and how wide the
    // pillars are (used as the covering-rectangle left margin).
    int border_top = 0;
    int pillar_top = 0;

    if (border == P_HINT_BORDER) {
        const bool single_piece = (graphics_type == kGraphicsTypeAmiga ||
                                   graphics_type == kGraphicsTypeMacBW);
        if (!single_piece) {
            border_top = height;
            pillar_top = border_top;
            get_image_size(P_HINT_LOC, &width, nullptr);
        } else {
            get_image_size(P_HINT_LOC, &width, &pillar_top);
        }
    } else {
        get_image_size(P_BORDER_LOC, &width, nullptr);
    }
    const int left_margin = width;
    shogun_banner_width_left = ceil(width * imagescalex) + 1;
    a2_graphical_banner_height = 0;

    // CGA hint border: a one-pixel-wide mask covers garbage at the right edge.
    if (border == P_HINT_BORDER && graphics_type == kGraphicsTypeCGA)
        draw_rectangle_on_bitmap(monochrome_black, hw_screenwidth - 1, 0, 1, gscreenh / imagescaley);

    BorderKind kind = BorderKind::ShogunGame;
    if (border == P_HINT_BORDER)
        kind = BorderKind::Hint;
    else if (start_menu_mode)
        kind = BorderKind::ShogunStartMenu;

    int bottom_offset = 0;
    if (border == P_BORDER2)
        bottom_offset = (graphics_type == kGraphicsTypeMacBW) ? 35 : 24;
    else if (border == P_HINT_BORDER)
        bottom_offset = (graphics_type == kGraphicsTypeCGA) ? 12 : 0;

    draw_border_common(border, BL, BR,
                       height, border_top, pillar_top,
                       left_margin,
                       bottom_offset,
                       kind);
}


// Z-machine entry point for border display. Transitions screen mode from
// slideshow to menu if needed, then draws the requested border (or the
// default P_BORDER if no argument is provided).
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

// Z-machine entry point: draws an inline image centered in the text buffer.
// Uses 2x scaling (or imagescalex for Mac B/W). The image is added to the
// margin image list for tracking across redraws.
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

#define P_CREST 31  // Picture number for the Toranaga family crest

// Z-machine entry point: displays the P_CREST image in a dedicated graphics
// window above the text buffer. Only used for the final crest image shown
// at the end of the game. Creates the crest window if it doesn't exist,
// sizes it to fit the image, and adjusts the text buffer below it.
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

// Z-machine entry point: draws an illustration as a margin image (floating
// left or right of text). Variable 1 is the picture number, optional
// variable 2 controls alignment (1=right, default; 0=left). Very wide
// images that span nearly the full width are treated as centered instead.
// Triggers a border redraw to update the palette.
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

#pragma mark - Maze Mini-Game

// Draws a single maze tile at grid position (x, y) in the maze. The tile
// is rendered to the offscreen pixmap at the appropriate pixel offset.
// Pic 0 means empty (no tile to draw).
static void display_maze_pic(int pic, int x, int y) {
    if (pic != 0) {
        draw_to_pixmap_unscaled(pic, maze_offset_x + x * maze_box_width, maze_offset_y + y * maze_box_height);
        image_needs_redraw = true;
    }
 }

// Z-machine entry point. Variables: 1=pic, 2=y, 3=x (note reversed order).
void DISPLAY_MAZE_PIC(void) {
    display_maze_pic(variable(1), variable(3), variable(2));
}

// Returns the maze grid dimensions in tiles. Uses Z-machine globals if
// available (later releases), otherwise falls back to hardcoded defaults
// (0x12 wide for Apple II, 0x25 for others; always 0x11 tall).
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

// Renders the entire maze grid from the MAZE_MAP byte array. Each byte
// represents a tile (0x00=street, 0x80=wall, or a specific tile image).
// The maze background image is drawn first, then each tile is overlaid.
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

// Sets up the maze display: enters maze mode, positions the text buffer
// below the maze area, calculates tile sizes and offsets, and renders
// the full maze. If 'clear' is true, clears the text buffer first.
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

// Z-machine entry point for displaying the maze.
void DISPLAY_MAZE(void) {
    display_maze(true);
}

// Handles mouse clicks in the maze. Determines the direction of the click
// relative to the player's current position and issues a movement command.
// The original source also allows diagonal movement, but since the maze
// has no diagonal passages, only cardinal directions are supported here.
static int maze_mouse_f(void) {
    int BX, BY;
    get_image_size(P_MAZE_BOX, &BX, &BY);

    // Pixel coordinates of the center of the player's current tile.
    const int player_center_x = maze_offset_x + get_global(sg.MAZE_X) * BX + BX / 2;
    const int player_center_y = maze_offset_y + get_global(sg.MAZE_Y) * BY + BY / 2;

    const uint16_t mouse_click_addr = header.extension_table + 2;
    const int16_t mouse_x = word(mouse_click_addr) - 1;
    const int16_t mouse_y = word(mouse_click_addr + 2) - 1;

    const int16_t dx = mouse_x - player_center_x;
    const int16_t dy = mouse_y - player_center_y;
    const int16_t abs_dx = (dx < 0) ? -dx : dx;
    const int16_t abs_dy = (dy < 0) ? -dy : dy;

    // Click inside the player's own tile — ignore.
    if (abs_dx <= BX / 2 && abs_dy <= BY / 2)
        return 0;

    // The axis with the larger magnitude wins; ties go to the vertical
    // axis, matching the original quadrant cascade.
    const uint16_t DIR = (abs_dx > abs_dy)
        ? ((dx > 0) ? so.EAST  : so.WEST)
        : ((dy > 0) ? so.SOUTH : so.NORTH);

    internal_call(pack_routine(sr.ADD_TO_INPUT), {DIR});
    glk_put_char(UNICODE_LINEFEED);
    return 13;
}

// Z-machine entry point for maze mouse handling. Stores the direction
// result (0 = no valid click, 13 = movement issued) in variable 1.
void MAZE_MOUSE_F(void) {
    store_variable(1, maze_mouse_f());
}

# pragma mark - Maze Simplification

// Maze tile constants for the simplification algorithm.
#define P_MAZE_WALL 0x80    // Wall tile byte value in MAZE_MAP
#define P_MAZE_STREET 0     // Open street tile byte value in MAZE_MAP

// Stores which directions are open from a given maze cell.
typedef struct MazeExits {
    uint8_t NORTH;
    uint8_t EAST;
    uint8_t SOUTH;
    uint8_t WEST;
} MazeExits;

// Checks whether maze cell 'i' is a dead end (has exactly one open
// neighbor). If so, populates 'me' with the exit directions. Skips
// the start position, wall cells, and border cells. Used by
// simplify_maze() to iteratively fill in dead-end paths.
bool deadend(int index, MazeExits *maze_exits) {
    int MAZE_WIDTH, MAZE_HEIGHT;

    get_maze_width_and_height(&MAZE_WIDTH, &MAZE_HEIGHT);

    int MAZE_MAP = get_global(sg.MAZE_MAP);
    int mazesize = MAZE_WIDTH * MAZE_HEIGHT;

    int start = get_global(sg.MAZE_XSTART) + get_global(sg.MAZE_YSTART) * MAZE_WIDTH;

    int exit_count = 0;

    if (index == start || memory[MAZE_MAP + index] != P_MAZE_STREET || index % MAZE_WIDTH == 0 || (index + 1) % MAZE_WIDTH == 0) { // skip left and right borders
        return false;
    }

    maze_exits->NORTH = P_MAZE_WALL;
    maze_exits->EAST = P_MAZE_WALL;
    maze_exits->SOUTH = P_MAZE_WALL;
    maze_exits->WEST = P_MAZE_WALL;

    if (index > MAZE_WIDTH && memory[MAZE_MAP + (index - MAZE_WIDTH)] == P_MAZE_STREET) {
        maze_exits->NORTH = P_MAZE_STREET;
        exit_count++;
    }
    if (index < mazesize - 1 && memory[MAZE_MAP + index + 1] == P_MAZE_STREET) {
        maze_exits->EAST = P_MAZE_STREET;
        exit_count++;
    }
    if (index < mazesize - MAZE_WIDTH && memory[MAZE_MAP + index + MAZE_WIDTH] == P_MAZE_STREET) {
        maze_exits->SOUTH = P_MAZE_STREET;
        exit_count++;
    }
    if (index > 1 && memory[MAZE_MAP + index - 1] == P_MAZE_STREET) {
        maze_exits->WEST = P_MAZE_STREET;
        exit_count++;
    }

    return (exit_count == 1);
}

// Debug utility: prints the maze grid to stderr as ASCII art.
// '0' = open street, 'X' = wall, '?' = unknown tile type.
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

// Simplifies the maze by iteratively filling in dead-end passages with
// walls. This leaves only the paths that are part of a route between
// two exits, making the maze much easier to navigate without graphics.
// Each dead end is followed along its single exit until a junction is
// reached, filling cells with walls as it goes.
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

// Solves the maze using breadth-first search and prints step-by-step
// directions from the start position to the exit. The exit is identified
// as an open cell on the maze border (top/bottom row or left/right column)
// that isn't the start position.
void solve_maze(void) {
    int MAZE_WIDTH, MAZE_HEIGHT;
    get_maze_width_and_height(&MAZE_WIDTH, &MAZE_HEIGHT);
    int MAZE_MAP = get_global(sg.MAZE_MAP);
    int mazesize = MAZE_WIDTH * MAZE_HEIGHT;

    int start = get_global(sg.MAZE_XSTART) + get_global(sg.MAZE_YSTART) * MAZE_WIDTH;

    // Find exit: an open cell on the border that isn't the start
    int exit_pos = -1;
    for (int x = 0; x < MAZE_WIDTH && exit_pos == -1; x++) {
        if (memory[MAZE_MAP + x] == P_MAZE_STREET && x != start)
            exit_pos = x;
        int bottom = (MAZE_HEIGHT - 1) * MAZE_WIDTH + x;
        if (memory[MAZE_MAP + bottom] == P_MAZE_STREET && bottom != start)
            exit_pos = bottom;
    }
    for (int y = 0; y < MAZE_HEIGHT && exit_pos == -1; y++) {
        int left = y * MAZE_WIDTH;
        if (memory[MAZE_MAP + left] == P_MAZE_STREET && left != start)
            exit_pos = left;
        int right = y * MAZE_WIDTH + MAZE_WIDTH - 1;
        if (memory[MAZE_MAP + right] == P_MAZE_STREET && right != start)
            exit_pos = right;
    }

    if (exit_pos == -1) {
        transcribe_and_print_string("Could not find maze exit.\n");
        return;
    }

    // BFS from start to exit
    std::vector<int> parent(mazesize, -1);
    std::vector<bool> visited(mazesize, false);
    std::queue<int> bfs;

    visited[start] = true;
    bfs.push(start);

    while (!bfs.empty()) {
        int pos = bfs.front();
        bfs.pop();

        if (pos == exit_pos)
            break;

        // Cardinal neighbors: north, east, south, west
        int deltas[] = { -MAZE_WIDTH, 1, MAZE_WIDTH, -1 };
        for (int d : deltas) {
            int next = pos + d;
            if (next < 0 || next >= mazesize || visited[next])
                continue;
            if (memory[MAZE_MAP + next] != P_MAZE_STREET)
                continue;
            // Prevent wrapping across row boundaries for east/west moves
            if (d == 1 && pos % MAZE_WIDTH == MAZE_WIDTH - 1)
                continue;
            if (d == -1 && pos % MAZE_WIDTH == 0)
                continue;
            visited[next] = true;
            parent[next] = pos;
            bfs.push(next);
        }
    }

    if (!visited[exit_pos]) {
        transcribe_and_print_string("No path found through the maze.\n");
        return;
    }

    // Reconstruct path from exit back to start
    std::vector<int> path;
    for (int pos = exit_pos; pos != -1; pos = parent[pos])
        path.push_back(pos);
    std::reverse(path.begin(), path.end());

    // Print directions
    transcribe_and_print_string("\nMaze solution:\n");
    int lastdiff = 0;
    for (size_t i = 1; i < path.size(); i++) {
        int diff = path[i] - path[i - 1];
        if (lastdiff != diff) {
            if (diff == -MAZE_WIDTH)
                transcribe_and_print_string("North\n");
            else if (diff == 1)
                transcribe_and_print_string("East\n");
            else if (diff == MAZE_WIDTH)
                transcribe_and_print_string("South\n");
            else if (diff == -1)
                transcribe_and_print_string("West\n");
        }
        lastdiff = diff;
    }
    transcribe_and_print_string("\n");
}

// Flag to suppress re-asking the maze simplification question on autorestore,
// since the player already answered it in the original session.
bool dont_repeat_question_on_autorestore = false;

// Called after the Z-machine builds the maze. Prompts the player to optionally
// simplify the maze (fill in dead ends) for a better text-only experience.
void after_BUILDMAZE(void) {
    if (skip_puzzle_prompt("Would you like me to simplify the city maze, to make it easier to traverse without seeing the graphics? (Y is affirmative): >")) {
        simplify_maze();
//        solve_maze();
    }
}

#pragma mark - Window Layout

// Recalculates and applies the positions and sizes of the status and text
// buffer windows. On Apple II, also adjusts the horizontal banner window.
// Sets foreground/background colors from Z-machine globals.
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

// Draws the Shogun title screen (picture 1) scaled to fill the screen width,
// positioned at the bottom of a white background. Creates a foreground
// graphics window covering the entire screen to display it.
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

// Tears down all auxiliary windows (menu, crest, Apple II border) in
// preparation for a screen rebuild or game restart.
void shogun_erase_screen(void) {
    v6_delete_win(&SHOGUN_MENU_WINDOW);
    v6_delete_win(&SHOGUN_MENU_BG_WIN);
    v6_delete_win(&SHOGUN_CREST_WINDOW);
    if (graphics_type != kGraphicsTypeApple2) {
        v6_delete_win(&SHOGUN_A2_BORDER_WIN);
    }
}

// Master resize handler. Dispatches to the appropriate rebuild logic based
// on the current screen mode (definitions, hints, slideshow, menu, maze,
// or normal gameplay). In normal mode: redraws borders, refreshes margin
// images, repositions windows, updates the status line, and redraws the
// maze or crest window if active.
//
// Window layout:
//   Window 0 (S-TEXT):    Text buffer, main narrative window
//   Window 1 (S-STATUS):  Two-line status bar
//   Window 2 (MENU-WINDOW): Grid window for menu choices (bottom-centered)
//   Window 3 (MAZE-WINDOW): Maze (no Glk window; drawn to bg pixmap)
//   Window 4 (CREST):     Graphics window for the final P-CREST image
//   Window 5:             Text buffer for menu prompt messages
//   Window 6 (S-BORDER):  Horizontal Apple II graphic banner
//   Window 7 (S-FULL):    Full-screen background for border/maze graphics
void shogun_update_on_resize(void) {

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

// Draws the current picture as an inline image in the text buffer with
// the specified alignment. Used by the shared V6 image display code.
void shogun_display_inline_image(glui32 align) {
    float inline_scale = 2.0;
    if (graphics_type == kGraphicsTypeMacBW)
        inline_scale = imagescaley;
    draw_inline_image(V6_TEXT_BUFFER_WINDOW.id, current_picture, align, current_picture, inline_scale, false);
    add_margin_image_to_list(current_picture);
}

#pragma mark - Save/Restore State

// Saves Shogun-specific UI state into the autosave data structure: the
// current graphics window tags, border type, and menu state. This allows
// the exact UI configuration to be restored on autorestore.
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

// Restores Shogun-specific UI state from autosave data: looks up Glk
// windows by their saved tags and restores the border type and menu state.
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

// Performs a full UI rebuild after a manual RESTORE command. Resets to
// normal mode, redraws the border and status line, clears auxiliary
// windows, re-applies colors, and re-describes the current room.
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

// Performs a lighter UI rebuild after autorestore (app relaunch).
// Re-applies colors to all windows, clears graphics, deletes the menu
// window, and triggers a window change event. If the slideshow was
// active, re-opens the foreground graphics window.
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

// Cleanup after a RESTART command: removes the crest window and resets
// the menu selection so the start menu works correctly.
void shogun_update_after_restart(void) {
    // Get rid of ending crest graphics window
    v6_delete_win(&SHOGUN_CREST_WINDOW);
    current_menu_selection = 0;
}

// Handles edge cases when autorestoring into the middle of a read_char call.
// Returns true if the autorestore should re-enter the interrupted input loop
// (hints, menu, definitions, or the maze simplification prompt).
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

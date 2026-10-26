// v6_hints.cpp
//
// The Hints screen (InvisiClues) shared by Zork Zero, Shogun and Arthur: a
// three-level hint menu (topics -> questions -> progressive hints) with
// keyboard and mouse navigation and VoiceOver menu integration.

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

    // upperwidth is unsigned; compute in signed so a status window narrower
    // than 15 columns yields a negative `left` and trips the fallback below,
    // instead of wrapping to ~2e9 and making every click hit-test as column 0.
    int left = ((int)upperwidth - 15) / 2;
    if (left < 0)
        left = (int)upperwidth / 3;
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


// The hint screen's header is drawn in colours that suit the border picture
// of the graphics format in use, where there is one.
static void set_hint_window_colours(bool is_macintosh) {
    update_user_defined_colours();

    upperwin_foreground = user_selected_foreground;
    upperwin_background = user_selected_background;

    if (!is_spatterlight_arthur) {
        switch (graphics_type) {
            case kGraphicsTypeVGA:
            case kGraphicsTypeBlorb:
            case kGraphicsTypeAmiga:
                upperwin_background = zcolor_Default;
                break;
            case kGraphicsTypeEGA:
                upperwin_background = HINT_MAUVE;
                break;
            case kGraphicsTypeMacBW:
            case kGraphicsTypeCGA:
            case kGraphicsTypeApple2:
                upperwin_foreground = user_selected_background;
                upperwin_background = user_selected_foreground;
                break;
            case kGraphicsTypeNoGraphics:
                break;
        }
        if (options.int_number == INTERP_MACINTOSH && graphics_type == kGraphicsTypeAmiga) {
            upperwin_foreground = 0;
        } else if (graphics_type == kGraphicsTypeVGA || graphics_type == kGraphicsTypeEGA || graphics_type == kGraphicsTypeBlorb) {
            upperwin_foreground = kRGBWhite;
            if (upperwin_background == kRGBWhite) {
                upperwin_foreground = user_selected_background;
                if (upperwin_foreground == kRGBWhite)
                    upperwin_foreground = 0;
            }
        }
        if (is_spatterlight_zork0) {
            z0_erase_screen();
            garglk_set_zcolors_stream(glk_window_get_stream(V6_STATUS_WINDOW.id), upperwin_foreground, upperwin_background);
        }
    }

    if (is_spatterlight_arthur || (upperwin_background != zcolor_Default && upperwin_background != HINT_MAUVE))
        win_setbgnd(V6_STATUS_WINDOW.id->peer, upperwin_background);
}

// Works out where the hint screen's two windows go, from the hint border
// picture of the game and graphics format. Zork Zero's border is drawn here
// as well, because its measurements depend on it being the current one.
static void measure_hint_windows(bool is_macintosh, int &width, int &height, int &status_x) {
    V6_STATUS_WINDOW.x = 0;
    V6_STATUS_WINDOW.y = 0;

    height = gcellh * 4 + ggridmarginy * 2;

    if (is_spatterlight_zork0) {
        // Global BORDER-ON, false if in text-only mode
        bool text_only_mode = !z0_border_on();
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
}

// Sets up the window layout for the hints screen. Configures colors
// based on graphics type and platform, positions the status bar (with
// navigation instructions) and the menu/hint text area. For topic and
// question levels, the text buffer is remapped to a text grid for
// fixed-position menu display. Handles game-specific border drawing.
static void draw_hints_windows(void) {

    bool is_macintosh = (graphics_type == kGraphicsTypeMacBW || (options.int_number == INTERP_MACINTOSH && graphics_type == kGraphicsTypeAmiga));

    set_hint_window_colours(is_macintosh);

    v6_set_grid_colors(style_Normal);

    if (hints_depth != kV6MenuTypeHint && V6_TEXT_BUFFER_WINDOW.id->type == wintype_TextBuffer) {
        v6_remap_win(&V6_TEXT_BUFFER_WINDOW, wintype_TextGrid, &stored_bufferwin);
    } else if (V6_TEXT_BUFFER_WINDOW.id->type == wintype_TextGrid) {
        v6_delete_win(&V6_TEXT_BUFFER_WINDOW);
        V6_TEXT_BUFFER_WINDOW.id = stored_bufferwin;
        v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
    }

    win_refresh(V6_TEXT_BUFFER_WINDOW.id->peer, 0, 0);
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);

    // The margin left and right of the text, the height of the header,
    // and how far the status window is inset from the screen edges
    int width = 1, height, status_x = 0;
    measure_hint_windows(is_macintosh, width, height, status_x);

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
        z0_display_hint_border();
    }
}

// Prints a string centered on a given line of the status window.
// If reverse is true, displays in reverse video.
static void center_line(const char *str, int line, int length, bool reverse) {
    glui32 width;
    winid_t win = V6_STATUS_WINDOW.id;
    glk_window_get_size(win, &width, nullptr);
    if (length > width)
        width = length;
    glk_window_move_cursor(win, (width - length) / 2, line - 1);
    // Pin explicit upper-window colours around the reverse-video title so the
    // fg/bg swap yields a contrasting glyph. Without this, reverse video swaps
    // against the grid style_Normal colours and draws the title in
    // user_selected_background (often white), making it invisible.
    //
    // Both colours handed to the swap must be concrete: if upperwin_background
    // is zcolor_Default (the case for VGA/Blorb/Amiga graphics), the reverse
    // resolves it to the theme's default *foreground*, which is white in a
    // dark theme, producing a white-on-white title. Resolve it to the concrete
    // window background instead.
    bool set_upperwin_colors = is_spatterlight_shogun || is_spatterlight_zork0;
    glui32 title_background = upperwin_background;
    if (title_background == zcolor_Default)
        title_background = user_selected_background;
    // After the reverse swap the title bar is painted in upperwin_foreground
    // and the glyphs in title_background. Some graphics/interpreter
    // combinations leave those two colours with too little contrast for the
    // current theme (e.g. Macintosh + Amiga forces a black bar while the dark
    // theme's background is also near-black). Fall back to a plain black or
    // white glyph chosen to contrast with the bar.
    if (fabs(perceived_brightness(title_background) - perceived_brightness(upperwin_foreground)) < 0.2) {
        title_background = (perceived_brightness(upperwin_foreground) < 0.5) ? kRGBWhite : 0;
    }
    if (reverse) {
        if (set_upperwin_colors) {
            garglk_set_zcolors(upperwin_foreground, title_background);
        }
        garglk_set_reversevideo(1);
    }
    glk_put_string(const_cast<char *>(str));
    garglk_set_reversevideo(0);
    if (set_upperwin_colors) {
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
    return (call_routine(ar.RT_SEE_QST, {obj}) == 1);
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

// Address of the byte in the seen-hints table that holds the count for the
// current question, and which of its nibbles the count is in.
static uint16_t seen_hints_address(bool *odd) {
    uint16_t cv = user_word(seen_hints_table_addr + (h_chapt_num - 1) * 2);
    *odd = ((h_quest_num & 1) == 1);
    return cv + (h_quest_num - 1) / 2;
}

// Name of entry `index` of the menu on screen: a topic or a question.
static uint16_t hint_item_name(uint16_t index) {
    if (hints_depth == kV6MenuTypeTopic)
        return hint_topic_name(index);
    return hint_question_name(index);
}

// Returns the number of hints already revealed for the current question.
// The seen-hints table is a nibble table: the high 4 bits of each byte
// store the count for odd-numbered questions; the low 4 bits store even.
static int16_t get_seen_hints(void) {
    bool odd;
    int16_t seen = user_byte(seen_hints_address(&odd));
    if (odd) {
        seen = seen >> 4;
    }
    return seen & 0xf;
}

// Decodes and prints a Z-string, then sends it as a VoiceOver menu item
// at the appropriate index for the current hint depth level.
static void print_and_send_menuitem(uint16_t str, int max) {
    char string[4000];
    int length = print_long_zstr_to_cstr(str, string, sizeof(string));
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
            fprintf(stderr, "print_and_send_menuitem: Illegal menu type!\n");
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
        str = hint_item_name(i);
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
    uint16_t string_address = hint_item_name(index);
    print_handler(unpack_string(string_address), nullptr);
    garglk_set_reversevideo(0);
    return x;
}

// Stores the number of hints revealed for the current question in the
// nibble table. Preserves the other nibble in the same byte.
static void store_hints_seen(uint8_t value) {
    bool odd;
    uint16_t address = seen_hints_address(&odd);
    uint8_t seen = user_byte(address);
    if (odd) {
        seen = seen & 0x0f;
        value = value << 4;
    } else {
        seen = seen & 0xf0;
        value = value & 0x0f;
    }
    value = seen | value;
    user_store_byte(address, value);
}

// Initializes the hint display for the current question: shows the title,
// checks how many hints have been seen, and removes the "Return for a hint"
// prompt if all hints are already revealed. Returns the maximum hint count
// and optionally stores the base address for hint text lookup.
static int16_t init_hints(int16_t *hints_base_address) {

    int16_t h = user_word(user_word(hints_table_addr + h_chapt_num * 2) + (h_quest_num + 1) * 2);
    char str[kHintTitleMax];

    int offset = is_spatterlight_arthur ? 4 : 2;

    int length = print_long_zstr_to_cstr(user_word(h + offset), str, sizeof(str));
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
                        [[fallthrough]];
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
    char str[kHintTitleMax];
    int length = print_long_zstr_to_cstr(user_word(user_word(hints_table_addr + h_chapt_num * 2) + 2), str, sizeof(str));
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
                [[fallthrough]];
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
        // Window 2 is the topmost window used by Arthur in its status,
        // inventory, and room description modes, and Shogun's menu window.
        v6_delete_win(&windows[2]);
        if (is_spatterlight_arthur) {
            clear_image_buffer();
            if (current_graphics_buf_win)
                glk_window_clear(current_graphics_buf_win);
        } else {
            // Shogun's crest window and the text beside its menu
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

    v6_redraw_game_screen();

    // Allow transcript in main buffer window
    V6_TEXT_BUFFER_WINDOW.attribute |= WINATTR_TRANSCRIPTING_BIT;
    glk_put_string(const_cast<char*>("Back to the story...\n"));
    glk_set_echo_line_event(V6_TEXT_BUFFER_WINDOW.id, 0);
}

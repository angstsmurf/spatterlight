// zorkzero_minigames.cpp
//
// The Zork Zero mini-games: Double Fanucci, Snarfem (Nim), Peggleboz and the
// Tower of Bozbar. Each keeps its own state; they share only the window
// helpers declared in zorkzero_internal.hpp. The rest of the Zork Zero UI is
// in zorkzero.cpp.

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "memory.h"
#include "objects.h"
#include "options.h"
#include "process.h"
#include "screen.h"
#include "stack.h"
#include "unicode.h"
#include "v6_specific.h"
#include "v6_shared.hpp"
#include "zterp.h"

#include "entrypoints.hpp"
#include "draw_image.hpp"
#include "draw_border.hpp"

#include "zorkzero.hpp"
#include "zorkzero_internal.hpp"

// Positions the text buffer window below a mini-game border identified by
// split picture `id`. The border's dimensions determine the text area origin,
// and the window extends to fill the remaining screen space.
static void adjust_text_window_by_split(uint16_t id) {
    int width, height;
    get_image_size(id, &width, &height);

    V6_TEXT_BUFFER_WINDOW.y_origin = (height + 1) * imagescaley;
    V6_TEXT_BUFFER_WINDOW.x_origin = width * imagescalex;
    V6_TEXT_BUFFER_WINDOW.x_size = gscreenw - V6_TEXT_BUFFER_WINDOW.x_origin * 2;
    V6_TEXT_BUFFER_WINDOW.y_size = gscreenh - V6_TEXT_BUFFER_WINDOW.y_origin;
    v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
}

// Prompts the player to auto-solve a visual puzzle (used for accessibility
// with VoiceOver). Returns true if the player accepts.
static bool autosolve_visual_puzzle(void) {
    return skip_puzzle_prompt("\n\nWould you like to auto-solve this visual puzzle? (Y is affirmative): >");
}

#pragma mark - Double Fanucci

// Image location constants for the Double Fanucci card game UI
constexpr int F_MENU_LOC = 384;
constexpr int J_SCORE_LOC = 385;
constexpr int F_DISCARD_LOC = 419;
constexpr int F_CARD_1_LOC = 420;
constexpr int F_CARD_SPACE = 421;

constexpr int F_DISCARD_PIC_LOC = 369;

// Attribute offset: attributes DRAW_CARDS_OFFSET+0..+4 on NOT-HERE-OBJECT
// track which card slots have been drawn
constexpr int DRAW_CARDS_OFFSET = 22;

// Card face image constants
constexpr int F_CARD_BACK = 100;
constexpr int F_CARD = 101;
constexpr int F_INKBLOTS = 102;
constexpr int F_RV_INKBLOTS = 117;
constexpr int F_RANK_PIC_LOC = 374;
constexpr int F_REV_RANK_PIC_LOC = 375;
constexpr int F_SUIT_PIC_LOC = 376;
constexpr int F_REV_SUIT_PIC_LOC = 377;

// Rank number images: F_0 + rank for normal, F_RV_0 + rank for reversed
constexpr int F_0 = 132;
constexpr int F_RV_0 = 143;

// Special card: Granola (rank 12+)
constexpr int F_GRANOLA = 154;

// Location of card slot 1 (cards 2-4 follow at consecutive pic locs)
constexpr int F_1_PIC_LOC = 370;

// Fanucci window layout (used during initial setup):
//   Window 1: Jester's Score
//   z0_right_status_window: Your Score
//   Windows 2-6: Card labels (DISCARD, 1, 2, 3, 4)
//   fanucci_command_grid: Command/play selection grid
//   Window 7 (S-FULL): Background graphics
//   Window 0 (S-TEXT): Text buffer for messages

// Creates or resizes a text grid window for Fanucci UI elements (score
// displays, card labels). Makes the window transparent so the border
// graphics show through.
static void fanucci_window(int index, int x, int y, int num_chars) {
    Window *win = &windows[index];
    win->x_origin = x - ggridmarginx;
    win->y_origin = y - ggridmarginy;
    win->x_size = num_chars * gcellw + 2 * ggridmarginx;
    win->y_size = gcellh + 2 * ggridmarginy;
    win->x = 1;
    win->y = 1;
    if (win->id != nullptr && win->id->type != wintype_TextGrid) {
        gli_delete_window(win->id);
        win->id = nullptr;
    }

    if (win->id == nullptr) {
        win->id = gli_new_window(wintype_TextGrid, 0);
    }
    win_maketransparent(win->id->peer);
    v6_sizewin(win);
    glk_window_clear(win->id);
    glk_set_window(win->id);
}

// Prints a zero-padded 3-digit score in the given window at the specified
// character offset. Used for both Jester's and player's Fanucci scores.
static void update_score(winid_t win, uint16_t global, int offset) {
    glk_set_window(win);
    glk_window_move_cursor(win, offset, 0);
    char score[8];
    snprintf(score, sizeof score, "%03d", get_global(global));
    glk_put_string(score);
}

// Refreshes both Jester's and player's score displays.
static void update_scores(void) {
    update_score(V6_STATUS_WINDOW.id, zg.J_SCORE, 17);
    update_score(z0_right_status_window, zg.YOUR_SCORE, 13);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

// Draws an image at a position offset from (x,y) by the dimensions stored
// in the offset picture's metadata. Used for placing rank and suit symbols
// on card faces.
static void draw_offset_image(int pic, int x, int y, int offpic) {
    int offsetx, offsety;
    get_image_size(offpic, &offsetx, &offsety);
    draw_to_pixmap_unscaled(pic, x + offsetx, y + offsety);
}

// Draws a single Fanucci card at position (x,y). The rank and suit indices
// are looked up in F_CARD_TABLE. Face-down cards show the card back;
// special cards (Granola etc.) use dedicated images; normal cards are
// composited from a blank card, rank numbers (normal + reversed), and
// suit inkblot symbols.
static void draw_card(int x, int y, int rank, int suit) {
    static const int rank_order[] = { -1, 4, 5, 6, 7, 8, 9, 0, 10, 1, 2, 3 };
    if (zt.DRAW_CARDS_TABLE != 0) {
        // r296 keeps the cards in a table of words
        rank = user_word(zt.F_CARD_TABLE + rank * 2);
        suit = user_word(zt.F_CARD_TABLE + suit * 2);
    } else {
        rank = user_byte(zt.F_CARD_TABLE + rank);
        suit = user_byte(zt.F_CARD_TABLE + suit);
    }
    if (rank == 0) {
        draw_to_pixmap_unscaled(F_CARD_BACK, x, y);
    } else if (rank > 11) {
        int pic = F_GRANOLA + rank - 12;
        draw_to_pixmap_unscaled(pic, x, y);
    } else {
        draw_to_pixmap_unscaled(F_CARD, x, y);
        draw_offset_image(F_0 + rank_order[rank], x, y, F_RANK_PIC_LOC);
        draw_offset_image(F_RV_0 + rank_order[rank], x, y, F_REV_RANK_PIC_LOC);
        draw_offset_image(F_INKBLOTS + suit - 1, x, y, F_SUIT_PIC_LOC);
        draw_offset_image(F_RV_INKBLOTS + suit - 1, x, y, F_REV_SUIT_PIC_LOC);
    }

}

// Draws all 5 card slots (discard pile + 4 hand cards) at their respective
// positions. Sets attributes on NOT-HERE-OBJECT to track which slots are
// visible (r296 instead clears the slot's word in DRAW-CARDS-TABLE).
static void draw_cards(void) {
    int x, y;
    for (int cnt = 0; cnt < 5; cnt++) {
        get_image_size(F_DISCARD_PIC_LOC + cnt, &x, &y);
        uint16_t draw_cards_attribute = DRAW_CARDS_OFFSET + cnt;
        if (zt.DRAW_CARDS_TABLE != 0)
            user_store_word(zt.DRAW_CARDS_TABLE + cnt * 2, 0);
        else if (zo.NOT_HERE_OBJECT != 0 && !internal_test_attr(zo.NOT_HERE_OBJECT, draw_cards_attribute))
            internal_set_attr(zo.NOT_HERE_OBJECT, draw_cards_attribute);
        draw_card(x, y, cnt * 2, cnt * 2 + 1);
    }
}

// The text grid window used for the Fanucci command/play selection menu
static winid_t fanucci_command_grid = nullptr;
// The grid is found again by its rock after an autorestore
constexpr glui32 FANUCCI_COMMAND_GRID_ROCK = 0x56364647; // 'V6FG'
// Whether to use narrow (Apple IIe) command labels with 8-char spacing
static bool fanucci_window_is_narrow = false;

// Displays or un-highlights a command in the Fanucci play selection grid.
// The 15 commands are arranged in a 5-column x 3-row grid. When bold is
// true, prefixes the command with '>' in Subheader style.
static void fanucci_select_command(int ptr, bool bold) {
    glk_set_window(fanucci_command_grid);
    glui32 menu_space = fanucci_window_is_narrow ? 8 : 14;
    glui32 line = ptr % 3;
    glui32 col = ptr / 3 * menu_space;
    glk_window_move_cursor(fanucci_command_grid, col, line);
    if (bold) {
        glk_set_style(style_Subheader);
        glk_put_char('>');
    } else {
        glk_put_char(UNICODE_SPACE);
    }
    uint16_t fanucci_command_table = fanucci_window_is_narrow ? zt.F_PLAY_TABLE_APPLE : zt.F_PLAY_TABLE;
    uint16_t string = user_word(fanucci_command_table + ptr * 2);
    print_handler(unpack_string(string), nullptr);
    glk_set_style(style_Normal);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

// Index of the currently highlighted command in the Fanucci grid (-1 = none;
// 0, the first command, when a game starts)
static int last_bold_move = 0;

// Highlights a command and records it as the current selection.
static void bold_move(int ptr) {
    fanucci_select_command(ptr, true);
    last_bold_move = ptr;
}

// Un-highlights a command (removes the '>' prefix).
static void unbold_move(int ptr) {
    fanucci_select_command(ptr, false);
    last_bold_move = -1;
}

// Converts a mouse click position in the command grid window to a command
// index (0-14). Returns -1 if the click is outside the grid.
static int mouse_ptr_in_grid(void) {
    uint16_t width = fanucci_window_is_narrow ?
        8 : 14;
    uint16_t mouse_click_addr = header.extension_table + 2;
    int16_t x = word(mouse_click_addr) - 1;
    int16_t y = word(mouse_click_addr + 2) - 1;

    if (y > 2)
        return -1;

    int column = x / width;
    if (column > 4)
        return -1;

    return column * 3 + y;
}

// Main input loop for selecting a Fanucci play. Handles keyboard navigation
// (arrow keys, U/D/L/R, Enter) and mouse clicks (single-click to highlight,
// double-click to select). Returns true if the player resigned.
static bool pick_play() {
    uint16_t ptr = 0;
    bool clicked_in_grid = false;
    int top, left;
    get_image_size(F_MENU_LOC, &left, &top);
    bold_move(0);
    bool finished = false;
    bool resigned = false;
    while (!finished) {
        flush_image_buffer();
        // A Glk mouse request is used up by the click it delivers, so it
        // has to be renewed before every read, or the second click (the
        // one that selects the highlighted play) never arrives.
        if (fanucci_command_grid != nullptr)
            glk_request_mouse_event(fanucci_command_grid);
        uint16_t key = internal_read_char();

        if (key == ZSCII_CLICK_SINGLE || key == ZSCII_CLICK_DOUBLE) {
            clicked_in_grid = false;
            int new_ptr = mouse_ptr_in_grid();
            if (new_ptr != -1) {
                clicked_in_grid = true;
                if (ptr == new_ptr) {
                    key = ZSCII_CLICK_DOUBLE;
                } else {
                    unbold_move(ptr);
                    bold_move(new_ptr);
                    ptr = new_ptr;
                }
            }
            if (!clicked_in_grid) {
                glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
                win_beep(1);
                glk_put_string(const_cast<char*>("Click on one of the items in the menu to highlight a play; double-click on it to select that play."));
                //                <2CR-TO-PRINTER>
            }
        }

        int column = ptr / 3;
        int line = ptr % 3;
        switch(tolower(key)) {
            case ZSCII_CLICK_SINGLE:
                break;
            case ZSCII_CLICK_DOUBLE:
                if (!clicked_in_grid)
                    break;
                // Fallthrough
            case ZSCII_NEWLINE:
                finished = true;
                resigned = (call_routine(zr.PLAY_SELECTED, {ptr}) == 1); // <PLAY-SELECTED PTR>
                if (!resigned) {
                    unbold_move(ptr);
                }
                break;
            case ZSCII_UP:
            case ZSCII_KEY8:
            case 'u':
                unbold_move(ptr);
                if (line > 0) {
                    ptr--;
                }
                else {
                    ptr += 2;
                }
                bold_move(ptr);
                break;

            case ZSCII_DOWN:
            case ZSCII_KEY2:
            case 'd':
                unbold_move(ptr);
                if (line < 2) {
                    ptr++;
                }
                else {
                    ptr -= 2;
                }
                bold_move(ptr);
                break;

            case ZSCII_LEFT:
            case ZSCII_KEY4:
            case 'l':
                unbold_move(ptr);
                if (column > 0) {
                    ptr -= 3;
                } else {
                    ptr += 12;
                }
                bold_move(ptr);
                break;

            case ZSCII_RIGHT:
            case ZSCII_KEY6:
            case 'r':
            case ZSCII_SPACE:
                unbold_move(ptr);
                if (column < 4) {
                    ptr += 3;
                } else {
                    ptr -= 12;
                }
                bold_move(ptr);
                break;

            default:
                glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
                win_beep(1);
                glk_put_string(const_cast<char*>("Use the arrow keys to highlight a play, or hit RETURN/ENTER to select the currently highlighted play."));
                //                <2CR-TO-PRINTER>
                break;
        }
    }

    return resigned;
}

// Z-machine entry point: triggers the jester's play by calling the game's
// J-PLAY routine.
void J_PLAY(void) {
    call_routine(zr.J_PLAY);
}

// Checks whether the Fanucci game is over (one player's score is high enough).
static bool score_check(void) {
    int result = call_routine(zr.SCORE_CHECK);
    return (result == 1);
}

// Deletes the Fanucci command grid and the card-label grids (windows[2..6])
// and returns to normal mode.
static void teardown_fanucci_windows(void) {
    if (fanucci_command_grid != nullptr) {
        gli_delete_window(fanucci_command_grid);
        fanucci_command_grid = nullptr;
    }
    for (int i = 2; i < 7; i++)
        v6_delete_win(&windows[i]);
    screenmode = MODE_NORMAL;
}

// Z-machine entry point: main Double Fanucci game loop. Alternates between
// player play selection and jester plays, updating cards and scores each
// round. The game ends when one side wins 3 rounds, the score threshold is
// met, or the player resigns. Offers auto-solve for accessibility.
void FANUCCI(void) {
    bold_move(0);
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
    glk_put_string(const_cast<char*>("Use the arrow keys -- or the U, D, L and R keys -- to highlight a play, then hit the RETURN/ENTER key."));
    if (mouse_available()) {
        glk_put_string(const_cast<char*>(" Or, you can use your mouse"));
        if (options.int_number == INTERP_APPLE_IIE) {
            glk_put_string(const_cast<char*>("/joystick"));
        }
        glk_put_string(const_cast<char*>(" to select your play."));
    }
    bool finished = false;

    if (zo.BROOM != 0 && autosolve_visual_puzzle()) {
        internal_clear_attr(zo.BROOM, zp.TRYTAKEBIT);
        finished = true;
    }

    while(!finished) {
        if (pick_play()) {
            finished = true;
        } else {
            draw_cards();
            update_scores();
            uint16_t f_win_count = get_global(zg.F_WIN_COUNT);
            if (f_win_count == 3) {
                uint16_t your_score = get_global(zg.YOUR_SCORE);
                set_global(zg.YOUR_SCORE, your_score + 1000);
                if (zo.BROOM != 0)
                    internal_clear_attr(zo.BROOM, zp.TRYTAKEBIT);
                finished = true;
            } else if (score_check()) {
                finished = true;
            } else {
                glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
                J_PLAY();
                draw_cards();
                uint16_t f_plays = get_global(zg.F_PLAYS);
                set_global(zg.F_PLAYS, f_plays + 1);
                update_scores();
            }
            if (score_check())
                finished = true;
        }
    }
    teardown_fanucci_windows();
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

// Demo-only window cleanup, called from zerase_window() on @erase_window.
//
// The demo's SLIDE-SHOW (prologue.zil) displays each sample screen --
// encyclopedia, map, Double Fanucci -- and separates them with @erase_window
// -1, ending with one more before it hands off to the interactive section.
// Unlike normal play it only *sets up* the mini-games (e.g. SETUP-FANUCCI,
// never the FANUCCI play loop above that tears its windows down at the end), so
// the Glk windows they create are never destroyed and linger on top of the
// following sample -- and into the interactive part. Mirror
// arthur_erase_window() and tear those leftovers down on the fullscreen clear.
//
// This is gated on the slideshow/mini-game screen modes, so it is inert during
// normal play: there @erase_window -1 is issued when *entering* the
// encyclopedia/map (PICTURED-ENTRY / DO-MAP) or toggling borders (V-MODE),
// while still in MODE_NORMAL, and the mini-games run their own C++ teardown
// (which resets screenmode to MODE_NORMAL) before any clear.
void z0_erase_window(int16_t index) {
    if (!is_spatterlight_zork0 || index != -1)
        return;

    switch (screenmode) {
    case MODE_SLIDESHOW: // title / rebus / encyclopedia sample
    case MODE_MAP:       // map sample
        // Encyclopedia caption window (only created for the encyclopedia
        // sample; a no-op for the others). Restore the text-buffer colour
        // hints it overrode, exactly as V_REFRESH does on a normal exit.
        if (Z0_ENCYCLOPEDIA_WINDOW.id != nullptr) {
            z0_close_encyclopedia_window();
        }
        // Hide the fullscreen foreground graphics window the sample took over
        // and switch drawing back to the background window (cf. DISPLAY_BORDER).
        if (current_graphics_buf_win == graphics_fg_glk) {
            win_sizewin(graphics_fg_glk->peer, 0, 0, 0, 0);
            glk_cancel_char_event(graphics_fg_glk);
            current_graphics_buf_win = graphics_bg_glk;
        }
        screenmode = MODE_NORMAL;
        break;

    case MODE_Z0_GAME: // Double Fanucci sample (SETUP-FANUCCI only)
        // fanucci_command_grid is the unique marker that Fanucci is set up;
        // tear it and the card-label grids (windows[2..6]) down exactly as the
        // FANUCCI play loop does on a normal exit.
        if (fanucci_command_grid != nullptr)
            teardown_fanucci_windows();
        break;

    default:
        break;
    }
}

// Tests whether the last mouse click position falls within the rectangle
// defined by (left, top, width, height) in unscaled image coordinates.
// Used by all mini-games for mouse hit-testing.
// A click in a graphics window is recorded in window pixels, and
// flush_bitmap() stretches the pixmap from the window's top left corner by
// imagescalex and imagescaley, so dividing by those gives the pixmap position.
static bool within(int left, int top, int width, int height) {
    uint16_t mouse_click_addr = header.extension_table + 2;
    int x = (int16_t)word(mouse_click_addr) / imagescalex;
    int y = (int16_t)word(mouse_click_addr + 2) / imagescaley;

    return (x >= left && x < left + width && y >= top && y < top + height);
}

// Redraws the entire Double Fanucci display: border graphics, all cards,
// score windows (Jester's in window 1, player's in right status window),
// card slot labels (windows 2-6), and the command selection grid.
// Adjusts layout for narrow screens by using abbreviated command labels.
//
// Window layout during Fanucci:
//   Window 1: Jester's Score     right_status: Your Score
//   Windows 2-6: Card labels (DISCARD, 1, 2, 3, 4)
//   fanucci_command_grid: 5x3 play selection menu
//   Window 7 (S-FULL): Background border
//   Window 0 (S-TEXT): Text buffer for messages
static void redraw_fanucci(void) {
    int x, y;

    get_image_size(J_SCORE_LOC, &x, &y);

    x *= imagescalex;
    if (graphics_type == kGraphicsTypeMacBW)
        y += 2;
    y *= imagescaley;
    // The two score windows are the status line windows, and keep whatever
    // text colour they had last. That is the background colour if the border
    // is off, and the theme's default in the demo's slide show, where
    // INIT-STATUS-LINE has not run yet. Give them the colour of a bordered
    // status line, as the scores are printed on top of the border picture.
    z0_set_bordered_status_colors();

    fanucci_window(1, x, y, 20);
    win_refresh(V6_STATUS_WINDOW.id->peer, 0, 0);
    glk_put_string(const_cast<char*>("Jester's Score:  000"));

    // Same vertical position as the Jester's score window
    const grect_t &jester = V6_STATUS_WINDOW.id->bbox;
    win_sizewin(z0_right_status_window->peer, std::max(gscreenw - 21 * gcellw - 2 * ggridmarginx, 0.0f), jester.y0, gscreenw - x, jester.y1);
    win_refresh(z0_right_status_window->peer, 0, 0);
    glk_set_window(z0_right_status_window);
    glk_window_clear(z0_right_status_window);
    glk_put_string(const_cast<char*>("Your Score:  000"));

    int card_num_y, discard_x, card_num_1_x, card_space, discard_width;

    get_image_size(F_DISCARD_LOC, nullptr, &card_num_y);

    card_num_y += 1; // Looks nicer with more vertical space

    get_image_size(F_DISCARD_PIC_LOC, &discard_x, nullptr);
    get_image_size(F_CARD, &discard_width, nullptr);

    discard_x *= imagescalex;
    discard_width *= imagescalex;
    discard_x += (discard_width - gcellw * 7) / 2;

    card_num_y *= imagescaley;
    get_image_size(F_DISCARD_PIC_LOC + 1, &card_num_1_x, nullptr);
    card_num_1_x *= imagescalex;
    card_num_1_x += discard_width / 2;
    get_image_size(F_CARD_SPACE, &card_space, nullptr);
    if (graphics_type != kGraphicsTypeApple2 && graphics_type != kGraphicsTypeMacBW && graphics_type != kGraphicsTypeCGA) {
        card_space += 1;
    }
    card_space *= imagescalex;

    update_scores();
    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(zorkzero_double_fanucci_border, 0, 0);
    draw_cards();

    // Make a separate grid window for each card label
    // (otherwise we can't center them properly)
    fanucci_window(2, discard_x, card_num_y, 7);
    glk_put_string(const_cast<char*>("DISCARD"));

    fanucci_window(3, card_num_1_x, card_num_y, 1);
    glk_put_char('1');
    fanucci_window(4, card_num_1_x + card_space, card_num_y, 1);
    glk_put_char('2');
    fanucci_window(5, card_num_1_x + card_space * 2, card_num_y, 1);
    glk_put_char('3');
    fanucci_window(6, card_num_1_x + card_space * 3, card_num_y, 1);
    glk_put_char('4');

    get_image_size(F_MENU_LOC, &x, &y);

    x = x * imagescalex; // We don't actually use this for the grid window
    y = (y + 3) * imagescaley; // Looks nicer with more vertical space

    // And a separate grid window for the commands
    if (fanucci_command_grid == nullptr)
        fanucci_command_grid = gli_new_window(wintype_TextGrid, FANUCCI_COMMAND_GRID_ROCK);

    win_maketransparent(fanucci_command_grid->peer);

    int fanucci_command_window_width = 64 * gcellw + 2 * ggridmarginx;
    int xpos = (gscreenw - fanucci_command_window_width) / 2;

    // Use abbreviated Apple 2 commands and
    // narrow spacing if we are out of
    // horizontal space
    int min_left = x - ggridmarginx - 2 * gcellw;
    if (xpos < min_left) {
        fanucci_window_is_narrow = true;
        fanucci_command_window_width = 40 * gcellw + 2 * ggridmarginx;
        xpos = (gscreenw - fanucci_command_window_width) / 2 - gcellw;
    } else {
        fanucci_window_is_narrow = false;
    }

    if (options.int_number == INTERP_APPLE_IIE)
        fanucci_window_is_narrow = true;

    if (xpos < min_left)
        xpos = min_left;

    win_sizewin(fanucci_command_grid->peer, xpos, y, xpos + fanucci_command_window_width, y + 4 * gcellh + 2 * ggridmarginy);

    glk_window_clear(fanucci_command_grid);
    glk_set_window(fanucci_command_grid);

    int stored_last_bold_move = last_bold_move;
    // Print the commands inside the grid
    for (int i = 0; i < 15; i++) {
        unbold_move(i);
    }
    if (stored_last_bold_move != -1)
        bold_move(stored_last_bold_move);

    V6_TEXT_BUFFER_WINDOW.x_origin = x;
    V6_TEXT_BUFFER_WINDOW.y_origin = y + 4 * gcellh + 2 * ggridmarginy;
    V6_TEXT_BUFFER_WINDOW.x_size = gscreenw - 2 * x;
    ADJUST_TEXT_WINDOW(F_BOTTOM);
    flush_image_buffer();
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

// Z-machine entry point: initializes the Double Fanucci game display.
// Redraws the board, enables mouse events on the command grid, clears
// the text buffer, and sets the screen mode to game mode.
void SETUP_FANUCCI(void) {
    last_bold_move = 0;
    redraw_fanucci();
    glk_request_mouse_event(fanucci_command_grid);
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
    screenmode = MODE_Z0_GAME;
}

#pragma mark - Snarfem (Nim)

// Snarfem is a Nim-variant game: players take turns removing items from piles.
// The game displays 4 piles of items, numbered selection boxes, and flower
// indicators that hint at the optimal move.

// Base picture numbers for pile display (PILE_OF_0 + count = pile image)
constexpr int PILE_OF_0 = 74;

// Flower images indicating the optimal move:
// Left flower shows which pile, right flower shows how many to take
constexpr int L_FLOWERS_0 = 94;
constexpr int R_FLOWERS_0 = 84;

// Image location for pile 1 (piles 2-4 follow at consecutive pic locs)
constexpr int PILE_1_PIC_LOC = 357;
constexpr int L_FLOWERS_PIC_LOC = 361;
constexpr int R_FLOWERS_PIC_LOC = 362;

// Numbered selection box images
constexpr int BOX_1 = 445;

// Location and spacing for the row of numbered boxes
constexpr int BOX_1_LOC = 470;
constexpr int SN_BOX_SPACE = 471;

// Dimmed vs active number images for the selection boxes
constexpr int DIMMED_SN_NUMBER = 453;
constexpr int SN_NUMBER = 444;

// Draws the row of 9 numbered selection boxes. Boxes are dimmed if:
// - No pile is selected (pile==0) and the box number exceeds 4 or the
//   corresponding pile is empty
// - A pile is selected and the box number exceeds that pile's count
static void snarfem_draw_numbered_boxes(int pile) {
    int x, y, space;

    get_image_size(BOX_1_LOC, &x, &y);
    get_image_size(SN_BOX_SPACE, &space, nullptr);
    for (int i = 1; i <= 9; i++) {
        bool dimmed = false;
        if (pile == 0) {
            if (i > 4 || user_word(zt.PILE_TABLE + i * 2) == 0) {
                dimmed = true;
            }
        } else if (user_word(zt.PILE_TABLE + pile * 2) < i) {
            dimmed = true;
        }

        draw_to_pixmap_unscaled(dimmed ? DIMMED_SN_NUMBER + i : SN_NUMBER + i, x, y);
        x += space;
    }
}

// Draws a single pile image showing the current count of items.
static void snarfem_draw_pile(int pile) {
    int num = user_word(zt.PILE_TABLE + pile * 2);
    int x, y;
    get_image_size(PILE_1_PIC_LOC + pile - 1, &x, &y);
    draw_to_pixmap_unscaled(PILE_OF_0 + num, x, y);
}

// Z-machine entry point: draws the flower hint indicators showing the
// computer's optimal move. If the current position is already "safe"
// (Nim-sum zero), shows neutral flowers. Otherwise, searches for a move
// (pile + count to remove) that leaves a safe position, and shows flowers
// indicating which pile and how many.
void DRAW_FLOWERS(void) {
    int left, right;
    uint16_t pile_table[4];
    for (int i = 0; i < 4; i++) {
        pile_table[i] = user_word(zt.PILE_TABLE + (i + 1) * 2);
    }
    uint16_t nim_sum = pile_table[0] ^ pile_table[1] ^ pile_table[2] ^ pile_table[3];
    if (nim_sum == 0) {
        left = L_FLOWERS_0;
        right = R_FLOWERS_0;
    } else {
        // Find a pile that can be reduced to make the Nim-sum zero.
        // The target value for that pile is pile[i] ^ nim_sum;
        // this is smaller than pile[i] when pile[i] has a leading bit of nim_sum set.
        int pile = 0;
        for (int i = 0; i < 4; i++) {
            if ((pile_table[i] ^ nim_sum) < pile_table[i]) {
                pile = i;
                break;
            }
        }
        int num = pile_table[pile] - (pile_table[pile] ^ nim_sum);
        left = L_FLOWERS_0 + 1 + pile;
        right = R_FLOWERS_0 + num;
    }
    int x, y;
    get_image_size(L_FLOWERS_PIC_LOC, &x, &y);
    draw_to_pixmap_unscaled(left, x, y);
    get_image_size(R_FLOWERS_PIC_LOC, &x, &y);
    draw_to_pixmap_unscaled(right, x, y);
}

// Hit-tests a mouse click for Snarfem. First checks numbered boxes (1-9),
// then pile images (if a pile hasn't been picked yet). Returns the clicked
// box/pile number, or 0 if nothing was hit.
static uint16_t snarfem_click(bool already_picked_pile) {
    int width, height;
    get_image_size(BOX_1, &width, &height);
    int left, top;
    get_image_size(BOX_1_LOC, &left, &top);
    int box_space;
    get_image_size(SN_BOX_SPACE, &box_space, nullptr);
    for (int i = 1; i <= 9; i++) {
        if (within(left, top, width, height)) {
            return i;
        }
        left += box_space;
    }
    if (already_picked_pile)
        return 0;
    // There are only four piles; PILE_1_PIC_LOC + 4 onward are the flower
    // and peg locations, not piles. Like the original SN-CLICK, every pile
    // uses the size of PILE-OF-1 as its hit box (all pile images are the
    // same size anyway).
    get_image_size(PILE_OF_0 + 1, &width, &height);
    for (int i = 1; i <= 4; i++) {
        get_image_size(PILE_1_PIC_LOC + i - 1, &left, &top);
        if (within(left, top, width, height)) {
            return i;
        }
    }

    return 0;
}

// Tracks which pile was last selected (for numbered box display)
static int last_pile = 0;

// Redraws the entire Snarfem display: border, all 4 piles, flower hints,
// numbered selection boxes, and positions the text buffer window below.
static void draw_snarfem(void) {
    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(zorkzero_snarfem_border, 0, 0);
    for(int i = 1; i <= 4; i++)
        snarfem_draw_pile(i);
    DRAW_FLOWERS();
    // The number boxes are there to be clicked on. r242 has no mouse support
    // in Snarfem (no SN-CLICK, no DRAW-SN-BOXES) and never draws them.
    if (zr.SN_CLICK != 0)
        snarfem_draw_numbered_boxes(last_pile);

    int x, y, distance_from_bottom, height;
    get_image_size(SN_SPLIT, &x, &y);
    set_global(zg.CURRENT_SPLIT, SN_SPLIT); // <SETG CURRENT-SPLIT .ID>
    x *= imagescalex;
    y++;
    y *= imagescaley;
    get_image_size(zorkzero_snarfem_border, nullptr, &height);
    height *= imagescaley;
    get_image_size(SN_BOTTOM, nullptr, &distance_from_bottom);
    distance_from_bottom *= imagescaley;

    v6_define_window(&V6_TEXT_BUFFER_WINDOW, x + imagescalex, y, gscreenw - 2 * x, height - y - distance_from_bottom);

    flush_image_buffer();
}


// Z-machine entry point: initializes the Snarfem game display.
// Hides the status bar, draws the board, and sets up the text window.
void SETUP_SN(void) {
    z0_hide_right_status();

    v6_define_window(&V6_STATUS_WINDOW, 0, 0, 0, 0);

    draw_snarfem();
    set_current_window(&V6_TEXT_BUFFER_WINDOW);
}

// Z-machine entry point: redraws the numbered selection boxes for the
// pile specified in local variable 1.
void DRAW_SN_BOXES(void) {
    last_pile = variable(1);
    snarfem_draw_numbered_boxes(variable(1));
}

// Z-machine entry point: redraws a single pile (pile number in variable 1).
void DRAW_PILE(void) {
    snarfem_draw_pile(variable(1));
}

// Z-machine entry point: processes a mouse click during Snarfem.
// Stores the clicked element (pile or number) in variable 2.
void SN_CLICK(void) {
    store_variable(2, snarfem_click(variable(1)));
}

// Z-machine entry point: offers auto-solve for the Snarfem game. This
// fires at the top of the game's main loop. If the player accepts, jump
// straight into the branch the game takes when the last pebble is removed,
// so the game itself does whatever a win entails in this release (which
// differs: r242 awards the fan and the points right there, later releases
// flag the win on the FAN object for the caller).
//
// The loop comes round once per keypress (pile, then number of pebbles), so
// remember that the question has been answered for this game.
static bool snarfem_asked = false;

void SNARFEM(void) {
    if (zr.SNARFEM_WIN == 0)
        return;
    // Local 4 is the stop flag. Whoever wins, the game sets it and loops back
    // here to return, which is the only way out of the routine: the game is
    // over, so the next one may ask again.
    if (variable(4) != 0) {
        snarfem_asked = false;
        return;
    }
    if (snarfem_asked)
        return;
    snarfem_asked = true;
    if (autosolve_visual_puzzle()) {
        // r242 (the only release where the branch moves the fan with an
        // INSERT_OBJ) continues the "You remove N pebbles from Pile #N"
        // sentence with its winning message, so give it something to continue.
        if (memory[zr.SNARFEM_WIN + 3] == 0xce)
            transcribe_and_print_string("You remove the last pebble");
        pc = zr.SNARFEM_WIN;
    }
}

#pragma mark - Peggleboz

// Peggleboz is a peg-solitaire puzzle. The player jumps pegs over each other
// to remove them, trying to leave as few pegs as possible. The board has 21
// peg positions. UI buttons: Restart, Show Moves, and Exit.

// The clickable boxes of Peggleboz and the Tower of Bozbar each have a
// picture, and a "location" picture whose size is where the box goes.
static void draw_box(int pic, int location_pic) {
    int x, y;
    get_image_size(location_pic, &x, &y);
    draw_to_pixmap_unscaled(pic, x, y);
}

// Whether the last mouse click was on the box.
static bool box_clicked(int pic, int location_pic) {
    int left, top, width, height;
    get_image_size(location_pic, &left, &top);
    get_image_size(pic, &width, &height);
    return within(left, top, width, height);
}

// Draws a Peggleboz UI button (Restart, Show Moves, or Exit) at its
// designated location.
void z0_draw_peggleboz_box_image(uint16_t pic) {
    switch (pic) {
        case RESTART_BOX:
        case DIM_RESTART_BOX:
            draw_box(pic, PBOZ_RESTART_BOX_LOC);
            return;
        case SHOW_MOVES_BOX:
        case DIM_SHOW_MOVES_BOX:
            draw_box(pic, PBOZ_SHOW_MOVES_BOX_LOC);
            return;
        case EXIT_BOX:
            draw_box(pic, PBOZ_EXIT_BOX_LOC);
            return;
        default:
            fprintf(stderr, "Error! Unhandled peggleboz image!\n");
    }
}

static const int kPegBoardTableSize = 43;

// Redraws the entire Peggleboz board: border, all peg positions (delegated
// to the game's DRAW-PEGS routine), UI buttons (dimmed if no move has been
// made), and positions the text window below the game area.
static void draw_peggles(void) {
    // Without these we would write the peg positions into low memory and
    // call a routine at address 0.
    if (zt.PBOZ_PIC_TABLE == 0 || zt.BOARD_TABLE == 0 || zr.DRAW_PEGS == 0)
        return;

    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(zorkzero_peggleboz_border, 0, 0);

    int width, height;
    for (int i = 2; i < kPegBoardTableSize; i += 2) {
        uint16_t pic = user_word(zt.PBOZ_PIC_TABLE + i - 2);
        get_image_size(pic, &width, &height);
        store_word(zt.BOARD_TABLE + i * 2, height);
        store_word(zt.BOARD_TABLE + (i + 1) * 2, width);
    }

    call_routine(zr.DRAW_PEGS); // <DRAW-PEGS>

    // The boxes are there to be clicked on. r242 has no mouse support in
    // Peggleboz (no PBOZ-CLICK) and never draws them.
    if (zr.PBOZ_CLICK != 0) {
        bool has_moved = get_global(zg.PEG_MOVE_NUMBER) > 0;
        draw_box(has_moved ? RESTART_BOX : DIM_RESTART_BOX, PBOZ_RESTART_BOX_LOC);
        draw_box(has_moved ? SHOW_MOVES_BOX : DIM_SHOW_MOVES_BOX, PBOZ_SHOW_MOVES_BOX_LOC);
        draw_box(EXIT_BOX, PBOZ_EXIT_BOX_LOC);
    }

    adjust_text_window_by_split(PBOZ_SPLIT);
    ADJUST_TEXT_WINDOW(PBOZ_BOTTOM);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

// Currently selected/highlighted peg position (0 = none, even index into
// BOARD_TABLE for the peg's coordinates)
static uint16_t selected_peg_pos = 0;

// When true, auto-solves Peggleboz: PEG_GAME() jumps to the game's winning
// branch and PBOZ_WIN_CHECK() fakes a winning board
static bool skip_pboz = false;

// Z-machine entry point: initializes the Peggleboz game. Hides the status
// bar, sets attributes on game objects, draws the board, and offers
// auto-solve for accessibility.
void SETUP_PBOZ(void) {
    z0_hide_right_status();

    if (V6_STATUS_WINDOW.id != nullptr) {
        v6_define_window(&V6_STATUS_WINDOW, 0, 0, 0, 0);
    }

    // Each of attributes 1-21 of NOT-HERE-OBJECT is a hole on the board, and
    // a set attribute means that its peg is out of play. The game starts
    // with a single empty hole, number 7. (PBOZ-WIN-CHECK considers the game
    // won when this is reversed: every attribute set except 7.)
    // (This does not apply to r242 and r296, which keep the pegs in PEG-TABLE
    // and whose SETUP-PBOZ leaves them alone.)
    if (zo.NOT_HERE_OBJECT != 0)
        internal_set_attr(zo.NOT_HERE_OBJECT, 7);
    if (zo.PBOZ_OBJECT != 0)
        internal_set_attr(zo.PBOZ_OBJECT, zp.TOUCHBIT);
    set_global(zg.PEG_MOVE_NUMBER, 0);
    selected_peg_pos = 0;
    draw_peggles();
    flush_image_buffer();

    // Nothing to tell the player here: the game clears the text window
    // before announcing the win.
    if (zr.PEG_GAME_WIN != 0 && autosolve_visual_puzzle())
        skip_pboz = true;

}

// Hit-tests a mouse click for Peggleboz. Checks UI buttons first
// (Restart='Z', Show Moves='Y', Exit='X'), then peg positions
// (returns ASCII 65+ for peg index). Beeps if nothing was hit.
static uint16_t pboz_click(void) {
    if (box_clicked(RESTART_BOX, PBOZ_RESTART_BOX_LOC))
        return 'Z';
    if (box_clicked(SHOW_MOVES_BOX, PBOZ_SHOW_MOVES_BOX_LOC))
        return 'Y';
    if (box_clicked(EXIT_BOX, PBOZ_EXIT_BOX_LOC))
        return 'X';

    int left, top;

    int expand_width, expand_height, peg_width, peg_height;
    get_image_size(EXPAND_HOT_SPOT, &expand_width, &expand_height);
    get_image_size(UNHL_PEG, &peg_width, &peg_height);

    for (int i = 2; i < kPegBoardTableSize; i += 2) {
        top = user_word(zt.BOARD_TABLE + i * 2);
        left = user_word(zt.BOARD_TABLE + (i + 1) * 2);
        if (within(left - expand_width, top, peg_width + 2 * expand_width, peg_height + expand_height)) {
            return i / 2 + 64; // "divide in half and convert to ASCII"
        }
    }
    win_beep(1);
    return ZSCII_CLICK_SINGLE;
}

// Z-machine entry point: processes a mouse click during Peggleboz.
void PBOZ_CLICK(void) {
    store_variable(2, pboz_click());
}

// Z-machine entry point: called in the main loop of PEG-GAME, right before
// it waits for a key. Records the selected peg position (variable 2 is the
// peg index, doubled for BOARD_TABLE indexing). During auto-solve, skips the
// key and jumps to the code that follows a legal move instead, which clears
// the text window and calls PBOZ-WIN-CHECK.
void PEG_GAME(void) {
    selected_peg_pos = variable(2) * 2;
    if (skip_pboz)
        pc = zr.PEG_GAME_WIN;
}

// Z-machine entry point: during auto-solve, sets attributes 1-21 (except 7)
// on NOT-HERE-OBJECT to fake a winning board state, and redraws the board
// to match.
void PBOZ_WIN_CHECK(void) {
    if (!skip_pboz)
        return;
    skip_pboz = false;
    if (zt.PEG_TABLE != 0) {
        // r242 and r296 have a table of words instead, where 1 means that the
        // peg is on the board: remove them all and put one back at number 7.
        for (int i = 1; i < 22; i++) {
            store_word(zt.PEG_TABLE + i * 2, 0);
        }
        store_word(zt.PEG_TABLE + 7 * 2, 1);
    } else if (zo.NOT_HERE_OBJECT != 0) {
        // Each of the first 21 attributes of NOT-HERE-OBJECT represents a peg on the board.
        // A set attribute means the peg is out-of-play. We simulate a winning state by
        // setting all of them and then clearing one (number 7.)
        for (int i = 1; i < 22; i++) {
            internal_set_attr(zo.NOT_HERE_OBJECT, i);
        }
        internal_clear_attr(zo.NOT_HERE_OBJECT, 7);
    }
    draw_peggles();
    flush_image_buffer();
}

// Z-machine entry point: outputs a line feed for the "Show Moves" display.
void DISPLAY_MOVES(void) {
    glk_put_char(UNICODE_LINEFEED);
}


#pragma mark - Tower of Bozbar

// Tower of Bozbar is a Tower of Hanoi variant with 6 weights on 3 pegs
// (left, center, right). The player moves weights between pegs following
// the standard rules (only the top weight can be moved, and a heavier
// weight cannot be placed on a lighter one).

static const int kTowerWeights = 6;

// Clears a peg table, removing all weights from that peg.
static void clear_peg_table(uint16_t table) {
    for (int i = 0; i < kTowerWeights; i++) {
        store_word(table + i * 2, 0);
    }
}

static void draw_tower_of_bozbar(void);

// Set by auto-solve to indicate the tower has been solved externally
static bool tower_solved = false;

// Auto-solves the Tower of Bozbar by moving all 6 weights (in order) onto
// the specified peg ('l', 'c', or 'r'). Updates both the Z-machine object
// tree and the peg tables, then redraws the display.
static void solve_tower(uint8_t c) {
    uint16_t peg = 0;
    uint16_t table = 0;
    switch (c) {
        case 'l':
            peg = zo.LEFT_PEG;
            table = zt.LEFT_PEG_TABLE;
            break;
        case 'c':
            peg = zo.CENTER_PEG;
            table = zt.CENTER_PEG_TABLE;
            break;
        case 'r':
            peg = zo.RIGHT_PEG;
            table = zt.RIGHT_PEG_TABLE;
            break;
        default:
            fprintf(stderr, "solve_tower: unknown peg '%c'\n", c);
            return;
    }

    clear_peg_table(zt.LEFT_PEG_TABLE);
    clear_peg_table(zt.CENTER_PEG_TABLE);
    clear_peg_table(zt.RIGHT_PEG_TABLE);

    // Heaviest first, so that the lightest ends up on top.
    const uint16_t weights[kTowerWeights] = {
        zo.SIX_WEIGHT, zo.FIVE_WEIGHT, zo.FOUR_WEIGHT,
        zo.THREE_WEIGHT, zo.TWO_WEIGHT, zo.ONE_WEIGHT
    };
    for (int i = 0; i < kTowerWeights; i++) {
        internal_insert(weights[i], peg);
        store_word(table + i * 2, weights[i]);
    }

    draw_tower_of_bozbar();

    if (zg.TOWER_CHANGED != 0) // Does not exist in r242
        set_global(zg.TOWER_CHANGED, 1);
    else
        tower_solved = true;
}

// Prompts the player to choose which peg to move all weights to for the
// auto-solve. Shows the current peg holding the weights and loops until
// a valid choice (L/C/R) is entered.
static void ask_which_peg(void) {
    uint16_t result = call_routine(zr.TOWER_WIN_CHECK);
    if (result > 0) {
        transcribe_and_print_string("Currently, the weights are on the ");
        if (result == zo.PYRAMID_L) {
            transcribe_and_print_string("left");
        } else if (result == zo.PYRAMID_R) {
            transcribe_and_print_string("right");
        } else {
            transcribe_and_print_string("center");
        }
        transcribe_and_print_string(" peg. ");
    }
    transcribe_and_print_string("To which peg do you want to move the weights? (L/C/R) >");

    bool done = false;
    uint8_t c;
    while (!done) {
        c = internal_read_char();
        glk_put_char(c);
        transcribe(c);
        transcribe_and_print_string("\n");
        c = tolower(c);
        switch (c) {
            case 'l':
            case 'r':
            case 'c':
                done = true;
                break;
            default:
                transcribe_and_print_string("Please type L, C, or R. > ");
                break;
        }
    }

    solve_tower(c);
}

// Z-machine entry point: called when the Tower of Bozbar puzzle is first
// encountered. Offers auto-solve for accessibility, or provides a VoiceOver
// hint about the D key for descriptions.
void SMALL_DOOR_F(void) {
    if (autosolve_visual_puzzle()) {
        ask_which_peg();
    } else if (v6_pictures_unseen()) {
        transcribe_and_print_string("[Note that you can get a description of the weights arrangement by pressing the D button during play.]");
    }
}

// Z-machine entry point: called each turn during Tower of Bozbar play.
// If the tower was auto-solved, clears the result variables so the game
// recognizes the win.
void TOWER_MODE(void) {
    if (tower_solved) {
        store_variable(5, 0);
        store_variable(6, 0);
        tower_solved = false;
    }
}


// Draws all weights on a single peg. Iterates through the peg's table
// entries (up to 6 weights), calling SET-B-PIC to get the image for each
// weight and drawing it at the appropriate x,y position.
static void draw_peg(uint16_t table, int offset) {
    uint16_t x = user_word(zt.B_X_TBL + offset * 2);
    for (int i = 0; i < kTowerWeights; i++) {
        uint16_t wgt = user_word(table + i * 2);
        if (wgt == 0)
            return;
        uint16_t pic = call_routine(zr.SET_B_PIC, {wgt}); //  <SET-B-PIC WGT>
        draw_to_pixmap_unscaled(pic, x, user_word(zt.B_Y_TBL + i * 2));
    }
}

// Whether the last move was an undo (controls whether the Undo button is dimmed)
static bool undoing = true;

// Redraws the entire Tower of Bozbar display: border, all weights on all
// 3 pegs, Undo and Exit buttons, and positions the text window below.
// The Undo button is dimmed if the last action was already an undo or
// if the puzzle is in its winning state.
static void draw_tower_of_bozbar(void) {
    z0_hide_right_status();

    if (V6_STATUS_WINDOW.id != nullptr) {
        v6_define_window(&V6_STATUS_WINDOW, 0, 0, 0, 0);
    }

    // Six location pictures give the six heights a weight can sit at, from
    // the bottom of a peg up. The first three also give the x of the left,
    // centre and right peg.
    static const int position_pics[kTowerWeights] = {
        B_1_L_PIC_LOC, B_2_C_PIC_LOC, B_3_R_PIC_LOC,
        B_4_PIC_LOC, B_5_PIC_LOC, B_6_PIC_LOC
    };
    for (int i = 0; i < kTowerWeights; i++) {
        int width, height;
        get_image_size(position_pics[i], &width, &height);
        store_word(zt.B_Y_TBL + (kTowerWeights - 1 - i) * 2, height);
        if (i < 3)
            store_word(zt.B_X_TBL + i * 2, width);
    }

    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(zorkzero_tower_of_bozbar_border, 0, 0);
    draw_peg(zt.LEFT_PEG_TABLE, 0);
    draw_peg(zt.CENTER_PEG_TABLE, 1);
    draw_peg(zt.RIGHT_PEG_TABLE, 2);

    // The boxes are there to be clicked on. r242 has no mouse support in the
    // tower (no B-MOUSE-PEG-PICK) and never draws them.
    if (zr.B_MOUSE_PEG_PICK != 0) {
        bool can_undo = !undoing && !call_routine(zr.TOWER_WIN_CHECK);
        draw_box(can_undo ? UNDO_BOX : DIM_UNDO_BOX, TOWER_UNDO_BOX_LOC);
        draw_box(EXIT_BOX, TOWER_EXIT_BOX_LOC);
    }
    flush_image_buffer();
    adjust_text_window_by_split(zorkzero_tower_of_bozbar_split);
    ADJUST_TEXT_WINDOW(zorkzero_tower_of_bozbar_bottom);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

// Z-machine entry point: redraws the tower display. Variable 1 indicates
// whether the last action was an undo (1) or a normal move (0).
void DRAW_TOWER(void) {
    undoing = (variable(1) == 1);
    draw_tower_of_bozbar();
}

// Hit-tests a mouse click against the 3 peg columns. Returns 'L', 'C',
// or 'R' for the clicked peg, or ZSCII_CLICK_DOUBLE if none was hit.
static uint8_t pick_peg(void) {
    int weight_width, left, top, peg_height;
    get_image_size(B_1_L_PIC_LOC, &left, &top);
    get_image_size(B_1_WEIGHT, &weight_width, nullptr);
    get_image_size(B_6_PIC_LOC, nullptr, &peg_height);
    if (within(left, top, weight_width, peg_height)) {
        return 'L';
    }
    get_image_size(B_2_C_PIC_LOC, &left, nullptr);
    if (within(left, top, weight_width, peg_height)) {
        return 'C';
    }
    get_image_size(B_3_R_PIC_LOC, &left, nullptr);
    if (within(left, top, weight_width, peg_height)) {
        return 'R';
    }
    win_beep(1);
    return ZSCII_CLICK_DOUBLE;
}

// Z-machine entry point: processes a mouse click for peg selection.
void B_MOUSE_PEG_PICK(void) {
    store_variable(2, pick_peg());
}

// Hit-tests a mouse click for weight selection. Checks Undo button ('U'),
// Exit button ('X'), then iterates through all weight positions on all 3
// pegs. Returns the weight number as ASCII ('1'-'6'), a button key, or
// ZSCII_CLICK_SINGLE if nothing valid was clicked.
static uint8_t pick_weight(void) {
    int left, top, weight_width, weight_height, num;
    if (box_clicked(UNDO_BOX, TOWER_UNDO_BOX_LOC))
        return 'U';
    if (box_clicked(EXIT_BOX, TOWER_EXIT_BOX_LOC))
        return 'X';
    get_image_size(B_1_WEIGHT, &weight_width, &weight_height);
    weight_height++;
    uint16_t tbl = zt.LEFT_PEG_TABLE;
    for (int cnt_x = 0; cnt_x < 3; cnt_x++) {
        for (int cnt_y = 0; cnt_y < kTowerWeights; cnt_y++) {
            left = user_word(zt.B_X_TBL + cnt_x * 2);
            top = user_word(zt.B_Y_TBL + cnt_y * 2);
            if (within(left, top, weight_width, weight_height)) {
                num = user_word(tbl + cnt_y * 2);
                if (num != 0) {
                    num = internal_get_prop(num, 0x29);
                    if (num >= 1 && num <= 6)
                        return num + 48;
                }
                win_beep(1);
                return ZSCII_CLICK_SINGLE;
            }
        }
        if (cnt_x == 0) {
            tbl = zt.CENTER_PEG_TABLE;
        } else {
            tbl = zt.RIGHT_PEG_TABLE;
        }
    }
    win_beep(1);
    return ZSCII_CLICK_SINGLE;
}

// Z-machine entry point: processes a mouse click for weight selection.
void B_MOUSE_WEIGHT_PICK(void) {
    uint16_t result = pick_weight();
    store_variable(2, result);
}

#pragma mark - Resize and Autorestore

// Redraw the active mini-game (Tower of Bozbar, Peggleboz, Snarfem, or
// Double Fanucci) for the current `CURRENT_SPLIT`. Returns true if a
// mini-game was matched and redrawn; false otherwise so the caller can
// fall through to the normal-mode path.
bool redraw_minigame_on_resize(uint16_t current_split) {
    if (current_split == zorkzero_tower_of_bozbar_split) {
        draw_tower_of_bozbar();
        flush_image_buffer();
        return true;
    }
    if (current_split == PBOZ_SPLIT) {
        draw_peggles();
        if (selected_peg_pos != 0) {
            uint16_t CY = user_word(zt.BOARD_TABLE + selected_peg_pos * 2);
            uint16_t CX = user_word(zt.BOARD_TABLE + (selected_peg_pos + 1) * 2);
            update_blink_coordinates(CX, CY);
            // Without a BLINK-TBL (before r366) the highlight is blinked by a
            // timed INPUT in BLINK. Re-arm the timer that z0_update_on_resize()
            // cancelled, or the peg stops blinking (as for the map marker there).
            if (zg.BLINK_TBL == 0)
                glk_request_timer_events(1);
        } else {
            flush_image_buffer();
        }
        return true;
    }
    if (current_split == SN_SPLIT) {
        draw_snarfem();
        return true;
    }
    if (current_split == F_SPLIT) {
        redraw_fanucci();
        return true;
    }
    return false;
}

// The pointer to the Double Fanucci command grid is not part of the autosave,
// but the window itself is. If we were restored into a game, find it again,
// or the plays can be neither highlighted nor clicked.
void restore_fanucci_command_grid(void) {
    if (fanucci_command_grid != nullptr || get_global(zg.CURRENT_SPLIT) != F_SPLIT)
        return;

    fanucci_command_grid = v6_grid_window_with_rock(FANUCCI_COMMAND_GRID_ROCK);
    if (fanucci_command_grid == nullptr)
        return;

    // As decided in redraw_fanucci(): the wide grid is 64 characters across
    glui32 width;
    glk_window_get_size(fanucci_command_grid, &width, nullptr);
    fanucci_window_is_narrow = (width < 64 || options.int_number == INTERP_APPLE_IIE);
    glk_request_mouse_event(fanucci_command_grid);
}

// The selected peg of a Peggleboz game is not part of the autosave. If we were
// restored into a game with a peg blinking, work out which one it is from
// the coordinates the game's BLINK routine is using.
void restore_selected_peg(void) {
    if ((screenmode != MODE_NORMAL && screenmode != MODE_Z0_GAME)
        || get_global(zg.CURRENT_SPLIT) != PBOZ_SPLIT || zt.BOARD_TABLE == 0)
        return;

    uint16_t x, y;
    if (zg.BLINK_TBL != 0) {
        y = user_word(get_global(zg.BLINK_TBL) + 3 * 2);
        x = user_word(get_global(zg.BLINK_TBL) + 4 * 2);
    } else if (current_frame_nlocals() >= 4 && variable(1) == UNHL_PEG && variable(2) == HL_PEG) {
        // The revisions without a BLINK-TBL (and global 0 is not a table) keep
        // the coordinates in the locals of BLINK only: PIC1, PIC2, Y and X.
        // With a peg selected, that is the routine we were restored into.
        y = variable(3);
        x = variable(4);
    } else {
        return;
    }
    if (x == 0 && y == 0)
        return;

    for (int i = 2; i < kPegBoardTableSize; i += 2) {
        if (user_word(zt.BOARD_TABLE + i * 2) == y
            && user_word(zt.BOARD_TABLE + (i + 1) * 2) == x) {
            selected_peg_pos = i;
            update_blink_coordinates(x, y);
            return;
        }
    }
}

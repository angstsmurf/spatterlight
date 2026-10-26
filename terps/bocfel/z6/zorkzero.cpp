// zorkzero.cpp
//
// Game-specific UI code for Zork Zero (Infocom, 1988).
//
// Zork Zero is a Z-machine version 6 game with an elaborate graphical UI
// featuring multiple mini-games and special display modes:
//
// - Compass rose: An interactive compass in the status bar showing available
//   exits, with highlighted/unhighlighted states for each direction.
// - Double Fanucci: A full card game with graphical cards, score windows,
//   and a command grid for selecting plays via keyboard or mouse.
// - Peggleboz: A peg-jumping puzzle game with clickable pegs and UI buttons
//   (restart, show moves, exit).
// - Snarfem (Nim): A mathematical strategy game with numbered piles and
//   flower indicators showing the optimal move.
// - Tower of Bozbar: A Tower of Hanoi variant with 6 weights on 3 pegs,
//   clickable weights and pegs, and an undo button.
// - Encyclopedia Frobozzica: Fullscreen encyclopedia entries with images
//   and a text overlay window.
// - Rebus: A number of fullscreen images depicting a rebus decreasingly covered
//   by animal-shaped jigsaw pieces.
// - Map: A fullscreen map with a blinking player-location indicator.
//
// The game supports multiple graphics platforms (Amiga, Mac B/W, Mac color,
// Apple II, CGA, EGA, VGA, Blorb) with platform-specific border styles,
// color schemes, and layout adjustments.
//
// Border types vary by location: castle, outside, underground, and hints,
// each with optional side pillars for non-Amiga/Mac platforms.
//
// The status line is split into left (room name + moves) and right
// (region + score) text grid windows positioned over the border graphics.

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
#include "shogun.hpp"

#include "zorkzero.hpp"
#include "zorkzero_internal.hpp"

// Window 1 (S-WINDOW) is used as the left status text grid
#define z0_left_status_window V6_STATUS_WINDOW.id

// Right status window: displays region name and score, positioned at the
// right edge of the border. Not part of the standard window array.
winid_t z0_right_status_window = nullptr;
// Width of the right status window in characters
static glui32 z0_right_status_width;

// Set once we've nudged the player toward the built-in InvisiClues on their
// first rebus encounter, so we don't keep reminding them.
static bool shown_rebus_hint_message = false;
static bool should_show_rebus_hint_message = false;
static bool should_show_map_note = false;

// Image scale (x and y) in effect when the map was last composited. The
// pre-release revisions (r242/r296/r66, r366 demo) don't recomposite the map on
// resize, so the live-redrawn
// you-are-here marker is drawn at this scale to stay aligned with the rest of
// the (GUI-rescaled) map. 0 means "no map drawn yet".
static float map_entry_imagescalex = 0;
static float map_entry_imagescaley = 0;

// Tracks how many times z0_update_colors has been called (used to detect
// first call for initial color setup)
static int number_of_update_color_calls = 0;

// Lookup tables for Z-machine globals, routines, tables, objects, and
// properties specific to Zork Zero. Populated during game initialization
// by scanning the game file for known identifiers.
ZorkGlobals zg;
ZorkRoutines zr;
ZorkTables zt;
ZorkObjects zo;
ZorkProperties zp;

#pragma mark - Image Classification

// Tower of Bozbar weight images (pictures 42-48)
static bool is_zorkzero_tower_image(int pic) {
    return (pic >= 42 && pic <= 48);
}

// Rebus puzzle images (pictures 34-40)
static bool is_zorkzero_rebus_image(int pic) {
    return (pic >= 34 && pic <= 40);
}

// Encyclopedia Frobozzica illustration images (pictures 26-33)
static bool is_zorkzero_encyclopedia_image(int pic) {
    return (pic >= 26 && pic <= 33);
}

// Peggleboz peg images (pictures 50-72: highlighted, unhighlighted, empty)
static bool is_zorkzero_peggleboz_image(int pic) {
    return (pic >= 50 && pic <= 72);
}

// Pictures that take over the entire foreground graphics window: the title
// screen, the encyclopedia border frame, the map border, and rebus puzzles.
// Rebus images only qualify when we're transitioning into the slideshow
// from normal gameplay — once already in slideshow, they're a no-op
// redraw, not a mode change.
static bool is_zorkzero_fullscreen_picture(int pic) {
    return pic == zorkzero_title_image
        || pic == zorkzero_encyclopedia_border
        || pic == zorkzero_map_border
        || (is_zorkzero_rebus_image(pic) && screenmode == MODE_NORMAL);
}

// Checks if the given picture is a Peggleboz UI button (restart, show moves,
// exit). Only returns true when the current game mode is Peggleboz.
static bool is_zorkzero_peggleboz_box_image(int pic) {
    uint16_t CURRENT_SPLIT = get_global(zg.CURRENT_SPLIT);

    if (CURRENT_SPLIT != PBOZ_SPLIT)
        return false;

    switch(pic) {
        case SHOW_MOVES_BOX:
        case RESTART_BOX:
        case DIM_RESTART_BOX:
        case DIM_SHOW_MOVES_BOX:
        case EXIT_BOX:
            return true;
        default:
            return false;
    }
}

#pragma mark - Status Line Utilities

// Hides the right status window by clearing it and shrinking it to zero size.
// Called when entering fullscreen modes (map, mini-games) that don't use
// the split status bar.
void z0_hide_right_status(void) {
    if (z0_right_status_window == nullptr) {
        fprintf(stderr, "z0_hide_right_status: no right status window\n");
        return; // nothing to hide, and dereferencing below would crash
    }
    glk_window_clear(z0_right_status_window);
    win_sizewin(z0_right_status_window->peer, 0, 0, 0, 0);
}

// Clears all display elements: image buffer, margin images, right status,
// text buffer, status grid, and background graphics window.
void z0_erase_screen(void) {
    clear_image_buffer();
    clear_margin_image_list();
    z0_hide_right_status();

    if (V6_TEXT_BUFFER_WINDOW.id != nullptr)
        glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
    if (V6_STATUS_WINDOW.id != nullptr)
        glk_window_clear(V6_STATUS_WINDOW.id);
    glk_window_clear(graphics_bg_glk);
}

#pragma mark - Window Layout

// Whether the decorative border is shown: the game's own BORDER-ON global
// (toggled by the MODE command), overridden while graphics are switched off
// in the settings. The global itself is left alone -- it is saved game state,
// and the border should come back when graphics are switched on again.
bool z0_border_on(void) {
    return get_global(zg.BORDER_ON) == 1 && !v6_graphics_off();
}

// Z-machine entry point: called after SPLIT-BY-PICTURE.
// In no-graphics mode, ensures the text window spans the full screen width.
void after_SPLIT_BY_PICTURE(void) {
    if (screenmode == MODE_NO_GRAPHICS) {
        V6_TEXT_BUFFER_WINDOW.x_size = gscreenw;
        v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
    }
}

// Z-machine entry point: toggles between graphics and no-graphics display modes.
void V_MODE(void) {
    if (screenmode != MODE_NO_GRAPHICS) {
        screenmode = MODE_NO_GRAPHICS;
        glk_window_clear(current_graphics_buf_win);
    } else {
        screenmode = MODE_NORMAL;
    }
}

// Deletes the encyclopedia caption window and restores the normal
// text-buffer colour hints that adjust_encyclopedia_text_window overrode for
// it, so later text-buffer windows aren't created with the encyclopedia's
// colours. (z0_update_colors sets these same values during normal play.)
void z0_close_encyclopedia_window(void) {
    v6_delete_win(&Z0_ENCYCLOPEDIA_WINDOW);
    glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_TextColor, user_selected_foreground);
    glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_BackColor, user_selected_background);
}

// Sets the text colour of the status line grids for printing on top of the
// border picture: black in the monochrome formats, otherwise the theme's
// text colour, on a transparent background.
void z0_set_bordered_status_colors(void) {
    glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_BackColor);

    if (graphics_type == kGraphicsTypeCGA || graphics_type == kGraphicsTypeMacBW) {
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, monochrome_black);
    } else {
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
    }
}

// Z-machine entry point: handles screen refresh. Cleans up window 3
// (encyclopedia caption) when returning from encyclopedia text view.
void V_REFRESH(void) {
    uint16_t CURRENT_SPLIT = get_global(zg.CURRENT_SPLIT);
    if (CURRENT_SPLIT == TEXT_WINDOW_PIC_LOC) {
        z0_close_encyclopedia_window();
    }
}

// Output callback used by the CENTER-n reimplementations: routes each
// character straight to the current Glk window under whatever Glk style
// we have set, bypassing the Z-machine style machinery (which would
// otherwise reset the style on every character).
static void center_put_char(uint8_t c) {
    glk_put_char(c);
}

// Shared body for the reimplemented CENTER-1/2/3 credit routines.
//
// The original game centers each line by measuring its width with
// output-stream 3 and then issuing @set_cursor (CURSET) to a pixel X in
// the main window (see CENTER-LINE/JUSTIFIED-LINE in the ZIL). Glk
// text-buffer windows ignore cursor positioning, so that approach leaves
// the text left-aligned. Instead we print each line through style_User1,
// which is hinted just_Centered during V6 init (screen.cpp), letting Glk
// do the centering. The multi-column variants (CENTER-2/3) put their
// strings on a single centered line separated by a few spaces, which is
// a reasonable rendering when per-column cursor positioning isn't
// available.
//
// These reimplementations are the sole output path for the credit lines,
// so each is responsible for the trailing newline the original printed.
//
// In the original, section headlines and the title are bold while the
// names are roman. Both centered styles are set up during V6 init:
// style_User1 is bold+fixed+centered and style_Note is roman+fixed+
// centered, so we pick between them with the `bold` flag.
static void center_credit_line(bool bold, std::initializer_list<uint16_t> strings) {
    set_current_window(&V6_TEXT_BUFFER_WINDOW);
    glk_set_style(bold ? style_User1 : style_Note);
    bool first = true;
    for (uint16_t str : strings) {
        if (str == 0)
            continue;
        if (!first) {
            glk_put_char(' ');
            glk_put_char(' ');
            glk_put_char(' ');
        }
        print_handler(unpack_string(str), center_put_char);
        first = false;
    }
    glk_put_char('\n');
    glk_set_style(style_Normal);
}

// Z-machine entry points replacing CENTER-1/2/3. These hooks fire at the
// routine's entry (before its first instruction executes), so the
// string arguments are already in the routine's locals. We print the
// centered version ourselves and then do_return() out of the routine so
// the original left-aligning body (which centers via @set_cursor, a
// no-op in a Glk text-buffer window) never runs. do_return is used
// rather than stubbing the routine's first byte with rtrue because the
// CENTER-2/3 anchor patterns are short and could match elsewhere;
// overwriting that byte would corrupt unrelated code.
void CENTER_1(void) {
    // CENTER-1(STR, BOLD): local 1 is the string, local 2 is the bold
    // flag (T for headlines/title, absent -> 0 for single names).
    center_credit_line(variable(2) != 0, { variable(1) });
    do_return(1);
}

void CENTER_2(void) {
    // CENTER-2/3 are always name rows, so they're roman (not bold).
    center_credit_line(false, { variable(1), variable(2) });
    do_return(1);
}

void CENTER_3(void) {
    center_credit_line(false, { variable(1), variable(2), variable(3) });
    do_return(1);
}


// Splits the screen using the dimensions of picture `id` to determine the
// border width and text area origin. Sets up the text buffer window below
// the border and the status window spanning the top. When borders are off,
// the text window uses full screen width.
static void SPLIT_BY_PICTURE(uint16_t id, bool clear_screen) {
    set_global(zg.CURRENT_SPLIT, id); // <SETG CURRENT-SPLIT .ID>
    v6_match_status_window_to_graphics_setting(&V6_STATUS_WINDOW);
    if (clear_screen) {
        glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
        glk_window_clear(V6_STATUS_WINDOW.id);
    }
    int width, height;
    get_image_size(id, &width, &height);

    uint16_t x, x_size;
    uint16_t y = (height + 1) * imagescaley;

    if (!z0_border_on() && id == TEXT_WINDOW_PIC_LOC) {
        x = 1;
        x_size = gscreenw;
        // The picture's height is that of the top border; without it
        // the text starts right below the two status lines.
        y = 2 * (gcellh + ggridmarginy) + 1;
    } else {
        x = width * imagescalex;
        x_size = gscreenw - x * 2;
    }

    v6_define_window(&V6_TEXT_BUFFER_WINDOW, x, y, x_size, gscreenh - y);
    v6_define_window(&V6_STATUS_WINDOW, 1, 1, gscreenw, y);
}

// Adjusts the text buffer window height to extend from its current y_origin
// to the bottom of the screen (if id == 0) or to the top of the bottom
// border strip identified by picture `id`.
void ADJUST_TEXT_WINDOW(int id) {
    int height = 0;
    int hw_screenheight = gscreenh;
    if (id != 0) {
        get_image_size(id, nullptr, &height);
        get_image_size(zorkzero_map_border, nullptr, &hw_screenheight);
        hw_screenheight *= imagescaley;
    }

    V6_TEXT_BUFFER_WINDOW.y_size = hw_screenheight - V6_TEXT_BUFFER_WINDOW.y_origin - height * imagescaley;
    v6_sizewin(&V6_TEXT_BUFFER_WINDOW);
}

#pragma mark - Border Display

// Draws a decorative border around the screen. The border type (castle,
// outside, underground) determines which top graphic and side pillars
// to use. Side pillars (BL/BR) are separate images on non-Amiga/Mac platforms
// and are drawn below the top border strip.
// Hint border drawing is skipped here and later handled by DO_HINTS().
void DISPLAY_BORDER(BorderType border) {
    if (screenmode == MODE_HINTS)
        return;

    // If the foreground graphics window is currently in use (slideshow,
    // map, etc.), hide it and cancel any pending input on it before
    // switching to the background graphics window where borders live.
    if (current_graphics_buf_win == graphics_fg_glk) {
        win_sizewin(graphics_fg_glk->peer, 0, 0, 0, 0);
        glk_cancel_char_event(graphics_fg_glk);
        current_graphics_buf_win = graphics_bg_glk;

        // Drawing a normal room border means we've left the fullscreen
        // map/slideshow. screenmode is C++-only state that those paths set
        // but never clear, so reconcile it here too; otherwise a later
        // resize (or restore) would take the stale fullscreen redraw path.
        // (z0_update_after_restore has the same guard for the restore case.)
        if (screenmode == MODE_MAP || screenmode == MODE_SLIDESHOW)
            screenmode = MODE_NORMAL;
    }
    current_graphics_buf_win = graphics_bg_glk;
    glk_request_mouse_event(current_graphics_buf_win);

    // Start each border draw from a clean pixmap so leftover pixels from
    // an earlier border or scene don't bleed through.
    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);

    // Step 1: stamp the main border image (the decorative top strip) at
    // the very top of the pixmap, and record its native dimensions so we
    // know where the side pillars start.
    draw_to_pixmap_unscaled(border, 0, 0);
    int X, Y;
    get_image_size(border, &X, &Y);

    // Step 2: pick the matching left/right pillar image numbers for this
    // border type. On Amiga and Mac B/W these images don't exist (the
    // top strip already includes the sides), and find_image() below will
    // return null for them.
    int BL = -1, BR = -1;
    switch(border) {
        case OUTSIDE_BORDER:
            BL = OUTSIDE_BORDER_L;
            BR = OUTSIDE_BORDER_R;
            break;
        case CASTLE_BORDER:
            BL = CASTLE_BORDER_L;
            BR = CASTLE_BORDER_R;
            break;
        case UNDERGROUND_BORDER:
            BL = UNDERGROUND_BORDER_L;
            BR = UNDERGROUND_BORDER_R;
            break;
        case HINT_BORDER:
            // Should never happen
            break;
    }

    // Draw the left pillar flush against the left edge, immediately below
    // the top strip.
    if (find_image(BL)) {
        draw_to_pixmap_unscaled(BL, 0, Y);
    }

    // Draw the right pillar flush against the right edge. We need BR's
    // width to compute its x-offset; height is implicit.
    if (find_image(BR)) {
        int width;
        get_image_size(BR, &width, nullptr);
        draw_to_pixmap_unscaled(BR, X - width, Y);
    }

    // Step 3: per-border vertical extension to fill taller modern screens.
    // The original DOS/Amiga/Mac art was sized for a 200-line display, so
    // anything taller than that needs the pillars tiled downward. Each
    // border type has its own tiling logic; the magic numbers below are
    // per-art-asset top_cut/foot/height/pillar/overlap measurements that
    // match the layout of each platform's source artwork.
    switch(border) {
        case CASTLE_BORDER:
            if (graphics_type == kGraphicsTypeVGA || graphics_type == kGraphicsTypeAmiga || graphics_type == kGraphicsTypeBlorb) {
                extend_pillars(43, 13, 200, 142, 0, false, false);
            } else if (graphics_type == kGraphicsTypeMacBW) {
                // Mac B/W castle has its own bespoke routine because the
                // pillar art is split into upper/ring/lower/foot sections.
                extend_mac_bw_castle_pillars();
            } else if (graphics_type == kGraphicsTypeEGA) {
                extend_pillars(45, 13, 203, 144, 9, false, false);
            } else if (graphics_type == kGraphicsTypeCGA) {
                // CGA uses flipped alternating tiles (the `true` argument).
                extend_pillars(52, 25, 205, 133, 11, true, false);
            }
            break;
        case UNDERGROUND_BORDER:
            // The underground border has stone-block sides that alternate
            // left/right tiles; Mac B/W uses a taller variant of the same art.
            if (graphics_type == kGraphicsTypeMacBW) {
                extend_underground_pillars(107, 80, 300, 115, 56, 43);
            } else {
                extend_underground_pillars(73, 54, 200, 74, 38, 37);
            }
            break;
        case OUTSIDE_BORDER:
            // The jungle border just repeats a single strip with no foot;
            // Mac B/W again uses a taller version of the same art.
            if (graphics_type != kGraphicsTypeMacBW) {
                extend_jungle_pillars(67, 210, 143, 59);
            } else {
                extend_jungle_pillars(53, 300, 247, 33);
            }
            break;
        case HINT_BORDER:
            // Hint border drawing is handled by DO_HINTS()
            break;
    }
}

// Called when line input is requested at the command prompt. The on-screen map
// is a full-screen modal view driven entirely by @read_char; its blinking
// marker loop reads a char with mouse events armed on the foreground graphics
// window. After an autorestore onto the map, resuming mid-loop and leaving it
// (e.g. clicking to move out) can leave a char-input request stranded on a
// window whose curwin moved underneath it (the blink-timeout routine flips
// SET_WINDOW), so it is never cancelled by the read's own cleanup. glkimp then
// routes the next keypress to that still-armed char window instead of the
// prompt's line field, which arrives as a spurious evtype_CharInput and -- once
// that request is also gone -- leaves the line read with no window holding
// input focus, so the prompt accepts nothing from the keyboard.
//
// Clear any lingering char-input request on every window the map could have
// armed before the line read is established, so the line field is the only
// thing holding input and keystrokes reach it. Cancelling char input on a
// window that has none is a harmless no-op. Also tear down the foreground
// graphics window if it is still covering the screen (screenmode left at
// MODE_MAP), mirroring DISPLAY_BORDER's exit.
void z0_leave_stale_map_for_line_input(void) {
    if (V6_TEXT_BUFFER_WINDOW.id != nullptr)
        glk_cancel_char_event(V6_TEXT_BUFFER_WINDOW.id);
    if (graphics_fg_glk != nullptr)
        glk_cancel_char_event(graphics_fg_glk);
    if (graphics_bg_glk != nullptr)
        glk_cancel_char_event(graphics_bg_glk);
    for (auto &window : windows) {
        if (window.id != nullptr)
            glk_cancel_char_event(window.id);
    }

    if (screenmode == MODE_MAP) {
        if (current_graphics_buf_win == graphics_fg_glk && graphics_fg_glk != nullptr) {
            win_sizewin(graphics_fg_glk->peer, 0, 0, 0, 0);
            glk_cancel_mouse_event(graphics_fg_glk);
            current_graphics_buf_win = graphics_bg_glk;
        }
        screenmode = MODE_NORMAL;
    }
}

// Prints the notes about a full-screen picture that were held back until
// the text window was showing again.
static void z0_show_pending_notes(void) {
    if (should_show_rebus_hint_message) {
        glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
        if (v6_graphics_off())
            transcribe_and_print_string("\n[The rebus is a picture, and graphics are turned off. To learn what it says, consult the built-in InvisiClues by typing HINT, and then navigating to GREAT HALL AREA, reading the rebus.]\n");
        else
            transcribe_and_print_string("\n[If the rebus images are causing you trouble, don't hesitate to consult the built-in InvisiClues by typing HINT, and then navigating to GREAT HALL AREA, reading the rebus.]\n");
        should_show_rebus_hint_message = false;
        shown_rebus_hint_message = true;
        // Requesting and cancelling a character event makes the front end
        // show the text now rather than at the next input request
        glk_request_char_event(V6_TEXT_BUFFER_WINDOW.id);
        glk_cancel_char_event(V6_TEXT_BUFFER_WINDOW.id);
    }
    if (should_show_map_note) {
        glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
        transcribe_and_print_string("\n[The map is a picture, and graphics are turned off.]\n");
        should_show_map_note = false;
    }
}

// Initializes the status line layout. Splits the screen by the current split
// picture, sets up border display (if borders are on), positions the status
// windows, and configures text styles for the status area.
// Returns true if the current split is for a mini-game (Fanucci/Peggleboz)
// which needs special text window adjustment.
static bool z0_init_status_line(bool DONT_CLEAR) {
    bool border_on = z0_border_on();
    if (!DONT_CLEAR) {
        // <CLEAR -1>
        if (V6_TEXT_BUFFER_WINDOW.id != nullptr) {
            glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
            z0_show_pending_notes();
        }
        clear_margin_image_list();
    }
    uint16_t CURRENT_SPLIT = get_global(zg.CURRENT_SPLIT);
    SPLIT_BY_PICTURE(CURRENT_SPLIT, false);

    if (CURRENT_SPLIT == TEXT_WINDOW_PIC_LOC) {
        ADJUST_TEXT_WINDOW(0);
    } else if (CURRENT_SPLIT == F_SPLIT) { // (<EQUAL? ,CURRENT-SPLIT ,F-SPLIT>
        ADJUST_TEXT_WINDOW(F_BOTTOM);
        return true;
    } else {
        ADJUST_TEXT_WINDOW(PBOZ_BOTTOM);
        return true;
    }

    z0_hide_right_status();

    if (border_on) { // <COND (,BORDER-ON

        z0_set_bordered_status_colors();

        set_current_window(&V6_GRAPHICS_BG); // <SCREEN ,S-FULL>
        if (zg.CURRENT_BORDER != 0) {
            set_global(zg.CURRENT_BORDER, call_routine(zr.SET_BORDER));
            DISPLAY_BORDER((BorderType)get_global(zg.CURRENT_BORDER));
        } else if (zr.SET_BORDER != 0) {
            DISPLAY_BORDER((BorderType)call_routine(zr.SET_BORDER));
        }
        // If neither the CURRENT-BORDER global nor a detected SET-BORDER routine
        // is available (the release-tuned patterns don't match the pre-release
        // r296/r66 layouts), skip the border draw rather than calling
        // pack_routine(0) into a garbage routine.
        // The game records which compass directions are currently drawn as
        // flags on ROOMS, flag 64 - <direction property>. Forget them all,
        // so that the whole rose is drawn again on the new border. (The
        // game's own INIT-STATUS-LINE does this from r366 on; r343 has the
        // flags but not yet the loop, and needs it just the same.)
        if (zo.ROOMS != 0) {
            for (int direction = P_NORTH; direction >= LOW_DIRECTION; direction--) {
                internal_clear_attr(zo.ROOMS, 64 - direction);
            }
        }

    } else {
        set_current_window(&V6_STATUS_WINDOW); // <SCREEN ,S-WINDOW>
        // Plain text: with no border behind it, the window shows the
        // ordinary background.
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
    }

    win_refresh(V6_STATUS_WINDOW.id->peer, 0, 0);
    win_refresh(z0_right_status_window->peer, 0, 0);

    set_current_window(&V6_TEXT_BUFFER_WINDOW); // <SCREEN ,S-TEXT>

    return false;
}

// Z-machine entry point: wraps z0_init_status_line with the DONT-CLEAR
// parameter from the Z-machine call stack.
void INIT_STATUS_LINE(void) {
    z0_init_status_line(variable(1));
}

// Prints the current room name in the left status window. Handles special
// cases: narrow displays (Apple IIe/DOS) use abbreviated property descriptions,
// and "Great Underground" rooms use a shortened prefix to save space.
static void draw_new_here(void) {
    V6_STATUS_WINDOW.x = 1;
    V6_STATUS_WINDOW.y = 1;

    int16_t HERE = get_global(zg.HERE);
    bool NARROW = options.int_number == INTERP_APPLE_IIE || options.int_number == INTERP_MSDOS;

    if (NARROW || HERE == zo.PHIL_HALL) {
        int16_t X = internal_get_prop(HERE, zp.P_APPLE_DESC);
        if (X) {
            print_handler(unpack_string(X), nullptr);
            return;
        }
    }

    if (HERE == zo.G_U_MOUNTAIN || HERE == zo.G_U_SAVANNAH || HERE == zo.G_U_HIGHWAY) {
        glk_put_string(const_cast<char*>("Great Undergd. "));
        //  <TELL "Great Undergd. ">
        if (HERE == zo.G_U_MOUNTAIN) {
            glk_put_string(const_cast<char*>("Mountain"));
            // <TELL "Mountain">
        } else if (HERE == zo.G_U_SAVANNAH) {
            glk_put_string(const_cast<char*>("Savannah"));
            // <TELL "Savannah">
        } else {
            glk_put_string(const_cast<char*>("Highway"));
            // <TELL "Highway">
        }
    } else {
        print_object(HERE, nullptr);
    }
}

// Prints the current score right-justified in the status window.
static void draw_new_score(void) {
    int16_t SCORE = get_global(zg.SCORE);
    print_right_justified_number(SCORE);
}

// Resizes and repositions the left and right status text grid windows
// to align with the current border graphics. The left window shows room
// name and move count; the right window shows region and score.
// When borders are off, uses a single full-width status bar instead.
static void z0_resize_status_windows(void) {

    bool BORDER_ON = z0_border_on();

    v6_match_status_window_to_graphics_setting(&V6_STATUS_WINDOW);

    if (z0_left_status_window == nullptr) {
        z0_left_status_window = gli_new_window(wintype_TextGrid, 0);
        if (BORDER_ON)
            win_maketransparent(z0_left_status_window->peer);
    }

    int x, y, width, height;

    V6_STATUS_WINDOW.x = 1;
    V6_STATUS_WINDOW.y = 1;

    int hereoffx, hereoffy;
    get_image_size(HERE_LOC, &hereoffx, &hereoffy);

    height = 2 * (gcellh + ggridmarginy);

    int text_area_height = 9;

    if (graphics_type == kGraphicsTypeMacBW) {
        text_area_height = 18;
    } else if (graphics_type == kGraphicsTypeCGA || graphics_type == kGraphicsTypeEGA) {
        text_area_height = 10;
    }

    int text_area_y_offset = text_area_height * imagescaley - gcellh;

    x = (hereoffx - 1) * imagescalex - ggridmarginx + text_area_y_offset;
    y = (hereoffy - 1) * imagescaley - ggridmarginy + text_area_y_offset;

    if (!BORDER_ON) {
        z0_hide_right_status();

        v6_define_window(&V6_STATUS_WINDOW, 1, 1, gscreenw, height);
        glk_window_clear(z0_left_status_window);
        z0_right_status_width = (gscreenw - 2 * ggridmarginx) / gcellw;
        return;
    }

    uint16_t HERE = get_global(zg.HERE);
    int stringlength = count_characters_in_object(HERE);
    int width_in_chars = std::max(stringlength, 13); // 13 characters to make room for "MOVES:" plus 2 spaces and 10000 moves
    width = width_in_chars * gcellw + 2 * ggridmarginx;

    z0_left_status_window->bbox.x0 = x;
    win_sizewin(z0_left_status_window->peer, x, y, x + width, y + height);
    glk_window_clear(z0_left_status_window);

    int16_t REGION = internal_get_prop(HERE, zp.P_REGION);

    z0_right_status_width = std::max(count_characters_in_zstring(REGION), 10); // 10 characters to make room for "SCORE:" plus 1000 points

    width = z0_right_status_width * gcellw + 2 * ggridmarginx;

    x = gscreenw - x - width;
    win_sizewin(z0_right_status_window->peer, x, y, x + width, y + height);

    // We trick the text printing routine into
    // thinking that it is printing to the left
    // window when it prints to the right one,
    // so we have to pretend that the left one
    // is much wider than it really is
    z0_left_status_window->bbox.x1 = x + width;

    glk_window_clear(z0_right_status_window);
}

#pragma mark - Hint Border Display

// Side pillar images for hint screens
constexpr int Z0_HINT_BORDER_L = 503;
constexpr int Z0_HINT_BORDER_R = 504;

// Height of the Amiga/Mac hint border top section (used for pillar placement)
constexpr int Z0_HINT_TOP_HEIGHT = 33;

// Draws the hint border. Unlike DISPLAY_BORDER (which handles the
// 4 in-game border types), this also handles vertical border extension for
// tall screens, platform-specific pillar placement, and a covering rectangle
// at the top (The raw graphics have a top bar that won't fit all the header
// text, so the original interpreters cover it with a solid color rectangle.)
void z0_display_hint_border(void) {
    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);

    int width, height;
    get_image_size(HINT_BORDER, &width, &height);

    // draw_border_common draws the main border with the current palette
    // (it does not load the image's own palette), so load it here. Shogun
    // does the equivalent in shogun_display_border. Without this the hint
    // border is drawn with a stale palette in the per-image-palette
    // formats (Amiga and Mac color), giving the wrong colors.
    extract_palette_from_picnum(HINT_BORDER);

    int border_top = 0;
    int pillar_top = 0;
    if (graphics_type != kGraphicsTypeAmiga && graphics_type != kGraphicsTypeMacBW) {
        border_top = height;
        pillar_top = border_top;
    } else {
        pillar_top = Z0_HINT_TOP_HEIGHT;
    }

    int bottom_offset = (graphics_type == kGraphicsTypeCGA) ? 10 : 0;

    draw_border_common(HINT_BORDER, Z0_HINT_BORDER_L, Z0_HINT_BORDER_R,
                       height, border_top, pillar_top,
                       0,     // left_margin
                       bottom_offset,
                       BorderKind::Hint);
}


static void z0_refresh_sl_loc_tbl(void);

// Z-machine entry point: updates the full status line display.
// Resizes status windows, prints room name and move count in the left
// window, draws the compass rose (when borders are on), and prints
// region name and score in the right window.
void z0_UPDATE_STATUS_LINE(void) {
    bool BORDER_ON = z0_border_on();
    uint16_t HERE = get_global(zg.HERE);
    uint16_t REGION = internal_get_prop(HERE, zp.P_REGION);

    // if interpreter is IBM and color flag is unset
    // or border is off
    // set reverse video

    z0_resize_status_windows();

    set_current_window(&V6_STATUS_WINDOW);
    if (v6_graphics_off()) {
        // A plain reverse-video bar, as there is no border to print on.
        garglk_set_reversevideo(0);
        V6_STATUS_WINDOW.style.reset(STYLE_REVERSE);
        garglk_set_zcolors(user_selected_background, user_selected_foreground);
        win_setbgnd(z0_left_status_window->peer, user_selected_foreground);
        glk_window_clear(z0_left_status_window);
    }
    glk_window_move_cursor(z0_left_status_window, 0, 0);


    draw_new_here();

    glk_window_move_cursor(z0_left_status_window, 0, 1);
    glk_put_string(const_cast<char*>("Moves:  "));
    print_number(get_global(zg.MOVES));

    if (BORDER_ON) {
        // Keep SL-LOC-TBL's compass metrics in sync with the *current* image
        // scale before drawing (and therefore before the next click reads the
        // table for hit-testing). r383/r387 fill these only once in Main and
        // never re-derive them on a normal turn, so after the intro->game
        // transition -- or any scale change that didn't go through an arrange
        // event -- the table is left at a stale scale and the compass rose is
        // both drawn and hit-tested in the wrong place (the reported r383
        // west->south / dead-arrow bug). Refreshing here makes draw and
        // hit-test always agree at the live scale. Harmless on the later
        // releases, whose SETUP-SCREEN already refills the table.
        z0_refresh_sl_loc_tbl();
        if (zr.DRAW_NEW_COMP != 0) {
            call_routine(zr.DRAW_NEW_COMP);
        } else if (zr.DRAW_COMPASS_ROSE != 0 && zt.PICINF_TBL != 0) {
            // Legacy fallback for the early releases (r242/beta) that have
            // DRAW-COMPASS-ROSE + PICINF-TBL but no DRAW-NEW-COMP. Guard on
            // both being detected: on the middle releases (r296/r343/r366)
            // none of the three is found, and running this unguarded wrote
            // to PICINF_TBL==0 (corrupting the header at address 0) and
            // called pack_routine(0) eight times -- which blanked the whole
            // bordered status line (no pillars, no compass).
            int width, height;
            get_image_size(COMPASS_PIC_LOC, &width, &height);

            store_word(zt.PICINF_TBL, (height + 1) * imagescaley);
            store_word(zt.PICINF_TBL + 2, (width + 1)  * imagescalex);

            // Direction property, highlighted picture, unhighlighted picture
            static const uint16_t compass_directions[][3] = {
                { P_NORTH, N_HIGHLIGHTED,  N_UNHIGHLIGHTED },
                { P_NE,    NE_HIGHLIGHTED, NE_UNHIGHLIGHTED },
                { P_EAST,  E_HIGHLIGHTED,  E_UNHIGHLIGHTED },
                { P_SE,    SE_HIGHLIGHTED, SE_UNHIGHLIGHTED },
                { P_SOUTH, S_HIGHLIGHTED,  S_UNHIGHLIGHTED },
                { P_SW,    SW_HIGHLIGHTED, SW_UNHIGHLIGHTED },
                { P_WEST,  W_HIGHLIGHTED,  W_UNHIGHLIGHTED },
                { P_NW,    NW_HIGHLIGHTED, NW_UNHIGHLIGHTED },
            };
            for (const auto &dir : compass_directions) {
                call_routine(zr.DRAW_COMPASS_ROSE, {dir[0], dir[1], dir[2]});
            }
        }
        glk_set_window(z0_right_status_window);
    }

    glk_window_move_cursor(BORDER_ON ? z0_right_status_window : z0_left_status_window, z0_right_status_width - count_characters_in_zstring(REGION), 0);
    print_handler(unpack_string(REGION), nullptr);

    glk_window_move_cursor(BORDER_ON ? z0_right_status_window : z0_left_status_window, z0_right_status_width - 10, 1);
    glk_put_string(const_cast<char*>("Score:"));
    draw_new_score();

    set_current_window(&V6_TEXT_BUFFER_WINDOW);
}


#pragma mark - Update Colors

// Configures foreground and background colors based on the current interpreter
// type and graphics mode. Each platform has different default colors:
//   Amiga: black on grey    IBM EGA/VGA: black on white
//   IBM CGA: white on black   Mac: black on white   Apple II: white on black
// Also handles color availability constraints when switching platforms,
// and updates Glk style hints for all text window types.
void z0_update_colors(void) {
    // Every system has its own default colors:
    // Amiga: black text on grey
    // IBM: EGA & VGA: black text on white. CGA: white text on black (on a real CGA monitor, "white" may be green or orange)
    // Macintosh: Black text on white
    // Apple 2: White text on black

    // Count invocations so the first-call default-colour reset below fires
    // exactly once. (This increment used to be a side effect of a debug
    // fprintf that was removed; the == 1 branch is dead without it.)
    number_of_update_color_calls++;

    uint16_t default_fg = get_global(zg.DEFAULT_FG);
    uint16_t default_bg = get_global(zg.DEFAULT_BG);

    uint16_t current_fg = get_global(fg_global_idx);
    uint16_t current_bg = get_global(bg_global_idx);


    switch (options.int_number) {
        case INTERP_AMIGA:
            default_fg = BLACK_COLOUR;
            default_bg = LIGHTGREY_COLOUR;
            break;
        case INTERP_MSDOS:
            if (gli_z6_graphics == kGraphicsTypeCGA) {
                default_fg = WHITE_COLOUR;
                default_bg = BLACK_COLOUR;
            } else {
                default_fg = BLACK_COLOUR;
                default_bg = WHITE_COLOUR;
            }
            break;
        case INTERP_MACINTOSH:
            default_fg = BLACK_COLOUR;
            default_bg = WHITE_COLOUR;
            break;
        case INTERP_APPLE_IIE:
            if (gli_z6_graphics == kGraphicsTypeApple2) {
                default_fg = WHITE_COLOUR;
                default_bg = BLACK_COLOUR;
            } else {
                default_fg = BLACK_COLOUR;
                default_bg = WHITE_COLOUR;
            }
            break;
        default:
            fprintf(stderr, "ERROR! Illegal interpreter number!\n");
            break;
    }

    if (default_fg == 0)
        default_fg = 1;
    if (default_bg == 0)
        default_bg = 1;

    // Under Spatterlight the game's DEFAULT-FG/DEFAULT-BG globals are kept at
    // 1 ("default", i.e. the theme colours): the DEFAULT_COLORS entry point
    // rewrites them to 1 whenever the game calls DEFAULT-COLORS. Write 1 here
    // too, not the computed platform defaults: r383+ paint the bordered status
    // line with <COLOR ,DEFAULT-FG ,DEFAULT-BG> (CLEAR-BORDER,
    // UPDATE-STATUS-LINE), and the manual-restore path
    // (z0_update_after_restore) is a caller with no game DEFAULT-COLORS call
    // afterwards -- pushing the platform values (Amiga: BLACK on LIGHTGREY)
    // into the globals there turned the left status window black-on-grey at
    // the first border change after a restore. The computed
    // default_fg/default_bg are still applied to the current colours below,
    // so the screen still gets the right defaults.
    //
    // DEFAULT-FG/DEFAULT-BG (and their DEFAULT-COLORS routine) do not exist in
    // early revisions such as r343; their finder leaves the indices at 0. Guard
    // the writes so we don't clobber global 0 every time colours update.
    if (zg.DEFAULT_FG != 0)
        set_global(zg.DEFAULT_FG, DEFAULT_COLOUR);
    if (zg.DEFAULT_BG != 0)
        set_global(zg.DEFAULT_BG, DEFAULT_COLOUR);

    if (graphics_type == kGraphicsTypeApple2) { // Only default colors are allowed with Apple 2 graphics
        current_fg = WHITE_COLOUR;
        current_bg = BLACK_COLOUR;
    } else {     // If machine is changed, the currently selected colors may not be available on the new machine. If so, reset them to default.
        if (options.int_number != INTERP_AMIGA) {
            if (current_fg == LIGHTGREY_COLOUR || current_fg == DARKGREY_COLOUR || current_bg == LIGHTGREY_COLOUR || current_bg == DARKGREY_COLOUR) {
                current_fg = default_fg;
                current_bg = default_bg;
            }
        }
    }

    if (number_of_update_color_calls == 1 && !(options.int_number != INTERP_MACINTOSH && graphics_type == kGraphicsTypeMacBW)) {
        current_fg = 1;
        current_bg = 1;
    }

    set_global(fg_global_idx, current_fg);
    set_global(bg_global_idx, current_bg);

    bool colorized_bw = (gli_z6_colorize &&
                         (gli_z6_graphics == kGraphicsTypeCGA || gli_z6_graphics == kGraphicsTypeMacBW));

    // When gli_enable_styles is true, all Spatterlight color settings (gfgcol, gbgcol) are ignored

    if (gli_enable_styles && !colorized_bw) {
        update_user_defined_colours();

        glk_stylehint_set(wintype_TextBuffer, style_Input, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_TextColor, user_selected_foreground);
        if (options.int_number == INTERP_MSDOS && gli_z6_graphics == kGraphicsTypeCGA)
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_background);
        else
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Preformatted, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Subheader, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Emphasized, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_BackColor, user_selected_background);
    } else {
        user_selected_foreground = gfgcol;
        user_selected_background = gbgcol;
    }

    if (colorized_bw) {
        update_monochrome_colours();
    }
}

#pragma mark - Encyclopedia Frobozzica

// Returns the background color for the encyclopedia text overlay window.
// Amiga and VGA use a parchment-tan color; Blorb uses a slightly lighter
// variant; Apple 2 uses black ; monochrome platforms use white.
static glsi32 encyclopedia_background_color(void) {
    switch (graphics_type) {
        case kGraphicsTypeAmiga:
        case kGraphicsTypeVGA:
            return 0xE8CDAE;
        case kGraphicsTypeBlorb:
            return 0xEAD3B7;
        case kGraphicsTypeApple2:
            return 0;
        default:
            return monochrome_white;
    }
}

// Returns the foreground (text) color for the encyclopedia text window,
// contrasting with encyclopedia_background_color(). Apple 2 has a fixed black
// background so its text is white; the parchment modes use fixed black text.
// Only the monochrome (CGA/MacBW) fallback pairs with the monochrome_white
// background, so it uses monochrome_black -- the matching dark colour, which
// (like monochrome_white) shifts when the "colorize 1-bit graphics" setting is
// on. Don't use monochrome_white/black for the fixed-palette modes above: those
// globals are only meaningfully white/black when colorize is off.
static glsi32 encyclopedia_foreground_color(void) {
    switch (graphics_type) {
        case kGraphicsTypeApple2:
            return 0xFFFFFF;
        case kGraphicsTypeAmiga:
        case kGraphicsTypeVGA:
        case kGraphicsTypeBlorb:
            return 0;
        default:
            return monochrome_black;
    }
}

// Positions and sizes window 3 (text buffer) to overlay the encyclopedia
// border's text area, using image metadata for placement. Sets the
// background to the parchment color.
static void adjust_encyclopedia_text_window(void) {
    int width, height, x, y;

    get_image_size(zorkzero_encyclopedia_text_location, &x, &y);
    get_image_size(zorkzero_encyclopedia_text_size, &width, &height);
    if (graphics_type == kGraphicsTypeAmiga) {
        height -= 10;
    }

    glsi32 encycl_bgnd = encyclopedia_background_color();
    glsi32 encycl_fgnd = encyclopedia_foreground_color();

    // Drive the encyclopedia text entirely from the style_Normal hints rather
    // than per-glyph Glk zcolors. Whenever text is printed, set_window_style()
    // pushes the window's fg_color/bg_color as the active zcolor, and
    // Spatterlight's coloredAttributes then overrides each glyph cell's colours
    // with those concrete values -- which would repaint the background instead
    // of the parchment and, in Apple 2 mode, leave black text on the black
    // background. The game keeps re-setting windows[3]'s colours up to the moment
    // it prints, so resetting them here doesn't stick; instead set_window_style()
    // is taught to push zcolor_Default for this window (see screen.cpp), which
    // leaves the hints below in charge. Because the colours come from the hints,
    // not a baked-in per-run zcolor, win_refresh re-applies them to the
    // already-printed text too -- so a graphics-type change on the fly
    // (e.g. Apple 2 -> EGA) recolours the existing text. V_REFRESH restores the
    // normal hints when the screen is dismissed.
    glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_TextColor, encycl_fgnd);
    glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_BackColor, encycl_bgnd);
    v6_define_window(&Z0_ENCYCLOPEDIA_WINDOW, x * imagescalex, y * imagescaley, width * imagescalex,
                     height * imagescaley);
    win_setbgnd(Z0_ENCYCLOPEDIA_WINDOW.id->peer, encycl_bgnd);
    win_refresh(Z0_ENCYCLOPEDIA_WINDOW.id->peer, 0, 0);
}

// Draws the encyclopedia display: border frame, illustration at the
// picture location, and positions the text overlay window.
static void draw_encyclopedia(void) {
    clear_image_buffer();
    current_graphics_buf_win = graphics_fg_glk;
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(zorkzero_encyclopedia_border, 0, 0);
    int width, height;
    get_image_size(zorkzero_encyclopedia_picture_location, &width, &height);
    draw_to_pixmap_unscaled(current_picture, width, height);
    flush_bitmap(graphics_fg_glk);
    adjust_encyclopedia_text_window();
}

// The demo's SLIDE-SHOW ends by printing the version banner and
// "[Hit any key to continue.]", and then goes straight to the first room of
// the interactive section without a line break or a clear, so that the room
// name ends up on the same line as the prompt. Start the game on a clean page.
void SLIDE_SHOW_END(void) {
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
}

#pragma mark - Map

// Updates the blinking location indicator's coordinates in the BLINK-TBL
// and the local variables used by the BLINK routine. Also flushes the
// image buffer and restarts the timer to trigger the next blink cycle.
void update_blink_coordinates(uint16_t x, uint16_t y) {
    // The release blink mechanism -- a timer-driven BLINK routine that reads the
    // marker coordinates from BLINK-TBL -- is absent in the pre-release
    // revisions (r242/r296/r66, r366 demo).
    // They blink the marker via an INPUT timeout + TYPED? flag and have no
    // BLINK-TBL, so zg.BLINK_TBL is 0. Writing to get_global(0) + offset there
    // scribbles over arbitrary memory, which knocks the map loop out of its read
    // and drops back to normal play -- both on a live resize and on autorestore
    // (screenmode == MODE_MAP is restored, so both reach here via the MODE_MAP
    // path). The betas also drive their own input timer, so we must not
    // commandeer it. Only touch BLINK-TBL and the timer when the release
    // mechanism is actually present; the store_variable() updates below target
    // the BLINK frame's Y/X locals and are correct either way.
    if (zg.BLINK_TBL != 0) {
        store_word(get_global(zg.BLINK_TBL) + 3 * 2, y);
        store_word(get_global(zg.BLINK_TBL) + 4 * 2, x);
    }

    store_variable(3, y);
    store_variable(4, x);

    flush_image_buffer();

    if (zg.BLINK_TBL != 0)
        glk_request_timer_events(1);
}

// Computes the map coordinates of the current room's "you are here" marker
// (the same values V-MAP-LOOP gets from MAP-X/MAP-Y). Returns false if the
// map routines or the P-MAP-LOC property weren't located for this revision:
// P_MAP_LOC == 0 would make internal_get_prop abort ("invalid property: 0"),
// and MAP_X/MAP_Y == 0 would pack_routine(0) into a garbage routine.
static bool z0_current_room_map_coords(uint16_t *cx, uint16_t *cy) {
    if (zp.P_MAP_LOC == 0 || zr.MAP_X == 0 || zr.MAP_Y == 0)
        return false;

    // Get the map location table of current room
    uint16_t TBL = internal_get_prop(get_global(zg.HERE), zp.P_MAP_LOC);
    // Get the coordinates of the map representation of current room
    *cy = call_routine(zr.MAP_Y, {user_word(TBL + 2)}); // <MAP-Y <ZGET .TBL 1>>
    *cx = call_routine(zr.MAP_X, {user_word(TBL + 4)}); // <MAP-X <ZGET .TBL 2>>
    return true;
}

// Z-machine entry point: called each iteration of the map interaction loop.
// Fixes a bug where resizing the screen during map mode and then clicking
// outside an eligible room would draw the blinking indicator at stale
// coordinates. We recalculate the current room's map position and update
// the local variables every iteration.
void V_MAP_LOOP(void) {
    // Bail if the map plumbing wasn't located for this revision. Without the
    // "you are here" coordinates the map still draws (sans marker) rather than
    // crashing.
    uint16_t CX, CY;
    if (!z0_current_room_map_coords(&CX, &CY))
        return;

    // Store into the local slots the game reads when it calls
    // BLINK-WHILE-AWAITING-INPUT. The slot numbers are revision-dependent
    // (recorded when the V-MAP-LOOP entrypoint was located); fall back to the
    // release layout if they weren't set. Using the wrong slots leaves the
    // marker's X holding the Y coordinate, so it draws too far left (the r242
    // beta inlines the loop into V-MAP and uses 5/6; r296/r66/r366 also inline
    // it but use the release 4/5 layout).
    uint8_t cx_var = zr.MAP_CX_VAR != 0 ? zr.MAP_CX_VAR : 4;
    uint8_t cy_var = zr.MAP_CY_VAR != 0 ? zr.MAP_CY_VAR : 5;
    store_variable(cx_var, CX);
    store_variable(cy_var, CY);
}

#pragma mark - Resize Handling

// Called when the display is resized. Redraws the appropriate screen mode:
// definitions, hints, map (with blinking indicator update), mini-games
// (Tower, Peggleboz, Snarfem, Fanucci), normal gameplay, slideshows,
// or encyclopedia. Also handles graphics type changes by refreshing
// margin images and window backgrounds.
//
// Window assignments:
//   0 (S-TEXT): Main text buffer
//   1 (S-WINDOW): Status text grid (left status)
//   2 (SOFT-WINDOW): Text grid for DEFINE command
//   3: Encyclopedia image captions (overlay on background)
//   7 (S-FULL): Full background graphics

// Rebuild SL-LOC-TBL, the table of (scaled) status/compass element metrics
// that DRAW-NEW-COMP reads to position the compass arrow overlays (and that
// DRAW-NEW-HERE uses for the room/region text). Each two-word entry mirrors
// the game's PICINF-PLUS-ONE: round(dim * scale) + 1, height then width.
//
// This must be redone whenever the image scale changes. Later releases
// (r392/r393) fill this table inside SETUP-SCREEN, which we already re-invoke
// on a resize, so for them this is just a harmless idempotent rewrite. But
// r383/r387 fill it only once, in their Main/init routine, which we never
// re-run -- so without this their compass arrows keep their startup-scale
// coordinates after an on-the-fly graphics-format switch and drift off the
// freshly redrawn rose. The Apple II release ("...side 1.woz") is r383.
static void z0_refresh_sl_loc_tbl(void) {
    if (zt.SL_LOC_TBL == 0)
        return;

    // NB: the ICON entry (word offset 20, i.e. word[10]) is deliberately NOT
    // refreshed. It is not a scalable dimension: the compass hit-test reads
    // word[10] as a *flag* (routine 0x14318) to choose between the angle-based
    // classifier 0x1c17c (when 0) and the Macintosh fixed-rectangle classifier
    // 0x1bfec (when non-zero). 0x1c17c adapts to the picture size and scale and
    // is the one that matches our scaled graphics; 0x1bfec's hard-coded native
    // pixel rects do not, so they misfire at scale > 1 (north read as east,
    // etc.). ICON's image size is (0,0), so the old generic dim*scale+1 rewrite
    // turned the flag into 1 and forced the broken path. Leave it untouched.
    static const struct { int picnum; int byte_offset; } entries[] = {
        { HERE_LOC,        0  },
        { REGION_LOC,      4  },
        { COMPASS_PIC_LOC, 8  },
        { U_BOX_LOC,       12 },
        { D_BOX_LOC,       16 },
    };

    for (const auto &e : entries) {
        int width = 0, height = 0;
        if (!get_image_size(e.picnum, &width, &height))
            continue;
        store_word(zt.SL_LOC_TBL + e.byte_offset,     std::lround(height * imagescaley) + 1);
        store_word(zt.SL_LOC_TBL + e.byte_offset + 2, std::lround(width  * imagescalex) + 1);
    }
}

// Recomposite the Zork Zero map for the pre-release revisions (r242/r296/r66,
// r366 demo), which lack a map-aware
// V-$REFRESH. We re-invoke the game's own V-MAP routine with its trailing
// BLINK-WHILE-AWAITING-INPUT call temporarily patched to rtrue, so it redraws
// the whole map (border, room icons, connections, compass) at the *current*
// image scale and then returns -- instead of entering the blink/input loop (and
// its exit-to-normal V-$REFRESH). The still-suspended blink loop keeps drawing
// the marker, now onto a freshly composited map, so resize rescales correctly
// and autorestore repopulates the empty pixmap rather than erasing the map.
//
// No-op (caller falls back to leaving the GUI-rescaled image in place) when the
// routine wasn't located. On a redraw the map's "ready? (y/n)" / jester prompts
// are already past, so V-MAP runs straight through to the drawing.
static void z0_redraw_map(void) {
    if (zr.V_MAP == 0 || zr.V_MAP_BLINK_CALL == 0)
        return;

    uint8_t saved = byte(zr.V_MAP_BLINK_CALL);
    store_byte(zr.V_MAP_BLINK_CALL, kOpRtrue); // rtrue: draw the map, then return
    call_routine(zr.V_MAP);
    store_byte(zr.V_MAP_BLINK_CALL, saved);

    flush_image_buffer();

    // The map is now composited at the live scale, so the marker (drawn live by
    // the blink loop) lands correctly without the map_entry_imagescale override.
    map_entry_imagescalex = imagescalex;
    map_entry_imagescaley = imagescaley;
}

// Redraws the map at the new scale, and moves the blinking "you are here"
// marker of the suspended blink loop to where the current room now is.
// The glk timer was stopped by the caller.
static void redraw_map_on_resize(void) {
    z0_hide_right_status();

    // The releases redraw the map through V-$REFRESH, which is
    // map-aware (it drives the map via the BLINK-TBL mechanism). The
    // pre-release revisions (r242/r296/r66, r366 demo) have no BLINK
    // routine/BLINK-TBL (zg.BLINK_TBL ==
    // 0) and a V-$REFRESH that only knows how to redraw the *normal*
    // screen -- calling it here drops out of the map (see z0_redraw_map).
    // Instead re-invoke the game's own V-MAP to recomposite the map at
    // the new scale, then re-arm the input timer that the
    // glk_request_timer_events(0) at the top of this function cancelled
    // so the still-suspended blink loop resumes.
    if (zg.BLINK_TBL == 0) {
        z0_redraw_map();

        // MAP-X/MAP-Y return *scaled* coordinates (picture_data scales by
        // imagescale), and z0_redraw_map recomposited the map at the live
        // scale -- so the entry-scale marker coordinates still sitting in
        // the suspended BLINK-WHILE-AWAITING-INPUT frame we're nested in
        // are now stale. Recompute them and write them into that frame's
        // Y/X locals (vars 3/4) so the resumed blink lands on its room.
        uint16_t CX, CY;
        if (z0_current_room_map_coords(&CX, &CY))
            update_blink_coordinates(CX, CY);

        glk_request_timer_events(1);
        return;
    }

    // Redraw the map. Skip when the routines weren't located (e.g.
    // r366, where the V-REFRESH/MAP-X/MAP-Y patterns don't match):
    // pack_routine(0) underflows to a bogus packed address that
    // unpacks into string space, so calling it crashes. Likewise skip
    // when P-MAP-LOC wasn't found (e.g. r66, which locates MAP-X/MAP-Y
    // but not V-MAP-LOOP): internal_get_prop(HERE, 0) below would abort
    // with "invalid property: 0". Refreshing is optional here; not
    // crashing is not.
    if (zr.V_REFRESH != 0 && zr.MAP_X != 0 && zr.MAP_Y != 0 && zp.P_MAP_LOC != 0) {
        call_routine(zr.V_REFRESH, {1}); // V-$REFRESH(DONT-CLEAR:true)

        // We are in a loop inside the BLINK routine and need to
        // update coordinates in the BLINK-TBL (which will be read by
        // the TYPED? routine) as well as the local Y and X variables
        // (which are used to redraw/unhighlight the previous
        // location icon when we move away).
        uint16_t CX, CY;
        if (z0_current_room_map_coords(&CX, &CY))
            update_blink_coordinates(CX, CY);
    }
}

// Redraws a full-screen picture at the new size. Returns true if nothing
// more needs to be done.
static bool redraw_slideshow_on_resize(void) {
    v6_define_window(&V6_TEXT_BUFFER_WINDOW, 1, 1, gscreenw, gscreenh);
    win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
    glk_window_clear(current_graphics_buf_win);
    if (is_zorkzero_encyclopedia_image(current_picture)) {
        draw_encyclopedia();
        return true;
    } else if (current_picture == zorkzero_title_image) {
        glk_window_set_background_color(graphics_fg_glk, graphics_type == kGraphicsTypeApple2 ? 0x101B9C : 0);
        glk_window_clear(graphics_fg_glk);
        draw_centered_title_image(current_picture);
    } else if (is_zorkzero_rebus_image(current_picture)) {
        clear_image_buffer();
        ensure_pixmap(current_graphics_buf_win);
        draw_to_pixmap_unscaled(current_picture, 0, 0);
        flush_bitmap(current_graphics_buf_win);
        return true;
    }
    return false;
}

void z0_update_on_resize(void) {
    int MACHINE = byte(0x1e);

    // SETUP_SCREEN and the various V-REFRESH paths below clobber both of
    // these — stash them now so we can put them back at the end.
    int stored_margin_image_number = number_of_margin_images;

    bool NARROW = (MACHINE == INTERP_MSDOS || MACHINE == INTERP_APPLE_IIE);
    set_global(zg.NARROW, NARROW);

    glk_request_timer_events(0);

    // SETUP-SCREEN copies image metrics for the header text and compass
    // into the SL-LOC-TBL.
    if (zr.SETUP_SCREEN != 0)
        call_routine(zr.SETUP_SCREEN);

    // SETUP-SCREEN only refills the compass position table in later releases;
    // do it ourselves so r383/r387 also pick up the new scale (see above).
    z0_refresh_sl_loc_tbl();

    switch (screenmode) {
        case MODE_DEFINE:
            adjust_definitions_window();
            return;

        case MODE_HINTS:
            redraw_hint_screen_on_resize();
            return;

        case MODE_MAP:
            redraw_map_on_resize();
            return;

        case MODE_NORMAL:
        case MODE_Z0_GAME:
            if (redraw_minigame_on_resize(get_global(zg.CURRENT_SPLIT)))
                return;

            // Skip when V-REFRESH wasn't located (e.g. r366): pack_routine(0)
            // underflows to a bogus packed address that unpacks into string
            // space, and calling it crashes (most visibly on autorestore,
            // which reaches here via after_V_COLOR()).
            if (zr.V_REFRESH != 0)
                call_routine(zr.V_REFRESH, {1}); // V-$REFRESH(DONT-CLEAR:true)
            if (V6_TEXT_BUFFER_WINDOW.id) {
                if (graphics_type_changed)
                    v6_refresh_text_buffer_images();
                win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);
            }
            break;

        case MODE_SLIDESHOW:
            if (redraw_slideshow_on_resize())
                return;
            break;

        default:
            break;
    }

    // flush_image_buffer() ends up running V-COLOR / refresh paths that
    // overwrite user_selected_background and reset number_of_margin_images
    // to zero. Snapshot, flush, then restore so margin-image tracking and
    // the chosen background color survive the resize.
    glui32 stored_bg = user_selected_background;
    flush_image_buffer();
    user_selected_background = stored_bg;
    number_of_margin_images = stored_margin_image_number;
}

#pragma mark - Save/Restore

// Called after a manual restore. Applies the color configuration
// in the save file.
void z0_update_after_restore(void) {
    // screenmode is C++ state that is NOT part of the saved game. A manual
    // @restore can only be issued from a normal command prompt, never from
    // a modal screen (map, slideshow, hints, definitions, ...). If a modal
    // mode lingers from before the restore -- e.g. MODE_MAP after the player
    // opened the MAP, since exiting it doesn't reset screenmode -- the
    // z0_update_on_resize() reached via after_V_COLOR() below would take
    // that mode's redraw path (for MODE_MAP: internal_call MAP_X/MAP_Y with
    // BLINK-TBL/P_MAP_LOC state from the restored, non-map game), which
    // diverges and crashes. Reconcile to a resting gameplay mode first.
    if (screenmode != MODE_NORMAL && screenmode != MODE_Z0_GAME &&
        screenmode != MODE_NO_GRAPHICS) {
        screenmode = MODE_NORMAL;
    }

    uint8_t fg = get_global(fg_global_idx);
    uint8_t bg = get_global(bg_global_idx);
    z0_update_colors();
    set_global(fg_global_idx, fg);
    set_global(bg_global_idx, bg);
    after_V_COLOR();
}

// Called after an automatic restore. Handles two cases:
//
// 1. Encyclopedia: the background graphics window (graphics_bg_glk) has the
//    normal-mode room graphic restored from the autosave, which shows around
//    and below the foreground encyclopedia window. The live entry path blanks
//    it in z0_display_picture, but autorestore doesn't go through there, so
//    blank it here. A restored caption window (Z0_ENCYCLOPEDIA_WINDOW) in slideshow mode
//    uniquely identifies the encyclopedia (title/rebus never create it).
//
// 2. Peggleboz: if we were in Peggleboz with a peg selected, the C++ static
//    selected_peg has been reset to 0 by the process restart, so the resize
//    handler no longer knows to restart the blink timer. Recover
//    selected_peg_pos by matching the BLINK-TBL coordinates (preserved as
//    Z-machine state) against the BOARD-TABLE peg positions, then restart the
//    blink timer.
void z0_update_after_autorestore(void) {
    if (screenmode == MODE_SLIDESHOW && Z0_ENCYCLOPEDIA_WINDOW.id != nullptr) {
        glk_window_clear(graphics_bg_glk);
        // Re-run the encyclopedia text-window setup the live entry path
        // performs (z0_display_picture -> adjust_encyclopedia_text_window).
        // It repositions/sizes windows[3] for the current screen and, more
        // importantly, re-establishes the process-global wintype_TextBuffer
        // style_Normal background hint (the parchment colour). That hint is
        // not part of the autosave, so without this the encyclopedia's
        // background bleeds into the main text window. Restoring it here puts
        // the hint in the same "encyclopedia is up" state as live play, so
        // V_REFRESH tears it back down to user_selected_background when the
        // player dismisses the entry.
        adjust_encyclopedia_text_window();
        return;
    }

    // Per-window text/background colours live in C++ Window state that is
    // not part of the autosave, so a process restart resets them to the
    // default (ANSI 1). The Z-machine colour globals *are* restored, so the
    // restored snapshot still looks right -- but the next status-line redraw
    // (z0_UPDATE_STATUS_LINE -> set_current_window -> set_window_style) would
    // reapply the stale default colours, giving the wrong status-window
    // colours. v6_set_window_colors_from_globals() below repopulates them
    // from the restored globals. We deliberately do not call after_V_COLOR() /
    // z0_update_on_resize() here: those take mode-specific redraw paths that
    // can crash during autorestore (see the screenmode guard in
    // z0_update_after_restore).
    update_user_defined_colours();

    // Re-assert the DEFAULT-FG/DEFAULT-BG == 1 invariant (see the comment in
    // z0_update_colors). An autosave written by a session where a manual
    // restore had pushed the platform defaults into these globals carries the
    // stale values, and the game paints the bordered status line with
    // <COLOR ,DEFAULT-FG ,DEFAULT-BG> on the next border change.
    if (zg.DEFAULT_FG != 0)
        set_global(zg.DEFAULT_FG, DEFAULT_COLOUR);
    if (zg.DEFAULT_BG != 0)
        set_global(zg.DEFAULT_BG, DEFAULT_COLOUR);

    v6_set_window_colors_from_globals();

    restore_selected_peg();
    restore_fanucci_command_grid();
}

#pragma mark - Image Display Dispatch

static const char *z0_fullscreen_image_description(int picnum) {
    switch (picnum) {
        case zorkzero_title_image:
            return "\n(Showing Zork Zero full-screen title image. ZORK ZERO written in stone masonry with the subtitle The Revenge of Mageboz on a banner below with a wizard's hat balanced on the edge of the banner.)\n";
        case zorkzero_map_border:
            return "\n(Showing a full screen graphical map where the locations are represented as interconnected boxes. There is a compass rose in the style of a sigil stamp at the top. Some of the room boxes have little icons and other decorations.)\n";
        default:
            if (is_zorkzero_rebus_image(picnum))
                return "\n(Showing a full screen rebus image, just as in the room description.)\n";
            return "\n(Showing a full screen image.)\n";
    }
}

// With graphics off, the title screen and the rebus would be a blank screen
// waiting for a key. Leave them out, and let the keypress that would have
// dismissed them return at once. Returns false for the full-screen pictures
// that are still set up as usual (the map and the encyclopedia, which have
// text of their own).
static bool z0_skip_fullscreen_picture(int pic) {
    if (pic == zorkzero_title_image) {
        v6_skip_next_keypress();
        return true;
    }
    if (is_zorkzero_rebus_image(pic)) {
        // The game clears the screen on its way back from the rebus;
        // z0_init_status_line() prints the note once that is done.
        should_show_rebus_hint_message = true;
        v6_skip_next_keypress();
        return true;
    }
    if (pic == zorkzero_map_border) {
        // The map is nothing but pictures. Let the game set it up, then
        // leave it at once, as if a key had been pressed.
        should_show_map_note = true;
        v6_skip_next_keypress();
        return true;
    }
    return false;
}

// Draws a picture that takes over the whole screen: the title, the map,
// the encyclopedia or a rebus.
static void display_fullscreen_picture(int x, int y) {
    if (v6_graphics_off() && z0_skip_fullscreen_picture(current_picture))
        return;

    // First time the player sees a rebus, nudge them toward the
    // built-in InvisiClues. We print into the text buffer while it
    // is still the active window (we're still in MODE_NORMAL here),
    // so the message is waiting in the scrollback once the player
    // dismisses the fullscreen image.
    if (gli_voiceover_on && is_zorkzero_rebus_image(current_picture) && !shown_rebus_hint_message) {
        shown_rebus_hint_message = true;
        should_show_rebus_hint_message = true;
        v6_describe_fullscreen_image(z0_fullscreen_image_description(current_picture));
    }

    z0_hide_right_status();
    current_graphics_buf_win = graphics_fg_glk;
    clear_image_buffer();
    ensure_pixmap(graphics_fg_glk);
    win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
    glk_request_mouse_event(graphics_fg_glk);

    if (current_picture == zorkzero_map_border) {
        screenmode = MODE_MAP;
        // The map takes over the floating foreground graphics window, but
        // the full-screen background window still holds the normal-mode
        // room graphic (e.g. the underground pillars). With a non-zero
        // window border the foreground window is inset, so a frame of those
        // pillars peeks through -- most visibly while resizing. Blank the
        // background once here; the map is not drawn into it, so it stays
        // clear, and the room is redrawn normally when map mode is torn
        // down. Mirrors the encyclopedia path below.
        glk_window_clear(graphics_bg_glk);
        // Remember the scale the map is composited at. The pre-release
        // revisions (r242/r296/r66, r366 demo) don't recomposite the map on
        // resize (the GUI rescales the rendered
        // image), so the live-redrawn you-are-here marker must use this same
        // scale to stay on its room icon -- see the default path below.
        map_entry_imagescalex = imagescalex;
        map_entry_imagescaley = imagescaley;
        draw_to_buffer(current_graphics_buf_win, current_picture, x, y);
        flush_bitmap(current_graphics_buf_win);
        v6_describe_fullscreen_image(z0_fullscreen_image_description(zorkzero_map_border));
        return;
    }

    screenmode = MODE_SLIDESHOW;

    if (current_picture == zorkzero_encyclopedia_border) {
        // The encyclopedia is drawn into the floating foreground graphics
        // window (and caption window 3), which sit on top of the
        // full-screen background graphics window holding the normal-mode
        // room graphic (e.g. the underground pillars). With a non-zero
        // window border the floating windows are inset, so a frame of that
        // background peeks through -- most visibly along the bottom, and
        // intermittently while resizing. Blank it once here: it is not
        // redrawn while the encyclopedia is up, so it stays clear across
        // resizes, and the room is redrawn normally when V_REFRESH tears
        // the encyclopedia down on exit.
        glk_window_clear(graphics_bg_glk);
        Z0_ENCYCLOPEDIA_WINDOW.id = gli_new_window(wintype_TextBuffer, 0);
        draw_to_pixmap_unscaled(zorkzero_encyclopedia_border, 0, 0);
        image_needs_redraw = true;
        adjust_encyclopedia_text_window();
        return;
    }

    if (current_picture == zorkzero_title_image) {
        glk_window_set_background_color(graphics_fg_glk, graphics_type == kGraphicsTypeApple2 ? 0x101B9C : 0);
        glk_window_clear(current_graphics_buf_win);
        draw_centered_title_image(current_picture);
        glk_window_set_background_color(graphics_fg_glk, user_selected_background);
        v6_describe_fullscreen_image(z0_fullscreen_image_description(zorkzero_title_image));
        return;
    }

    // Rebus image (the remaining case in this branch).
    draw_to_buffer(current_graphics_buf_win, current_picture, x, y);
    flush_bitmap(current_graphics_buf_win);
}

// Draws a picture in a text buffer window, as a margin image that the text
// wraps around.
static void display_inline_picture(Window *win) {
    if (v6_graphics_off()) {
        // The illuminated initials stand in for the first letter of the
        // text that follows; print it instead. (Screen only: the game
        // adds the letter to the transcript itself.)
        if (current_picture == zorkzero_prologue_letter || current_picture == zorkzero_epilogue_letter)
            glk_put_char_stream(glk_window_get_stream(win->id), 'A');
        else if (current_picture == zorkzero_main_letter)
            glk_put_char_stream(glk_window_get_stream(win->id), 'Y');
        return;
    }
    pending_flowbreak = true;
    float inline_scale = v6_inline_image_scale();
    draw_inline_image(win->id, current_picture, imagealign_MarginLeft, current_picture, inline_scale, false);
    add_margin_image_to_list(current_picture);
}

// Main image display handler for Zork Zero. Routes each picture to the
// appropriate rendering path based on the image type and current screen mode:
// - Tower/Peggleboz images: drawn directly to the offscreen pixmap
// - Peggleboz buttons: delegated to z0_draw_peggleboz_box_image
// - Title, encyclopedia border, map border, rebus images: switch to
//   fullscreen mode with the foreground graphics window
// - Encyclopedia illustrations: drawn with border frame and text overlay
// - Text buffer images (room illustrations): drawn as margin images
// - Other images: drawn to the current background graphics buffer
bool z0_display_picture(int x, int y, Window *win) {
    // Tower of Bozbar weights and Peggleboz pegs are drawn straight into
    // the offscreen pixmap; their parent screens flush it explicitly later.
    if (is_zorkzero_tower_image(current_picture) || is_zorkzero_peggleboz_image(current_picture)) {
        draw_to_pixmap_unscaled(current_picture, x, y);
        return true;
    }

    // Peggleboz UI buttons (Restart, Show Moves, Exit) have their own
    // helper.
    if (is_zorkzero_peggleboz_box_image(current_picture)) {
        z0_draw_peggleboz_box_image(current_picture);
        return true;
    }

    // Fullscreen images: title screen, encyclopedia border, map border,
    // and the first rebus draw of a session each take over the foreground
    // graphics window.
    if (is_zorkzero_fullscreen_picture(current_picture)) {
        display_fullscreen_picture(x, y);
        return true;
    }

    // Inline image in a text buffer: drawn as a margin image so text
    // wraps around it.
    if (win->id && win->id->type == wintype_TextBuffer) {
        display_inline_picture(win);
        return true;
    }

    // Encyclopedia Frobozzica illustrations are drawn with the surrounding
    // border frame and caption window; draw_encyclopedia() does the work.
    if (is_zorkzero_encyclopedia_image(current_picture)) {
        win_sizewin(graphics_fg_glk->peer, 0, 0, gscreenw, gscreenh);
        draw_encyclopedia();
        return true;
    }

    // Default: composite into the current background graphics buffer.
    if (current_graphics_buf_win == nullptr)
        current_graphics_buf_win = graphics_bg_glk;

    // On the pre-release revisions (r242/r296/r66, r366 demo) the map isn't
    // recomposited on resize -- the GUI
    // rescales the already-rendered map image -- but the game redraws the
    // blinking you-are-here marker live, and draw_to_buffer would place it at
    // the current (resized) scale, off its room icon. Composite it at the scale
    // the map was drawn with so it stays put. At map entry the two scales are
    // equal, so this only differs once the window has actually been resized.
    if (screenmode == MODE_MAP && zg.BLINK_TBL == 0 && map_entry_imagescalex != 0 &&
        (imagescalex != map_entry_imagescalex || imagescaley != map_entry_imagescaley)) {
        float saved_x = imagescalex, saved_y = imagescaley;
        imagescalex = map_entry_imagescalex;
        imagescaley = map_entry_imagescaley;
        draw_to_buffer(current_graphics_buf_win, current_picture, x, y);
        imagescalex = saved_x;
        imagescaley = saved_y;
        return true;
    }

    draw_to_buffer(current_graphics_buf_win, current_picture, x, y);
    return true;
}

// Z-machine entry point: resets color globals to 1 (meaning "use default").
void DEFAULT_COLORS(void) {
    set_global(zg.DEFAULT_FG, 1);
    set_global(zg.DEFAULT_BG, 1);
}

// Saves Zork Zero's window tags into the library state structure for
// serialization during save. Tags are stable identifiers that survive
// window recreation.
void z0_stash_state(library_state_data *dat) {
    if (!dat)
        return;

    v6_stash_graphics_windows(dat);
    if (z0_right_status_window)
        dat->z0_right_status_tag = z0_right_status_window->tag;

    dat->z0_shown_rebus_hint_message = shown_rebus_hint_message;
}

// Restores Zork Zero's window pointers from saved tags after a restore
// operation. Looks up the current window objects by their stable tags.
void z0_recover_state(library_state_data *dat) {
    if (!dat)
        return;

    v6_recover_graphics_windows(dat);
    z0_right_status_window = gli_window_for_tag(dat->z0_right_status_tag);

    shown_rebus_hint_message = dat->z0_shown_rebus_hint_message;
}

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

#include <cmath>
#include <initializer_list>

#include "memory.h"
#include "objects.h"
#include "options.h"
#include "screen.h"
#include "stack.h"
#include "unicode.h"
#include "v6_specific.h"
#include "v6_shared.hpp"
#include "zterp.h"

#include "entrypoints.hpp"
#include "draw_image.hpp"
#include "shogun.hpp"

#include "zorkzero.hpp"

// Window 1 (S-WINDOW) is used as the left status text grid
#define z0_left_status_window windows[1].id

#define MAX(a,b) (a > b ? a : b)

// Right status window: displays region name and score, positioned at the
// right edge of the border. Not part of the standard window array.
winid_t z0_right_status_window = nullptr;
// Width of the right status window in characters
glui32 z0_right_status_width;

// Set once we've nudged the player toward the built-in InvisiClues on their
// first rebus encounter, so we don't keep reminding them.
static bool shown_rebus_hint_message = false;
static bool should_show_rebus_hint_message = false;

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
        fprintf(stderr, "Error!\n");
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

// Z-machine entry point: handles screen refresh. Cleans up window 3
// (encyclopedia caption) when returning from encyclopedia text view.
void V_REFRESH(void) {
    uint16_t CURRENT_SPLIT = get_global(zg.CURRENT_SPLIT);
    if (CURRENT_SPLIT == TEXT_WINDOW_PIC_LOC) {
        v6_delete_win(&windows[3]);
        // Restore the normal text-buffer colour hints that
        // adjust_encyclopedia_text_window overrode for the encyclopedia, so
        // later text-buffer windows aren't created with the encyclopedia's
        // colours. (z0_update_colors sets these same values during normal
        // play.)
        glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_BackColor, user_selected_background);
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
void SPLIT_BY_PICTURE(uint16_t id, bool clear_screen) {
    set_global(zg.CURRENT_SPLIT, id); // <SETG CURRENT-SPLIT .ID>
    if (clear_screen) {
        glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
        glk_window_clear(V6_STATUS_WINDOW.id);
    }
    int width, height;
    get_image_size(id, &width, &height);

    uint16_t x, x_size;
    uint16_t y = (height + 1) * imagescaley;

    if (get_global(zg.BORDER_ON) == 0 && id == TEXT_WINDOW_PIC_LOC) {
        x = 1;
        x_size = gscreenw;
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

//    set_global(zg.NEW_COMPASS, 1);

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

// Placeholder for any special handling needed when autorestoring during
// an internal_read_char call. Currently unused for Zork Zero.
void z0_autorestore_internal_read_char_hacks(void) {
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

// Initializes the status line layout. Splits the screen by the current split
// picture, sets up border display (if borders are on), positions the status
// windows, and configures text styles for the status area.
// Returns true if the current split is for a mini-game (Fanucci/Peggleboz)
// which needs special text window adjustment.
static bool z0_init_status_line(bool DONT_CLEAR) {
//    set_global(zg.COMPASS_CHANGED, 1);  // <SETG COMPASS-CHANGED T>
//    set_global(zg.NEW_COMPASS, 1);  // <SETG NEW-COMPASS T>

    bool border_on = get_global(zg.BORDER_ON) == 1;
    if (!DONT_CLEAR) {
        // <CLEAR -1>
        if (V6_TEXT_BUFFER_WINDOW.id != nullptr) {
            glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
            if (should_show_rebus_hint_message) {
                glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
                transcribe_and_print_string("\n[If the rebus images are causing you trouble, don't hesitate to consult the built-in InvisiClues by typing HINT, and then navigating to GREAT HALL AREA, reading the rebus.]\n");
                should_show_rebus_hint_message = false;
                shown_rebus_hint_message = true;
                glk_request_char_event(V6_TEXT_BUFFER_WINDOW.id);
                glk_cancel_char_event(V6_TEXT_BUFFER_WINDOW.id);
            }
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

        glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_BackColor);

        if (graphics_type == kGraphicsTypeCGA || graphics_type == kGraphicsTypeMacBW) {
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, monochrome_black);
        } else {
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
        }

        set_current_window(&windows[7]); // <SCREEN ,S-FULL>
        if (zg.CURRENT_BORDER != 0) {
            set_global(zg.CURRENT_BORDER, internal_call(pack_routine(zr.SET_BORDER)));
            DISPLAY_BORDER((BorderType)get_global(zg.CURRENT_BORDER));
        } else if (zr.SET_BORDER != 0) {
            DISPLAY_BORDER((BorderType)internal_call(pack_routine(zr.SET_BORDER)));
        }
        // If neither the CURRENT-BORDER global nor a detected SET-BORDER routine
        // is available (the release-tuned patterns don't match the pre-release
        // r296/r66 layouts), skip the border draw rather than calling
        // pack_routine(0) into a garbage routine.
        for (int i = 1; i < 14; i++) {
            internal_clear_attr(0x166, i);
        }

    } else {
        set_current_window(&V6_STATUS_WINDOW); // <SCREEN ,S-WINDOW>
//        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_ReverseColor, 1);
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_background);
//        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, user_selected_foreground);
    }

    win_refresh(V6_STATUS_WINDOW.id->peer, 0, 0);
    win_refresh(z0_right_status_window->peer, 0, 0);

//    set_global(0x9a, 1); //  <SETG FONT-X <LOWCORE (FWRD 1)>>

    set_current_window(&V6_TEXT_BUFFER_WINDOW); // <SCREEN ,S-TEXT>

    return false;
}

// Z-machine entry point: wraps z0_init_status_line with the DONT-CLEAR
// parameter from the Z-machine call stack.
void INIT_STATUS_LINE(void) {
    z0_init_status_line(variable(1));
}


//FG_COLOR = 0->45; // Header byte 0x2d is default foreground color
//BG_COLOR = 0->44; // Header byte 0x2c is default background color

// Header byte 0x1e is interpreter number
// Header word 8 is flags 2, bit 6 means game wants to use colours

//if (0->30 == IBM && 0-->8 & 64 == 64) {
//    DEFAULT_FG = BLACK;
//    DEFAULT_BG = WHITE;
//    Default_Colors();
//} else if (0->30 == AMIGA) {
//    DEFAULT_FG = BLACK;
//    DEFAULT_BG = LIGHTGREY;
//    Default_Colors();
//}

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

    bool BORDER_ON = (get_global(zg.BORDER_ON) == 1);

    if (z0_left_status_window == nullptr) {
        z0_left_status_window = gli_new_window(wintype_TextGrid, 0);
        fprintf(stderr, "z0_resize_status_windows: creating a new left status window with peer %d\n", z0_left_status_window->peer);
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
    } else if (graphics_type == kGraphicsTypeCGA) {
        text_area_height = 10;
    } else if (graphics_type == kGraphicsTypeEGA) {
        text_area_height = 10;
    }

    int text_area_y_offset = text_area_height * imagescaley - gcellh;

    x = (hereoffx - 1) * imagescalex - ggridmarginx + text_area_y_offset;
    y = (hereoffy - 1) * imagescaley - ggridmarginy + text_area_y_offset;

    if (!BORDER_ON) {
        fprintf(stderr, "z0_resize_status_windows: border is off\n");
        z0_hide_right_status();

        v6_define_window(&V6_STATUS_WINDOW, 1, 1, gscreenw, height);
        glk_window_clear(z0_left_status_window);
        z0_right_status_width = (gscreenw - 2 * ggridmarginx) / gcellw;
        return;
    }

    uint16_t HERE = get_global(zg.HERE);
    int stringlength = count_characters_in_object(HERE);
    int width_in_chars = MAX(stringlength, 13); // 13 characters to make room for "MOVES:" plus 2 spaces and 10000 moves
    width = width_in_chars * gcellw + 2 * ggridmarginx;

    z0_left_status_window->bbox.x0 = x;
    win_sizewin(z0_left_status_window->peer, x, y, x + width, y + height);
    glk_window_clear(z0_left_status_window);

    int16_t REGION = internal_get_prop(HERE, zp.P_REGION);

    z0_right_status_width = MAX(count_characters_in_zstring(REGION), 10); // 10 characters to make room for "SCORE:" plus 1000 points

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
#define Z0_HINT_BORDER_L 503
#define Z0_HINT_BORDER_R 504

// Height of the Amiga/Mac hint border top section (used for pillar placement)
#define Z0_HINT_TOP_HEIGHT 33

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
    get_image_size(Z0_HINT_BORDER, &width, &height);

    // draw_border_common draws the main border with the current palette
    // (it does not load the image's own palette), so load it here. Shogun
    // does the equivalent in shogun_display_border. Without this the hint
    // border is drawn with a stale palette in the per-image-palette
    // formats (Amiga and Mac color), giving the wrong colors.
    extract_palette_from_picnum(Z0_HINT_BORDER);

    int border_top = 0;
    int pillar_top = 0;
    if (graphics_type != kGraphicsTypeAmiga && graphics_type != kGraphicsTypeMacBW) {
        border_top = height;
        pillar_top = border_top;
    } else {
        pillar_top = Z0_HINT_TOP_HEIGHT;
    }

    int bottom_offset = (graphics_type == kGraphicsTypeCGA) ? 10 : 0;

    draw_border_common(Z0_HINT_BORDER, Z0_HINT_BORDER_L, Z0_HINT_BORDER_R,
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
    bool BORDER_ON = (get_global(zg.BORDER_ON) == 1);
//    bool COMPASS_CHANGED = (get_global(zg.COMPASS_CHANGED) == 1);
//    if (!COMPASS_CHANGED)
//        fprintf(stderr, "z0_UPDATE_STATUS_LINE: COMPASS_CHANGED == %d\n", get_global(zg.COMPASS_CHANGED));
    uint16_t HERE = get_global(zg.HERE);
    uint16_t REGION = internal_get_prop(HERE, zp.P_REGION);

    // if interpreter is IBM and color flag is unset
    // or border is off
    // set reverse video

    z0_resize_status_windows();

    set_current_window(&V6_STATUS_WINDOW);
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
//        if (COMPASS_CHANGED) {
            if (zr.DRAW_NEW_COMP != 0) {
                internal_call(pack_routine(zr.DRAW_NEW_COMP));
            } else if (zr.DRAW_COMPASS_ROSE != 0 && zt.PICINF_TBL != 0) {
                // Legacy fallback for the early releases (r242/beta) that have
                // DRAW-COMPASS-ROSE + PICINF-TBL but no DRAW-NEW-COMP. Guard on
                // both being detected: on the middle releases (r296/r343/r366)
                // none of the three is found, and running this unguarded wrote
                // to PICINF_TBL==0 (corrupting the header at address 0) and
                // called pack_routine(0) eight times -- which blanked the whole
                // bordered status line (no pillars, no compass).
//                set_global(zg.COMPASS_CHANGED, 0);
                int width, height;
                get_image_size(COMPASS_PIC_LOC, &width, &height);

                store_word(zt.PICINF_TBL, (height + 1) * imagescaley);
                store_word(zt.PICINF_TBL + 2, (width + 1)  * imagescalex);

                fprintf(stderr, "COMPASS-PIC-LOC: width:%d height:%d\n", width, height);

                fprintf(stderr, "PICINF-TBL 0:0x%x\n", word(zt.PICINF_TBL));
                fprintf(stderr, "PICINF-TBL 1:0x%x\n", word(zt.PICINF_TBL + 2));

                fprintf(stderr, "pixelwidth:%f\n", pixelwidth);
                fprintf(stderr, "imagescalex:%f\n", imagescalex);
                fprintf(stderr, "imagescaley:%f\n", imagescaley);

                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_NORTH, N_HIGHLIGHTED, N_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_NE, NE_HIGHLIGHTED, NE_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_EAST, E_HIGHLIGHTED, E_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_SE, SE_HIGHLIGHTED, SE_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_SOUTH, S_HIGHLIGHTED, S_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_SW, SW_HIGHLIGHTED, SW_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_WEST, W_HIGHLIGHTED, W_UNHIGHLIGHTED});
                internal_call(pack_routine(zr.DRAW_COMPASS_ROSE), {P_NW, NW_HIGHLIGHTED, NW_UNHIGHLIGHTED});
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

    // DEFAULT-FG/DEFAULT-BG (and their DEFAULT-COLORS routine) do not exist in
    // early revisions such as r343; their finder leaves the indices at 0. Guard
    // the writes so we don't clobber global 0 every time colours update. The
    // computed default_fg/default_bg are still applied to the current colours
    // below, so the screen still gets the right defaults.
    if (zg.DEFAULT_FG != 0)
        set_global(zg.DEFAULT_FG, default_fg);
    if (zg.DEFAULT_BG != 0)
        set_global(zg.DEFAULT_BG, default_bg);

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
    v6_define_window(&windows[3], x * imagescalex, y * imagescaley, width * imagescalex,
                     height * imagescaley);
    win_setbgnd(windows[3].id->peer, encycl_bgnd);
    win_refresh(windows[3].id->peer, 0, 0);
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
#define F_MENU_LOC    384
#define J_SCORE_LOC   385
#define F_DISCARD_LOC 419
#define F_CARD_1_LOC  420
#define F_CARD_SPACE  421

#define F_DISCARD_PIC_LOC 369

// Attribute offset: attributes DRAW_CARDS_OFFSET+0..+4 on NOT-HERE-OBJECT
// track which card slots have been drawn
#define DRAW_CARDS_OFFSET 22

// Card face image constants
#define F_CARD_BACK 100
#define F_CARD 101
#define F_INKBLOTS 102
#define F_RV_INKBLOTS 117
#define F_RANK_PIC_LOC 374
#define F_REV_RANK_PIC_LOC 375
#define F_SUIT_PIC_LOC 376
#define F_REV_SUIT_PIC_LOC 377

// Rank number images: F_0 + rank for normal, F_RV_0 + rank for reversed
#define F_0 132
#define F_RV_0 143

// Special card: Granola (rank 12+)
#define F_GRANOLA 154

#define F_CARD 101
// Location of card slot 1 (cards 2-4 follow at consecutive pic locs)
#define F_1_PIC_LOC 370

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
    uint16_t score = get_global(global);
    if (score < 10) {
        glk_put_string(
                       const_cast<char*>("00"));
    } else if (score < 100) {
        glk_put_char('0');
    }
    print_number(score);
}

// Refreshes both Jester's and player's score displays.
static void update_scores(void) {
    update_score(V6_STATUS_WINDOW.id, 0x39, 17);
    update_score(z0_right_status_window, 0x3b, 13);
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
    int rank_order[] = { -1, 4, 5, 6, 7, 8, 9, 0, 10, 1, 2, 3 };
    rank = user_byte(zt.F_CARD_TABLE + rank);
    suit = user_byte(zt.F_CARD_TABLE + suit);
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
// visible.
static void draw_cards(void) {
    zo.NOT_HERE_OBJECT = 0x159;

    int x, y;
    for (int cnt = 0; cnt < 5; cnt++) {
        get_image_size(F_DISCARD_PIC_LOC + cnt, &x, &y);
        uint16_t draw_cards_attribute = DRAW_CARDS_OFFSET + cnt;
        if (!internal_test_attr(zo.NOT_HERE_OBJECT, draw_cards_attribute))
            internal_set_attr(zo.NOT_HERE_OBJECT, draw_cards_attribute);
        draw_card(x, y, cnt * 2, cnt * 2 + 1);
    }
};

// The text grid window used for the Fanucci command/play selection menu
static winid_t fanucci_command_grid = nullptr;
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

// Index of the currently highlighted command in the Fanucci grid (-1 = none)
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
                resigned = (internal_call(pack_routine(zr.PLAY_SELECTED), {ptr}) == 1); // <PLAY-SELECTED PTR>
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
    internal_call(pack_routine(zr.J_PLAY));
}

// Checks whether the Fanucci game is over (one player's score is high enough).
static bool score_check(void) {
    int result = internal_call(pack_routine(zr.SCORE_CHECK));
    return (result == 1);
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

    if (autosolve_visual_puzzle()) {
        internal_clear_attr(0x012f, 0x1e); // <FCLEAR ,BROOM ,TRYTAKEBIT>
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
                internal_clear_attr(0x012f, 0x1e); // <FCLEAR ,BROOM ,TRYTAKEBIT>
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
    gli_delete_window(fanucci_command_grid);
    fanucci_command_grid = nullptr;
    for (int i = 2; i < 7; i++)
        v6_delete_win(&windows[i]);
    screenmode = MODE_NORMAL;
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
        if (windows[3].id != nullptr) {
            v6_delete_win(&windows[3]);
            glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_TextColor, user_selected_foreground);
            glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_BackColor, user_selected_background);
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
        if (fanucci_command_grid != nullptr) {
            gli_delete_window(fanucci_command_grid);
            fanucci_command_grid = nullptr;
            for (int i = 2; i < 7; i++)
                v6_delete_win(&windows[i]);
            screenmode = MODE_NORMAL;
        }
        break;

    default:
        break;
    }
}

// Tests whether the last mouse click position falls within the rectangle
// defined by (left, top, width, height) in unscaled image coordinates.
// Used by all mini-games for mouse hit-testing.
static bool within(uint16_t left, uint16_t top, uint16_t width, uint16_t height) {
    uint16_t mouse_click_addr = header.extension_table + 2;
    int16_t x = word(mouse_click_addr);
    int16_t y = word(mouse_click_addr + 2);

    x = x / imagescalex - imagescalex;
    y = y / imagescaley - imagescaley;

    uint16_t right = left + width;
    uint16_t bottom = top + height;

    return (x >= left - 1 && x < right && y >= top - 1 && y < bottom);
}

// Hit-tests a mouse click against the 4 hand card slots. Returns the ASCII
// code '1'-'4' for the clicked card, or ZSCII_CLICK_SINGLE if no card was hit.
uint16_t F_MOUSE_CARD_PICK(void) {
    int x, y, width, height;
    get_image_size(F_CARD, &width, &height);
    for (int i = 0; i < 4; i++) {
        get_image_size(F_1_PIC_LOC + i, &x, &y);
        if (within(x, y, width, height))
            return 49 + i;
    }
    return ZSCII_CLICK_SINGLE;
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
    fanucci_window(1, x, y, 20);
    glk_put_string(const_cast<char*>("Jester's Score:  000"));

//    if (z0_right_status_window == nullptr) {
////        z0_right_status_window = gli_new_window(wintype_TextGrid, 0);
//        fprintf(stderr, "Error!\n");
//    }

    win_sizewin(z0_right_status_window->peer, MAX(gscreenw - 21 * gcellw - 2 * ggridmarginx, 0), y, gscreenw - x, y + V6_STATUS_WINDOW.y_size);
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
        fanucci_command_grid = gli_new_window(wintype_TextGrid, 0);

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
#define PILE_OF_0 74

// Flower images indicating the optimal move:
// Left flower shows which pile, right flower shows how many to take
#define L_FLOWERS_0 94
#define R_FLOWERS_0 84

// Image location for pile 1 (piles 2-4 follow at consecutive pic locs)
#define PILE_1_PIC_LOC 357
#define L_FLOWERS_PIC_LOC 361
#define R_FLOWERS_PIC_LOC 362

// Numbered selection box images
#define BOX_1 445

// Location and spacing for the row of numbered boxes
#define BOX_1_LOC 470
#define SN_BOX_SPACE 471

// Dimmed vs active number images for the selection boxes
#define DIMMED_SN_NUMBER 453
#define SN_NUMBER 444

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
    for (int i = 1; i < 9; i++) {
        if (within(left, top, width, height)) {
            return i;
        }
        left += box_space;
    }
    if (already_picked_pile)
        return 0;
    for (int i = 1; i < 9; i++) {
        get_image_size(PILE_OF_0 + i, &width, &height);
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
    last_pile = last_pile;
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

// Z-machine entry point: offers auto-solve for the Snarfem game.
// If accepted, immediately wins by clearing the TRYTAKEBIT on the FAN object.
void SNARFEM(void) {
    if (autosolve_visual_puzzle()) {
        transcribe_and_print_string("\n");
        store_variable(4, 1);
        internal_clear_attr(zo.FAN, zp.TRYTAKEBIT);
    }
}

#pragma mark - Peggleboz

// Peggleboz is a peg-solitaire puzzle. The player jumps pegs over each other
// to remove them, trying to leave as few pegs as possible. The board has 21
// peg positions. UI buttons: Restart, Show Moves, and Exit.

// Draws a Peggleboz UI button (Restart, Show Moves, or Exit) at its
// designated location.
static void z0_draw_peggleboz_box_image(uint16_t pic, uint16_t x, uint16_t y) {
    int width, height;
    switch (pic) {
        case RESTART_BOX:
        case DIM_RESTART_BOX:
            get_image_size(PBOZ_RESTART_BOX_LOC, &width, &height);
            draw_to_pixmap_unscaled(pic, width, height);
            return;
        case SHOW_MOVES_BOX:
        case DIM_SHOW_MOVES_BOX:
            get_image_size(PBOZ_SHOW_MOVES_BOX_LOC, &width, &height);
            draw_to_pixmap_unscaled(pic, width, height);
            return;
        case EXIT_BOX:
            get_image_size(PBOZ_EXIT_BOX_LOC, &width, &height);
            draw_to_pixmap_unscaled(pic, width, height);
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

    internal_call(pack_routine(zr.DRAW_PEGS)); // <DRAW-PEGS>

    get_image_size(PBOZ_RESTART_BOX_LOC, &width, &height);
    if (get_global(zg.PEG_MOVE_NUMBER) > 0) {
        draw_to_pixmap_unscaled(RESTART_BOX, width, height);
    } else {
        draw_to_pixmap_unscaled(DIM_RESTART_BOX, width, height);
    }
    get_image_size(PBOZ_SHOW_MOVES_BOX_LOC, &width, &height);
    if (get_global(zg.PEG_MOVE_NUMBER) > 0) {
        draw_to_pixmap_unscaled(SHOW_MOVES_BOX, width, height);
    } else {
        draw_to_pixmap_unscaled(DIM_SHOW_MOVES_BOX, width, height);
    }
    get_image_size(PBOZ_EXIT_BOX_LOC, &width, &height);
    draw_to_pixmap_unscaled(EXIT_BOX, width, height);

    adjust_text_window_by_split(PBOZ_SPLIT);
    ADJUST_TEXT_WINDOW(PBOZ_BOTTOM);
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

// Currently selected/highlighted peg position (0 = none, even index into
// BOARD_TABLE for the peg's coordinates)
static uint16_t selected_peg_pos = 0;

// When true, auto-solves Peggleboz by faking input to win immediately
static bool skip_pboz = false;

// Z-machine entry point: initializes the Peggleboz game. Hides the status
// bar, sets attributes on game objects, draws the board, and offers
// auto-solve for accessibility.
void SETUP_PBOZ(void) {
    z0_hide_right_status();

    if (V6_STATUS_WINDOW.id != nullptr) {
        v6_define_window(&V6_STATUS_WINDOW, 0, 0, 0, 0);
    }

    // (Note that the below does not apply to release 242)
    // What does this do, exactly? Attribute 7 is checked in PBOZ-WIN-CHECK:
    // Game is considered won if the first 21 attributes (1-21) of NOT-HERE-OBJECT are set
    // except attribute 7, which must not be set
    internal_set_attr(zo.NOT_HERE_OBJECT, 7);
    zp.TOUCHBIT = 0x20;
    internal_set_attr(zo.PBOZ_OBJECT, zp.TOUCHBIT); // 0x20 is TOUCHBIT
    set_global(zg.PEG_MOVE_NUMBER, 0);
    selected_peg_pos = 0;
    draw_peggles();
    flush_image_buffer();

    if (autosolve_visual_puzzle()) {
        transcribe_and_print_string("\n");
        skip_pboz = true;
        transcribe_and_print_string("Press any key twice to win.\n");
    }

}

// Hit-tests a mouse click for Peggleboz. Checks UI buttons first
// (Restart='Z', Show Moves='Y', Exit='X'), then peg positions
// (returns ASCII 65+ for peg index). Beeps if nothing was hit.
static uint16_t pboz_click(void) {
    int left, top, width, height;
    get_image_size(PBOZ_RESTART_BOX_LOC, &left, &top);
    get_image_size(RESTART_BOX, &width, &height);
    if (within(left, top, width, height)) {
        return 'Z';
    }

    get_image_size(PBOZ_SHOW_MOVES_BOX_LOC, &left, &top);
    get_image_size(SHOW_MOVES_BOX, &width, &height);
    if (within(left, top, width, height)) {
        return 'Y';
    }

    get_image_size(PBOZ_EXIT_BOX_LOC, &left, &top);
    get_image_size(EXIT_BOX, &width, &height);
    if (within(left, top, width, height)) {
        return 'X';
    }

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

// Z-machine entry point: records the selected peg position (variable 2
// is the peg index, doubled for BOARD_TABLE indexing).
void PEG_GAME(void) {
    selected_peg_pos = variable(2) * 2;
}

// Z-machine entry point: intercepts character input during auto-solve.
// If skip_pboz is set, returns 'Q' (quit) on the first call and 'G' (go)
// on subsequent calls to quickly win the game.
void PEG_GAME_READ_CHAR(void) {
    if (skip_pboz) {
        if (selected_peg_pos == 0)
            store_variable(3, 'Q');
        else
            store_variable(3, 'G');
    }
}


// Z-machine entry point: during auto-solve, sets attributes 1-21 (except 7)
// on NOT-HERE-OBJECT to fake a winning board state.
void PBOZ_WIN_CHECK(void) {
    // Each of the first 21 attributes of NOT-HERE-OBJECT represents a peg on the board.
    // A set attribute means the peg is out-of-play. We simulate a winning state by
    // setting all of them and then clearing one (number 7.)
    if (skip_pboz) {
        for (int i = 1; i < 22; i++) {
            internal_set_attr(zo.NOT_HERE_OBJECT, i);
        }
        internal_clear_attr(zo.NOT_HERE_OBJECT, 7);
        skip_pboz = false;
    }
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

// Clears a peg table (6 entries), removing all weights from that peg.
static void clear_peg_table(uint16_t table) {
    for (int i = 0; i < 6; i++) {
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
            fprintf(stderr, "ERROR!\n");
            return;
    }

    internal_insert(zo.SIX_WEIGHT, peg);
    internal_insert(zo.FIVE_WEIGHT, peg);
    internal_insert(zo.FOUR_WEIGHT, peg);
    internal_insert(zo.THREE_WEIGHT, peg);
    internal_insert(zo.TWO_WEIGHT, peg);
    internal_insert(zo.ONE_WEIGHT, peg);

    clear_peg_table(zt.LEFT_PEG_TABLE);
    clear_peg_table(zt.CENTER_PEG_TABLE);
    clear_peg_table(zt.RIGHT_PEG_TABLE);

    store_word(table, zo.SIX_WEIGHT);
    store_word(table + 2, zo.FIVE_WEIGHT);
    store_word(table + 4, zo.FOUR_WEIGHT);
    store_word(table + 6, zo.THREE_WEIGHT);
    store_word(table + 8, zo.TWO_WEIGHT);
    store_word(table + 10,zo.ONE_WEIGHT);

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
    uint16_t result = internal_call(pack_routine(zr.TOWER_WIN_CHECK));
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
    } else if (gli_voiceover_on) {
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
    for (int i = 0; i < 6; i++) {
        uint16_t wgt = user_word(table + i * 2);
        if (wgt == 0)
            return;
        uint16_t pic = internal_call(pack_routine(zr.SET_B_PIC), {wgt}); //  <SET-B-PIC WGT>
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

    int width, height;
    get_image_size(B_1_L_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL + 5 * 2, height);
    store_word(zt.B_X_TBL, width);

    get_image_size(B_2_C_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL + 4 * 2, height);
    store_word(zt.B_X_TBL + 2, width);

    get_image_size(B_3_R_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL + 3 * 2, height);
    store_word(zt.B_X_TBL + 2 * 2, width);

    get_image_size(B_4_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL + 2 * 2, height);

    get_image_size(B_5_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL + 2, height);

    get_image_size(B_6_PIC_LOC, &width, &height);
    store_word(zt.B_Y_TBL,  height);

    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(zorkzero_tower_of_bozbar_border, 0, 0);
    draw_peg(zt.LEFT_PEG_TABLE, 0);
    draw_peg(zt.CENTER_PEG_TABLE, 1);
    draw_peg(zt.RIGHT_PEG_TABLE, 2);

    get_image_size(TOWER_UNDO_BOX_LOC, &width, &height);
    if (undoing || internal_call(pack_routine(zr.TOWER_WIN_CHECK))) {
        draw_to_pixmap_unscaled(DIM_UNDO_BOX, width, height);
    } else {
        draw_to_pixmap_unscaled(UNDO_BOX, width, height);
    }

    get_image_size(TOWER_EXIT_BOX_LOC, &width, &height);
    draw_to_pixmap_unscaled(EXIT_BOX, width, height);
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
uint8_t pick_weight(void) {
    int left, top, width, height, weight_width, weight_height, num;
    get_image_size(TOWER_UNDO_BOX_LOC, &left, &top);
    get_image_size(UNDO_BOX, &width, &height);
    if (within(left, top, width, height))
        return 'U';
    get_image_size(TOWER_EXIT_BOX_LOC, &left, &top);
    get_image_size(EXIT_BOX, &width, &height);
    if (within(left, top, width, height))
        return 'X';
    get_image_size(B_1_WEIGHT, &weight_width, &weight_height);
    weight_height++;
    uint16_t tbl = zt.LEFT_PEG_TABLE;
    for (int cnt_x = 0; cnt_x < 3; cnt_x++) {
        for (int cnt_y = 0; cnt_y < 6; cnt_y++) {
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

#pragma mark - Map

// Updates the blinking location indicator's coordinates in the BLINK-TBL
// and the local variables used by the BLINK routine. Also flushes the
// image buffer and restarts the timer to trigger the next blink cycle.
static void update_blink_coordinates(uint16_t x, uint16_t y) {
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

// Z-machine entry point: called each iteration of the map interaction loop.
// Fixes a bug where resizing the screen during map mode and then clicking
// outside an eligible room would draw the blinking indicator at stale
// coordinates. We recalculate the current room's map position and update
// the local variables every iteration.
void V_MAP_LOOP(void) {
    // Bail if the map plumbing wasn't located for this revision: P_MAP_LOC == 0
    // would make internal_get_prop abort ("invalid property: 0"), and
    // MAP_X/MAP_Y == 0 would pack_routine(0) into a garbage routine. Without the
    // "you are here" coordinates the map still draws (sans marker) rather than
    // crashing.
    if (zp.P_MAP_LOC == 0 || zr.MAP_X == 0 || zr.MAP_Y == 0)
        return;

    // Get the map location table of current room
    uint16_t TBL = internal_get_prop(get_global(zg.HERE), zp.P_MAP_LOC);
    // Get the coordinates of the map representation of current room
    uint16_t CY = internal_call(pack_routine(zr.MAP_Y), {user_word(TBL + 2)}); // <MAP-Y <ZGET .TBL 1>>
    uint16_t CX = internal_call(pack_routine(zr.MAP_X), {user_word(TBL + 4)}); // <MAP-X <ZGET .TBL 2>>

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

// Redraw the active mini-game (Tower of Bozbar, Peggleboz, Snarfem, or
// Double Fanucci) for the current `CURRENT_SPLIT`. Returns true if a
// mini-game was matched and redrawn; false otherwise so the caller can
// fall through to the normal-mode path.
static bool redraw_minigame_on_resize(uint16_t current_split) {
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
    store_byte(zr.V_MAP_BLINK_CALL, 0xb0); // rtrue: draw the map, then return
    internal_call(pack_routine(zr.V_MAP), {});
    store_byte(zr.V_MAP_BLINK_CALL, saved);

    flush_image_buffer();

    // The map is now composited at the live scale, so the marker (drawn live by
    // the blink loop) lands correctly without the map_entry_imagescale override.
    map_entry_imagescalex = imagescalex;
    map_entry_imagescaley = imagescaley;
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
        internal_call(pack_routine(zr.SETUP_SCREEN));

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

        case MODE_MAP: {
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
                if (zr.MAP_X != 0 && zr.MAP_Y != 0 && zp.P_MAP_LOC != 0) {
                    uint16_t TBL = internal_get_prop(get_global(zg.HERE), zp.P_MAP_LOC);
                    uint16_t CY = internal_call(pack_routine(zr.MAP_Y), {user_word(TBL + 2)});
                    uint16_t CX = internal_call(pack_routine(zr.MAP_X), {user_word(TBL + 4)});
                    update_blink_coordinates(CX, CY);
                }

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
                internal_call(pack_routine(zr.V_REFRESH), {1}); // V-$REFRESH(DONT-CLEAR:true)

                // We are in a loop inside the BLINK routine and need to
                // update coordinates in the BLINK-TBL (which will be read by
                // the TYPED? routine) as well as the local Y and X variables
                // (which are used to redraw/unhighlight the previous
                // location icon when we move away).
                uint16_t TBL = internal_get_prop(get_global(zg.HERE), zp.P_MAP_LOC);
                uint16_t CY = internal_call(pack_routine(zr.MAP_Y), {user_word(TBL + 2)});
                uint16_t CX = internal_call(pack_routine(zr.MAP_X), {user_word(TBL + 4)});
                update_blink_coordinates(CX, CY);
            }
            return;
        }

        case MODE_NORMAL:
        case MODE_Z0_GAME:
            if (redraw_minigame_on_resize(get_global(zg.CURRENT_SPLIT)))
                return;

            // Skip when V-REFRESH wasn't located (e.g. r366): pack_routine(0)
            // underflows to a bogus packed address that unpacks into string
            // space, and calling it crashes (most visibly on autorestore,
            // which reaches here via after_V_COLOR()).
            if (zr.V_REFRESH != 0)
                internal_call(pack_routine(zr.V_REFRESH), {1}); // V-$REFRESH(DONT-CLEAR:true)
            if (V6_TEXT_BUFFER_WINDOW.id) {
                if (graphics_type_changed) {
                    refresh_margin_images();
                    float yscalefactor = 2.0;
                    if (graphics_type == kGraphicsTypeMacBW)
                        yscalefactor = imagescalex;
                    float xscalefactor = yscalefactor * pixelwidth;
                    win_refresh(V6_TEXT_BUFFER_WINDOW.id->peer, xscalefactor, yscalefactor);
                }
                win_setbgnd(V6_TEXT_BUFFER_WINDOW.id->peer, user_selected_background);
            }
            break;

        case MODE_SLIDESHOW:
            // We are showing a fullscreen image.
            v6_define_window(&V6_TEXT_BUFFER_WINDOW, 1, 1, gscreenw, gscreenh);
            win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
            glk_window_clear(current_graphics_buf_win);
            if (is_zorkzero_encyclopedia_image(current_picture)) {
                draw_encyclopedia();
                return;
            } else if (current_picture == zorkzero_title_image) {
                glk_window_set_background_color(graphics_fg_glk, graphics_type == kGraphicsTypeApple2 ? 0x101B9C : 0);
                glk_window_clear(graphics_fg_glk);
                draw_centered_title_image(current_picture);
            } else if (is_zorkzero_rebus_image(current_picture)) {
                clear_image_buffer();
                ensure_pixmap(current_graphics_buf_win);
                draw_to_pixmap_unscaled(current_picture, 0, 0);
                flush_bitmap(current_graphics_buf_win);
                return;
            }
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
//    blank it here. A restored caption window (windows[3]) in slideshow mode
//    uniquely identifies the encyclopedia (title/rebus never create it).
//
// 2. Peggleboz: if we were in Peggleboz with a peg selected, the C++ static
//    selected_peg has been reset to 0 by the process restart, so the resize
//    handler no longer knows to restart the blink timer. Recover
//    selected_peg_pos by matching the BLINK-TBL coordinates (preserved as
//    Z-machine state) against the BOARD-TABLE peg positions, then restart the
//    blink timer.
void z0_update_after_autorestore(void) {
    if (screenmode == MODE_SLIDESHOW && windows[3].id != nullptr) {
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
    // colours. Repopulate the window colours from the restored globals,
    // matching the struct assignment after_V_COLOR() performs on a manual
    // restore (including the status window's default background), as
    // arthur_update_after_autorestore()/shogun_update_after_autorestore() do
    // for their games. We deliberately do not call after_V_COLOR() /
    // z0_update_on_resize() here: those take mode-specific redraw paths that
    // can crash during autorestore (see the screenmode guard in
    // z0_update_after_restore).
    update_user_defined_colours();
    uint8_t fg = get_global(fg_global_idx);
    uint8_t bg = get_global(bg_global_idx);
    for (auto &window : windows) {
        window.fg_color = Color(Color::Mode::ANSI, fg);
        if (&window == &V6_STATUS_WINDOW) {
            window.bg_color = Color();
        } else {
            window.bg_color = Color(Color::Mode::ANSI, bg);
        }

        // The Glk window backgrounds are not part of the autosave either, so
        // the text-buffer window keeps the host theme's default (e.g. black
        // under Lectrote Dark) unless we repaint it, as after_V_COLOR() does
        // on a manual restore. Normal output stays readable because each glyph
        // carries the game's background as a per-glyph zcolor, but echoed input
        // text uses the window background, so without this it renders in the
        // game's foreground on the theme background (black on black).
        winid_t glkwin = window.id;
        if (glkwin != nullptr && glkwin->type == wintype_TextBuffer) {
            win_setbgnd(glkwin->peer, user_selected_background);
        }
    }

    if ((screenmode != MODE_NORMAL && screenmode != MODE_Z0_GAME)
        || get_global(zg.CURRENT_SPLIT) != PBOZ_SPLIT)
        return;

    uint16_t y = user_word(get_global(zg.BLINK_TBL) + 3 * 2);
    uint16_t x = user_word(get_global(zg.BLINK_TBL) + 4 * 2);
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

extern bool pending_flowbreak;

#pragma mark - Image Display Dispatch

void z0_image_description(int picnum) {
    if (gli_voiceover_on) {
        glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
        const char *message;
        switch (picnum) {
            case zorkzero_title_image:
                message = "\n(Showing Zork Zero full-screen title image. ZORK ZERO written in stone masonry with the subtitle The Revenge of Mageboz on a banner below with a wizard's hat balanced on the edge of the banner.)\n";
                break;
            case zorkzero_map_border:
                message = "\n(Showing a full screen graphical map where the locations are represented as interconnected boxes. There is a compass rose in the style of a sigil stamp at the top. Some of the room boxes have little icons and other decorations.)\n";
                break;
            default:
                if (is_zorkzero_rebus_image(picnum))
                    message = "\n(Showing a full screen rebus image, just as in the room description.)\n";
                else
                    message = "\n(Showing a full screen image.)\n";
                break;
        }
        glk_put_string_stream(glk_window_get_stream(V6_TEXT_BUFFER_WINDOW.id), const_cast<char*>(message));
    }

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
        z0_draw_peggleboz_box_image(current_picture, x, y);
        return true;
    }

    // Fullscreen images: title screen, encyclopedia border, map border,
    // and the first rebus draw of a session each take over the foreground
    // graphics window.
    if (is_zorkzero_fullscreen_picture(current_picture)) {
        // First time the player sees a rebus, nudge them toward the
        // built-in InvisiClues. We print into the text buffer while it
        // is still the active window (we're still in MODE_NORMAL here),
        // so the message is waiting in the scrollback once the player
        // dismisses the fullscreen image.
        if (gli_voiceover_on && is_zorkzero_rebus_image(current_picture) && !shown_rebus_hint_message) {
            shown_rebus_hint_message = true;
            should_show_rebus_hint_message = true;
            z0_image_description(current_picture);
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
            z0_image_description(zorkzero_map_border);
            return true;
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
            windows[3].id = gli_new_window(wintype_TextBuffer, 0);
            draw_to_pixmap_unscaled(zorkzero_encyclopedia_border, 0, 0);
            image_needs_redraw = true;
            adjust_encyclopedia_text_window();
            return true;
        }

        if (current_picture == zorkzero_title_image) {
            glk_window_set_background_color(graphics_fg_glk, graphics_type == kGraphicsTypeApple2 ? 0x101B9C : 0);
            glk_window_clear(current_graphics_buf_win);
            draw_centered_title_image(current_picture);
            glk_window_set_background_color(graphics_fg_glk, user_selected_background);
            z0_image_description(zorkzero_title_image);
            return true;
        }

        // Rebus image (the remaining case in this branch).
        draw_to_buffer(current_graphics_buf_win, current_picture, x, y);
        flush_bitmap(current_graphics_buf_win);
        return true;
    }

    // Inline image in a text buffer: drawn as a margin image so text
    // wraps around it.
    if (win->id && win->id->type == wintype_TextBuffer) {
        pending_flowbreak = true;
        float inline_scale = (graphics_type == kGraphicsTypeMacBW) ? imagescalex : 2.0;
        draw_inline_image(win->id, current_picture, imagealign_MarginLeft, current_picture, inline_scale, false);
        add_margin_image_to_list(current_picture);
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

    if (current_graphics_buf_win)
        dat->current_graphics_win_tag = current_graphics_buf_win->tag;
    if (graphics_fg_glk)
        dat->graphics_fg_tag = graphics_fg_glk->tag;
    if (stored_bufferwin)
        dat->stored_lower_tag = stored_bufferwin->tag;
    if (z0_right_status_window)
        dat->z0_right_status_tag = z0_right_status_window->tag;

    dat->z0_shown_rebus_hint_message = shown_rebus_hint_message;
}

// Restores Zork Zero's window pointers from saved tags after a restore
// operation. Looks up the current window objects by their stable tags.
void z0_recover_state(library_state_data *dat) {
    if (!dat)
        return;

    current_graphics_buf_win = gli_window_for_tag(dat->current_graphics_win_tag);
    graphics_fg_glk = gli_window_for_tag(dat->graphics_fg_tag);
    stored_bufferwin = gli_window_for_tag(dat->stored_lower_tag);
    z0_right_status_window = gli_window_for_tag(dat->z0_right_status_tag);

    shown_rebus_hint_message = dat->z0_shown_rebus_hint_message;
}

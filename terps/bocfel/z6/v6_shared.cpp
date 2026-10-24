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
// - Credits display and color management after V-COLOR.
// - Autosave support: stashing and recovering the graphics windows, and
//   rebuilding the window colours from the game's globals.
// - MenuString, the bounded string builder behind the VoiceOver menus.
// - Graphics-off support: the opaque status window and the key press that
//   is skipped after a picture that was left out.
// - Visual puzzle skip prompt for accessibility.
// - The per-game hook tables (v6_game()).
//
// The Definitions screen is in v6_definitions.cpp and the Hints screen in
// v6_hints.cpp.

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

#pragma mark - Margin Image Tracking

// List of image numbers currently displayed as margin (inline) images in
// text buffer windows. When the graphics type changes, these images need
// to be recreated at the new resolution.
// The list is saved in the autosave, which has room for this many.
int margin_images[BOCFEL_MAX_MARGIN_IMAGES];
int number_of_margin_images = 0;

// Shifts the margin image list when full, discarding the oldest entry.
static void shift_margin_image_list(void) {
    fprintf(stderr, "Margin image list full (%d entries); forgetting the oldest margin image (%d)\n", BOCFEL_MAX_MARGIN_IMAGES, margin_images[0]);
    for (int i = 1; i < number_of_margin_images; i++) {
       margin_images[i - 1] = margin_images[i];
    }
    number_of_margin_images = BOCFEL_MAX_MARGIN_IMAGES - 1;
}

// Adds an image to the margin image list if not already present.
// Shifts the list if full.
void add_margin_image_to_list(int image) {
    for (int i = 0; i < number_of_margin_images; i++) {
        if (margin_images[i] == image) {
            // We have already added this image to the list
            return;
        }
    }
    if (number_of_margin_images >= BOCFEL_MAX_MARGIN_IMAGES) {
        shift_margin_image_list();
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

// Prints an integer to the current Glk window.
void print_number(int number) {
    char str[16];
    snprintf(str, sizeof str, "%d", number);
    glk_put_string(str);
}

// Prints a number right-justified in a 4-character field, padding with
// leading spaces. Used for score and move count display in status lines.
void print_right_justified_number(int number) {
    char str[16];
    snprintf(str, sizeof str, "%4d", number);
    glk_put_string(str);
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
// the string is shorter than 2 characters (and then `str` is empty).
int print_long_zstr_to_cstr(uint16_t addr, char *str, int maxlen) {
    int length = count_characters_in_zstring(addr);
    if (length < 2) {
        str[0] = 0;
        return 0;
    }
    string_buf_ptr = str;
    string_buf_pos = 0;
    string_maxlen = maxlen;
    print_handler(unpack_string(addr), print_to_string_buffer);
    // Terminate at the number of characters actually written (the callback
    // caps writes at maxlen), never at the untruncated decoded length --
    // otherwise a string >= maxlen writes the NUL past the caller's buffer.
    if (string_buf_pos >= maxlen)
        string_buf_pos = maxlen - 1;
    str[string_buf_pos] = 0;
    return string_buf_pos;
}

// Convenience wrapper: decodes a Z-string with the default buffer size.
int print_zstr_to_cstr(uint16_t addr, char *str) {
    return print_long_zstr_to_cstr(addr, str, STRING_BUFFER_SIZE);
}

void MenuString::append_char(char c) {
    if (remaining() <= 0)
        return;
    m_buffer[m_length++] = c;
    m_buffer[m_length] = 0;
}

void MenuString::append(const char *str) {
    for (int i = 0; str[i] != 0 && remaining() > 0; i++)
        append_char(str[i]);
}

void MenuString::append_zscii(uint16_t addr, int count) {
    for (int i = 0; i < count && remaining() > 0; i++)
        append_char((char)zscii_to_unicode[user_byte(addr + i)]);
}

int MenuString::append_zstr(uint16_t addr) {
    // print_long_zstr_to_cstr() needs room for at least one character plus the
    // NUL; it terminates the buffer itself, so m_length just moves up by the
    // number of characters it reports writing.
    if (remaining() < 1)
        return 0;
    int written = print_long_zstr_to_cstr(addr, m_buffer + m_length, remaining() + 1);
    m_length += written;
    return written;
}

// Autosave helpers for the window pointers that Arthur, Shogun and Zork Zero
// all keep: the current graphics buffer window, the foreground graphics
// window and the stored text buffer window, saved by their Glk tags.
void v6_stash_graphics_windows(library_state_data *dat) {
    if (current_graphics_buf_win)
        dat->current_graphics_win_tag = current_graphics_buf_win->tag;
    if (graphics_fg_glk)
        dat->graphics_fg_tag = graphics_fg_glk->tag;
    if (stored_bufferwin)
        dat->stored_lower_tag = stored_bufferwin->tag;
}

void v6_recover_graphics_windows(library_state_data *dat) {
    current_graphics_buf_win = gli_window_for_tag(dat->current_graphics_win_tag);
    graphics_fg_glk = gli_window_for_tag(dat->graphics_fg_tag);
    stored_bufferwin = gli_window_for_tag(dat->stored_lower_tag);
}

void v6_set_grid_colors(glui32 style) {
    glk_stylehint_set(wintype_TextGrid, style, stylehint_TextColor, user_selected_foreground);
    glk_stylehint_set(wintype_TextGrid, style, stylehint_BackColor, user_selected_background);
}

// Per-window colours are not part of the autosave, so after an autorestore
// they are rebuilt from the (restored) Z-machine colour globals, as
// after_V_COLOR() does on a manual restore: the status window keeps a default
// background, so that the main window background shows through.
// update_user_defined_colours() must have been called first.
void v6_set_window_colors_from_globals(void) {
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
        // a text buffer window keeps the theme's default (black under a dark
        // theme, say) unless it is repainted. Normal output stays readable,
        // because every glyph carries the game's background as a zcolor, but
        // echoed input uses the window background, so without this it is
        // drawn in the game's foreground on the theme background.
        winid_t glkwin = window.id;
        if (glkwin != nullptr && glkwin->type == wintype_TextBuffer) {
            win_setbgnd(glkwin->peer, user_selected_background);
        }
    }
}

// Scale used for pictures drawn inline in the text buffer (Shogun and
// Zork Zero): 2x the original, or the screen scale for the Mac B/W graphics,
// which are already high resolution (and have square pixels).
float v6_inline_image_scale(void) {
    if (graphics_type == kGraphicsTypeMacBW)
        return imagescalex;
    return 2.0;
}

// After the graphics type changes, redraw the margin images in the text
// buffer window at the new scale.
void v6_refresh_text_buffer_images(void) {
    refresh_margin_images();
    float yscalefactor = v6_inline_image_scale();
    float xscalefactor = yscalefactor * pixelwidth;
    win_refresh(V6_TEXT_BUFFER_WINDOW.id->peer, xscalefactor, yscalefactor);
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
    for (int i = 0; str[i] != 0; i++) {
        transcribe(str[i]);
    }
}

// Puts the game's normal screen back after the definitions or hints screen
// has taken it over. (Arthur does this by itself.)
void v6_redraw_game_screen(void) {
    if (is_spatterlight_zork0) {
        z0_update_on_resize();
    } else if (is_spatterlight_shogun) {
        call_routine(sr.V_REFRESH);
        shogun_update_on_resize();
    }
}

#pragma mark - Credits

// Z-machine entry point: called before credits text is printed.
// Ensures output goes to the main text buffer window.
//
// Shogun prints its credits inline, bracketed by HLIGHT BOLD / HLIGHT
// NORMAL; those @set_text_style operands are rewritten to bold+fixed /
// bold+italic+fixed during entrypoint resolution so the text routes
// through the centered styles style_User1 / style_Note. Zork Zero prints
// its credits via the CENTER-1/2/3 routines instead, which are
// reimplemented directly (see CENTER_1/2/3 in zorkzero.cpp) to print
// each line through style_User1, so nothing release-specific is needed
// here for it.
void V_CREDITS(void) {
    glk_set_window(V6_TEXT_BUFFER_WINDOW.id);
}

// Z-machine entry point: called after credits text. Resets bold, italic,
// and fixed-width text styles that may have been set during credit display.
// For Zork Zero, also restores the default (proportional) hint on
// style_User1 and style_Note that V_CREDITS overrode.
void after_V_CREDITS(void) {
    if (is_spatterlight_zork0) {
        glk_stylehint_clear(wintype_TextBuffer, style_User1, stylehint_Proportional);
        glk_stylehint_clear(wintype_TextBuffer, style_Note, stylehint_Proportional);
        win_refresh(V6_TEXT_BUFFER_WINDOW.id->peer, 0, 0);
    }
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
    } else if (is_spatterlight_zork0) {
        z0_update_on_resize();
    }
}


#pragma mark - Graphics Off

bool v6_graphics_off(void) {
    return gli_enable_graphics == 0;
}

bool v6_pictures_unseen(void) {
    return gli_voiceover_on || v6_graphics_off();
}

// In Shogun and Zork Zero the status window is transparent, and the bar
// behind it is part of the border picture. With graphics off there is no
// picture, so an opaque window takes its place and supplies the bar itself.
// (A window can't be made opaque again, and the original belongs to the Glk
// window tree and can't be deleted, so it is kept and shrunk to nothing
// meanwhile.) The windows are found by rock, which also works after an
// autorestore.
constexpr int V6_STATUS_ROCK = 2; // As opened in init_screen()
constexpr int V6_OPAQUE_STATUS_ROCK = 0x5636534f; // 'V6SO'

winid_t v6_grid_window_with_rock(glui32 wanted) {
    glui32 rock;
    for (winid_t win = glk_window_iterate(nullptr, &rock); win != nullptr; win = glk_window_iterate(win, &rock)) {
        if (rock == wanted && win->type == wintype_TextGrid)
            return win;
    }
    return nullptr;
}

void v6_match_status_window_to_graphics_setting(Window *status) {
    bool want_opaque = v6_graphics_off();
    winid_t transparent = v6_grid_window_with_rock(V6_STATUS_ROCK);
    winid_t opaque = v6_grid_window_with_rock(V6_OPAQUE_STATUS_ROCK);
    if (want_opaque && opaque == nullptr)
        opaque = gli_new_window(wintype_TextGrid, V6_OPAQUE_STATUS_ROCK);
    winid_t wanted = want_opaque ? opaque : transparent;
    winid_t other = want_opaque ? transparent : opaque;
    if (wanted == nullptr || status->id == wanted)
        return;
    status->id = wanted;
    if (other != nullptr)
        win_sizewin(other->peer, 0, 0, 0, 0);
}

bool v6_skip_keypress = false;

uint16_t call_routine(uint32_t address, std::vector<uint16_t> args) {
    return internal_call(pack_routine(address), std::move(args));
}

// A full-screen picture hides the text window. With VoiceOver on, replace the
// text with a description of what is being shown.
void v6_describe_fullscreen_image(const char *message) {
    if (!gli_voiceover_on)
        return;
    glk_window_clear(V6_TEXT_BUFFER_WINDOW.id);
    glk_put_string_stream(glk_window_get_stream(V6_TEXT_BUFFER_WINDOW.id), const_cast<char *>(message));
}

void v6_skip_next_keypress(void) {
    v6_skip_keypress = true;
}

#pragma mark - Skip Puzzles Prompt

// Prompts the user to skip a visual puzzle for accessibility. Only shows
// the prompt when VoiceOver is active, graphics are off, or when autorestoring to a point
// where the prompt was previously shown. Returns true if the user
// accepts (types 'Y'), false if they decline ('N') or if the prompt
// was not shown.
bool skip_puzzle_prompt(const char *str) {
    if (v6_pictures_unseen() || dont_repeat_question_on_autorestore) {
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

#pragma mark - Per-game Hooks

static void no_op(void) {}
static void no_op_state(library_state_data *) {}

static const V6GameHooks no_game_hooks = {
    no_op_state, no_op_state, no_op, no_op, no_op, no_op, no_op
};

static const V6GameHooks arthur_hooks = {
    arthur_stash_state,
    arthur_recover_state,
    arthur_update_after_restore,
    arthur_update_after_autorestore,
    arthur_update_after_restart,
    arthur_update_on_resize,
    no_op,
};

static const V6GameHooks journey_hooks = {
    journey_stash_state,
    journey_recover_state,
    journey_update_after_restore,
    no_op, // autorestore: the whole screen is rebuilt on the first input
    no_op,
    journey_update_on_resize,
    journey_autorestore_internal_read_char_hacks,
};

static const V6GameHooks shogun_hooks = {
    shogun_stash_state,
    shogun_recover_state,
    shogun_update_after_restore,
    shogun_update_after_autorestore,
    shogun_update_after_restart,
    shogun_update_on_resize,
    shogun_autorestore_internal_read_char_hacks,
};

static const V6GameHooks z0_hooks = {
    z0_stash_state,
    z0_recover_state,
    z0_update_after_restore,
    z0_update_after_autorestore,
    z0_update_colors, // after a restart
    z0_update_on_resize,
    no_op,
};

const V6GameHooks &v6_game(void) {
    if (is_spatterlight_arthur)
        return arthur_hooks;
    if (is_spatterlight_journey)
        return journey_hooks;
    if (is_spatterlight_shogun)
        return shogun_hooks;
    if (is_spatterlight_zork0)
        return z0_hooks;
    return no_game_hooks;
}

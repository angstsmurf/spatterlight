//
//  v6_shared.hpp
//  bocfel6
//
//  Created by Administrator on 2024-06-09.
//

#ifndef v6_shared_hpp
#define v6_shared_hpp

extern "C" {
#include "glk.h"
#include "glkimp.h"
#include "spatterlight-autosave.h"
}

#include <cstddef>
#include <cstdint>
#include <vector>

void adjust_definitions_window(void);
void redraw_hint_screen_on_resize(void);
void print_number(int number);
void print_right_justified_number(int number);

extern int number_of_margin_images;
extern int margin_images[BOCFEL_MAX_MARGIN_IMAGES];

void add_margin_image_to_list(int image);
void clear_margin_image_list(void);
void refresh_margin_images(void);
void v6_close_and_reopen_front_graphics_window(void);
void v6_stash_graphics_windows(library_state_data *dat);
void v6_recover_graphics_windows(library_state_data *dat);
void v6_set_window_colors_from_globals(void);
// Style hints: text grids opened from now on get the user's colours in this style.
void v6_set_grid_colors(glui32 style);
float v6_inline_image_scale(void);
void v6_refresh_text_buffer_images(void);
void transcribe_and_print_string(const char *str);
bool skip_puzzle_prompt(const char *str);

// True when the user has switched graphics off in the settings. The picture
// data stays loaded -- the games size their windows from it -- but nothing is
// drawn, so each game falls back on a text-only layout while this holds.
bool v6_graphics_off(void);
// True for players who can't see the pictures: VoiceOver or graphics off.
// Gates the picture descriptions and the offers to skip visual puzzles.
bool v6_pictures_unseen(void);
// Swaps in an opaque status window while graphics are off, and the
// transparent one back when they are on. Call before sizing or printing.
void v6_match_status_window_to_graphics_setting(Window *status);
// Make the next @read_char return at once: used for the "press any key"
// after a full-screen picture that was left out.
void v6_skip_next_keypress(void);

// Calls a game routine found by find_entrypoints(), which records them as
// byte addresses.
uint16_t call_routine(uint32_t address, std::vector<uint16_t> args = {});
void v6_describe_fullscreen_image(const char *message);
extern bool v6_skip_keypress;

void DO_HINTS(void);

void after_V_COLOR(void);

void V_DEFINE(void);

void V_CREDITS(void);
void after_V_CREDITS(void);

int print_zstr_to_cstr(uint16_t addr, char *str);
int print_long_zstr_to_cstr(uint16_t addr, char *str, int maxlen);

// Bounded builder for the fixed-size C strings that the VoiceOver menus hand to
// win_menuitem(). Everything appended is game-supplied — a decoded Z-string, or
// a run of ZSCII bytes whose length is a byte or a global in the story file — so
// each append truncates at the buffer's capacity instead of running past it, and
// the buffer is left NUL-terminated. Build over the caller's buffer:
//
//     char str[60];
//     MenuString menu_string(str, sizeof(str));
//     menu_string.append_zstr(user_word(fdef));
//     win_menuitem(kV6MenuTypeDefine, index, 18, 0, str, menu_string.length());
class MenuString {
public:
    MenuString(char *buffer, size_t capacity) : m_buffer(buffer), m_capacity(capacity) {
        if (m_capacity > 0)
            m_buffer[0] = 0;
    }

    // Characters written so far, not counting the terminating NUL. This is the
    // length to pass to win_menuitem(): it always matches what is in the buffer.
    int length() const { return m_length; }

    // Characters that can still be appended before truncation kicks in.
    int remaining() const { return (int)m_capacity - 1 - m_length; }

    void append_char(char c);
    void append(const char *str);

    // Appends `count` raw ZSCII bytes read from Z-machine memory at `addr`.
    void append_zscii(uint16_t addr, int count);

    // Decodes the Z-string at `addr` and appends it. Returns the number of
    // characters actually appended.
    int append_zstr(uint16_t addr);

private:
    char *m_buffer;
    size_t m_capacity;
    int m_length = 0;
};

extern uint8_t fg_global_idx, bg_global_idx;

extern uint8_t hint_chapter_global_idx, hint_quest_global_idx;
extern uint16_t hints_table_addr;
extern uint16_t seen_hints_table_addr;

extern winid_t stored_bufferwin;

extern uint16_t fkeys_table_addr;
extern uint16_t fnames_table_addr;
extern int global_define_line;

extern InfocomV6MenuType hints_depth;

#define V6_TEXT_BUFFER_WINDOW windows[0]
#define V6_STATUS_WINDOW windows[1]
#define V6_GRAPHICS_BG windows[7]

constexpr int STRING_BUFFER_SIZE = 15;

// Room for a hint topic or question title and its terminator
constexpr int kHintTitleMax = 64;

constexpr int kRGBWhite = 0xffffff;

// Flat colours standing in for the EGA hint border's dithered patterns
constexpr int ROSE_TAUPE = 0x836767;
constexpr int HINT_MAUVE = 0xd47fd4;

#endif /* v6_shared_hpp */

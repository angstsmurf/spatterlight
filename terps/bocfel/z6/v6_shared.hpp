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
}

#include <stdio.h>

void adjust_definitions_window(void);
void redraw_hint_screen_on_resize(void);
void print_number(int number);
void print_right_justified_number(int number);

extern int number_of_margin_images;
extern int margin_images[100];

void add_margin_image_to_list(int image);
void clear_margin_image_list(void);
void refresh_margin_images(void);
void v6_close_and_reopen_front_graphics_window(void);
void transcribe_and_print_string(const char *str);
bool skip_puzzle_prompt(const char *str);

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

#define STRING_BUFFER_SIZE 15

#define ROSE_TAUPE 0x836767
#define BROWN 0xd47fd4
#endif /* v6_shared_hpp */

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
void update_monochrome_colours(void);

void DO_HINTS(void);
void DISPLAY_HINT(void);
void RT_SEE_QST(void);

void V_COLOR(void);
void after_V_COLOR(void);

void V_DEFINE(void);

void V_CREDITS(void);
void after_V_CREDITS(void);

int print_zstr_to_cstr(uint16_t addr, char *str);
int print_long_zstr_to_cstr(uint16_t addr, char *str, int maxlen);

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

#endif /* v6_shared_hpp */

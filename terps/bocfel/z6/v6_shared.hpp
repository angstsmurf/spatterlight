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

void add_margin_image_to_list(int image);
void clear_margin_image_list(void);
void refresh_margin_images(void);

void DO_HINTS(void);

extern uint8_t fg_global_idx, bg_global_idx;

void V_COLOR(void);
void after_V_COLOR(void);

#define V6_TEXT_BUFFER_WINDOW windows[0]
#define V6_STATUS_WINDOW windows[1]

#endif /* v6_shared_hpp */

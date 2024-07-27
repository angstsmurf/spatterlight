//
//  journey.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

#ifndef journey_hpp
#define journey_hpp

#include <stdio.h>
void journey_adjust_image(int picnum, int *x, int *y, int width, int height, int winwidth, int winheight, float *scale, float pixelwidth);

void journey_update_on_resize(void);
void journey_update_after_restore(void);

void journey_draw_cursor(void);

void move_v6_cursor(int column, int line);

// Journey
void after_INTRO(void);
void REFRESH_SCREEN(void);
void INIT_SCREEN(void);
void DIVIDER(void);
void WCENTER(void);
void BOLD_CURSOR(void);
void PRINT_COLUMNS(void);
void PRINT_CHARACTER_COMMANDS(void);
void REFRESH_CHARACTER_COMMAND_AREA(void);
void GRAPHIC_STAMP(void);
void READ_ELVISH(void);
void CHANGE_NAME(void);
void ERASE_COMMAND(void);

extern int16_t selected_journey_line;
extern int16_t selected_journey_column;

extern uint32_t init_screen_address;
extern uint32_t refresh_screen_address;
extern uint8_t global_text_window_left_idx;


#endif /* journey_hpp */

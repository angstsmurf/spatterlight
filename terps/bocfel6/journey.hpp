//
//  journey.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

#ifndef journey_hpp
#define journey_hpp

#include <stdio.h>
void adjust_journey_image(int picnum, int *x, int *y, int width, int height, int winwidth, int winheight, float *scale, float pixelwidth);
void draw_journey_stamp_image(winid_t win, int16_t picnum, int16_t where, float pixelwidth);

void update_journey_on_resize(void);
void resize_journey_windows_after_restore(void);

// Journey
//void INTRO(void);
//void after_TITLE_PAGE(void);
void after_INTRO(void);
void REFRESH_SCREEN(void);
void INIT_SCREEN(void);
void DIVIDER(void);
void WCENTER(void);
void BOLD_CURSOR(void);
//void BOLD_PARTY_CURSOR(void);
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

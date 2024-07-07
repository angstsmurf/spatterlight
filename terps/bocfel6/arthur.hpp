//
//  arthur.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

#ifndef arthur_hpp
#define arthur_hpp

#include <stdio.h>
bool is_arthur_room_image(int picnum);
bool is_arthur_stamp_image(int picnum);
bool is_arthur_map_image(int picnum);

void adjust_arthur_room_image(int picnum, int width, int height, int *x, int *y);
void adjust_arthur_top_margin(void);
void draw_arthur_room_image(int picnum);
void draw_arthur_map_image(int picnum, int x, int y);
void draw_arthur_title_image(int picnum);
void arthur_window_adjustments(void);
void adjust_arthur_windows(void);



void V_COLOR(void);
void after_V_COLOR(void);

void RT_UPDATE_PICT_WINDOW(void);
void RT_UPDATE_INVT_WINDOW(void);
void RT_UPDATE_STAT_WINDOW(void);
void RT_UPDATE_MAP_WINDOW(void);

void ARTHUR_UPDATE_STATUS_LINE(void);

bool arthur_display_picture(glsi32 x, glsi32 y);

extern int arthur_text_top_margin;
extern uint8_t global_map_grid_y_idx;

#endif /* arthur_hpp */

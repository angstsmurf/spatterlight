//
//  arthur.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

#ifndef arthur_hpp
#define arthur_hpp

#include <stdio.h>
bool is_arthur_map_image(int picnum);

void adjust_arthur_top_margin(void);
void arthur_update_on_resize(void);
void arthur_adjust_windows(void);
void arthur_toggle_slideshow_windows(void);
void arthur_change_current_window(void);
void arthur_hotkeys(uint8_t key);

void RT_UPDATE_PICT_WINDOW(void);
void RT_UPDATE_INVT_WINDOW(void);
void RT_UPDATE_STAT_WINDOW(void);
void RT_UPDATE_MAP_WINDOW(void);

void ARTHUR_UPDATE_STATUS_LINE(void);
void INIT_STATUS_LINE(void);
void after_INIT_STATUS_LINE(void);

bool arthur_display_picture(glui32 picnum, glsi32 x, glsi32 y);
void arthur_draw_room_image(int picnum);

extern int arthur_text_top_margin;

typedef struct ArthurGlobals {
    uint8_t UPDATE; // G0e•
    uint8_t NAME_COLUMN; // Ga3•
    uint8_t GL_MAP_GRID_Y; // Ga3•
    uint8_t GL_WINDOW_TYPE; // Ga3•
    uint8_t GL_AUTHOR_SIZE;
    uint8_t WINDOW_2_X;
    uint8_t WINDOW_2_Y;
    uint8_t GL_TIME_WIDTH;
    uint8_t GL_SL_HERE;
    uint8_t GL_SL_VEH;
    uint8_t GL_SL_HIDE;
    uint8_t GL_SL_TIME;
    uint8_t GL_SL_FORM;
} ArthurGlobals;

extern ArthurGlobals ag;

typedef struct ArthurRoutines {
    uint32_t UPDATE_STATUS_LINE; //
    uint32_t RT_UPDATE_PICT_WINDOW; // •
    uint32_t RT_UPDATE_INVT_WINDOW; // •
    uint32_t RT_UPDATE_STAT_WINDOW; // 0x1951c•
    uint32_t RT_UPDATE_MAP_WINDOW; // 0x78a0•
    uint32_t RT_UPDATE_DESC_WINDOW; // 0x78a0•
    uint32_t REFRESH_SCREEN; // 0x676c•
} ArthurRoutines;

extern ArthurRoutines ar;

typedef struct ArthurTables {
    uint16_t K_DIROUT_TBL;
    uint16_t K_HINT_ITEMS;
} ArthurTables;

extern ArthurTables at;

#endif /* arthur_hpp */

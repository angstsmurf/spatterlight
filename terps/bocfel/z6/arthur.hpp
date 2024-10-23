//
//  arthur.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

#ifndef arthur_hpp
#define arthur_hpp

extern "C" {
#include "glk.h"
#include "glkimp.h"
#include "spatterlight-autosave.h"
}

#include <stdio.h>
bool is_arthur_map_image(int picnum);

void adjust_arthur_top_margin(void);
void arthur_update_on_resize(void);
void arthur_adjust_windows(void);
void arthur_erase_window(int16_t index);
void arthur_move_cursor(int16_t y, int16_t x, winid_t win);

void RT_UPDATE_PICT_WINDOW(void);
void RT_UPDATE_INVT_WINDOW(void);
void RT_UPDATE_STAT_WINDOW(void);
void RT_UPDATE_MAP_WINDOW(void);
void RT_UPDATE_DESC_WINDOW(void);
void RT_HOT_KEY(void);
void after_V_COLOR(void);

void ARTHUR_UPDATE_STATUS_LINE(void);
void arthur_INIT_STATUS_LINE(void);
void RT_AUTHOR_OFF(void);
void RT_TH_EXCALIBUR(void);

bool arthur_display_picture(glui32 picnum, glsi32 x, glsi32 y);
void arthur_draw_room_image(int picnum);

void arthur_update_after_restore(void);
void arthur_update_after_autorestore(void);

bool arthur_autorestore_internal_read_char_hacks(void);
void arthur_close_and_reopen_front_graphics_window(void);

void arthur_sync_screenmode(void);

void stash_arthur_state(library_state_data *dat);
void recover_arthur_state(library_state_data *dat);

typedef struct ArthurGlobals {
    uint8_t UPDATE; // G0e•
    uint8_t NAME_COLUMN; // Ga3•
    uint8_t GL_MAP_GRID_Y; // Ga3•
    uint8_t GL_WINDOW_TYPE; // Ga3•
    uint8_t GL_AUTHOR_SIZE;
    uint8_t GL_TIME_WIDTH;
    uint8_t GL_SL_HERE;
    uint8_t GL_SL_VEH;
    uint8_t GL_SL_HIDE;
    uint8_t GL_SL_TIME;
    uint8_t GL_SL_FORM;
} ArthurGlobals;

extern ArthurGlobals ag;

typedef struct ArthurRoutines {
    uint32_t UPDATE_STATUS_LINE;
    uint32_t RT_UPDATE_PICT_WINDOW;
    uint32_t RT_UPDATE_INVT_WINDOW;
    uint32_t RT_UPDATE_STAT_WINDOW;
    uint32_t RT_UPDATE_MAP_WINDOW;
    uint32_t RT_UPDATE_DESC_WINDOW;
    uint32_t RT_SEE_QST;
} ArthurRoutines;

extern ArthurRoutines ar;

typedef struct ArthurTables {
    uint16_t K_DIROUT_TBL;
    uint16_t K_HINT_ITEMS;
} ArthurTables;

extern ArthurTables at;

#endif /* arthur_hpp */

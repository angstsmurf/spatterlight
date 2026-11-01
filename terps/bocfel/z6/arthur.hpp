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
}

#include "spatterlight-autosave.h"

#include <cstdint>

// Picture numbers the border drawing code needs too.
constexpr int K_PIC_BANNER = 54; // Decorative border/banner image
constexpr int K_PIC_BANNER_MARGIN = 100; // Dummy image used to measure banner margins
constexpr int K_PIC_LEFT_POLE = 170; // Left side pole of the border
constexpr int K_PIC_RIGHT_POLE = 171; // Right side pole of the border

struct ArthurGlobals {
    uint8_t UPDATE;
    uint8_t NAME_COLUMN;
    uint8_t GL_MAP_GRID_Y;
    uint8_t GL_WINDOW_TYPE;
    uint8_t GL_AUTHOR_SIZE;
    uint8_t GL_TIME_WIDTH;
    uint8_t GL_SL_HERE;
    uint8_t GL_SL_VEH;
    uint8_t GL_SL_HIDE;
    uint8_t GL_SL_TIME;
    uint8_t GL_SL_FORM;
};

struct ArthurRoutines {
    uint32_t UPDATE_STATUS_LINE;
    uint32_t RT_UPDATE_PICT_WINDOW;
    uint32_t RT_UPDATE_INVT_WINDOW;
    uint32_t RT_UPDATE_STAT_WINDOW;
    uint32_t RT_UPDATE_MAP_WINDOW;
    uint32_t RT_UPDATE_DESC_WINDOW;
    uint32_t RT_SEE_QST;
};

struct ArthurTables {
    uint16_t K_DIROUT_TBL;
    uint16_t K_HINT_ITEMS;
};

// The rows of the entrypoint table: entered when the game reaches the routine
void RT_UPDATE_PICT_WINDOW(void);
void RT_UPDATE_INVT_WINDOW(void);
void RT_UPDATE_STAT_WINDOW(void);
void RT_UPDATE_MAP_WINDOW(void);
void RT_HOT_KEY(void);
void ARTHUR_UPDATE_STATUS_LINE(void);
void arthur_INIT_STATUS_LINE(void);
void RT_AUTHOR_OFF(void);

// The V6GameHooks of this game (v6_shared.cpp)
void arthur_update_on_resize(void);
bool arthur_display_picture(glui32 picnum, glsi32 x, glsi32 y);
void arthur_update_after_restore(void);
void arthur_update_after_autorestore(void);
void arthur_update_after_restart(void);
void arthur_stash_state(library_state_data *dat);
void arthur_recover_state(library_state_data *dat);

// Called from other files
bool is_arthur_map_image(int picnum);
void adjust_arthur_top_margin(void);
void arthur_erase_window(int16_t index);
void arthur_move_cursor(int16_t y, int16_t x, winid_t win);
void RT_UPDATE_DESC_WINDOW(void);
void arthur_draw_room_image(int picnum);
void arthur_sync_screenmode(void);

// State other files read
extern int arthur_pic_top_margin;
extern bool showing_wide_arthur_room_image;
extern ArthurGlobals ag;
extern ArthurRoutines ar;
extern ArthurTables at;

#endif /* arthur_hpp */

//
//  shogun.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-21.
//

#ifndef shogun_hpp
#define shogun_hpp

#include <stdio.h>

extern "C" {
#include "glk.h"
#include "glkimp.h"
#include "spatterlight-autosave.h"
}


void shogun_update_on_resize(void);
void shogun_draw_title_image(void);
void shogun_update_after_restore(void);
void shogun_update_after_autorestore(void);
void shogun_update_after_restart(void);

void shogun_stash_state(library_state_data *dat);
void shogun_recover_state(library_state_data *dat);
bool shogun_autorestore_internal_read_char_hacks(void);

void SCENE_SELECT(void);
void V_VERSION(void);
void after_V_VERSION(void);
void SETUP_TEXT_AND_STATUS(void);
void MAC_II(void);
void after_MAC_II(void);
void shogun_DISPLAY_BORDER(void);
void GET_FROM_MENU(void);
void after_GET_FROM_MENU(void);
void shogun_UPDATE_STATUS_LINE(void);
void INTERLUDE_STATUS_LINE(void);
void CENTER_PIC_X(void);
void CENTER_PIC(void);
void MARGINAL_PIC(void);
void V_REFRESH(void);
void V_BOW(void);
void after_BUILDMAZE(void);

void DISPLAY_MAZE(void);
void DISPLAY_MAZE_PIC(void);
void MAZE_F(void);
void MAZE_MOUSE_F(void);

void DESCRIBE_ROOM(void);
void DESCRIBE_OBJECTS(void);

#define kShogunTitleImage 1

#define P_HINT_LOC 49

enum ShogunBorderType {
    NO_BORDER = 0,
    P_BORDER = 3,
    P_BORDER2 = 4,
    P_HINT_BORDER = 50
};

void shogun_display_border(ShogunBorderType border);
void shogun_display_inline_image(glui32 align);
void shogun_erase_screen(void);

typedef struct ShogunGlobals {
    uint8_t SCENE;
    uint8_t HERE;
    uint8_t SCORE;
    uint8_t MOVES;
    uint8_t CURRENT_BORDER;
    uint8_t WINNER;
    uint8_t SHIP_DIRECTION;
    uint8_t SHIP_COURSE;
    uint8_t MAZE_MAP;
    uint8_t MAZE_X;
    uint8_t MAZE_Y;
    uint8_t MAZE_XSTART;
    uint8_t MAZE_YSTART;
    uint8_t MAZE_WIDTH;
    uint8_t MAZE_HEIGHT;
    uint8_t MACHINE;
    uint8_t FONT_X;
    uint8_t FONT_Y;
} ShogunGlobals;

extern ShogunGlobals sg;

typedef struct ShogunRoutines {
    uint32_t V_REFRESH;
    uint32_t TELL_THE;
    uint32_t TELL_DIRECTION;
    uint32_t ADD_TO_INPUT;
    uint32_t DESCRIBE_ROOM;
    uint32_t DESCRIBE_OBJECTS;
} ShogunRoutines;

extern ShogunRoutines sr;

typedef struct ShogunTables {
    uint16_t K_HINT_ITEMS;
    uint16_t SCENE_NAMES;
    uint16_t MAZE_BOX_TABLE;
} ShogunTables;

extern ShogunTables st;

typedef struct ShogunMenus {
    uint8_t YOU_MAY_CHOOSE;
    uint16_t PART_MENU;
    uint16_t SCENE_SELECT_F;
} ShogunMenus;

extern ShogunMenus sm;

typedef struct ShogunObjects {
    uint8_t GALLEY;
    uint8_t BRIDGE_OF_ERASMUS;
    uint16_t WHEEL;
    uint16_t GALLEY_WHEEL;
    uint16_t NORTH;
    uint16_t SOUTH;
    uint16_t EAST;
    uint16_t WEST;
} ShogunObjects;

extern ShogunObjects so;

extern bool dont_repeat_question_on_autorestore;

#endif /* shogun_hpp */

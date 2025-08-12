//
//  zorkzero.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-30.
//

#ifndef zorkzero_hpp
#define zorkzero_hpp

extern "C" {
#include "glk.h"
#include "glkimp.h"
}

#include <stdio.h>
#include "screen.h"

void after_SPLIT_BY_PICTURE(void);
void CENTER(void);
//void V_CREDITS(void);
//void after_V_CREDITS(void);
void V_REFRESH(void);
void V_MODE(void);
void V_DEFINE(void);
void DEFAULT_COLORS(void);
void INIT_STATUS_LINE(void);
void SET_BORDER(void);
void DRAW_NEW_HERE(void);
void DRAW_NEW_COMP(void);
void DRAW_COMPASS_ROSE(void);
void SETUP_SCREEN(void);
void MAP_X(void);
void PLAY_SELECTED(void);
void SMALL_DOOR_F(void);
void J_PLAY(void);
void DRAW_PEGS(void);
void SET_B_PIC(void);
void TOWER_MODE(void);
void TOWER_WIN_CHECK(void);
void SCORE_CHECK(void);
void z0_UPDATE_STATUS_LINE(void);
void DRAW_TOWER(void);
void B_MOUSE_PEG_PICK(void);
void B_MOUSE_WEIGHT_PICK(void);
void SETUP_PBOZ(void);
void PBOZ_CLICK(void);
void PEG_GAME(void);
void PEG_GAME_READ_CHAR(void);
void PBOZ_WIN_CHECK(void);
void DISPLAY_MOVES(void);
void SNARFEM(void);
void SETUP_SN(void);
void DRAW_SN_BOXES(void);
void DRAW_PILE(void);
void DRAW_FLOWERS(void);
void SN_CLICK(void);
void FANUCCI(void);
void SETUP_FANUCCI(void);
void V_MAP_LOOP(void);
void WINPROP(void);
void BLINK(void);

enum BorderType {
    CASTLE_BORDER = 5,
    OUTSIDE_BORDER = 6,
    UNDERGROUND_BORDER = 7,
    HINT_BORDER = 8
};

#define zorkzero_title_image 1

#define zorkzero_encyclopedia_border 25

#define zorkzero_tower_of_bozbar_border 41
#define zorkzero_peggleboz_border 49
#define zorkzero_snarfem_border 73
#define zorkzero_double_fanucci_border 99

#define zorkzero_map_border 163

#define zorkzero_encyclopedia_picture_location 378
#define zorkzero_encyclopedia_text_location 379
#define zorkzero_encyclopedia_text_size 380

#define HERE_LOC 382
#define TEXT_WINDOW_PIC_LOC 0x183
#define COMPASS_PIC_LOC 381

#define CASTLE_BORDER_L 0x1f1
#define CASTLE_BORDER_R 0x1f2

#define UNDERGROUND_BORDER_L 0x1f3
#define UNDERGROUND_BORDER_R 0x1f4

#define OUTSIDE_BORDER_L 0x1f5
#define OUTSIDE_BORDER_R 0x1f6

#define HINT_BORDER_L 0x1f7
#define HINT_BORDER_R 0x1f8

#define F_SPLIT 422
#define F_BOTTOM 423

#define PBOZ_SPLIT 402
#define PBOZ_BOTTOM 424

#define SN_SPLIT 427
#define SN_BOTTOM 425

#define N_HIGHLIGHTED    0x09
#define NE_HIGHLIGHTED   0x0a
#define E_HIGHLIGHTED    0x0b
#define SE_HIGHLIGHTED   0x0c
#define S_HIGHLIGHTED    0x0d
#define SW_HIGHLIGHTED   0x0e
#define W_HIGHLIGHTED    0x0f
#define NW_HIGHLIGHTED   0x10

#define N_UNHIGHLIGHTED  0x11
#define NE_UNHIGHLIGHTED 0x12
#define E_UNHIGHLIGHTED  0x13
#define SE_UNHIGHLIGHTED 0x14
#define S_UNHIGHLIGHTED  0x15
#define SW_UNHIGHLIGHTED 0x16
#define W_UNHIGHLIGHTED  0x17
#define NW_UNHIGHLIGHTED 0x18

#define P_NW 0x38
#define P_WEST 0x39
#define P_SW 0x3a
#define P_SOUTH 0x3b
#define P_SE 0x3c
#define P_EAST 0x3d
#define P_NE 0x3e
#define P_NORTH 0x3f

#define zorkzero_tower_of_bozbar_bottom 426
#define zorkzero_tower_of_bozbar_split 428

#define EXIT_BOX 463
#define SHOW_MOVES_BOX 464
#define UNDO_BOX 465
#define RESTART_BOX 466
#define DIM_SHOW_MOVES_BOX 467
#define DIM_UNDO_BOX 468
#define DIM_RESTART_BOX 469

#define PBOZ_RESTART_BOX_LOC 472
#define PBOZ_SHOW_MOVES_BOX_LOC 473
#define PBOZ_EXIT_BOX_LOC 474

#define EXPAND_HOT_SPOT 477

#define UNHL_PEG 50

#define B_1_WEIGHT 43

#define B_1_L_PIC_LOC 363
#define B_2_C_PIC_LOC 364
#define B_3_R_PIC_LOC 365
#define B_4_PIC_LOC 366
#define B_5_PIC_LOC 367
#define B_6_PIC_LOC 368

#define UNDO_BOX 465
#define RESTART_BOX 466
#define DIM_SHOW_MOVES_BOX 467
#define DIM_UNDO_BOX 468
#define EXIT_BOX 463

#define TOWER_UNDO_BOX_LOC 475
#define TOWER_EXIT_BOX_LOC 476

extern winid_t z0_left_status_window;
extern winid_t z0_right_status_window;

void z0_update_on_resize(void);
void z0_update_after_restore(void);
void z0_update_after_autorestore(void);
void z0_update_colors(void);
void refresh_margin_images(void);
void clear_margin_image_list(void);
void DISPLAY_BORDER(BorderType border);
bool z0_display_picture(int x, int y, Window *win);
void z0_autorestore_internal_read_char_hacks(void);
void z0_erase_screen(void);
void z0_stash_state(library_state_data *dat);
void z0_recover_state(library_state_data *dat);

typedef struct ZorkGlobals {
    uint8_t HERE;
//    uint8_t COMPASS_CHANGED;
    uint8_t NEW_COMPASS;
    uint8_t MOVES;
    uint8_t SCORE;
    uint8_t CURRENT_BORDER;
    uint8_t BORDER_ON;
    uint8_t CURRENT_SPLIT;
    uint8_t F_WIN_COUNT;
    uint8_t F_PLAYS;
    uint8_t YOUR_SCORE;
    uint8_t DEFAULT_FG;
    uint8_t DEFAULT_BG;
    uint8_t BLINK_TBL;
    uint8_t TOWER_CHANGED;
    uint8_t NARROW;
} ZorkGlobals;

extern ZorkGlobals zg;

typedef struct ZorkRoutines {
    uint32_t V_REFRESH; 
    uint32_t SETUP_SCREEN;
    uint32_t SET_BORDER;
    uint32_t DRAW_NEW_COMP;
    uint32_t DRAW_COMPASS_ROSE;
    uint32_t MAP_X;
    uint32_t MAP_Y;
    uint32_t J_PLAY;
    uint32_t SCORE_CHECK;
    uint32_t DRAW_PEGS;
    uint32_t SET_B_PIC;
    uint32_t TOWER_WIN_CHECK;
    uint32_t PLAY_SELECTED;
} ZorkRoutines;

extern ZorkRoutines zr;

typedef struct ZorkTables {
    uint16_t K_HINT_ITEMS;
    uint16_t F_PLAY_TABLE;
    uint16_t F_PLAY_TABLE_APPLE;
    uint16_t LEFT_PEG_TABLE;
    uint16_t CENTER_PEG_TABLE;
    uint16_t RIGHT_PEG_TABLE;
    uint16_t PILE_TABLE;
    uint16_t PBOZ_PIC_TABLE;
    uint16_t BOARD_TABLE;
    uint16_t B_X_TBL;
    uint16_t B_Y_TBL;
    uint16_t F_CARD_TABLE;
    uint16_t PICINF_TBL;
} ZorkTables;

extern ZorkTables zt;

typedef struct ZorkObjects {
    uint16_t PHIL_HALL;
    uint16_t G_U_MOUNTAIN;
    uint16_t G_U_SAVANNAH;
    uint16_t G_U_HIGHWAY;
    uint16_t NOT_HERE_OBJECT;
    uint16_t LEFT_PEG;
    uint16_t RIGHT_PEG;
    uint16_t CENTER_PEG;
    uint16_t ONE_WEIGHT;
    uint16_t TWO_WEIGHT;
    uint16_t THREE_WEIGHT;
    uint16_t FOUR_WEIGHT;
    uint16_t FIVE_WEIGHT;
    uint16_t SIX_WEIGHT;
    uint16_t PBOZ_OBJECT;
    uint16_t PYRAMID_L;
    uint16_t PYRAMID;
    uint16_t PYRAMID_R;
    uint16_t FAN;
} ZorkObjects;

extern ZorkObjects zo;

typedef struct ZorkProperties {
    uint8_t P_REGION;
    uint8_t P_APPLE_DESC;
    uint8_t TOUCHBIT;
    uint8_t TRYTAKEBIT;
    uint8_t P_MAP_LOC;
} ZorkProperties;

extern ZorkProperties zp;

#endif /* zorkzero_hpp */

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
void V_CREDITS(void);
void after_V_CREDITS(void);
void V_REFRESH(void);
void V_MODE(void);
void V_DEFINE(void);
void INIT_STATUS_LINE(void);
void UPDATE_STATUS_LINE(void);
void DRAW_TOWER(void);
void B_MOUSE_PEG_PICK(void);
void B_MOUSE_WEIGHT_PICK(void);
void SETUP_PBOZ(void);
void PBOZ_CLICK(void);
void PEG_GAME(void);
void DISPLAY_MOVES(void);
void SETUP_SN(void);
void DRAW_SN_BOXES(void);
void DRAW_PILE(void);
void DRAW_FLOWERS(void);
void SN_CLICK(void);
void FANUCCI(void);
void SETUP_FANUCCI(void);
void V_MAP_LOOP(void);

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

#define CASTLE_BORDER_L 0x1f1
#define CASTLE_BORDER_R 0x1f2

#define UNDERGROUND_BORDER_L 0x1f3
#define UNDERGROUND_BORDER_R 0x1f4

#define OUTSIDE_BORDER_L 0x1f5
#define OUTSIDE_BORDER_R 0x1f6

#define HINT_BORDER_L 0x1f7
#define HINT_BORDER_R 0x1f8

#define TEXT_WINDOW_PIC_LOC 0x183

#define F_SPLIT 422
#define F_BOTTOM 423

#define PBOZ_SPLIT 402
#define PBOZ_BOTTOM 424

#define SN_SPLIT 427
#define SN_BOTTOM 425


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



bool is_zorkzero_vignette(int picnum);
bool is_zorkzero_border_image(int picnum);
bool is_zorkzero_game_bg(int pic);
bool is_zorkzero_rebus_image(int pic);
bool is_zorkzero_tower_image(int pic);
bool is_zorkzero_peggleboz_image(int pic);
bool is_zorkzero_peggleboz_box_image(int pic);

extern winid_t z0_left_status_window;
extern winid_t z0_right_status_window;

void z0_update_on_resize(void);
void z0_resize_windows_after_restore(void);
void z0_update_colors(void);
void refresh_margin_images(void);
void clear_margin_image_list(void);
void z0_resize_status_windows(void);

bool z0_display_picture(int x, int y, Window *win);

#endif /* zorkzero_hpp */

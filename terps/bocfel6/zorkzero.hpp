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

void after_SPLIT_BY_PICTURE(void);
void CENTER(void);
void V_CREDITS(void);
void after_V_CREDITS(void);
void V_REFRESH(void);
void V_MODE(void);
void V_DEFINE(void);
void INIT_STATUS_LINE(void);
void UPDATE_STATUS_LINE(void);

#define zorkzero_map_border 163
#define zorkzero_title_image 1
#define zorkzero_encyclopaedia_border 25

bool is_zorkzero_vignette(int picnum);
bool is_zorkzero_border_image(int picnum);
bool is_zorkzero_game_bg(int pic);
bool is_zorkzero_rebus_image(int pic);

extern winid_t z0_left_status_window;
extern winid_t z0_right_status_window;

void z0_update_on_resize(void);
void z0_resize_windows_after_restore(void);
void z0_update_colors(void);
void z0_refresh_margin_images(void);
void z0_clear_margin_images(void);
void z0_resize_status_windows(void);

void z0_add_margin_image(int image);

#endif /* zorkzero_hpp */

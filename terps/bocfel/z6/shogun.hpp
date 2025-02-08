//
//  shogun.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-21.
//

#ifndef shogun_hpp
#define shogun_hpp

#include <stdio.h>

void shogun_update_on_resize(void);
void shogun_draw_title_image(void);
void shogun_update_after_restore(void);

void SCENE_SELECT(void);
void V_VERSION(void);
void after_V_VERSION(void);
void SETUP_TEXT_AND_STATUS(void);
void MAC_II(void);
void after_MAC_II(void);
void shogun_DISPLAY_BORDER(void);
void GET_FROM_MENU(void);
void shogun_UPDATE_STATUS_LINE(void);
void INTERLUDE_STATUS_LINE(void);
void CENTER_PIC_X(void);
void CENTER_PIC(void);
void MARGINAL_PIC(void);

void DISPLAY_MAZE(void);
void DISPLAY_MAZE_PIC(void);
void MAZE_MOUSE_F(void);

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

#endif /* shogun_hpp */

//
//  shogun.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-21.
//

#ifndef shogun_hpp
#define shogun_hpp

#include <stdio.h>
bool is_shogun_inline_image(int picnum);
bool is_shogun_map_image(int picnum);
bool is_shogun_border_image(int picnum);

void adjust_shogun_window(void);
void shogun_update_on_resize(void);
void shogun_draw_title_image(void);
void shogun_update_after_restore(void);

void SCENE_SELECT(void);
void GOTO_SCENE(void);
void V_VERSION(void);
void after_V_VERSION(void);
void SETUP_TEXT_AND_STATUS(void);
//void after_V_HINT(void);
void after_V_DEFINE(void);
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


#define kShogunHintBorder 50
#define kShogunTitleImage 1

enum ShogunBorderType {
    P_BORDER = 3,
    P_BORDER2 = 4,
    P_HINT_BORDER = 50
};

void shogun_display_border(ShogunBorderType border);
void shogun_display_inline_image(glui32 align);

extern int shogun_graphical_banner_width_left;
extern int shogun_graphical_banner_width_right;

#endif /* shogun_hpp */

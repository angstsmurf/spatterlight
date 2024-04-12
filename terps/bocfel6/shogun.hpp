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

void SCENE_SELECT(void);
void GOTO_SCENE(void);
void V_VERSION(void);
void after_V_VERSION(void);
void after_SETUP_TEXT_AND_STATUS(void);
void after_V_HINT(void);
void after_V_DEFINE(void);
void MAC_II(void);
void after_MAC_II(void);

//void adjust_shogun_image(int picnum, int *x, int *y, int width, int height, int winwidth, int winheight, float *scale, int pixelwidth);

extern int shogun_graphical_banner_width_left;
extern int shogun_graphical_banner_width_right;

#endif /* shogun_hpp */

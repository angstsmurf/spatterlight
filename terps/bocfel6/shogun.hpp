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

//void adjust_shogun_image(int picnum, int *x, int *y, int width, int height, int winwidth, int winheight, float *scale, int pixelwidth);

#endif /* shogun_hpp */

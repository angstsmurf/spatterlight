//
//  journey.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

#ifndef journey_hpp
#define journey_hpp

#include <stdio.h>
bool is_journey_room_image(int picnum);
bool is_journey_stamp_image(int picnum);
void adjust_journey_image(int picnum, int *x, int *y, int width, int height, int winwidth, int winheight, float *scale, float pixelwidth);

#endif /* journey_hpp */

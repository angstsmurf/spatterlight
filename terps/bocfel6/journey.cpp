//
//  journey.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

extern "C" {
#include "glk.h"
#include "glkimp.h"
}

#include "journey.hpp"

enum Interpreter {
    kTerpDEC = 0,
    kTerpIBM    = 1,
    kTerpApple2  = 2,
    kTerpAmiga   = 3,
    kTerpMac   = 4
};

extern float imagescalex, imagescaley;

bool is_journey_stamp_image(int picnum) {
    switch (picnum) {
        case 6:
        case 81:
        case 82:
        case 85:
        case 91:
        case 101:
        case 115:
        case 158:
            return true;
        default:
            break;
    }
    return false;
}

bool is_journey_arrow_image(int picnum) {
    return (picnum >= 151 && picnum <= 156);
}

bool is_journey_room_image(int picnum) {
    return (picnum != 160 && !is_journey_arrow_image(picnum) && !is_journey_stamp_image(picnum));
}

static int journey_diffx = 0, journey_diffy = 0;

static float journey_img_scale = 1.0;

void adjust_journey_image(int picnum, int *x, int *y, int width, int height, int winwidth, int winheight, float *scale, float pixwidth) {
    int newx = *x, newy = *y;
    if (is_journey_room_image(picnum) || picnum == 160) {
        journey_img_scale = (float)winwidth / ((float)width * (float)pixwidth) ;
        if (height * journey_img_scale > winheight) {
            journey_img_scale = winheight / height;
            newx = (winwidth - width * journey_img_scale) / 2;
            newy = 0;
        } else {
            newx = 0;
            newy = (winheight - height * journey_img_scale) / 2;
        }
        fprintf(stderr, "adjust_journey_image: window width: %d image width = %d * scale %f == %f\n", winwidth, width, journey_img_scale, (float)width * journey_img_scale);
        journey_diffx = newx - *x;
        journey_diffy = newy - *y;
    } else if (is_journey_stamp_image(picnum)) {
        newx = *x + journey_diffx;
        newy = *y + journey_diffy;
    }

    *scale = journey_img_scale;

    *x = newx;
    *y = newy;
}

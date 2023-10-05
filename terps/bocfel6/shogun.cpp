//
//  shogun.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-21.
//

extern "C" {
#include "glk.h"
#include "glkimp.h"
}

#include "shogun.hpp"

extern float imagescalex, imagescaley;

bool is_shogun_inline_image(int picnum) {
    return (picnum >= 7 && picnum <= 37);
}

bool is_shogun_map_image(int picnum) {
    return (picnum >= 38);
}

bool is_shogun_border_image(int picnum) {
    return ((picnum >= 3 && picnum <= 5) || picnum == 59 || picnum == 50);
}

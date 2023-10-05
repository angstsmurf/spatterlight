//
//  zorkzero.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-30.
//

#include "zorkzero.hpp"

bool is_zorkzero_vignette(int picnum) {
    return false;
}
bool is_zorkzero_border_image(int pic) {
    return (pic >= 2 && pic <= 24 && pic != 8);
}

bool is_zorkzero_game_bg(int pic) {
    return (pic == 41 || pic == 49 || pic == 73 || pic == 99);
}


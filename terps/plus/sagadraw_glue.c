//
//  sagadrawglue.c
//  plus
//
//  Created by Administrator on 2025-12-15.
//

#include "common.h"
#include "graphics.h"

void AdjustGraphicsWindowHeight(void) {
    glui32 curheight, curwidth;
    glk_window_get_size(Graphics, &curwidth, &curheight);
    int optimal_height = ImageHeight * pixel_size;
    if (curheight != optimal_height && ImageWidth * pixel_size <= curwidth) {
        x_offset = (curwidth - ImageWidth * pixel_size) / 2;
        right_margin = (ImageWidth * pixel_size) + x_offset;
        winid_t parent = glk_window_get_parent(Graphics);
        if (parent) {
            glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                                       optimal_height, NULL);
        }
    }
}

int C64A8AdjustPlus(int width, int height, int *x_origin) {
    if (*x_origin < 0)
        *x_origin = 0;

    // We only want to adjust the graphics window height in Atari 8 Spider-Man.
    // In C64 Spider-Man, all room images have the same height.
    // In Buckaroo Banzai and Fantastic Four, the smaller images look better with
    // space above and below.
    if (CurrentSys == SYS_ATARI8 && CurrentGame == SPIDERMAN &&
        ((height == 126 && width == 39) || (height == 158 && width == 38))) {
        ImageHeight = height + 2;
        ImageWidth = width * 8;
        if (height == 158)
            ImageWidth -= 24;
        else
            ImageWidth -= 17;
    }

    AdjustGraphicsWindowHeight();

    return width;
}

void Apple2AdjustPlus(uint8_t width, uint8_t height, uint8_t y_origin) {

    /* Adjust the graphics window to the image height,
       but not for small object images */
    if (y_origin == 0 && (LastImgType == IMG_ROOM || LastImgType == IMG_SPECIAL)) {
        ImageHeight = (height + 2) * 2;
        ImageWidth = (width + 1) * 14;
    }

    AdjustGraphicsWindowHeight();
}

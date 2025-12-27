//
//  c64a8scott.c
//  Spatterlight
//
//  The ScottFree-specific code required by
//  the common code in common_sagadraw/c64a8draw.c
//  to draw the bitmap graphics of Atari 8-bit and
//  Commodore 64 games.
//
//  Created by Administrator on 2025-12-15.
//

#include "scott.h"
#include "c64a8draw.h"
#include "c64a8scott.h"

static USImage *current_image = NULL;

int C64A8AdjustScott(int width, int height, int *x_origin) {

    USImage *image = current_image;
    if (image == NULL)
        return width;

    glui32 curheight, curwidth;
    glk_window_get_size(Graphics, &curwidth, &curheight);

    if (image->usage == IMG_ROOM || (width == 38 && *x_origin < 1)) {
        width = width - 1 - image->cropright / 8;
        left_margin = image->cropleft;

        ImageWidth = width * 8 - 17 + left_margin;
        ImageHeight = height + 2;

        if (image->index == 19 && image->systype == SYS_ATARI8) {
            width++;
            ImageWidth = 308;
        }

        int width_in_pixels = ImageWidth * pixel_size;
        while (width_in_pixels > curwidth && pixel_size > 1) {
            pixel_size--;
            width_in_pixels = ImageWidth * pixel_size;
        }

        x_offset = (curwidth - width_in_pixels) / 2;

        int optimal_height = ImageHeight * pixel_size;

        if (curheight != optimal_height) {
            winid_t parent = glk_window_get_parent(Graphics);
            if (parent)
                glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                                           optimal_height, NULL);
        }
    }

    return width;
}

int DrawC64A8Image(USImage *image)
{
    current_image = image;
    return DrawC64A8ImageFromData(image->imagedata, image->datasize, CurrentGame == COUNT_US || CurrentGame == VOODOO_CASTLE_US, C64A8AdjustScott, image->systype == SYS_C64);
}


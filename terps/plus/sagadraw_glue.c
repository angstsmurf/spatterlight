//
//  sagadrawglue.c
//  plus
//
//  Created by Administrator on 2025-12-15.
//
#include <stdint.h>
#include <stdio.h>

#include "common.h"
#include "graphics.h"

/* C64 colors */
static const RGB black = { 0, 0, 0 };
static const RGB white = { 255, 255, 255 };
static const RGB red = { 191, 97, 72 };
static const RGB purple = { 177, 89, 185 };
static const RGB green = { 121, 213, 112 };
static const RGB blue = { 95, 72, 233 };
static const RGB yellow = { 247, 255, 108 };
static const RGB orange = { 186, 134, 32 };
static const RGB brown = { 131, 112, 0 };
static const RGB lred = { 231, 154, 132 };
static const RGB grey = { 167, 167, 167 };
static const RGB lgreen = { 192, 255, 185 };
static const RGB lblue = { 162, 143, 255 };

/* Atari 8-bit colors */
static const RGB cyan = { 0x9c, 0xe2, 0xc5 };
static const RGB lpurple = { 0xdf, 0xaa, 0xff };
static const RGB apurple = { 0xdb, 0x47, 0xdd };
static const RGB deeppurple = { 0x73, 0x1c, 0x73 };
static const RGB dblue = { 0x30, 0x24, 0xff };
static const RGB agreen = { 0x08, 0x88, 0x17 };
static const RGB dgreen = { 0x4f, 0x74, 0x20 };
static const RGB darkergreen = { 0x08, 0x38, 0x00 };
static const RGB ablue = { 0x36, 0x6e, 0xff };
static const RGB ayellow = { 0xef, 0xf2, 0x58 };
static const RGB aorange = { 0xbf, 0x77, 0x30 };
static const RGB abrown = { 0xab, 0x51, 0x1f };
static const RGB dbrown = { 0x73, 0x2c, 0x00 };
static const RGB alred = { 0xc2, 0x52, 0x57 };
static const RGB beige = { 0xff, 0x8f, 0x8f };
static const RGB dred = { 0xa2, 0x3f, 0x40 };
static const RGB agrey = { 0x92, 0x92, 0x92 };
static const RGB lgrey = { 0xb4, 0xb5, 0xb4 };
static const RGB algreen = { 0x5f, 0x8f, 0x00 };
static const RGB tan = { 0xaf, 0x99, 0x3a };
static const RGB lilac = { 0x83, 0x58, 0xee };

void TransAtariColorPlus(uint8_t index, uint8_t value)
{
    switch (value) {
        case 0:
        case 224:
            SetColor(index, &black);
            break;
        case 5:
        case 7:
            SetColor(index, &agrey);
            break;
        case 6:
        case 8:
            SetColor(index, &ablue);
            break;
        case 9:
        case 10:
        case 12:
            SetColor(index, &lgrey);
            break;
        case 14:
            SetColor(index, &white);
            break;
        case 17:
        case 32:
        case 36:
            SetColor(index, &dbrown);
            break;
        case 40:
            SetColor(index, &aorange);
            break;
        case 54:
            if (CurrentGame == FANTASTIC4)
                SetColor(index, &aorange);
            else
                SetColor(index, &dred);
            break;
        case 56:
            SetColor(index, &alred);
            break;
        case 58:
        case 60:
            SetColor(index, &beige);
            break;
        case 69:
        case 71:
            SetColor(index, &deeppurple);
            break;
        case 68:
        case 103:
            SetColor(index, &apurple);
            break;
        case 89:
            SetColor(index, &lilac);
            break;
        case 85:
        case 101:
            SetColor(index, &dblue);
            break;
        case 110:
            SetColor(index, &lpurple);
            break;
        case 135:
            SetColor(index, &ablue);
            break;
        case 174:
            SetColor(index, &cyan);
            break;
        case 182:
        case 196:
        case 214:
        case 198:
        case 200:
            SetColor(index, &agreen);
            break;
        case 194:
            SetColor(index, &darkergreen);
            break;
        case 212:
            SetColor(index, &dgreen);
            break;
        case 199:
        case 215:
        case 201:
            SetColor(index, &algreen);
            break;
        case 230:
            SetColor(index, &tan);
            break;
        case 237:
            SetColor(index, &ayellow);
            break;
        case 244:
            SetColor(index, &abrown);
            break;
        case 246:
        case 248:
            SetColor(index, &aorange);
            break;
        default:
            fprintf(stderr, "Unknown color %d ", value);
            break;
    }
}

/*
 The values below are determined by looking at the games
 running in the VICE C64 emulator.
 I have no idea how the original interpreter calculates
 them. I might have made some mistakes. */

void TransC64ColorPlus(uint8_t index, uint8_t value)
{
    switch (value) {
        case 0:
            SetColor(index, &purple);
            break;
        case 1:
            SetColor(index, &blue);
            break;
        case 3:
            SetColor(index, &white);
            break;
        case 7:
        case 8:
            SetColor(index, &blue);
            break;
        case 9:
        case 10:
        case 12:
        case 14:
        case 15:
            SetColor(index, &white);
            break;
        case 17:
            SetColor(index, &green);
            break;
        case 36:
        case 40:
            SetColor(index, &brown);
            break;
        case 46:
            SetColor(index, &yellow);
            break;
        case 52:
        case 54:
        case 56:
        case 60:
            SetColor(index, &orange);
            break;
        case 68:
        case 69:
        case 71:
            SetColor(index, &red);
            break;
        case 77:
        case 85:
        case 87:
            SetColor(index, &purple);
            break;
        case 89:
            SetColor(index, &lred);
            break;
        case 101:
        case 103:
            SetColor(index, &purple);
            break;
        case 110:
            SetColor(index, &lblue);
            break;
        case 135:
        case 137:
            SetColor(index, &blue);
            break;
        case 157:
            SetColor(index, &lblue);
            break;
        case 161:
            SetColor(index, &grey);
            break;
        case 179:
        case 182:
        case 194:
        case 196:
        case 198:
        case 199:
        case 200:
            SetColor(index, &green);
            break;
        case 201:
            SetColor(index, &lgreen);
            break;
        case 212:
        case 214:
        case 215:
            SetColor(index, &green);
            break;
        case 224:
            SetColor(index, &purple);
            break;
        case 230:
        case 237:
            SetColor(index, &yellow);
            break;
        case 244:
        case 246:
            SetColor(index, &brown);
            break;
        case 248:
            if (CurrentGame == FANTASTIC4)
                SetColor(index, &orange);
            else
                SetColor(index, &brown);
            break;
        case 252:
            SetColor(index, &yellow);
            break;
        default:
            fprintf(stderr, "Unknown color %d ", value);
            break;
    }
}

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

//
//  palette.h
//  scott
//
//  Created by Administrator on 2025-12-13.
//

#ifndef palette_h
#define palette_h

#include <stdint.h>

#include "glk.h"

typedef enum {
    NO_PALETTE,
    ZX,
    ZXOPT,
    C64A,
    C64B,
    C64C,
    C64D,
    C64QP3,
    C64TAYLOR,
    VGA
} palette_type;

uint8_t Remap(uint8_t color);
void DefinePalette(void);
void SwitchPalettes(int pal1, int pal2);

extern glui32 pal[16];
extern palette_type palchosen;

// Used by Robin of Sherwood waterfall animation
// and Secret Mission screen animation
extern int white_colour;
extern int blue_colour;
// Used by Seas of Blood
extern glui32 dice_colour;

#endif /* palette_h */

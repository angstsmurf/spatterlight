//
//  animations.c
//  taylor
//
//  Created by Administrator on 2022-03-24.
//

#include "sagadraw.h"
#include "animations.h"

#define UNFOLDING_SPACE 50
#define STARS_ANIMATION_RATE 15

int AnimationRunning = 0;

extern uint8_t buffer[384][9];

int rotate_left_with_carry(uint8_t *byte, int last_carry)
{
    int carry = ((*byte & 0x80) > 0);
    *byte = *byte << 1;
    if (last_carry)
        *byte = *byte | 0x01;
    return carry;
}

int rotate_right_with_carry(uint8_t *byte, int last_carry)
{
    int carry = ((*byte & 0x01) > 0);
    *byte = *byte >> 1;
    if (last_carry)
        *byte = *byte | 0x80;
    return carry;
}

void animate_stars(void)
{
    int carry;
    RectFill(48, 16, 160, 32, 0, 0);
    for (int line = 3; line < 7; line++) {
        for (int row = 0; row < 8; row++) {
            carry = 0;
            for (int col = 15; col > 5; col--) {
                uint8_t attribute = buffer[col + line * 32][8];
                glui32 ink = attribute & 7;
                ink += 8 * ((attribute & 64) == 64);
                for (int pix = 0; pix < 8; pix++) {
                    if ((buffer[col + line * 32][row] & (1 << pix)) != 0) {
                        PutPixel(col * 8 + pix, line * 8 + row, ink);
                    }
                }
                carry = rotate_right_with_carry(&(buffer[col + line * 32][row]), carry);
            }
            buffer[line * 32 + 15][row] = buffer[line * 32 + 15][row] | carry;
            carry = 0;
            for (int col = 16; col < 26; col++) {
                uint8_t attribute = buffer[col + line * 32][8];
                glui32 ink = attribute & 7;
                ink += 8 * ((attribute & 64) == 64);
                for (int pix = 0; pix < 8; pix++) {
                    if ((buffer[col + line * 32][row] & (1 << pix)) != 0) {
                        PutPixel(col * 8 + pix, line * 8 + row, ink);
                    }
                }
                carry = rotate_left_with_carry(&(buffer[col + line * 32][row]), carry);
            }
            if (carry) {
                buffer[line * 32 + 16][row] = buffer[line * 32 + 16][row] | 128;
            }
        }
    }
}

void UpdateRebelAnimations(void)
{
    if (MyLoc == 1 && ObjectLoc[UNFOLDING_SPACE] == 1) {
        animate_stars();
    } else {
        glk_request_timer_events(0);
        AnimationRunning = 0;
    }
}

void StartAnimations(void) {
    if (CurrentGame == REBEL_PLANET && MyLoc == 1 && ObjectLoc[UNFOLDING_SPACE] == 1) {
        if (AnimationRunning != STARS_ANIMATION_RATE) {
            glk_request_timer_events(STARS_ANIMATION_RATE);
            AnimationRunning = STARS_ANIMATION_RATE;
        }
    }
}


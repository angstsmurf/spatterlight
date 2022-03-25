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
#define KAYLETH_ANIMATION_RATE 175


int AnimationRunning = 0;
int AnimationStage = 0;
int AnimationStartFrame = 0;

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
    /* First fill area with black, erasing all stars */
    RectFill(48, 16, 160, 32, 0, 0);
    /* We go line by line and pixel row by pixel row */
    for (int line = 3; line < 7; line++) {
        for (int pixrow = 0; pixrow < 8; pixrow++) {
            carry = 0;
            /* The left half is rotated one pixel to the left, */
            /* byte by byte */
            for (int col = 15; col > 5; col--) {
                uint8_t attribute = buffer[col + line * 32][8];
                glui32 ink = attribute & 7;
                ink += 8 * ((attribute & 64) == 64);
                for (int bit = 0; bit < 8; bit++) {
                    if ((buffer[col + line * 32][pixrow] & (1 << bit)) != 0) {
                        PutPixel(col * 8 + bit, line * 8 + pixrow, ink);
                    }
                }
                carry = rotate_right_with_carry(&(buffer[col + line * 32][pixrow]), carry);
            }
            if (carry) {
                buffer[line * 32 + 15][pixrow] = buffer[line * 32 + 15][pixrow] | 128;
            }
            carry = 0;
            /* Then the right half */
            for (int col = 16; col < 26; col++) {
                uint8_t attribute = buffer[col + line * 32][8];
                glui32 ink = attribute & 7;
                ink += 8 * ((attribute & 64) == 64);
                for (int pix = 0; pix < 8; pix++) {
                    if ((buffer[col + line * 32][pixrow] & (1 << pix)) != 0) {
                        PutPixel(col * 8 + pix, line * 8 + pixrow, ink);
                    }
                }
                carry = rotate_left_with_carry(&(buffer[col + line * 32][pixrow]), carry);
            }
            buffer[line * 32 + 16][pixrow] = buffer[line * 32 + 16][pixrow] | carry;
        }
    }
}

void UpdateKaylethAnimations(void)
{
    AnimationStage++;
    if (AnimationStage > 9)
        AnimationStage = 0;

    if (AnimationStage % 2) {
        if (MyLoc == 1)
            DrawTaylor(98);
        else
            DrawTaylor(114);
    } else {
        DrawTaylor(AnimationStartFrame + AnimationStage / 2);
        if (MyLoc == 1)
            DrawTaylor(97);
         else
            DrawTaylor(109);
    }


    DrawSagaPictureFromBuffer();
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

void StartKaylethAnimation(int start) {
    AnimationStartFrame = start;
    glk_request_timer_events(KAYLETH_ANIMATION_RATE);
}


void StartAnimations(void) {
    if (CurrentGame == REBEL_PLANET && MyLoc == 1 && ObjectLoc[UNFOLDING_SPACE] == 1) {
        if (AnimationRunning != STARS_ANIMATION_RATE) {
            glk_request_timer_events(STARS_ANIMATION_RATE);
            AnimationRunning = STARS_ANIMATION_RATE;
        }
    } else if (CurrentGame == KAYLETH) {
        switch(MyLoc) {
            case 1:
                StartKaylethAnimation(92); // Conveyor belt
                break;
            case 2:
                StartKaylethAnimation(110); // Conveyor belt
                break;
            default:
                glk_request_timer_events(0);
                AnimationRunning = 0;
                break;
        }
    }
}


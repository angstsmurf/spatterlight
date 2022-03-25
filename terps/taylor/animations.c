//
//  animations.c
//  taylor
//
//  Created by Administrator on 2022-03-24.
//

#include "sagadraw.h"
#include "animations.h"
#include "utility.h"

#define UNFOLDING_SPACE 50
#define STARS_ANIMATION_RATE 15

int AnimationRunning = 0;
int KaylethAnimationRoom = 0;

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
            /* byte by byte, but we actually rotate to the right */
            /* because the bytes are flipped in our implementation */
            /* for some reason */
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

int UpdateKaylethAnimations(void) // Draw animation frame
{
    if (MyLoc == 0) {
        return 0;
    }
    uint8_t *ptr = &FileImage[0xfded - 0x4000 + FileBaselineOffset];
    int counter = 0;
    do {
        if (*ptr == 0xff)
            counter++;
        ptr++;
    } while(counter < MyLoc);
    while(1) {
        if (*ptr == 0xff) {
            return 0;
        }
        int Stage = *ptr;
        ptr++;
        uint8_t *LastInstruction = ptr;
        if (ObjectLoc[*ptr] != MyLoc && *ptr != 0) {
            //Returning because location of object *ptr is not current location
            return 0;
        }
        ptr++;
        if (KaylethAnimationRoom != MyLoc) {
            KaylethAnimationRoom = MyLoc;
            *ptr = 0;
        }

        ptr += *ptr + 1;
        int AnimationRate = *ptr;

        if (AnimationRate != 0xff) {
            ptr++;
            (*ptr)++;
            if (AnimationRate != *ptr) {
                //Skipping this frame because animationrate != current animation stage
                return 1;
            }
            *ptr = 0;
            // Draw "room image" *(ptr + 1) (Actually an animation frame)
            DrawTaylor(*(ptr + 1));
            DrawSagaPictureFromBuffer();
            ptr = LastInstruction + 1;
            (*ptr) += 3;
            return 1;
        } else {
            ptr = LastInstruction + 1;
            *ptr = Stage;
            ptr -= 2;
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
    } else if (CurrentGame == KAYLETH) {
        int speed = 0;
        if (UpdateKaylethAnimations())
            speed = 20;
        KaylethAnimationRoom = 0;
        if (ObjectLoc[0] == MyLoc)
            speed = 13; // Azap chamber
        switch(MyLoc) {
            case 1:
            case 2: // Conveyor belt
                speed = 14;
                break;
            case 53: // Twin peril forest
                speed = 300;
                break;
            case 56: // Citadel of Zenron
                speed = 40;
                break;
            case 36: // Guard dome
                speed = 12;
                break;
            default:
                break;
        }
        if (AnimationRunning != speed) {
            glk_request_timer_events(speed);
            AnimationRunning = speed;
        }
    }
}


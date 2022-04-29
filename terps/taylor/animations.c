//
//  animations.c
//  taylor
//
//  Created by Administrator on 2022-03-24.
//

// This is currently inefficient in two ways: 1) we redraw the entire
// image when only parts have changed, and 2) we sometimes draw the
// entire image, and then draw a second animation on top of this.
//
// There should be a second buffer array copy that represents what is
// currently on-screen, and every pixel should be compared against this
// to see if it is the same before being redrawn.
//
// The click shelf animation should be done by setting the bitmap in a
// buffer array first, before drawing the image. This will require a
// second copy of the image buffer.

#include "sagadraw.h"
#include "animations.h"
#include "utility.h"

#define UNFOLDING_SPACE 50
#define STARS_ANIMATION_RATE 15
#define SERPENT_ANIMATION_RATE 100

int AnimationRunning = 0;
static int KaylethAnimationIndex = 0;
static int AnimationStage = 0;
static int ClickShelfStage = 0;

extern uint8_t buffer[384][9];

static void animate_stars(void)
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

static void AnimateKaylethClickShelves(int stage)
{
    RectFill(100, 0, 56, 81, 0, 0);
    for (int line = 0; line < 10; line++) {
        for (int col = 12; col < 20; col++) {
            for (int i = 0; i < 8; i++) {
                int ypos = line * 8 + i + stage;
                if (ypos > 79)
                    ypos = ypos - 80;
                uint8_t attribute = buffer[col + (ypos / 8) * 32][8];
                glui32 ink = attribute & 7;
                ink += 8 * ((attribute & 64) == 64);
                attribute = buffer[col + ((79 - ypos) / 8) * 32][8];
                glui32 ink2 = attribute & 7;
                ink2 += 8 * ((attribute & 64) == 64);
                for (int j = 0; j < 8; j++)
                    if (col > 15) {
                        if ((buffer[col + line * 32][i] & (1 << j)) != 0) {
                            PutPixel(col * 8 + j, ypos, ink);
                        }
                    } else {
                        if ((buffer[col + (9 - line) * 32][7 - i] & (1 << j)) != 0) {
                            PutPixel(col * 8 + j, 79 - ypos, ink2);
                        }
                    }
            }
        }
    }
}

static int UpdateKaylethAnimationFrames(void) // Draw animation frame
{
    if (MyLoc == 0) {
        return 0;
    }

    uint8_t *ptr = &FileImage[0xbded + FileBaselineOffset];
    int counter = 0;
    // Jump to animation index equal to location index
    // Animations are delimited by 0xff
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
        if (ObjectLoc[*ptr] != MyLoc && *ptr != 0 && *ptr != 122) {
            //Returning because location of object *ptr is not current location
            return 0;
        }
        ptr++;
        // Reset animation if we are in a new room
        if (KaylethAnimationIndex != MyLoc) {
            KaylethAnimationIndex = MyLoc;
            *ptr = 0;
        }

        ptr += *ptr + 1;
        int AnimationRate = *ptr;

        if (AnimationRate != 0xff) {
            ptr++;
            (*ptr)++;
            if (AnimationRate == *ptr) {
                *ptr = 0;
                // Draw "room image" *(ptr + 1) (Actually an animation frame)
                int result = *(ptr + 1);
                DrawTaylor(result);
                DrawSagaPictureFromBuffer();
                ptr = LastInstruction + 1;
                (*ptr) += 3;
                return result;
            }
            return 1;
        } else {
            ptr = LastInstruction + 1;
            *ptr = Stage;
            ptr -= 2;
        }
    }
}

void UpdateKaylethAnimations(void) {
    // This is an attempt to make the animation jerky like the original.
    ClickShelfStage++;
    if (ClickShelfStage == 9)
        ClickShelfStage = 0;
    UpdateKaylethAnimationFrames();
    if (MyLoc == 3) {
        AnimateKaylethClickShelves(AnimationStage);
        if (ClickShelfStage < 7)
            AnimationStage++;
        if (AnimationStage > 80)
            AnimationStage = 0;
    }
}

extern Image *images;

void UpdateRebelAnimations(void)
{
    if (MyLoc == 1 && ObjectLoc[UNFOLDING_SPACE] == 1) {
        animate_stars();
    } else if (MyLoc > 28 && MyLoc < 34) {
        if (ObjectLoc[92] == MyLoc) {
            AnimationStage++;
            if (AnimationStage > 5) {
                AnimationStage = 5;
                glk_request_timer_events(0);
                AnimationRunning = 0;
            }
        } else {
            AnimationStage--;
            if (AnimationStage < 0) {
                AnimationStage = 0;
                glk_request_timer_events(0);
                AnimationRunning = 0;
            } else if (AnimationStage == 4) {
                glk_request_timer_events(50);
            }
            DrawTaylor(MyLoc);
        }
        if (AnimationStage) {
            DrawSagaPictureAtPos(62 + AnimationStage, 14, 10 - AnimationStage - (AnimationStage > 2));
        }
        DrawSagaPictureFromBuffer();
    } else {
        glk_request_timer_events(0);
        AnimationRunning = 0;
        AnimationStage = 0;
    }
}

void StartAnimations(void) {
    if (CurrentGame == REBEL_PLANET) {

        if (MyLoc == 1 && ObjectLoc[UNFOLDING_SPACE] == 1) {
            if (AnimationRunning != STARS_ANIMATION_RATE) {
                glk_request_timer_events(STARS_ANIMATION_RATE);
                AnimationRunning = STARS_ANIMATION_RATE;
            }
        } else if (MyLoc > 28 && MyLoc < 34) {
            glk_request_timer_events(SERPENT_ANIMATION_RATE);
            UpdateRebelAnimations();
        }
    } else if (CurrentGame == KAYLETH) {
        int speed = 0;
        if (UpdateKaylethAnimationFrames()) {
            speed = 20;
            KaylethAnimationIndex = 0;
        }
        if (ObjectLoc[0] == MyLoc)
            speed = 13; // Azap chamber
        switch(MyLoc) {
            case 1:
                speed = 14;
                break;
            case 2: // Conveyor belt
                speed = 10;
                break;
            case 3: // Click shelves
                speed = 40;
                break;
            case 53: // Twin peril forest
                speed = 350;
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


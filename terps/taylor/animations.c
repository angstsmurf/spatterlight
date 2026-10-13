//
//  animations.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-03-24.
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

#include "graphics.h"
#include "taylordraw.h"
#include "irmak.h"
#include "utility.h"

#include "animations.h"

/* Object index for the "unfolding space" star view in Rebel Planet */
#define UNFOLDING_SPACE 50

/* Timer intervals (ms) for each Rebel Planet animation */
#define STARS_ANIMATION_RATE 15
#define STARS_ANIMATION_RATE_64 40
#define FIELD_ANIMATION_RATE 10
#define SERPENT_ANIMATION_RATE 100
#define ROBOT_ANIMATION_RATE 400
#define SNAPPER_ANIMATION_RATE 400
#define QUEEN_ANIMATION_RATE 120

int AnimationRunning = 0;              /* Current timer rate, or 0 if idle */
static int KaylethAnimationIndex = 0;  /* Room index of the last Kayleth animation */
/* Each animation keeps its own frame counter, so that a restore or undo
   that jumps from one animated room straight to another cannot start the
   new animation at the old one's frame. */
static int QueenStage = 0;             /* Queen computer colour cycle, 0–15 */
static int SerpentStage = 0;           /* How far the Sycane Serpent has emerged, 0–5 */
static int TwoFrameStage = 0;          /* Security robot and crag snapper, 0 or 1 */
static int ShelfScrollStage = 0;       /* Kayleth click-shelf scroll position, 0–80 */
static int ClickShelfStage = 0;        /* Sub-counter for Kayleth click-shelf jerkiness */

/* Set the Glk timer to the given interval (ms) unless it is already
   running at that rate. */
static void SetAnimationRate(int rate)
{
    if (AnimationRunning != rate) {
        glk_request_timer_events(rate);
        AnimationRunning = rate;
    }
}

static void StopAnimations(void)
{
    glk_request_timer_events(0);
    AnimationRunning = 0;
}

/* Animate the star field in Rebel Planet room 1 (view through a window).
   The left half of the image scrolls left and the right half scrolls right,
   creating a parallax / hyperspace effect. Works by rotating bits in the
   ZX Spectrum-format imagebuffer (8 pixel-rows per character cell, 1 bit
   per pixel) and redrawing the affected region. */
static void AnimateStars(void)
{
    int carry;
    /* First fill area with black, erasing all stars */
    RectFill(48, 16, 160, 32, 0);
    /* We go line by line and pixel row by pixel row */
    for (int line = 3; line < 7; line++) {
        for (int pixrow = 0; pixrow < 8; pixrow++) {
            carry = 0;
            /* The left half is rotated one pixel to the left, */
            /* byte by byte */
            for (int col = 15; col > 5; col--) {
                uint8_t attribute = imagebuffer[col + line * IRMAK_IMGWIDTH][IRMAK_CELL_ATTR];
                glui32 ink = Remap(AttributeInk(attribute));
                for (int bit = 0; bit < 8; bit++) {
                    if (isNthBitSet(imagebuffer[col + line * IRMAK_IMGWIDTH][pixrow], 7 - bit)) {
                        PutPixel(col * 8 + bit, line * 8 + pixrow, ink);
                    }
                }
                carry =
                rotate_left_with_carry(&(imagebuffer[col + line * IRMAK_IMGWIDTH][pixrow]), carry);
            }
            imagebuffer[line * IRMAK_IMGWIDTH + 15][pixrow] =
                imagebuffer[line * IRMAK_IMGWIDTH + 15][pixrow] | carry;
            carry = 0;
            /* Then the right half */
            for (int col = 16; col < 26; col++) {
                uint8_t attribute = imagebuffer[col + line * IRMAK_IMGWIDTH][IRMAK_CELL_ATTR];
                glui32 ink = Remap(AttributeInk(attribute));
                for (int bit = 0; bit < 8; bit++) {
                    if (isNthBitSet(imagebuffer[col + line * IRMAK_IMGWIDTH][pixrow], 7 - bit)) {
                        PutPixel(col * 8 + bit, line * 8 + pixrow, ink);
                    }
                }
                carry =
                rotate_right_with_carry(&(imagebuffer[col + line * IRMAK_IMGWIDTH][pixrow]), carry);
            }
            if (carry) {
                imagebuffer[line * IRMAK_IMGWIDTH + 16][pixrow] =
                imagebuffer[line * IRMAK_IMGWIDTH + 16][pixrow] | 128;
            }
        }
    }
}

/* Animate the force field in Rebel Planet room 86. The entire field
   region is rotated one pixel to the right each frame, with the carry
   bit wrapping from the rightmost column back to the leftmost. */
static void AnimateForcefield(void)
{
    int carry;
    /* First fill door area with black, erasing field */
    RectFill(104, 16, 48, 39, 0);

    /* Use the colour attribute from the top-left cell for the whole field */
    uint8_t colour = imagebuffer[2 * IRMAK_IMGWIDTH + 13][IRMAK_CELL_ATTR];
    glui32 ink = Remap(colour & INK_MASK);

    for (int line = 2; line < 7; line++) {
        for (int pixrow = 0; pixrow < 8; pixrow++) {
            carry = 0;
            for (int col = 13; col < 19; col++) {
                for (int pix = 0; pix < 8; pix++) {
                    if (isNthBitSet(imagebuffer[col + line * IRMAK_IMGWIDTH][pixrow], 7 - pix)) {
                        PutPixel(col * 8 + pix, line * 8 + pixrow, ink);
                    }
                }
                /* The force field is rotated one pixel to the right, */
                /* byte by byte */
                carry = rotate_right_with_carry(&(imagebuffer[col + line * IRMAK_IMGWIDTH][pixrow]), carry);
            }
            if (carry)
                imagebuffer[line * IRMAK_IMGWIDTH + 13][pixrow] = imagebuffer[line * IRMAK_IMGWIDTH + 13][pixrow] | 128;
        }
    }
}

/* Fill the unset (background) pixels of a character cell with the given
   colour. Used by AnimateQueenComputer to cycle background colours. */
static void FillCell(int cell, glui32 ink)
{
    int startx = (cell % IRMAK_IMGWIDTH) * 8;
    int starty = (cell / IRMAK_IMGWIDTH) * 8;
    for (int pixrow = 0; pixrow < 8; pixrow++) {
        for (int pix = 0; pix < 8; pix++) {
            if (!isNthBitSet(imagebuffer[cell][pixrow], 7 - pix)) {
                PutPixel(startx + pix, starty + pixrow, ink);
            }
        }
    }
}

/* Animate the Queen's computer in Rebel Planet room 88. Cycles bright
   colours through three pairs of indicator cells (at columns 3, 10, 27)
   on two rows, plus three cells on the bottom row. Every 16th frame the
   Arcadian head image is toggled on or off. */
static void AnimateQueenComputer(void)
{
    /* Leftmost column of each pair of indicator cells */
    static const int indicator_columns[3] = { 3, 10, 27 };
    /* Map QueenStage (0–15) to a colour index that wraps 0–7 */
    int rotatingink = 7 - (QueenStage - 7 * (QueenStage > 7));
    /* Fill indicator cells with rotating bright colours */
    for (int i = 0; i < 3; i++) {
        for (int line = 3; line <= 6; line += 3) {
            for (int cell = 0; cell < 2; cell++) {
                FillCell(line * IRMAK_IMGWIDTH + indicator_columns[i] + cell, 8 + rotatingink);
                rotatingink--;
                if (rotatingink < 0)
                    rotatingink = 7;
            }
        }
    }
    for (int cell = 0; cell < 3; cell++) {
        FillCell(7 * IRMAK_IMGWIDTH + 18 + cell, 8 + rotatingink--);
        if (rotatingink < 0)
            rotatingink = 7;
    }
    if (QueenStage == 0) {
        /* Draw the Arcadian head overlay (image block 18) */
        DrawPictureAtPos(18, 18, 1, 0);
    }

    if (QueenStage == 7) {
        /* Erase the Arcadian head area, creating a blink effect */
        RectFill(144, 8, 24, 40, 0);
    }

    QueenStage++;
    if (QueenStage > 15)
        QueenStage = 0;
}

/* Animate the click shelves in Kayleth room 3. The shelves scroll
   vertically with the stage parameter controlling the offset. The left
   half (cols 12–15) scrolls downward using vertically mirrored data,
   while the right half (cols 16–19) scrolls upward. Both wrap around
   at 80 pixels to create an endless loop. */
static void AnimateKaylethClickShelves(int stage)
{
    RectFill(100, 0, 56, 81, 0);
    for (int line = 0; line < 10; line++) {
        for (int col = 12; col < 20; col++) {
            for (int i = 0; i < 8; i++) {
                /* Compute wrapped vertical position */
                int ypos = line * 8 + i + stage;
                if (ypos > 79)
                    ypos = ypos - 80;
                /* Look up colour attributes from the destination rows */
                uint8_t attribute = imagebuffer[col + (ypos / 8) * IRMAK_IMGWIDTH][IRMAK_CELL_ATTR];
                glui32 ink = Remap(AttributeInk(attribute));
                attribute = imagebuffer[col + ((79 - ypos) / 8) * IRMAK_IMGWIDTH][IRMAK_CELL_ATTR];
                glui32 ink2 = Remap(AttributeInk(attribute));
                for (int j = 0; j < 8; j++) {
                    if (col > 15) {
                        /* Right half: draw from source row, scrolling up */
                        if (isNthBitSet(imagebuffer[col + line * IRMAK_IMGWIDTH][i], 7 - j)) {
                            PutPixel(col * 8 + j, ypos, ink);
                        }
                    } else {
                        /* Left half: draw from mirrored source row, scrolling down */
                        if (isNthBitSet(imagebuffer[col + (9 - line) * IRMAK_IMGWIDTH][7 - i], 7 - j)) {
                            PutPixel(col * 8 + j, 79 - ypos, ink2);
                        }
                    }
                }
            }
        }
    }
}

/* A single frame in a Kayleth animation sequence. The counter increments
   each timer tick; when it reaches counter_to_draw_at, the frame's image
   is drawn and the animation advances. */
typedef struct {
    int counter_to_draw_at; /* Number of ticks before this frame is drawn */
    int counter;            /* Current tick count (reset after drawing) */
    int image;              /* Image index to draw for this frame */
} KaylethAnimationFrame;

/* A complete animation sequence for one Kayleth room. Animations only
   play when required_object is present in the current room. After the
   last frame, the sequence loops back to the frame at loop_to. */
typedef struct {
    int loop_to;            /* Frame index to restart from after the last frame */
    int required_object;    /* Object that must be present to trigger this animation */
    int number_of_frames;
    int current_frame;
    KaylethAnimationFrame *frames;
} KaylethAnimation;

/* Per-room animation table, indexed by room number (NULL = no animation) */
static KaylethAnimation **KaylethAnimations = NULL;

/* Total number of rooms in Kayleth; NULL entries for rooms without animation */
#define NUMBER_OF_KAYLETH_ANIMATIONS 92

/* First image of the Azap chamber animation, whose timing is patched */
#define AZAP_FIRST_IMAGE 118

/* Not a real object (Kayleth has 120): a slot that the picture
   instructions test to draw the destroyer droid in the current room. */
#define KAYLETH_DROID_SLOT 122

/* Parse the Kayleth animation data from the game file image into the
   KaylethAnimations table. The data format is a sequence of room entries:
   - 0xFF bytes mark rooms with no animation (one per room).
   - Otherwise: loop_to (byte, in units of 3), required_object (byte),
     skip byte, then 3-byte frame records (delay, skip, image) terminated
     by 0xFF.
   After parsing, some timing values are patched to better match the
   original ZX Spectrum playback speed. */
void LoadKaylethAnimationData(void)
{
    KaylethAnimations = MemAlloc(sizeof(KaylethAnimation *) * NUMBER_OF_KAYLETH_ANIMATIONS);
    for (int i = 0; i < NUMBER_OF_KAYLETH_ANIMATIONS; i++)
        KaylethAnimations[i] = NULL;
    /* No animation data was found in this file: no room is animated */
    if (AnimationData == 0 || AnimationData >= FileImageLen)
        return;
    uint8_t *ptr = &FileImage[AnimationData];
    uint8_t *end = FileImage + FileImageLen;
    int counter = 0;
    while (counter < NUMBER_OF_KAYLETH_ANIMATIONS && ptr < end) {
        while (counter < NUMBER_OF_KAYLETH_ANIMATIONS && ptr < end && *ptr == 0xff) {
            counter++;
            ptr++;
        }
        /* An entry is a 3-byte header and at least a terminator */
        if (counter < NUMBER_OF_KAYLETH_ANIMATIONS && end - ptr > 3) {
            KaylethAnimation *anim = MemAlloc(sizeof(KaylethAnimation));
            anim->loop_to = *ptr / 3;
            ptr++;
            anim->current_frame = 0;
            anim->required_object = *ptr;
            ptr += 2; /* Skip the runtime counter byte (always zero in file) */
            /* Count frames by scanning ahead for the 0xFF terminator */
            anim->number_of_frames = 0;
            for (int i = 0; ptr + i < end && ptr[i] != 0xff; i += 3) {
                anim->number_of_frames++;
            }
            /* Truncated data: the last frame record is incomplete */
            if (ptr + anim->number_of_frames * 3 > end)
                anim->number_of_frames = (int)((end - ptr) / 3);
            anim->frames = MemAlloc(anim->number_of_frames * sizeof(KaylethAnimationFrame));
            for (int i = 0; i < anim->number_of_frames; i++) {
                anim->frames[i].counter_to_draw_at = *ptr;
                anim->frames[i].counter = 0;
                ptr += 2; /* Skip the runtime counter byte */
                anim->frames[i].image = *ptr++;
            }

            /* Patch Azap chamber timing to better match original speed */
            if (anim->number_of_frames > 4 && anim->frames[0].image == AZAP_FIRST_IMAGE) {
                anim->frames[1].counter_to_draw_at = 5;
                anim->frames[3].counter_to_draw_at = 5;
                anim->frames[4].counter_to_draw_at = 8;
            }

            if (anim->loop_to >= anim->number_of_frames)
                anim->loop_to = 0;

            KaylethAnimations[counter++] = anim;

            ptr++; /* Skip the 0xFF terminator */
        }
    }

    /* Patch assembly line (room 2) timing to match original speed */
    KaylethAnimation *belt = KaylethAnimations[2];
    if (belt != NULL && belt->number_of_frames > 2) {
        belt->frames[0].counter_to_draw_at = 10;
        belt->frames[1].counter_to_draw_at = 20;
        belt->frames[2].counter_to_draw_at = 20;
    }
}

/* Advance the Kayleth animation for the current room by one tick.
   Returns 1 if the current room has an animation, 0 otherwise.
   Object 0 is temporarily placed in the room so room-image drawing
   can reference it. If the destroyer droid is pursuing (Flag[10] > 1),
   the droid slot is also temporarily placed here. */
static int UpdateKaylethAnimationFrames(void)
{
    if (KaylethAnimations == NULL || MyLoc >= NUMBER_OF_KAYLETH_ANIMATIONS
        || KaylethAnimations[MyLoc] == NULL) {
        return 0;
    }

    int obj0loc = ObjectLoc[0];
    ObjectLoc[0] = MyLoc;
    if (Flag[10] > 1)
        ObjectLoc[KAYLETH_DROID_SLOT] = MyLoc;

    KaylethAnimation *anim = KaylethAnimations[MyLoc];

    /* Reset animation sequence when entering a new room */
    if (KaylethAnimationIndex != MyLoc) {
        KaylethAnimationIndex = MyLoc;
        anim->current_frame = 0;
    }

    if (anim->current_frame < anim->number_of_frames
        && ObjectLoc[anim->required_object] == MyLoc) {

        KaylethAnimationFrame *frame = &anim->frames[anim->current_frame];

        if (frame->counter >= frame->counter_to_draw_at) {
            DrawTaylor(frame->image, MyLoc);
            DrawBufferedPicture();
            anim->current_frame++;
            frame->counter = 0;
            if (anim->current_frame >= anim->number_of_frames) {
                anim->current_frame = anim->loop_to;
            }
        } else {
            frame->counter++;
        }
    }

    ObjectLoc[0] = obj0loc;
    ObjectLoc[KAYLETH_DROID_SLOT] = Destroyed();
    return 1;
}

/* Timer callback for all Kayleth animations. Drives the per-room frame
   animation and, in room 3, the click-shelf scrolling effect. The shelf
   animation only advances on 7 out of every 9 ticks, reproducing the
   jerky cadence of the original ZX Spectrum version. */
void UpdateKaylethAnimations(void)
{
    ClickShelfStage++;
    if (ClickShelfStage == 9)
        ClickShelfStage = 0;
    UpdateKaylethAnimationFrames();
    if (MyLoc == 3) {
        AnimateKaylethClickShelves(ShelfScrollStage);
        if (ClickShelfStage < 7)
            ShelfScrollStage++;
        if (ShelfScrollStage > 80)
            ShelfScrollStage = 0;
    }
}

/* Timer callback for all Rebel Planet animations. Dispatches to the
   appropriate animation based on the current room and object presence.
   Stops the timer when the player leaves an animated room. */
void UpdateRebelAnimations(void)
{
    if (MyLoc == 1 && ObjectLoc[UNFOLDING_SPACE] == 1) {
        AnimateStars();
    } else if (MyLoc == 86 && ObjectLoc[42] == 86) {
        AnimateForcefield();
    } else if (MyLoc == 88 && ObjectLoc[107] == 88) {
        AnimateQueenComputer();
    } else if (MyLoc > 28 && MyLoc < 34) {
        /* Sycane Serpent in sewers (rooms 29–33). SerpentStage tracks
           how far the serpent has emerged (0–5). When the serpent object
           is present it grows; when absent it retreats, speeding up as
           it nears full retraction. */
        if (ObjectLoc[92] == MyLoc) {
            SerpentStage++;
            if (SerpentStage > 5) {
                SerpentStage = 5;
                StopAnimations();
            }
        } else {
            SerpentStage--;
            if (SerpentStage < 0) {
                SerpentStage = 0;
                StopAnimations();
            } else if (SerpentStage == 4) {
                glk_request_timer_events(50);
            }
            DrawTaylor(MyLoc, MyLoc);
        }
        if (SerpentStage) {
            /* Draw serpent overlay; higher stages use earlier image blocks
               and move higher up the screen */
            DrawPictureAtPos(62 + SerpentStage, 14, 10 - SerpentStage - (SerpentStage > 2), 1);
        }
        DrawBufferedPicture();
    } else if (MyLoc == 50 && ObjectLoc[58] == 50) {
        /* Killer security robot: alternates between two frames */
        DrawPictureAtPos(138 + TwoFrameStage, 13, 2, 0);
        TwoFrameStage = (TwoFrameStage == 0);
    } else if (MyLoc == 71 && ObjectLoc[36] == 71) {
        /* Crag snapper: alternates between two poses at different positions */
        if (!TwoFrameStage)
            DrawPictureAtPos(133, 14, 4, 0);
        else
            DrawPictureAtPos(142, 17, 6, 0);
        TwoFrameStage = (TwoFrameStage == 0);
    } else {
        /* No animation for this room — stop the timer */
        StopAnimations();
        QueenStage = SerpentStage = TwoFrameStage = 0;
    }
}

/* Start or adjust the Glk timer for the animation in the current room.
   Called after drawing the room image. Sets the timer interval appropriate
   for the room's animation and, for some Rebel Planet rooms, kicks off
   the first animation update immediately. For Kayleth, also handles
   per-room speed overrides for specific locations. */
void StartAnimations(void)
{
    if (BaseGame == REBEL_PLANET) {
        /* The serpent's timer stops once it is fully out, so nothing else
           would reset it: without this it would be seen withdrawing on the
           next visit to the sewers. */
        if (MyLoc <= 28 || MyLoc >= 34)
            SerpentStage = 0;
        if (MyLoc == 1 && ObjectLoc[UNFOLDING_SPACE] == 1) {
            int rate = STARS_ANIMATION_RATE;
            if (CurrentGame == REBEL_PLANET_64)
                rate = STARS_ANIMATION_RATE_64;
            SetAnimationRate(rate);
        } else if (MyLoc == 86 && ObjectLoc[42] == 86) {
            // Force field
            SetAnimationRate(FIELD_ANIMATION_RATE);
        } else if (MyLoc == 88 && ObjectLoc[107] == 88) {
            // Queen computer
            SetAnimationRate(QUEEN_ANIMATION_RATE);
        } else if (MyLoc > 28 && MyLoc < 34) {
            // Sycane Serpent in sewers
            glk_request_timer_events(SERPENT_ANIMATION_RATE);
            UpdateRebelAnimations();
        } else if (MyLoc == 50 && ObjectLoc[58] == 50) {
            // Killer security robot in museum passage
            glk_request_timer_events(ROBOT_ANIMATION_RATE);
        } else if (MyLoc == 71 && ObjectLoc[36] == 71) {
            // Crag snapper in cave
            glk_request_timer_events(SNAPPER_ANIMATION_RATE);
        }
    } else if (BaseGame == KAYLETH) {
        int speed = 0;
        if (UpdateKaylethAnimationFrames()) {
            speed = 20;
            KaylethAnimationIndex = 0;
        }
        if (ObjectLoc[0] == MyLoc && CurrentGame == KAYLETH_64) { // Azap chamber
            speed = 30;
        }
        switch (MyLoc) {
        case 1:
            speed = 14;
            break;
        case 2: // Conveyor belt
            speed = 10;
            break;
        case 3: // Click shelves
            if (CurrentGame == KAYLETH_64)
                speed = 80;
            else
                speed = 40;
            break;
        case 32: // Videodrome
            speed = 50;
            break;
        case 36: // Guard dome
            speed = 12;
            break;
        case 52:
        case 53: // Twin peril forest
            if (ObjectLoc[30] == MyLoc) {
                speed = 150;
                UpdateKaylethAnimationFrames();
            }
            break;
        case 56: // Citadel of Zenron
            speed = 40;
            break;
        default:
            break;
        }
        SetAnimationRate(speed);
    }
}

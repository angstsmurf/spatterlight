//
//  animations.c
//  plus
//
//  Created by Administrator on 2022-03-24.
//
#include <stdlib.h>
#include <string.h>
#include "definitions.h"
#include "glk.h"
#include "common.h"
#include "graphics.h"
#include "animations.h"

#define ANIMATION_RATE 200
#define CANNON_RATE 300
#define MAX_ANIM_FRAMES 32

int AnimationRunning = 0;
int AnimationBackground = 0;
int LastAnimationBackground = 0;

static int AnimationStage = 0;
static int StopNext = 0;
static int ImgTail = 0;
static int FramesTail = 0;


static char *AnimationFilenames[MAX_ANIM_FRAMES];
static int AnimationFrames[MAX_ANIM_FRAMES];

/*
 The "Fire cannon at blob" animation in Fantastic Four is actually two
 animations sequences, one after another, which conflicts with our
 animation system, so we use a hack to stitch them together, basically
 adding the new frames to the end of the buffer rather than clearing it
 and starting over.
 */

static int PostCannonAnimationSeam = 0;
static int CannonAnimationPause = 0;

void AddImageToBuffer(char *basename) {
    if (ImgTail >= MAX_ANIM_FRAMES - 1)
        return;
    size_t len = strlen(basename) + 1;
    if (AnimationFilenames[ImgTail] != NULL)
        free(AnimationFilenames[ImgTail]);
    AnimationFilenames[ImgTail] = MemAlloc((int)len);
    debug_print("AddImageToBuffer: Setting AnimationFilenames[%d] to %s\n", ImgTail, basename);
    memcpy(AnimationFilenames[ImgTail++], basename, len);
    AnimationFilenames[ImgTail] = NULL;
}

void AddFrameToBuffer(int frameidx) {
    if (FramesTail >= MAX_ANIM_FRAMES - 1)
        return;
    debug_print("AddFrameToBuffer: Setting AnimationFrames[%d] to %d\n", FramesTail, frameidx);
    AnimationFrames[FramesTail++] = frameidx;
    AnimationFrames[FramesTail] = -1;
}

void AddRoomImage(int image) {
    char *shortname = ShortNameFromType('R', image);
    AddImageToBuffer(shortname);
    free(shortname);
}

void AddItemImage(int image) {
    char *shortname = ShortNameFromType('B', image);
    AddImageToBuffer(shortname);
    free(shortname);
}

void AddSpecialImage(int image) {
    char *shortname = ShortNameFromType('S', image);
    AddImageToBuffer(shortname);
    free(shortname);
}

void Animate(int frame) {

    int cannonanimation = (CurrentGame == FANTASTIC4 && MyLoc == 5);

    if (PostCannonAnimationSeam) {
        AddFrameToBuffer(10 + frame);
        return;
    }

    AddFrameToBuffer(frame);
    if (!AnimationRunning) {

        if (!AnimationBackground)
            LastAnimationBackground = 0;

        if (cannonanimation)
            glk_request_timer_events(CANNON_RATE);
        else
            glk_request_timer_events(ANIMATION_RATE);

        AnimationRunning = 1;
        AnimationStage = 0;
    }

    if (cannonanimation && frame == 9)
        PostCannonAnimationSeam = 1;
}

void ClearAnimationBuffer(void) {

    if (PostCannonAnimationSeam)
        return;

    for (int i = 0; i < MAX_ANIM_FRAMES && AnimationFilenames[i] != NULL; i++) {
        if (AnimationFilenames[i] != NULL)
            free(AnimationFilenames[i]);
        AnimationFilenames[i] = NULL;
    }
    ImgTail = 0;
    FramesTail = 0;
}

void ClearFrames(void) {
    for (int i = 0; i < FramesTail; i++) {
        AnimationFrames[i] = -1;
    }
    FramesTail = 0;
}

void InitAnimationBuffer(void) {
    for (int i = 0; i < MAX_ANIM_FRAMES; i++) {
        AnimationFilenames[i] = NULL;
        AnimationFrames[i] = -1;
    }
}

void StopAnimation(void) {
    glk_request_timer_events(0);
    debug_print("StopAnimation: stopped timer\n");
    AnimationStage = 0;
    AnimationRunning = 0;
    PostCannonAnimationSeam = 0;
}

void UpdateAnimation(void) // Draw animation frame
{
    if (CannonAnimationPause) {
        glk_request_timer_events(CANNON_RATE);
        CannonAnimationPause = 0;
    }

    if (StopNext) {
        StopNext = 0;
        if (AnimationFrames[AnimationStage] == -1)
            ClearFrames();
        StopAnimation();
        glk_window_clear(Graphics);
        DrawCurrentRoom();
        return;
    }

    if (AnimationStage >= MAX_ANIM_FRAMES - 1) {
        StopNext = 1;
        return;
    }

    if (AnimationBackground) {
        char buf[1024];
        sprintf(buf, "S0%02d", AnimationBackground);
        LastAnimationBackground = AnimationBackground;
        AnimationBackground = 0;
        DrawImageWithName(buf);
        return;
    }

    if (AnimationFrames[AnimationStage] != -1) {
        debug_print("UpdateAnimation: Drawing AnimationFrames[%d] (%d)\n", AnimationStage, AnimationFrames[AnimationStage]);
        if (!DrawImageWithName(AnimationFilenames[AnimationFrames[AnimationStage++]]))
            StopNext = 1;

        if (PostCannonAnimationSeam && AnimationStage == 10) {
            CannonAnimationPause = 1;
            glk_request_timer_events(2000);
        }

    } else {
        debug_print("StopNext\n");
        glk_request_timer_events(2000);
        StopNext = 1;
    }
}

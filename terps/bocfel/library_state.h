//
//  library_state.h
//  bocfel
//
//  Created by Administrator on 2021-01-06.
//

#ifndef library_state_h
#define library_state_h

#include <stdio.h>


typedef struct library_state_data_struct {
    int active; /* does this structure contain meaningful data? */
    int headerfixedfont;
    int wintag0;
    int wintag1;
    int wintag2;
    int wintag3;
    int wintag4;
    int wintag5;
    int wintag6;
    int wintag7;
    int curwintag;
    int mainwintag;
    int statuswintag;
    int upperwintag;
    long upperwinheight;
    long upperwinwidth;
    long upperwinx;
    long upperwiny;
    uint16_t fgcolor;
    int fgmode;
    uint16_t bgcolor;
    int bgmode;
    int style;
    uint16_t routine;
    int next_sample;
    int next_volume;
    bool locked;
    bool playing;
    int sound_channel_tag;
} library_state_data;

#endif /* library_state_h */

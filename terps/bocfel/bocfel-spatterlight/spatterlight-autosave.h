
/* spatterlight-autosave.h
 *
 * This file is part of Spatterlight.
 *
 * Copyright (c) 2021 Petter Sjölund, adapted from code by Andrew Plotkin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef SPATTERLIGHT_AUTOSAVE_H
#define SPATTERLIGHT_AUTOSAVE_H

#include "stack.h"
#include "v6_specific.h"

void spatterlight_do_autosave(enum SaveOpcode saveopcode);
bool spatterlight_restore_autosave(enum SaveOpcode *saveopcode);

struct JourneyWords {
    uint16_t pcm;
    uint16_t pcf;
    uint16_t str;
};

enum inputMode {
    INPUT_PARTY,
    INPUT_OBJECT,
    INPUT_SPECIAL,
    INPUT_ELVISH,
    INPUT_NAME,
};

typedef struct library_state_data_struct {
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
    int errorwintag;
    int upperwintag;
    int graphicswintag;

    uint16_t routine;
    int queued_sound;
    int sound_channel_tag;
    long last_random_seed;
    int random_calls_count;
    int queued_volume;

    int autosave_version;

    V6ScreenMode screenmode;

    int selected_journey_line;
    int selected_journey_column;
    inputMode current_input_mode;
    int current_input_length;
    int number_of_journey_words;
    struct JourneyWords journey_words[4];
} library_state_data;

void recover_library_state(library_state_data *dat);
void stash_library_state(library_state_data *dat);

#endif

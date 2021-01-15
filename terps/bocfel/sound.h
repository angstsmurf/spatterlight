// vim: set ft=c:

#ifndef ZTERP_SOUND_H
#define ZTERP_SOUND_H

#include <stdbool.h>

#include "library_state.h"

void stash_library_sound_state(library_state_data *dat);
void recover_library_sound_state(library_state_data *dat);

void init_sound(void);
bool sound_loaded(void);

void zsound_effect(void);

void end_of_sound(void);

#endif

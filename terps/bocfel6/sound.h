// vim: set ft=cpp:

#ifndef ZTERP_SOUND_H
#define ZTERP_SOUND_H

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
}
#ifdef SPATTERLIGHT
#include "spatterlight-autosave.h"
#endif
#endif

#include "types.h"

#ifdef GLK_MODULE_SOUND2
void sound_stopped(glui32 chantype);
uint16_t sound_get_routine(glui32 chantype);
#endif

void init_sound();
bool sound_loaded();

void zsound_effect();

#ifdef SPATTERLIGHT
void stash_library_sound_state(library_state_data *dat);
void recover_library_sound_state(library_state_data *dat);
#endif

#endif

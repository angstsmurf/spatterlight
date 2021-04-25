// vim: set ft=c:

#ifndef ZTERP_SOUND_H
#define ZTERP_SOUND_H

#include <stdbool.h>
#include <stdint.h>

#include "spatterlight-autosave.h"

void stash_library_sound_state(library_state_data *dat);
void recover_library_sound_state(library_state_data *dat);

#ifdef ZTERP_GLK
#include "glk.h"
#endif

#ifdef GLK_MODULE_SOUND
extern uint16_t routine;
void sound_stopped(void);
#endif

void init_sound(void);
bool sound_loaded(void);

void zsound_effect(void);

#endif

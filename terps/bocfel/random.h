// vim: set ft=cpp:

#ifndef ZTERP_MT_H
#define ZTERP_MT_H

#include "iff.h"
#include "io.h"

void init_random(bool first_run);
IFF::TypeID random_write_rand(IO &io);
void random_read_rand(IO &io);

void zrandom();

#ifdef SPATTERLIGHT
uint32_t random_autosave_state(bool *predictable);
void random_autorestore_state(uint32_t state, bool predictable);
#endif

#endif

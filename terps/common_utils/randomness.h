//  randomness.h
//  The seedable random number generator of Glulxe, shared by many terps.
//  terps/scarier/common_utils has a byte-identical copy: after an edit, copy
//  this file there and run `make -f Makefile.headless vendorcheck` in scarier.

#ifndef randomness_h
#define randomness_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Seed 0 selects the native generator of randomness.c; any other seed the
   xoshiro128** one, whose sequence is the same on every platform. */
void set_erkyrath_random(uint32_t seed);
uint32_t erkyrath_random(void);
void erkyrath_random_get_detstate(int *usenative, uint32_t **arr, int *count);
void erkyrath_random_set_detstate(int usenative, uint32_t *arr, int count);

#ifdef __cplusplus
}
#endif

#endif /* randomness_h */

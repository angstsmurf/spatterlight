/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- randomness.
 *
 * v5 randomness is routed through the shared erkyrath_random()
 * (terps/common_utils/randomness.c), the same seedable, cross-platform
 * generator the other Scarier engines (and the v3.8/3.9/4.0 path in
 * scutils.cpp) use.  a5rand_seed(1234) selects the deterministic xoshiro128**
 * sequence used for reproducible walkthroughs/regression scripts; seed 0
 * selects the platform-native RNG.  The first draw lazily seeds 1234 if the
 * caller never seeded, so the headless harnesses are deterministic by default.
 */

#ifndef SCARIER_A5RAND_H
#define SCARIER_A5RAND_H

#ifdef __cplusplus
extern "C" {
#endif

/* (Re)seed the v5 RNG.  Call once per game so each run is reproducible. */
extern void a5rand_seed (unsigned int seed);

/* A random integer in the inclusive range [lo, hi], mirroring frankendrift
   Global.Random(iMin, iMax) (which returns r.Next(iMin, iMax + 1)).  Swaps the
   bounds if hi < lo, exactly like frankendrift. */
extern long a5rand_between (long lo, long hi);

#ifdef __cplusplus
}
#endif

#endif

/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- randomness.  See a5rand.h.
 */

#include <stdint.h>

#include "a5rand.h"

/* The shared generator, defined in terps/common_utils/randomness.c.  Declared
   here (rather than including randomness.h) so the v5 engine does not pull in
   glk.h at this layer; glui32 is uint32_t. */
extern "C" {
  void   set_erkyrath_random (uint32_t seed);
  uint32_t erkyrath_random   (void);
}

static int a5rand_seeded = 0;

void
a5rand_seed (unsigned int seed)
{
  set_erkyrath_random ((uint32_t) seed);
  a5rand_seeded = 1;
}

long
a5rand_between (long lo, long hi)
{
  unsigned long span;
  long t;
  if (hi == lo)            /* a fixed value draws no RNG (mirrors a from==to
                              FromTo, and keeps the RAND() task sequence stable
                              regardless of how many fixed-length events tick) */
    return lo;
  if (!a5rand_seeded)
    {
      /* Deterministic by default, like the other Scarier headless engines. */
      set_erkyrath_random (1234u);
      a5rand_seeded = 1;
    }
  if (hi < lo) { t = lo; lo = hi; hi = t; }
  span = (unsigned long) (hi - lo) + 1UL;
  return lo + (long) (erkyrath_random () % span);
}

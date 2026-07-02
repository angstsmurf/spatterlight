/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- randomness.  See a5rand.h.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "a5rand.h"

/* The shared generator, defined in terps/common_utils/randomness.c.  Declared
   here (rather than including randomness.h) so the v5 engine does not pull in
   glk.h at this layer; glui32 is uint32_t. */
extern "C" {
  void   set_erkyrath_random (uint32_t seed);
  uint32_t erkyrath_random   (void);
  void erkyrath_random_get_detstate (int *usenative, uint32_t **arr, int *count);
  void erkyrath_random_set_detstate (int usenative, uint32_t *arr, int count);
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
  {
    long r = lo + (long) (erkyrath_random () % span);
    /* A5_TRACE_RAND=1: print every draw, for aligning the stream against an
       equally-instrumented FrankenDrift XoshiroRandom (FD_RNG_TRACE=1). */
    static int trace = -1;
    if (trace < 0)
      trace = getenv ("A5_TRACE_RAND") != NULL;
    if (trace)
      fprintf (stderr, "RAND(%ld,%ld)=%ld\n", lo, hi, r);
    return r;
  }
}

void
a5rand_get_state (int *native, unsigned int state[4])
{
  uint32_t *arr = NULL;
  int count = 0, n = 0, i;
  erkyrath_random_get_detstate (&n, &arr, &count);
  if (native != NULL)
    *native = n;
  for (i = 0; i < 4; i++)
    state[i] = (arr != NULL && i < count) ? (unsigned int) arr[i] : 0u;
}

void
a5rand_set_state (int native, const unsigned int state[4])
{
  uint32_t arr[4];
  int i;
  for (i = 0; i < 4; i++)
    arr[i] = (uint32_t) state[i];
  erkyrath_random_set_detstate (native, arr, 4);
  a5rand_seeded = 1;
}

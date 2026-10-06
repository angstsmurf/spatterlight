/* The random generator of the headless build: the interpreter's own
   (common_utils/randomness.c, which the Makefile compiles here with its two
   entry points renamed to xo_*), or, with SCOTT_OLD_RANDOM set, the one the
   interpreter had before February 2026.

   Some of the command scripts in test/scripts were written in the app back
   then, with the Determinism option on: srand(1234) and the C library's
   rand(). Which turn the bell tolls on in The Count, or where a ship attacks
   in Seas of Blood, is decided by those numbers, so the scripts only play to
   the end with them (test/scripts/README.md).

   rand() is spelled out, as macOS has it, so that the numbers are the same
   on every system. */

#include <stdint.h>
#include <stdlib.h>

#include "glk.h"

glui32 xo_erkyrath_random(void);
void xo_set_erkyrath_random(glui32 seed);

static int old = -1;
static unsigned long state;

static int old_rand(void)
{
    long hi = state / 127773;
    long lo = state % 127773;
    long x = 16807 * lo - 2836 * hi;
    if (x < 0)
        x += 0x7fffffff;
    state = x;
    return x % 0x80000000;
}

void set_erkyrath_random(glui32 seed)
{
    if (old < 0)
        old = getenv("SCOTT_OLD_RANDOM") != NULL;
    /* rand() cannot start from 0, and neither could the old code: seed 0 is
       "seed from the clock", which the headless build never asks for. */
    state = seed ? seed : 123459876;
    xo_set_erkyrath_random(seed);
}

glui32 erkyrath_random(void)
{
    if (old <= 0)
        return xo_erkyrath_random();
    /* The old RandomPercent() took ((uint64_t)rand() << 6 & 0xffffffff) % 100,
       and today's takes erkyrath_random() % 100. Nothing else in a headless
       run draws a number: the dice of Seas of Blood are not rolled (AUTOWIN),
       and the fish of the SAGA pictures are not drawn. */
    return (glui32)((uint64_t)old_rand() << 6);
}

//  debugprint.h
//  debug_print(fmt, ...) prints to stderr if DEBUG_PRINT is non-zero. Define
//  DEBUG_PRINT to 1 before including this, or with -D, to turn it on. When off,
//  the call still compiles, so its format string and arguments cannot go stale,
//  and the optimizer removes it.

#ifndef DEBUG_PRINT
#define DEBUG_PRINT 0
#endif

#ifndef debug_print

#include <stdio.h>

#define debug_print(...)                     \
    do {                                     \
        if (DEBUG_PRINT)                     \
            fprintf(stderr, __VA_ARGS__);    \
    } while (0)

#endif /* debug_print */

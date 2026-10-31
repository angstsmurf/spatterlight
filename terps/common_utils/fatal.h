//  fatal.h
//  The prototype of Fatal(), for code that reports errors but has no other
//  use for Glk. The function itself is in common_utils.c, or in the stub of a
//  headless tool.

#ifndef fatal_h
#define fatal_h

#if defined(__GNUC__) || defined(__clang__)
#define FATAL_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#define FATAL_NORETURN __declspec(noreturn)
#else
#define FATAL_NORETURN
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Prints x and an exclamation mark, then ends the program */
FATAL_NORETURN void Fatal(const char *x);

#ifdef __cplusplus
}
#endif

#endif /* fatal_h */

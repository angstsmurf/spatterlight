//  common_utils.c
//  Glk helpers of the ScottFree family.

#include <stdio.h>

#include "common_utils.h"

/* Supplied by the terp: see common_utils.h */
extern winid_t Bottom;
GLK_ATTRIBUTE_NORETURN void CleanupAndExit(void);
void Display(winid_t w, const char *fmt, ...);

void Fatal(const char *x)
{
    fprintf(stderr, "%s!\n", x);
    if (!Bottom) {
        Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
    }
    if (Bottom)
        Display(Bottom, "%s!\n", x);
    CleanupAndExit();
}

winid_t FindGlkWindowWithRock(glui32 rock)
{
    winid_t win;
    glui32 rockptr;
    for (win = glk_window_iterate(NULL, &rockptr); win;
         win = glk_window_iterate(win, &rockptr)) {
        if (rockptr == rock)
            return win;
    }
    return 0;
}

#include "glkimp.h"

void glk_stylehint_set(glui32 wintype, glui32 styl, glui32 hint, glsi32 val)
{
    win_stylehint(wintype, styl, hint, val);
}

void glk_stylehint_clear(glui32 wintype, glui32 styl, glui32 hint)
{
    win_clearhint(wintype, styl, hint);
}

glui32 glk_style_distinguish(winid_t win, glui32 styl1, glui32 styl2)
{
    /* Styles are never distinguishable. */
    return FALSE;
}

glui32 glk_style_measure(winid_t win, glui32 styl, glui32 hint, glui32 *result)
{
    /* We can't measure any style attributes. */
    return FALSE;
}

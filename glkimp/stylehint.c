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
    glui32 res;

    if (win->type != wintype_TextGrid && win->type != wintype_TextBuffer)
    {
        gli_strict_warning("glk_style_measure called with invalid window type!");
        return FALSE;
    }

    switch (hint)
    {
        case stylehint_Indentation:
        case stylehint_ParaIndentation:
            *result = 0;
            return TRUE;
            
        case stylehint_Justification:
            *result = stylehint_just_LeftFlush;
            return TRUE;
            
        case stylehint_Size:
            *result = 1;
            return TRUE;

        default:
            res = win_style_measure(win->peer, styl, hint, result);
            return res;
    }
}

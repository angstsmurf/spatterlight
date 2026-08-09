/* cssbasic.c: Glk CSS Basic extension (provisional codes 0x1220–0x1226). */

#include "glkimp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GLK_MODULE_CSS_BASIC

void glk_css_hint_set(glui32 wintype, glui32 styl, glui32 par_or_span,
    char *prop, glui32 proplen, char *val, glui32 vallen)
{
    if (!prop || !proplen)
        return;
    win_css_hint(wintype, styl, par_or_span, prop, proplen, val, vallen);
}

void glk_css_hint_set_num(glui32 wintype, glui32 styl, glui32 par_or_span,
    char *prop, glui32 proplen, glsi32 val)
{
    char numbuf[32];
    int n;

    if (!prop || !proplen)
        return;
    n = snprintf(numbuf, sizeof numbuf, "%ld", (long)val);
    if (n < 0)
        return;
    win_css_hint(wintype, styl, par_or_span, prop, proplen, numbuf, (glui32)n);
}

void glk_css_hint_clear(glui32 wintype, glui32 styl, glui32 par_or_span,
    char *prop, glui32 proplen)
{
    if (!prop || !proplen)
        return;
    win_css_hint_clear(wintype, styl, par_or_span, prop, proplen);
}

void glk_css_hint_clear_all(glui32 wintype, glui32 styl)
{
    win_css_hint_clear_all(wintype, styl);
}

void glk_css_inline_set(char *prop, glui32 proplen, char *val, glui32 vallen)
{
    stream_t *str = glk_stream_get_current();

    if (!prop || !proplen)
        return;
    if (!str || !str->writable || str->type != strtype_Window || !str->win)
        return;
    win_css_inline_set(str->win->peer, prop, proplen, val, vallen);
}

void glk_css_inline_set_num(char *prop, glui32 proplen, glsi32 val)
{
    char numbuf[32];
    int n;
    stream_t *str = glk_stream_get_current();

    if (!prop || !proplen)
        return;
    if (!str || !str->writable || str->type != strtype_Window || !str->win)
        return;
    n = snprintf(numbuf, sizeof numbuf, "%ld", (long)val);
    if (n < 0)
        return;
    win_css_inline_set(str->win->peer, prop, proplen, numbuf, (glui32)n);
}

void glk_css_inline_clear(char *prop, glui32 proplen)
{
    stream_t *str = glk_stream_get_current();

    if (!prop || !proplen)
        return;
    if (!str || !str->writable || str->type != strtype_Window || !str->win)
        return;
    win_css_inline_clear(str->win->peer, prop, proplen);
}

#endif /* GLK_MODULE_CSS_BASIC */

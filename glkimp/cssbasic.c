/* cssbasic.c: Glk CSS Basic extension (codes 0x1110–0x111C). */

#include "glkimp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GLK_MODULE_CSS_BASIC

void glk_css_hint_set(glui32 wintype, glui32 styl, glui32 span_or_par,
    const char *prop, glui32 proplen, const char *val, glui32 vallen)
{
    if (!prop || !proplen)
        return;
    win_css_hint(wintype, styl, span_or_par, prop, proplen, val, vallen);
}

void glk_css_hint_set_num(glui32 wintype, glui32 styl, glui32 span_or_par,
    const char *prop, glui32 proplen, glsi32 val)
{
    char numbuf[32];
    int n;

    if (!prop || !proplen)
        return;
    n = snprintf(numbuf, sizeof numbuf, "%ld", (long)val);
    if (n < 0)
        return;
    win_css_hint(wintype, styl, span_or_par, prop, proplen, numbuf, (glui32)n);
}

void glk_css_hint_clear(glui32 wintype, glui32 styl, glui32 span_or_par,
    const char *prop, glui32 proplen)
{
    if (!prop || !proplen)
        return;
    win_css_hint_clear(wintype, styl, span_or_par, prop, proplen);
}

void glk_css_hint_selector_set(glui32 wintype, const char *sel, glui32 sellen,
    const char *prop, glui32 proplen, const char *val, glui32 vallen)
{
    if (!prop || !proplen)
        return;
    if (!sel)
        sellen = 0;
    win_css_hint_selector(wintype, sel, sellen, prop, proplen, val, vallen);
}

void glk_css_hint_selector_set_num(glui32 wintype, const char *sel, glui32 sellen,
    const char *prop, glui32 proplen, glsi32 val)
{
    char numbuf[32];
    int n;

    if (!prop || !proplen)
        return;
    if (!sel)
        sellen = 0;
    n = snprintf(numbuf, sizeof numbuf, "%ld", (long)val);
    if (n < 0)
        return;
    win_css_hint_selector(wintype, sel, sellen, prop, proplen, numbuf, (glui32)n);
}

void glk_css_hint_selector_clear(glui32 wintype, const char *sel, glui32 sellen,
    const char *prop, glui32 proplen)
{
    if (!prop || !proplen)
        return;
    if (!sel)
        sellen = 0;
    win_css_hint_selector_clear(wintype, sel, sellen, prop, proplen);
}

void glk_css_inline_set(glui32 span_or_par, const char *prop, glui32 proplen,
    const char *val, glui32 vallen)
{
    stream_t *str = glk_stream_get_current();

    if (!prop || !proplen)
        return;
    if (!str || !str->writable || str->type != strtype_Window || !str->win)
        return;
    win_css_inline_set(str->win->peer, span_or_par, prop, proplen, val, vallen);
}

void glk_css_inline_set_num(glui32 span_or_par, const char *prop, glui32 proplen,
    glsi32 val)
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
    win_css_inline_set(str->win->peer, span_or_par, prop, proplen, numbuf, (glui32)n);
}

void glk_css_inline_clear(glui32 span_or_par, const char *prop, glui32 proplen)
{
    stream_t *str = glk_stream_get_current();

    if (!prop || !proplen)
        return;
    if (!str || !str->writable || str->type != strtype_Window || !str->win)
        return;
    win_css_inline_clear(str->win->peer, span_or_par, prop, proplen);
}

void glk_css_hint_clear_all_by_style(glui32 wintype, glui32 styl)
{
    win_css_hint_clear_all_by_style(wintype, styl);
}

void glk_css_hint_clear_all_by_selector(glui32 wintype, const char *sel,
    glui32 sellen)
{
    if (!sel)
        sellen = 0;
    win_css_hint_clear_all_by_selector(wintype, sel, sellen);
}

void glk_css_hint_clear_all_by_window(glui32 wintype)
{
    win_css_hint_clear_all_by_window(wintype);
}

void glk_css_hint_clear_all_inline(void)
{
    stream_t *str = glk_stream_get_current();

    if (!str || !str->writable || str->type != strtype_Window || !str->win)
        return;
    win_css_hint_clear_all_inline(str->win->peer);
}

#endif /* GLK_MODULE_CSS_BASIC */

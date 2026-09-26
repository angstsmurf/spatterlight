/* cssbasic.c: Glk CSS Basic (0x1110–0x1118) and CSS Supports (0x1119–0x111A). */

#include "glkimp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GLK_MODULE_CSS_BASIC

void glk_css_hint_set(glui32 wintype, glui32 csstarget, glui32 style,
    const char *prop, glui32 proplen, const char *val, glui32 vallen)
{
    if (!prop || !proplen)
        return;
    win_css_hint(wintype, csstarget, style, prop, proplen, val, vallen);
}

void glk_css_hint_set_num(glui32 wintype, glui32 csstarget, glui32 style,
    const char *prop, glui32 proplen, glsi32 val)
{
    char numbuf[32];
    int n;

    if (!prop || !proplen)
        return;
    n = snprintf(numbuf, sizeof numbuf, "%ld", (long)val);
    if (n < 0)
        return;
    win_css_hint(wintype, csstarget, style, prop, proplen, numbuf, (glui32)n);
}

void glk_css_hint_clear(glui32 wintype, glui32 csstarget, glui32 style,
    const char *prop, glui32 proplen)
{
    if (!prop || !proplen)
        return;
    win_css_hint_clear(wintype, csstarget, style, prop, proplen);
}

void glk_css_inline_set(glui32 csstarget, const char *prop, glui32 proplen,
    const char *val, glui32 vallen)
{
    stream_t *str = glk_stream_get_current();

    if (!prop || !proplen)
        return;
    if (!str || !str->writable || str->type != strtype_Window || !str->win)
        return;
    win_css_inline_set(str->win->peer, csstarget, prop, proplen, val, vallen);
}

void glk_css_inline_set_num(glui32 csstarget, const char *prop, glui32 proplen,
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
    win_css_inline_set(str->win->peer, csstarget, prop, proplen, numbuf, (glui32)n);
}

void glk_css_inline_clear(glui32 csstarget, const char *prop, glui32 proplen)
{
    stream_t *str = glk_stream_get_current();

    if (!prop || !proplen)
        return;
    if (!str || !str->writable || str->type != strtype_Window || !str->win)
        return;
    win_css_inline_clear(str->win->peer, csstarget, prop, proplen);
}

void glk_css_hint_clear_all_by_style(glui32 wintype, glui32 style)
{
    win_css_hint_clear_all_by_style(wintype, style);
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

#ifdef GLK_MODULE_CSS_SUPPORTS

/* Hardcoded probe matching GlkCSSBasic.m (+ CSS Basic profile).
 * Answers are local/synchronous — no RemGlk round-trip. */

static int css_is_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

/* Copy [buf,buf+len), trim ASCII whitespace, lowercase ASCII letters.
 * Returns malloc'd NUL-terminated string, or NULL. */
static char *css_norm_copy(const char *buf, glui32 len)
{
    char *out;
    glui32 i, start = 0, end = len;

    if (!buf)
        return NULL;
    while (start < end && css_is_space((unsigned char)buf[start]))
        start++;
    while (end > start && css_is_space((unsigned char)buf[end - 1]))
        end--;
    out = malloc((size_t)(end - start) + 1);
    if (!out)
        return NULL;
    for (i = start; i < end; i++) {
        unsigned char c = (unsigned char)buf[i];
        out[i - start] = (char)((c >= 'A' && c <= 'Z') ? (c + 32) : c);
    }
    out[end - start] = '\0';
    return out;
}

static int css_streq(const char *a, const char *b)
{
    return a && b && strcmp(a, b) == 0;
}

static int css_known_property(const char *prop)
{
    return css_streq(prop, "color")
        || css_streq(prop, "background-color")
        || css_streq(prop, "-iftf-reverse-video")
        || css_streq(prop, "font-weight")
        || css_streq(prop, "font-style")
        || css_streq(prop, "font-size")
        || css_streq(prop, "font-family")
        || css_streq(prop, "text-decoration")
        || css_streq(prop, "text-decoration-line")
        || css_streq(prop, "text-align")
        || css_streq(prop, "margin-left")
        || css_streq(prop, "margin-right")
        || css_streq(prop, "text-indent")
        || css_streq(prop, "border-style");
}

static int css_is_named_color(const char *v)
{
    static const char *const names[] = {
        "transparent", "black", "silver", "gray", "grey", "white",
        "maroon", "red", "purple", "fuchsia", "green", "lime", "olive",
        "yellow", "navy", "blue", "teal", "aqua", "orange", "cyan",
        "magenta", "pink", NULL
    };
    int i;

    for (i = 0; names[i]; i++) {
        if (css_streq(v, names[i]))
            return 1;
    }
    return 0;
}

static int css_is_hex_digit(unsigned char c)
{
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'f');
}

/* #rgb / #rgba / #rrggbb / #rrggbbaa (lowercase hex already). */
static int css_is_hex_color(const char *v)
{
    size_t n;
    size_t i;

    if (!v || v[0] != '#')
        return 0;
    n = strlen(v + 1);
    if (n != 3 && n != 4 && n != 6 && n != 8)
        return 0;
    for (i = 1; i <= n; i++) {
        if (!css_is_hex_digit((unsigned char)v[i]))
            return 0;
    }
    return 1;
}

static int css_is_color(const char *v);

/* light-dark(<color>, <color>) — both args must themselves be colors. */
static int css_is_light_dark(const char *v)
{
    const char *inner;
    const char *end;
    const char *comma = NULL;
    const char *p;
    int depth = 0;
    size_t n;
    char *left = NULL;
    char *right = NULL;
    int ok = 0;

    if (!v || strncmp(v, "light-dark(", 11) != 0)
        return 0;
    n = strlen(v);
    if (n < 14 || v[n - 1] != ')')
        return 0;
    inner = v + 11;
    end = v + n - 1; /* points at ')' */
    for (p = inner; p < end; p++) {
        if (*p == '(')
            depth++;
        else if (*p == ')')
            depth--;
        else if (*p == ',' && depth == 0) {
            if (comma)
                return 0; /* more than two args */
            comma = p;
        }
    }
    if (!comma || depth != 0)
        return 0;

    left = css_norm_copy(inner, (glui32)(comma - inner));
    right = css_norm_copy(comma + 1, (glui32)(end - (comma + 1)));
    if (left && right && left[0] && right[0]
        && css_is_color(left) && css_is_color(right))
        ok = 1;
    free(left);
    free(right);
    return ok;
}

static int css_is_color(const char *v)
{
    if (!v || !v[0])
        return 0;
    if (css_is_named_color(v) || css_is_hex_color(v))
        return 1;
    return css_is_light_dark(v);
}

/* Spatterlight lengthValue: optional sign, number, optional px|pt|em|%. */
static int css_is_length(const char *v, int positive_only)
{
    const char *p;
    int saw_digit = 0;

    if (!v || !v[0])
        return 0;
    p = v;
    if (*p == '+' || *p == '-') {
        if (positive_only && *p == '-')
            return 0;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        saw_digit = 1;
        p++;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            saw_digit = 1;
            p++;
        }
    }
    if (!saw_digit)
        return 0;
    if (*p == '\0')
        return 1; /* unitless number (incl. 0), as GlkCSSBasic accepts */
    if (p[0] == 'p' && p[1] == 'x' && p[2] == '\0')
        return 1;
    if (p[0] == 'p' && p[1] == 't' && p[2] == '\0')
        return 1;
    if (p[0] == 'e' && p[1] == 'm' && p[2] == '\0')
        return 1;
    if (p[0] == '%' && p[1] == '\0')
        return 1;
    return 0;
}

static int css_contains_token(const char *hay, const char *needle)
{
    const char *p = hay;
    size_t nlen;

    if (!hay || !needle)
        return 0;
    nlen = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) {
        int before_ok = (p == hay) || css_is_space((unsigned char)p[-1]);
        int after_ok = (p[nlen] == '\0') || css_is_space((unsigned char)p[nlen]);
        if (before_ok && after_ok)
            return 1;
        p += nlen;
    }
    return 0;
}

static int css_value_supported(const char *prop, const char *val)
{
    if (css_streq(prop, "color") || css_streq(prop, "background-color"))
        return css_is_color(val);

    if (css_streq(prop, "-iftf-reverse-video")) {
        return css_streq(val, "reverse") || css_streq(val, "none")
            || css_streq(val, "1") || css_streq(val, "0")
            || css_streq(val, "true") || css_streq(val, "false")
            || css_streq(val, "yes") || css_streq(val, "no");
    }

    if (css_streq(prop, "font-weight")) {
        return css_streq(val, "normal") || css_streq(val, "bold")
            || css_streq(val, "400") || css_streq(val, "700")
            || css_streq(val, "bolder") || css_streq(val, "lighter");
    }

    if (css_streq(prop, "font-style")) {
        return css_streq(val, "normal") || css_streq(val, "italic")
            || css_streq(val, "oblique");
    }

    if (css_streq(prop, "font-size")) {
        if (css_streq(val, "small") || css_streq(val, "medium")
            || css_streq(val, "large") || css_streq(val, "larger")
            || css_streq(val, "smaller"))
            return 1;
        return css_is_length(val, 1); /* positive only */
    }

    if (css_streq(prop, "font-family")) {
        /* Match CSS.supports: any family list is accepted; do not probe fonts. */
        return val[0] != '\0';
    }

    if (css_streq(prop, "text-decoration")
        || css_streq(prop, "text-decoration-line")) {
        if (css_streq(val, "none"))
            return 1;
        return css_contains_token(val, "underline");
    }

    if (css_streq(prop, "text-align")) {
        return css_streq(val, "left") || css_streq(val, "right")
            || css_streq(val, "center") || css_streq(val, "justify");
    }

    if (css_streq(prop, "margin-left") || css_streq(prop, "margin-right")
        || css_streq(prop, "text-indent"))
        return css_is_length(val, 0);

    if (css_streq(prop, "border-style"))
        return css_streq(val, "solid") || css_streq(val, "none");

    return 0;
}

glui32 glk_css_supports(const char *prop, glui32 proplen,
    const char *val, glui32 vallen)
{
    char *nprop = NULL;
    char *nval = NULL;
    glui32 result = 0;

    if (!prop || !proplen)
        return 0;

    nprop = css_norm_copy(prop, proplen);
    if (!nprop || !nprop[0] || !css_known_property(nprop))
        goto done;

    if (vallen == 0 || !val) {
        /* Empty value: "is this property known?" */
        result = 1;
        goto done;
    }

    nval = css_norm_copy(val, vallen);
    if (!nval)
        goto done;

    /* font-family: preserve original (case / quotes). Re-copy without lowercasing. */
    if (css_streq(nprop, "font-family")) {
        free(nval);
        nval = NULL;
        {
            glui32 start = 0, end = vallen, i;
            char *raw;

            while (start < end && css_is_space((unsigned char)val[start]))
                start++;
            while (end > start && css_is_space((unsigned char)val[end - 1]))
                end--;
            raw = malloc((size_t)(end - start) + 1);
            if (!raw)
                goto done;
            for (i = start; i < end; i++)
                raw[i - start] = val[i];
            raw[end - start] = '\0';
            result = raw[0] != '\0' ? 1 : 0;
            free(raw);
            goto done;
        }
    }

    result = css_value_supported(nprop, nval) ? 1 : 0;

done:
    free(nprop);
    free(nval);
    return result;
}

glui32 glk_css_supports_num(const char *prop, glui32 proplen, glsi32 val)
{
    char numbuf[32];
    int n;

    n = snprintf(numbuf, sizeof numbuf, "%ld", (long)val);
    if (n < 0)
        return 0;
    return glk_css_supports(prop, proplen, numbuf, (glui32)n);
}

#endif /* GLK_MODULE_CSS_SUPPORTS */

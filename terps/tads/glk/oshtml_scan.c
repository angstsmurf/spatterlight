/* oshtml_scan.c -- reading HTML TADS markup: tag and attribute names,
   attributes one at a time, and the scanners that look a banner's text
   over before it is laid out */

#include <stddef.h>

#include "glk.h"
#include "oshtml_scan.h"

int is_alnum(glui32 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9');
}

int is_space(glui32 c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int tag_is(const glui32 *name, size_t len, const char *s)
{
    size_t i;

    for (i = 0 ; i < len && s[i] != '\0' ; ++i)
    {
        glui32 c = name[i];
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        if (c != (unsigned char)s[i])
            return 0;
    }
    return i == len && s[i] == '\0';
}

int name_in(const glui32 *name, size_t len, const char *const *list)
{
    for ( ; *list ; ++list)
        if (tag_is(name, len, *list))
            return 1;
    return 0;
}

int to_int(const glui32 *v, size_t len)
{
    int n = 0;
    size_t k;

    for (k = 0 ; k < len && v[k] >= '0' && v[k] <= '9' ; ++k)
        n = n * 10 + (int)(v[k] - '0');
    return n;
}

/* the attributes in t[s..e), s after the tag's name */
void attr_start(attr_iter *it, const glui32 *t, size_t s, size_t e,
                enum attr_reading how)
{
    it->t = t;
    it->i = s;
    it->e = e;
    it->how = how;
    it->name = it->namelen = it->v = it->vlen = 0;
}

/* the next attribute: false if there are no more */
int attr_next(attr_iter *it)
{
    const glui32 *t = it->t;
    size_t i = it->i, e = it->e;
    int raw = it->how == ATTR_RAW;

    while (i < e)
    {
        glui32 c = t[i];

        if (!raw && (c == '"' || c == '\''))
        {
            for (++i ; i < e && t[i] != c ; ++i)
                ;
            ++i;
            continue;
        }
        if (raw ? !is_alnum(c)
            : !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        {
            ++i;
            continue;
        }
        for (it->name = i ; i < e && is_alnum(t[i]) ; ++i)
            ;
        it->namelen = i - it->name;
        if (raw)
            while (i < e && is_space(t[i]))
                ++i;
        it->v = raw ? i : 0;
        it->vlen = 0;
        if (i < e && t[i] == '=')
        {
            ++i;
            if (raw)
                while (i < e && is_space(t[i]))
                    ++i;
            if (i < e && (t[i] == '"' || t[i] == '\''))
            {
                glui32 qu = t[i++];
                for (it->v = i ; i < e && t[i] != qu ; ++i)
                    ;
                it->vlen = i - it->v;
                if (i < e)
                    ++i;
            }
            else if (it->how != ATTR_WORD)
            {
                for (it->v = i ; i < e && !is_space(t[i]) ; ++i)
                    ;
                it->vlen = i - it->v;
            }
            else
            {
                it->v = i;
                if (i < e)
                    ++i;
                for ( ; i < e && is_alnum(t[i]) ; ++i)
                    ;
                it->vlen = i - it->v;
            }
        }
        it->i = i;
        return 1;
    }
    it->i = i;
    return 0;
}

/*
 *   The tag at t[*i], a '<': the text between its brackets is t[*s..*e),
 *   and *i moves past it.  False if it has no '>'.
 */
int raw_tag(const glui32 *t, size_t len, size_t *i,
            size_t *s, size_t *e)
{
    size_t k;
    glui32 quote = 0;

    for (k = *i + 1 ; k < len && (quote || t[k] != '>') ; ++k)
    {
        if (quote && t[k] == quote)
            quote = 0;
        else if (!quote && (t[k] == '"' || t[k] == '\''))
            quote = t[k];
    }
    if (k == len)
        return 0;
    *s = *i + 1;
    *e = k;
    *i = k + 1;
    return 1;
}

/* the name of the tag in t[s..e), its length, and whether it's an end */
size_t raw_name(const glui32 *t, size_t s, size_t e,
                size_t *name, int *end)
{
    while (s < e && is_space(t[s]))
        ++s;
    *end = s < e && (t[s] == '/' || t[s] == '\\');
    if (*end)
        for (++s ; s < e && is_space(t[s]) ; ++s)
            ;
    for (*name = s ; s < e && !is_space(t[s]) ; ++s)
        ;
    return s - *name;
}

/*
 *   The value of an attribute of the tag in t[s..e), read as ATTR_RAW: false
 *   if the tag hasn't got it.  *v and *vlen are left as the last
 *   attribute's.
 */
int raw_attr(const glui32 *t, size_t s, size_t e, const char *attr,
             size_t *v, size_t *vlen)
{
    attr_iter it;
    size_t name;
    int end;

    s = raw_name(t, s, e, &name, &end);
    attr_start(&it, t, s + name, e, ATTR_RAW);
    while (attr_next(&it))
    {
        *v = it.v;
        *vlen = it.vlen;
        if (tag_is(t + it.name, it.namelen, attr))
            return 1;
    }
    return 0;
}

int raw_int(const glui32 *t, size_t s, size_t e, const char *attr,
            int dflt)
{
    size_t v, vlen;
    return raw_attr(t, s, e, attr, &v, &vlen) ? to_int(t + v, vlen) : dflt;
}

/*
 *   Is the last thing in t[0..len) a table, or tables with only line breaks
 *   between them, with only line breaks and the ends of elements after it?
 *   Their text is t[*s..*e).  1 if so, 2 if it's an <IMG USEMAP> instead.
 *   If an <IMG USEMAP> comes right before the tables, *m is where it starts,
 *   otherwise (size_t)-1.
 */
int find_pictable(const glui32 *t, size_t len, size_t *s, size_t *e,
                  size_t *m)
{
    size_t v, vlen;
    size_t i = 0, ts, te, name, namelen, imgstart = (size_t)-1;
    int depth = 0, end, found = 0;

    *m = (size_t)-1;

    while (i < len)
    {
        size_t start = i;

        if (t[i] != '<')
        {
            if (depth == 0 && !is_space(t[i]))
                found = 0;
            ++i;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            return 0;
        namelen = raw_name(t, ts, te, &name, &end);
        if (tag_is(t + name, namelen, "table"))
        {
            if (!end && depth == 0 && found != 1)
                *m = found == 2 ? imgstart : (size_t)-1;
            if (!end && depth++ == 0 && found != 1)
                *s = start;
            else if (end && depth > 0 && --depth == 0)
            {
                *e = i;
                found = 1;
            }
        }
        else if (depth == 0 && !end && tag_is(t + name, namelen, "img")
                 && raw_attr(t, ts, te, "usemap", &v, &vlen))
        {
            *s = start;
            *e = i;
            imgstart = start;
            found = 2;
        }
        else if (depth == 0 && !end && !tag_is(t + name, namelen, "br"))
            found = 0;
    }
    return depth == 0 ? found : 0;
}

/* the tags a page of pictures can have, and those that lay nothing out */
static const char *const page_tags[] = {
    "body", "font", "map", "area", "center", "p", "br", "img", "bq",
    "blockquote", "sound", "div", "tab", NULL
};

int page_quiet_tag(const glui32 *t, size_t len)
{
    return tag_is(t, len, "body") || tag_is(t, len, "font")
        || tag_is(t, len, "map") || tag_is(t, len, "area")
        || tag_is(t, len, "sound") || tag_is(t, len, "div")
        || tag_is(t, len, "tab");
}

/*
 *   Is t[0..len) only centred pictures and line breaks, on a <BODY
 *   BACKGROUND>?  Its picture is t[*bg..*bg+*bglen).  Six's introduction.
 */
int find_picpage(const glui32 *t, size_t len, size_t *bg,
                 size_t *bglen)
{
    size_t i = 0, ts, te, name, namelen, v, vlen;
    int end, found = 0, pics = 0;

    while (i < len)
    {
        if (t[i] != '<')
        {
            if (!is_space(t[i]))
                return 0;
            ++i;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            return 0;
        namelen = raw_name(t, ts, te, &name, &end);
        if (!name_in(t + name, namelen, page_tags))
            return 0;
        if (!end && tag_is(t + name, namelen, "body"))
        {
            found = raw_attr(t, ts, te, "background", &v, &vlen) && vlen;
            if (found)
            {
                *bg = v;
                *bglen = vlen;
            }
        }
        else if (!end && tag_is(t + name, namelen, "img"))
            ++pics;
    }
    return found && pics;
}

/* is t[s..e) only tags and whitespace? */
int only_tags(const glui32 *t, size_t s, size_t e)
{
    size_t ts, te;

    while (s < e)
    {
        if (t[s] == '<')
        {
            if (!raw_tag(t, e, &s, &ts, &te))
                return 0;
        }
        else if (!is_space(t[s++]))
            return 0;
    }
    return 1;
}

/*
 *   the characters a cell's text t[s..e) shows: its tags are none, an
 *   entity one, and its runs of spaces and line breaks one between words
 */
int cell_chars(const glui32 *t, size_t s, size_t e)
{
    size_t ts, te, name, namelen;
    int n = 0, space = 0, end;

    while (s < e)
    {
        if (t[s] == '<')
        {
            if (!raw_tag(t, e, &s, &ts, &te))
                return -1;
            namelen = raw_name(t, ts, te, &name, &end);
            if (tag_is(t + name, namelen, "br"))
                space = n > 0;
            continue;
        }
        if (is_space(t[s]))
        {
            space = n > 0;
            ++s;
            continue;
        }
        n += space ? 2 : 1;
        space = 0;
        if (t[s] == '&')
        {
            size_t k;
            for (k = s + 1 ; k < e && k < s + 10 && t[k] != ';' ; ++k)
                ;
            if (k < e && t[k] == ';')
                s = k;
        }
        ++s;
    }
    return n;
}

/*
 *   The rest of the row so far: 0 if it hasn't ended, else 1 if its last
 *   cell is a picture alone, 2 if not.  Tables inside it don't count.
 */
int cell_row_scan(const glui32 *s, size_t n)
{
    size_t i = 0, name, namelen;
    int depth = 0, pics = 0, text = 0, end;
    glui32 quote;

    while (i < n)
    {
        if (s[i] != '<')
        {
            if (!is_space(s[i]))
                text = 1;
            ++i;
            continue;
        }
        end = 0;
        quote = 0;
        for (++i ; i < n && is_space(s[i]) ; ++i)
            ;
        if (i < n && s[i] == '/')
        {
            end = 1;
            ++i;
        }
        for (name = i ; i < n && !is_space(s[i]) && s[i] != '>' ; ++i)
            ;
        namelen = i - name;
        for ( ; i < n && (quote || s[i] != '>') ; ++i)
        {
            if (quote && s[i] == quote)
                quote = 0;
            else if (!quote && (s[i] == '"' || s[i] == '\''))
                quote = s[i];
        }
        if (i == n)
            return 0;
        ++i;

        if (tag_is(s + name, namelen, "table"))
            depth += end ? -1 : 1;
        if (depth > 0)
            continue;
        if (depth < 0 || tag_is(s + name, namelen, "tr"))
            return pics == 1 && !text ? 1 : 2;
        if (!end && (tag_is(s + name, namelen, "td")
                     || tag_is(s + name, namelen, "th")))
            pics = text = 0;
        else if (tag_is(s + name, namelen, "img"))
            ++pics;
    }
    return 0;
}

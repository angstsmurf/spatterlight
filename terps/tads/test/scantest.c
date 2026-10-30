/* scantest.c -- unit tests of glk/oshtml_scan.c, which reads HTML TADS
 * markup without Glk or a parser
 *
 *   cc -I../glk -I../../../glkimp -o scantest scantest.c ../glk/oshtml_scan.c
 *   ./scantest
 *
 * Prints each failure and a count, and exits 1 if any failed.
 */

#include <stdio.h>
#include <string.h>

#include "glk.h"
#include "oshtml_scan.h"

static int failures, checks;

#define CHECK(cond) check((cond), #cond, __LINE__)

static void check(int ok, const char *what, int line)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        printf("scantest.c:%d: %s\n", line, what);
    }
}

/* s as the parser holds text: one glui32 per character */
static glui32 buf[512];
static size_t buflen;

static const glui32 *u(const char *s)
{
    for (buflen = 0 ; s[buflen] != '\0' ; ++buflen)
        buf[buflen] = (unsigned char)s[buflen];
    return buf;
}

/* is t[v..v+vlen) s? */
static int span_is(const glui32 *t, size_t v, size_t vlen, const char *s)
{
    size_t i;

    if (vlen != strlen(s))
        return 0;
    for (i = 0 ; i < vlen ; ++i)
        if (t[v + i] != (unsigned char)s[i])
            return 0;
    return 1;
}

static void test_names(void)
{
    static const char *const list[] = { "td", "th", NULL };
    const glui32 *t;

    t = u("IMG");
    CHECK(tag_is(t, buflen, "img"));
    CHECK(!tag_is(t, 2, "img"));
    t = u("imgx");
    CHECK(!tag_is(t, buflen, "img"));
    t = u("Th");
    CHECK(name_in(t, buflen, list));
    t = u("tr");
    CHECK(!name_in(t, buflen, list));

    t = u("42px");
    CHECK(to_int(t, buflen) == 42);
    CHECK(to_int(t, 1) == 4);
    t = u("x1");
    CHECK(to_int(t, buflen) == 0);
}

#define NEXT(n, value) \
    (CHECK(attr_next(&it)), \
     CHECK(span_is(t, it.name, it.namelen, n)), \
     CHECK(span_is(t, it.v, it.vlen, value)))

static void test_attrs(void)
{
    attr_iter it;
    const glui32 *t;

    /* as HTML TADS reads them: an unquoted value goes up to a space */
    t = u(" src=\"a b.jpg\" width=10 alt=x/y bgcolor=red;");
    attr_start(&it, t, 0, buflen, ATTR_TO_SPACE);
    NEXT("src", "a b.jpg");
    NEXT("width", "10");
    NEXT("alt", "x/y");
    NEXT("bgcolor", "red;");
    CHECK(!attr_next(&it));

    /* as TADS 2 reads the rest: the first character, then a word */
    t = u(" size=+2 color=#ff00ff bgcolor=red; align='left'");
    attr_start(&it, t, 0, buflen, ATTR_WORD);
    NEXT("size", "+2");
    NEXT("color", "#ff00ff");
    NEXT("bgcolor", "red");
    NEXT("align", "left");
    CHECK(!attr_next(&it));

    /* a quoted string where a name would be is skipped, and a name starts
       with a letter */
    t = u(" \"foo=bar\" 2x=3");
    attr_start(&it, t, 0, buflen, ATTR_WORD);
    NEXT("x", "3");
    CHECK(!attr_next(&it));

    /* the '=' has no spaces around it, so this is two without values */
    t = u(" a = 1");
    attr_start(&it, t, 0, buflen, ATTR_TO_SPACE);
    NEXT("a", "");
    CHECK(!attr_next(&it));

    /* as the scanners read them: spaces around the '=', a name may start
       with a digit, and one without a value has it where one would be */
    t = u(" 2x = \"a\" flag width = 5");
    attr_start(&it, t, 0, buflen, ATTR_RAW);
    NEXT("2x", "a");
    NEXT("flag", "");
    CHECK(t[it.v] == 'w');
    NEXT("width", "5");
    CHECK(!attr_next(&it));
}

static void test_raw(void)
{
    const glui32 *t;
    size_t i, s, e, name, v, vlen;
    int end;

    /* a '>' in quotes doesn't end the tag */
    t = u("<a href=\">\">x");
    i = 0;
    CHECK(raw_tag(t, buflen, &i, &s, &e));
    CHECK(span_is(t, s, e - s, "a href=\">\""));
    CHECK(t[i] == 'x');
    t = u("<a href=x");
    i = 0;
    CHECK(!raw_tag(t, buflen, &i, &s, &e));

    t = u(" / td ");
    CHECK(raw_name(t, 0, buflen, &name, &end) == 2);
    CHECK(end && span_is(t, name, 2, "td"));
    t = u("\\tr");
    CHECK(raw_name(t, 0, buflen, &name, &end) == 2);
    CHECK(end && span_is(t, name, 2, "tr"));
    t = u("img src=a");
    CHECK(raw_name(t, 0, buflen, &name, &end) == 3);
    CHECK(!end && name == 0);

    t = u("img width=40 HEIGHT = 30 src=\"a.png\"");
    CHECK(raw_int(t, 0, buflen, "width", -1) == 40);
    CHECK(raw_int(t, 0, buflen, "height", -1) == 30);
    CHECK(raw_int(t, 0, buflen, "border", -1) == -1);
    CHECK(raw_attr(t, 0, buflen, "src", &v, &vlen));
    CHECK(span_is(t, v, vlen, "a.png"));
    /* the tag's own name isn't an attribute */
    CHECK(!raw_attr(t, 0, buflen, "img", &v, &vlen));
}

static void test_pictable(void)
{
    const glui32 *t;
    size_t s, e, m;

    t = u("Hello<br><table><tr><td><img src=a></td></tr></table><br></body>");
    CHECK(find_pictable(t, buflen, &s, &e, &m) == 1);
    CHECK(s == 9 && span_is(t, e - 8, 8, "</table>"));
    CHECK(m == (size_t)-1);

    /* tables with only line breaks between them are one */
    t = u("<table></table><br><table></table>");
    CHECK(find_pictable(t, buflen, &s, &e, &m) == 1);
    CHECK(s == 0 && e == buflen);

    /* the map picture before them */
    t = u("<img usemap=#m><table></table>");
    CHECK(find_pictable(t, buflen, &s, &e, &m) == 1);
    CHECK(m == 0 && s == 15 && e == buflen);

    t = u("<img usemap=#m>");
    CHECK(find_pictable(t, buflen, &s, &e, &m) == 2);
    CHECK(s == 0 && e == buflen);

    /* not the last thing, or not closed */
    t = u("<table></table>x");
    CHECK(find_pictable(t, buflen, &s, &e, &m) == 0);
    t = u("<table></table><b>");
    CHECK(find_pictable(t, buflen, &s, &e, &m) == 0);
    t = u("<table><table></table>");
    CHECK(find_pictable(t, buflen, &s, &e, &m) == 0);
}

static void test_picpage(void)
{
    const glui32 *t;
    size_t bg, bglen;

    t = u("<body background=bg.jpg><center><img src=a></center><br>");
    CHECK(find_picpage(t, buflen, &bg, &bglen));
    CHECK(span_is(t, bg, bglen, "bg.jpg"));
    t = u("<body background=bg.jpg><img src=a>x");
    CHECK(!find_picpage(t, buflen, &bg, &bglen));
    t = u("<body background=bg.jpg><br>");
    CHECK(!find_picpage(t, buflen, &bg, &bglen));
    t = u("<body background=bg.jpg><b><img src=a>");
    CHECK(!find_picpage(t, buflen, &bg, &bglen));
    t = u("<body><img src=a>");
    CHECK(!find_picpage(t, buflen, &bg, &bglen));

    t = u("TAB");
    CHECK(page_quiet_tag(t, buflen));
    t = u("img");
    CHECK(!page_quiet_tag(t, buflen));
}

static void test_cells(void)
{
    const glui32 *t;

    t = u("  <br> <p>\n");
    CHECK(only_tags(t, 0, buflen));
    t = u("<br>x");
    CHECK(!only_tags(t, 0, buflen));
    t = u("<br");
    CHECK(!only_tags(t, 0, buflen));

    /* runs of spaces are one between words, none at the ends */
    t = u("  Hi  there ");
    CHECK(cell_chars(t, 0, buflen) == 8);
    t = u("<b>A&amp;B</b>");
    CHECK(cell_chars(t, 0, buflen) == 3);
    t = u("A<br>B");
    CHECK(cell_chars(t, 0, buflen) == 3);
    /* an '&' without a ';' close by is a character of its own */
    t = u("A & B");
    CHECK(cell_chars(t, 0, buflen) == 5);
    t = u("A<b");
    CHECK(cell_chars(t, 0, buflen) == -1);

    t = u("<img src=a></td><tr>");
    CHECK(cell_row_scan(t, buflen) == 1);
    t = u("x<img src=a></td></tr>");
    CHECK(cell_row_scan(t, buflen) == 2);
    t = u("<img src=a><img src=b></table>");
    CHECK(cell_row_scan(t, buflen) == 2);
    /* only the last cell counts */
    t = u("text</td><td><img src=a></td></tr>");
    CHECK(cell_row_scan(t, buflen) == 1);
    /* a table inside doesn't end the row */
    t = u("<img src=a><table><tr><td></td></tr></table></tr>");
    CHECK(cell_row_scan(t, buflen) == 1);
    t = u("<img src=a></td><td");
    CHECK(cell_row_scan(t, buflen) == 0);
}

int main(void)
{
    test_names();
    test_attrs();
    test_raw();
    test_pictable();
    test_picpage();
    test_cells();
    printf("%d of %d checks failed\n", failures, checks);
    return failures != 0;
}

/* oshtml.c -- HTML TADS markup to Glk
 *
 * The TADS 2 formatter is built with USE_HTML, so once a game turns on HTML
 * mode with \H+, its tags and entities reach os_print unmodified, along with
 * markup of the formatter's own: <BR HEIGHT=0> after each newline, <BR> for
 * a blank line, <TAB MULTIPLE=4> for \t, &nbsp; for "\ " and <B>/</B> for
 * \( and \).  The TADS 3 formatter is built from vmconhtm.cpp, so it is in
 * HTML mode from the start, in the main window and in every text-buffer
 * banner, and writes markup of the same kind: <BR HEIGHT=0>, <BR>,
 * <TAB MULTIPLE=4> and &#nnn; for its special spaces.  This file parses
 * that markup.
 *
 * Whitespace and line breaks work as in the formatter's own text-mode filter
 * (tads2/output.c, tads3/vmconhmp.cpp), and the tags work as in HTML TADS
 * (htmltads/htmlprs.cpp, htmltags.cpp), as far as a Glk text buffer can
 * show them:
 *
 *   - whitespace is collapsed, and dropped at the start of a line and before
 *     a line break, as the formatter does it for plain text; TADS 2 doubles
 *     it after . ? ! and : (looking back past quotes and closing brackets);
 *   - each open tag goes on a stack with the state from before it, so an
 *     end tag restores what its start tag changed, and an end tag closes
 *     the tags left open inside it; clearing the window closes them all,
 *     as HTML TADS's clear_page does;
 *   - text is bold, italic, fixed, Input and centred in any combination
 *     (OSSTY_*), and osstyles.c picks a Glk style for each one; <b>, <i>,
 *     <tt>, <font face>, <hN> and the formatter's own attributes set the
 *     bits;
 *   - <center>, <hN align>, <p align> and <div align> are blocks: they
 *     start a new line, and centre their lines.  If the window doesn't
 *     centre the style (by its Justification hint), the line is padded
 *     with spaces instead;
 *   - <p> and the other blocks leave one blank line between them, however
 *     many ask for one, as HTML TADS's add_line_spacing does;
 *   - <blockquote>, <ul>, <ol> and <dl> indent with spaces, and list items
 *     start with a bullet or a number; <hr> is a rule of box-drawing
 *     characters; <pre> keeps its whitespace; <q> gives curly quotes;
 *   - <font color bgcolor> and <body text> set the text colours, and <body
 *     bgcolor> the window's;
 *   - in TADS 3, <tab> takes ID, TO, ALIGN, DP, INDENT and MULTIPLE;
 *   - the ALT text of <img> and <sound> is printed;
 *   - <title> sets the window title, and nothing inside <title> or
 *     <aboutbox> (or in TADS 3, <banner>) is shown;
 *   - every other tag is dropped and its contents shown.
 *
 * In TADS 2, the main window and the status line share one parser, as they
 * shared the formatter's text-mode filter: the status line's closing
 * newline puts the main window's text at the start of a line too, so the
 * formatter's newline after a command doesn't make a blank line, and a <b>
 * left open in one is closed in the other.  Text the formatter writes while
 * the status line is hidden (os_status(2)) goes through the parser too, and
 * is thrown away.  In TADS 3, each text-buffer banner has a parser of its
 * own (osglkban.c), as it has a formatter of its own.
 *
 * Text reaches the parser in Unicode: each os_print chunk is decoded with
 * os_parse_chars first, exactly as os_put_buffer decoded it, so the
 * character set is guessed from the same text as before.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os.h"
#include "glk.h"
#include "oshtml.h"
#include "osstyles.h"

extern glui32 os_parse_chars(unsigned char *buf, glui32 buflen,
                             glui32 *out, glui32 outlen);

/* the longest tag and entity name we keep; the rest is skipped */
#define TAG_MAX     1024
#define ENTITY_MAX  9           /* as TADS 2 has it: "thetasym" fits */
#define PENDING_MAX 256
#define OUT_MAX     256
#define TITLE_MAX   256
#define HOLD_MAX    1024        /* text after a right, center or decimal tab */
#define ELEMS_MAX   64          /* open tags we keep */
#define ELEM_NAME   12
#define TABS_MAX    64          /* <TAB ID> stops */
#define TABID_MAX   32

enum token { TOK_TEXT, TOK_TAG, TOK_ENTITY };

/* <TAB ALIGN> */
enum tab_align { TAB_NONE, TAB_LEFT, TAB_RIGHT, TAB_CENTER, TAB_DECIMAL };

/* text attributes, as OSSTY_* bits, and colours */
typedef struct color
{
    int attr;
    os_color_t fg, bg;
} color;

/* the innermost list */
enum list_kind { LIST_NONE, LIST_UL, LIST_OL };

/* an open tag, and the state from before its start tag */
typedef struct elem
{
    char name[ELEM_NAME];
    color cur;
    int indent;
    enum list_kind list;
    int nitem;
    int nobr;
    int banner;
} elem;

struct oshtml_parser
{
    /* where the text goes */
    const oshtml_sink *sink;
    void *ctx;

    /* tokenizer */
    enum token state;
    glui32 tag[TAG_MAX];
    size_t taglen;
    glui32 tagquote;            /* the quote we're inside in a tag, or 0 */
    char entity[ENTITY_MAX + 2];
    size_t entitylen;

    /* line state */
    int col;                    /* characters printed on this line */
    glui32 lastchar;            /* the last one, quotes and ) ] } aside */
    glui32 pending[PENDING_MAX]; /* spaces not yet printed... */
    color pending_color[PENDING_MAX]; /* ...their attributes... */
    char pending_quoted[PENDING_MAX]; /* ...and which are quoted */
    size_t npending;

    /*
     *   TADS 3: whether the formatter has text in its buffer, which it
     *   keeps until a flush (and it keeps the trailing spaces even then),
     *   and whether its last flush ended a line
     */
    int buffered;
    int just_nl;

    /* tag state */
    int html;                   /* \H+ is on */
    int ignore;                 /* inside <title> or <aboutbox> */
    int title;                  /* inside <title> */
    int pre;                    /* inside <pre> */
    int quote_level;            /* <q> nesting */
    glui32 titlebuf[TITLE_MAX];
    size_t titlelen;

    /* the formatter's attributes (os_set_text_attr), as OSSTY_* bits */
    int os_attr;

    /* the attributes and colours from the tags in effect */
    color cur;

    /* the block state from the tags in effect */
    int indent;                 /* left indent, in steps of two spaces */
    enum list_kind list;
    int nitem;                  /* the list's items so far */
    int nobr;                   /* inside <nobr> */
    int banner;                 /* TADS 2: inside <banner> */

    /* the open tags */
    elem elems[ELEMS_MAX];
    int nelems;

    /*
     *   the line: newlines since the last visible character, whether
     *   anything visible is on it, and if so, whether it's centred
     */
    int nl_count;
    int line_open;
    int line_center;

    /* whether the window centres each combination: 0 unknown, 1, -1 */
    signed char centers[OSSTY_COMBOS];

    /* the attributes and colours the sink has */
    color sent;

    /* the window background the sink has, from <BODY BGCOLOR> */
    os_color_t screen;

    /* the text colour from <BODY>, which outlasts clearing the window */
    os_color_t text;

    /*
     *   TADS 3: a right, center or decimal <TAB>, and the text after it;
     *   or a centred line, which the window doesn't centre (center_hold)
     */
    enum tab_align tab_align;
    int center_hold;
    int tab_start;              /* the column of the <TAB> */
    int tab_to;                 /* the column of its TO, or -1 */
    size_t tab_lead;            /* the pending spaces before it */
    glui32 tab_dp;
    color tab_color;
    glui32 hold[HOLD_MAX];
    color hold_color[HOLD_MAX];
    size_t nhold;

    /* TADS 3: <TAB ID> stops */
    struct { glui32 id[TABID_MAX]; size_t len; int col; } tabs[TABS_MAX];
    int ntabs;

    /*
     *   output: the sink, or 'dst' while oshtml_print_status is running
     */
    glui32 out[OUT_MAX];
    size_t outlen;
    glui32 *dst;
    size_t dstlen, dstmax;
};

typedef struct oshtml_parser parser;

static int tads3 = 0;

extern winid_t mainwin;

static int is_space(glui32 c);

/* ------------------------------------------------------------------------ */
/*
 *   The main window
 */

static void main_put(void *ctx, const glui32 *buf, size_t len)
{
    (void)ctx;
    glk_put_buffer_uni((glui32 *)buf, (glui32)len);
}

static void main_set_attr(void *ctx, int combo)
{
    (void)ctx;
    glk_set_style(osstyles_style(combo));
}

static void main_set_color(void *ctx, os_color_t fg, os_color_t bg)
{
    (void)ctx;
    osglk_set_text_color(fg, bg);
}

static void main_set_screen_color(void *ctx, os_color_t color)
{
    (void)ctx;
    os_set_screen_color(color);
}

static int main_width(void *ctx)
{
    (void)ctx;
    return G_os_linewidth;
}

/* the style's Justification hint, if the library tells us */
static int main_centers(void *ctx, int combo)
{
    glui32 just;

    (void)ctx;
    return mainwin
        && glk_style_measure(mainwin, osstyles_style(combo),
                             stylehint_Justification, &just)
        && just == stylehint_just_Centered;
}

static const oshtml_sink main_sink =
{
    main_put, main_set_attr, main_set_color, main_set_screen_color,
    main_width, main_centers
};

static parser fmt =
{
    .sink = &main_sink,
    .cur = { 0, OS_COLOR_P_TEXT, OS_COLOR_P_TRANSPARENT },
    .sent = { 0, OS_COLOR_P_TEXT, OS_COLOR_P_TRANSPARENT },
    .screen = OS_COLOR_P_TEXTBG,
    .text = OS_COLOR_P_TEXT
};

/* ------------------------------------------------------------------------ */
/*
 *   Output
 */

static int same_color(const color *a, const color *b)
{
    return a->attr == b->attr && a->fg == b->fg && a->bg == b->bg;
}

static void flush_out(parser *p)
{
    if (p->outlen)
        p->sink->put(p->ctx, p->out, p->outlen);
    p->outlen = 0;
}

/* does the window show this combination centred? */
static int centers(parser *p, int combo)
{
    signed char *known = &p->centers[combo & (OSSTY_COMBOS - 1)];

    if (*known == 0)
        *known = p->sink->centers(p->ctx, combo) ? 1 : -1;
    return *known > 0;
}

/*
 *   give the sink these attributes and colours, where they're new to it;
 *   centring it can't show is ours to do (start_line)
 */
static void send_color(parser *p, const color *c)
{
    color s = *c;

    if ((s.attr & OSSTY_CENTER) && !centers(p, s.attr))
        s.attr &= ~OSSTY_CENTER;
    if (s.attr != p->sent.attr)
    {
        flush_out(p);
        p->sink->set_attr(p->ctx, s.attr);
    }
    if (s.fg != p->sent.fg || s.bg != p->sent.bg)
    {
        flush_out(p);
        p->sink->set_color(p->ctx, s.fg, s.bg);
    }
    p->sent = s;
}

static color cur_color(parser *p)
{
    color c = p->cur;

    c.attr |= p->os_attr;
    return c;
}

static void expand_tab(parser *p, int allow_anon);
static void line_break(parser *p);

/*
 *   The TADS 3 formatter brings the window up to date with the current
 *   attributes and colours whenever it flushes, even with no text to show
 *   in them
 */
static void sync_color(parser *p)
{
    if (tads3 && !p->dst && p->tab_align == TAB_NONE)
        send_color(p, &p->cur);
}

/* print one character with the given attributes */
static void emit_now(parser *p, glui32 c, color col);

static void emit(parser *p, glui32 c, color col)
{
    if (p->tab_align != TAB_NONE && !p->dst)
    {
        if (p->nhold == HOLD_MAX)
            expand_tab(p, 0);
        else
        {
            p->hold[p->nhold] = c;
            p->hold_color[p->nhold++] = col;
            return;
        }
    }
    emit_now(p, c, col);
}

/* the same, not held back after a <TAB> */
static void emit_now(parser *p, glui32 c, color col)
{
    if (p->dst)
    {
        if (p->dstlen < p->dstmax)
            p->dst[p->dstlen++] = c;
        return;
    }
    send_color(p, &col);
    if (p->outlen == OUT_MAX)
        flush_out(p);
    p->out[p->outlen++] = c;
}

static int is_quote(glui32 c)
{
    return c == '"' || c == '\'' || c == ')' || c == ']' || c == '}'
        || c == 0x2018 || c == 0x2019 || c == 0x201C || c == 0x201D;
}

/* print the trailing spaces, now that something follows them */
static void print_pending(parser *p)
{
    size_t i;

    /* the spaces before a <TAB> go before its own */
    for (i = 0 ; i < p->npending ; ++i)
        if (i < p->tab_lead)
            emit_now(p, p->pending[i], p->pending_color[i]);
        else
            emit(p, p->pending[i], p->pending_color[i]);
    p->tab_lead = 0;
    if (p->npending)
    {
        p->col += (int)p->npending;
        p->lastchar = ' ';
        p->just_nl = 0;
    }
    p->npending = 0;
}

static void add_pending_quoted(parser *p, glui32 c, int quoted)
{
    if (p->npending == PENDING_MAX)
        print_pending(p);
    p->buffered = 1;
    p->pending[p->npending] = c;
    p->pending_quoted[p->npending] = (char)quoted;
    p->pending_color[p->npending++] = cur_color(p);
}

static void add_pending(parser *p, glui32 c)
{
    add_pending_quoted(p, c, 0);
}

/*
 *   Before a visible character.  Centring belongs to the whole line, as
 *   alignment belongs to a paragraph in HTML TADS: a change of it ends the
 *   line.  A new line gets the indent, or if it's centred and the window
 *   won't centre it, the text is held back, as after a <TAB ALIGN=CENTER>,
 *   to be padded with spaces when the line ends.
 */
static void start_line(parser *p)
{
    color c = cur_color(p);
    int center = c.attr & OSSTY_CENTER;
    int n;

    if (p->line_open && center == p->line_center)
        return;
    if (p->line_open)
        line_break(p);
    p->line_open = 1;
    p->line_center = center;
    if (center)
    {
        if (!centers(p, c.attr) && p->tab_align == TAB_NONE)
        {
            p->tab_align = TAB_CENTER;
            p->center_hold = 1;
            p->tab_lead = p->npending;
            p->tab_start = p->col + (int)p->npending;
            p->tab_to = -1;
            p->tab_color = c;
        }
        return;
    }
    for (n = 2 * p->indent ; n > 0 ; --n)
    {
        emit_now(p, 0xA0, c);
        ++p->col;
    }
}

/* print a visible character */
static void put_visible(parser *p, glui32 c)
{
    if (!p->dst)
        start_line(p);
    print_pending(p);

    /*
     *   TADS 3: a character that would reach the end of the formatter's
     *   buffer flushes it, and the column starts over (which matters only
     *   to the tabs on lines thousands of characters long)
     */
    if (tads3 && p->col + 1 >= OS_MAXWIDTH)
        p->col = 0;
    emit(p, c, cur_color(p));
    ++p->col;
    p->buffered = 1;
    p->just_nl = 0;
    p->nl_count = 0;
    if (!is_quote(c))
        p->lastchar = c;
}

/*
 *   TADS 3: the text after a right, center or decimal <TAB> is held back
 *   until the next <TAB> or the end of the line.  Now put enough spaces in
 *   front of it, in the colours in effect at the <TAB>, to align it, and
 *   print it.  A <TAB> without TO aligns at the end of the line, which the
 *   formatter doesn't know before the line ends (allow_anon).
 */
static void expand_tab(parser *p, int allow_anon)
{
    int linecol = p->col + (int)p->npending;
    int spaces = 0, txtlen, start, i;
    size_t n, k, lead;

    switch (p->tab_align)
    {
    case TAB_NONE:
    case TAB_LEFT:
        return;

    case TAB_RIGHT:
        if (p->tab_to >= 0)
            spaces = p->tab_to - linecol;
        else if (allow_anon)
            spaces = p->sink->width(p->ctx) - 1 - linecol;
        break;

    case TAB_CENTER:
        txtlen = linecol - p->tab_start;
        if (p->tab_to >= 0)
        {
            start = p->tab_to - txtlen / 2;
            spaces = start - p->tab_start;
        }
        else if (allow_anon)
            spaces = ((p->sink->width(p->ctx) - p->tab_start) - txtlen) / 2;
        break;

    case TAB_DECIMAL:
        i = p->tab_start;
        for (k = 0 ; k < p->nhold && p->hold[k] != p->tab_dp ; ++k)
            ++i;
        if (k == p->nhold)
            i += (int)p->npending;
        spaces = p->tab_to - i;
        break;
    }

    p->tab_align = TAB_NONE;
    p->center_hold = 0;
    lead = p->tab_lead < p->npending ? p->tab_lead : p->npending;
    p->tab_lead = 0;
    n = p->nhold;
    p->nhold = 0;
    if (n == 0 && spaces > 0)
    {
        /*
         *   nothing after the tab but spaces: they all go at the end of the
         *   line, and the formatter would trim them
         */
        glui32 c[PENDING_MAX];
        color col[PENDING_MAX];
        char quoted[PENDING_MAX];
        size_t np = p->npending - lead;
        color cur = p->cur;

        memcpy(c, p->pending + lead, np * sizeof(glui32));
        memcpy(col, p->pending_color + lead, np * sizeof(color));
        memcpy(quoted, p->pending_quoted + lead, np);
        p->npending = lead;
        p->cur = p->tab_color;
        for (i = 0 ; i < spaces ; ++i)
            add_pending_quoted(p, ' ', 1);
        p->cur = cur;
        for (k = 0 ; k < np ; ++k)
        {
            add_pending_quoted(p, c[k], quoted[k]);
            p->pending_color[p->npending - 1] = col[k];
        }
        return;
    }
    for (i = 0 ; i < spaces ; ++i)
        emit(p, ' ', p->tab_color);
    if (spaces > 0)
        p->col += spaces;
    for (k = 0 ; k < n ; ++k)
        emit(p, p->hold[k], p->hold_color[k]);
}

/* end the line, in the style of the text before it */
static void put_newline(parser *p)
{
    if (p->center_hold)
        expand_tab(p, 1);
    p->npending = 0;
    emit(p, '\n', p->sent);
    p->col = 0;
    p->lastchar = 0;
    p->line_open = 0;
    ++p->nl_count;
    sync_color(p);
}

/*
 *   The formatter's flush with a newline: drop the trailing spaces, and end
 *   the line unless nothing is on it (in <pre>, end it anyway).  The TADS 3
 *   formatter counts the spaces of a tab as being on the line, unless its
 *   last flush ended a line.  Then the formatter brings the style up to
 *   date.
 */
static void line_break(parser *p)
{
    color c;
    int spaces;

    expand_tab(p, 1);
    spaces = tads3 && p->npending != 0 && !p->just_nl;
    p->npending = 0;
    if (p->col != 0 || p->pre || spaces)
        put_newline(p);
    p->buffered = 0;
    p->just_nl = 1;
    c = cur_color(p);
    if (!p->dst && !same_color(&c, &p->sent))
        send_color(p, &c);
}

/* the formatter's blank line: a line break, then a newline */
static void blank_line(parser *p)
{
    line_break(p);
    put_newline(p);
}

/* the space between blocks: a blank line, unless there is one already */
static void para_break(parser *p)
{
    line_break(p);
    if (p->nl_count == 1)
        put_newline(p);
}

/*
 *   a tab: spaces to the next multiple of 'multiple', or 'indent' spaces,
 *   as the formatter's \t and <TAB INDENT>
 */
static void put_tab(parser *p, int indent, int multiple)
{
    int linecol = p->col + (int)p->npending;

    if (indent > 0)
    {
        for ( ; indent > 0 && linecol < OS_MAXWIDTH ; --indent, ++linecol)
            add_pending(p, ' ');
        return;
    }
    if (multiple <= 0)
        return;
    do
    {
        if (tads3 && linecol >= OS_MAXWIDTH)
            break;
        add_pending(p, ' ');
        ++linecol;
    } while ((linecol + 1) % multiple != 0
             && (tads3 || linecol < OS_MAXWIDTH));
}

/* an ordinary space in HTML mode */
static void put_space(parser *p)
{
    glui32 prev;

    if (p->pre)
    {
        add_pending_quoted(p, ' ', 1);
        return;
    }

    /* none at the start of a line, and never two in a row... */
    if (p->col == 0 && p->npending == 0)
        return;
    if (p->npending != 0)
        return;

    /* <nobr>: a space that holds the words on either side together */
    if (p->nobr)
    {
        if (p->lastchar != 0xA0)
            put_visible(p, 0xA0);
        return;
    }

    /* ...except after the end of a sentence, in TADS 2 */
    add_pending(p, ' ');
    prev = p->lastchar;
    if (!tads3 && (prev == '.' || prev == '?' || prev == '!' || prev == ':'))
        add_pending(p, ' ');
}

/*
 *   TADS 3: a space that doesn't combine with others, 'wid' columns wide.
 *   It takes the place of an ordinary space before it, and isn't dropped at
 *   the start of a line, but is at the end.
 */
static void put_quoted_space(parser *p, int wid)
{
    if (p->npending != 0 && !p->pending_quoted[p->npending - 1])
        --p->npending;
    for ( ; wid > 0 ; --wid)
        add_pending_quoted(p, ' ', 1);
}

/* text, in HTML mode, with the markup taken out */
static void put_html_text(parser *p, glui32 c)
{
    if (p->ignore)
    {
        /*
         *   The formatter has doubled the spaces after full stops and
         *   colons by now, which its text-mode filter, reading the game's
         *   text, did not see: collapse runs of spaces.
         */
        if (is_space(c))
            c = ' ';
        if (p->title && p->titlelen < TITLE_MAX
            && !(c == ' ' && p->titlelen != 0
                 && p->titlebuf[p->titlelen - 1] == ' '))
            p->titlebuf[p->titlelen++] = c;
        return;
    }

    switch (c)
    {
    case '\n':
        if (p->pre)
        {
            line_break(p);
            break;
        }
        /* fall through */
    case ' ':
    case '\t':
    case '\r':
        put_space(p);
        break;

    case 0xA0:
        /* a non-breaking space, the formatter's quoted space */
        if (tads3)
            put_visible(p, ' ');
        else
            add_pending(p, ' ');
        break;

    default:
        if (!tads3)
        {
            put_visible(p, c);
            break;
        }

        /* the TADS 3 formatter's special spaces, as it renders them */
        switch (c)
        {
        case 0x00AD:            /* soft hyphen */
        case 0xFEFF:            /* zero-width non-breaking space */
        case 0x200B:            /* zero-width space */
        case 0x200A:            /* hair space */
        case 0x2008:            /* punctuation space */
            break;

        case 0x0015:            /* the formatter's quoted space */
        case 0x2005:            /* four-per-em space */
        case 0x2006:            /* six-per-em space */
        case 0x2007:            /* figure space */
        case 0x2009:            /* thin space */
            put_quoted_space(p, 1);
            break;

        case 0x2002:            /* en space */
        case 0x2004:            /* three-per-em space */
            put_quoted_space(p, 2);
            break;

        case 0x2003:            /* em space */
            put_quoted_space(p, 3);
            break;

        default:
            put_visible(p, c);
            break;
        }
        break;
    }
}

/* text outside HTML mode, as the formatter laid it out */
static void put_plain_text(parser *p, glui32 c)
{
    int i, n;

    print_pending(p);
    switch (c)
    {
    case '\n':
        put_newline(p);
        break;

    case '\t':
        n = 0;
        do
            ++n;
        while (((p->col + n + 1) & 3) != 0 && p->col + n < OS_MAXWIDTH);
        for (i = 0 ; i < n ; ++i)
            put_visible(p, ' ');
        break;

    default:
        put_visible(p, c);
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Entities
 */

struct entity { const char *name; glui32 ch; };

/*
 *   the TADS 3 table (tads3/vmconhmp.cpp), with TADS 2's emdash and endash,
 *   sorted for strcmp
 */
static const struct entity entities[] =
{
    { "AElig", 198 }, { "Aacute", 193 }, { "Abreve", 258 },
    { "Acirc", 194 }, { "Agrave", 192 }, { "Alpha", 913 },
    { "Aogon", 260 }, { "Aring", 197 }, { "Atilde", 195 },
    { "Auml", 196 }, { "Beta", 914 }, { "Cacute", 262 },
    { "Ccaron", 268 }, { "Ccedil", 199 }, { "Chi", 935 },
    { "Dagger", 8225 }, { "Dcaron", 270 }, { "Delta", 916 },
    { "Dstrok", 272 }, { "ETH", 208 }, { "Eacute", 201 },
    { "Ecaron", 282 }, { "Ecirc", 202 }, { "Egrave", 200 },
    { "Eogon", 280 }, { "Epsilon", 917 }, { "Eta", 919 },
    { "Euml", 203 }, { "Gamma", 915 }, { "Iacute", 205 },
    { "Icirc", 206 }, { "Igrave", 204 }, { "Iota", 921 },
    { "Iuml", 207 }, { "Kappa", 922 }, { "Lacute", 313 },
    { "Lambda", 923 }, { "Lcaron", 317 }, { "Lstrok", 321 },
    { "Mu", 924 }, { "Nacute", 323 }, { "Ncaron", 327 },
    { "Ntilde", 209 }, { "Nu", 925 }, { "OElig", 338 },
    { "Oacute", 211 }, { "Ocirc", 212 }, { "Odblac", 336 },
    { "Ograve", 210 }, { "Omega", 937 }, { "Omicron", 927 },
    { "Oslash", 216 }, { "Otilde", 213 }, { "Ouml", 214 },
    { "Phi", 934 }, { "Pi", 928 }, { "Prime", 8243 },
    { "Psi", 936 }, { "Racute", 340 }, { "Rcaron", 344 },
    { "Rho", 929 }, { "Sacute", 346 }, { "Scaron", 352 },
    { "Scedil", 350 }, { "Sigma", 931 }, { "THORN", 222 },
    { "Tau", 932 }, { "Tcaron", 356 }, { "Tcedil", 354 },
    { "Theta", 920 }, { "Uacute", 218 }, { "Ucirc", 219 },
    { "Udblac", 368 }, { "Ugrave", 217 }, { "Upsilon", 933 },
    { "Uring", 366 }, { "Uuml", 220 }, { "Xi", 926 },
    { "Yacute", 221 }, { "Yuml", 376 }, { "Zacute", 377 },
    { "Zcaron", 381 }, { "Zdot", 379 }, { "Zeta", 918 },
    { "aacute", 225 }, { "abreve", 259 }, { "acirc", 226 },
    { "acute", 180 }, { "aelig", 230 }, { "agrave", 224 },
    { "alefsym", 8501 }, { "alpha", 945 }, { "amp", '&' },
    { "and", 8743 }, { "ang", 8736 }, { "aogon", 261 },
    { "aring", 229 }, { "asymp", 8776 }, { "atilde", 227 },
    { "auml", 228 }, { "bdquo", 8222 }, { "beta", 946 },
    { "breve", 728 }, { "brvbar", 166 }, { "bull", 8226 },
    { "cacute", 263 }, { "cap", 8745 }, { "caron", 711 },
    { "ccaron", 269 }, { "ccedil", 231 }, { "cedil", 184 },
    { "cent", 162 }, { "chi", 967 }, { "circ", 710 },
    { "clubs", 9827 }, { "cong", 8773 }, { "copy", 169 },
    { "crarr", 8629 }, { "cup", 8746 }, { "curren", 164 },
    { "dArr", 8659 }, { "dagger", 8224 }, { "darr", 8595 },
    { "dblac", 733 }, { "dcaron", 271 }, { "deg", 176 },
    { "delta", 948 }, { "diams", 9830 }, { "divide", 247 },
    { "dot", 729 }, { "dstrok", 273 }, { "eacute", 233 },
    { "ecaron", 283 }, { "ecirc", 234 }, { "egrave", 232 },
    { "emdash", 8212 }, { "empty", 8709 }, { "emsp", 0x2003 },
    { "endash", 8211 }, { "ensp", 0x2002 },
    { "eogon", 281 }, { "epsilon", 949 }, { "equiv", 8801 },
    { "eta", 951 }, { "eth", 240 }, { "euml", 235 },
    { "exist", 8707 }, { "figsp", 0x2007 }, { "fnof", 402 },
    { "forall", 8704 }, { "fpmsp", 0x2005 }, { "frac12", 189 },
    { "frac14", 188 }, { "frac34", 190 }, { "frasl", 8260 },
    { "gamma", 947 }, { "ge", 8805 }, { "gt", '>' },
    { "hArr", 8660 }, { "hairsp", 0x200a }, { "harr", 8596 },
    { "hearts", 9829 }, { "hellip", 8230 }, { "iacute", 237 },
    { "icirc", 238 }, { "iexcl", 161 }, { "igrave", 236 },
    { "image", 8465 }, { "infin", 8734 }, { "int", 8747 },
    { "iota", 953 }, { "iquest", 191 }, { "isin", 8712 },
    { "iuml", 239 }, { "kappa", 954 }, { "lArr", 8656 },
    { "lacute", 314 }, { "lambda", 955 }, { "lang", 9001 },
    { "laquo", 171 }, { "larr", 8592 }, { "lcaron", 318 },
    { "lceil", 8968 }, { "ldq", 8220 }, { "ldquo", 8220 },
    { "le", 8804 }, { "lfloor", 8970 }, { "lowast", 8727 },
    { "loz", 9674 }, { "lrm", 8206 }, { "lsaquo", 8249 },
    { "lsq", 8216 }, { "lsquo", 8216 }, { "lstrok", 322 },
    { "lt", '<' }, { "macr", 175 }, { "mdash", 8212 },
    { "micro", 181 }, { "middot", 183 }, { "minus", 8722 },
    { "mu", 956 }, { "nabla", 8711 }, { "nacute", 324 },
    { "nbsp", 0x00A0 }, { "ncaron", 328 }, { "ndash", 8211 },
    { "ne", 8800 }, { "ni", 8715 }, { "not", 172 },
    { "notin", 8713 }, { "nsub", 8836 }, { "ntilde", 241 },
    { "nu", 957 }, { "oacute", 243 }, { "ocirc", 244 },
    { "odblac", 337 }, { "oelig", 339 }, { "ogon", 731 },
    { "ograve", 242 }, { "oline", 8254 }, { "omega", 969 },
    { "omicron", 959 }, { "oplus", 8853 }, { "or", 8744 },
    { "ordf", 170 }, { "ordm", 186 }, { "oslash", 248 },
    { "otilde", 245 }, { "otimes", 8855 }, { "ouml", 246 },
    { "para", 182 }, { "part", 8706 }, { "permil", 8240 },
    { "perp", 8869 }, { "phi", 966 }, { "pi", 960 },
    { "piv", 982 }, { "plusmn", 177 }, { "pound", 163 },
    { "prime", 8242 }, { "prod", 8719 }, { "prop", 8733 },
    { "psi", 968 }, { "puncsp", 0x2008 }, { "quot", '"' },
    { "rArr", 8658 }, { "racute", 341 }, { "radic", 8730 },
    { "rang", 9002 }, { "raquo", 187 }, { "rarr", 8594 },
    { "rcaron", 345 }, { "rceil", 8969 }, { "rdq", 8221 },
    { "rdquo", 8221 }, { "real", 8476 }, { "reg", 174 },
    { "rfloor", 8971 }, { "rho", 961 }, { "rlm", 8207 },
    { "rsaquo", 8250 }, { "rsq", 8217 }, { "rsquo", 8217 },
    { "sacute", 347 }, { "sbquo", 8218 }, { "scaron", 353 },
    { "scedil", 351 }, { "sdot", 8901 }, { "sect", 167 },
    { "shy", 173 }, { "sigma", 963 }, { "sigmaf", 962 },
    { "sim", 8764 }, { "spades", 9824 }, { "spmsp", 0x2006 },
    { "sub", 8834 }, { "sube", 8838 }, { "sum", 8721 },
    { "sup", 8835 }, { "sup1", 185 }, { "sup2", 178 },
    { "sup3", 179 }, { "supe", 8839 }, { "szlig", 223 },
    { "tau", 964 }, { "tcaron", 357 }, { "tcedil", 355 },
    { "there4", 8756 }, { "theta", 952 }, { "thetasym", 977 },
    { "thinsp", 0x2009 }, { "thorn", 254 }, { "tilde", 732 },
    { "times", 215 }, { "trade", 8482 }, { "uArr", 8657 },
    { "uacute", 250 }, { "uarr", 8593 }, { "ucirc", 251 },
    { "udblac", 369 }, { "ugrave", 249 }, { "uml", 168 },
    { "upsih", 978 }, { "upsilon", 965 }, { "uring", 367 },
    { "uuml", 252 }, { "weierp", 8472 }, { "xi", 958 },
    { "yacute", 253 }, { "yen", 165 }, { "yuml", 255 },
    { "zacute", 378 }, { "zcaron", 382 }, { "zdot", 380 },
    { "zeta", 950 }, { "zwj", 8205 }, { "zwnj", 8204 },
    { "zwsp", 0x200b }
};

static glui32 find_entity(const char *name)
{
    size_t lo = 0, hi = sizeof(entities) / sizeof(entities[0]);

    while (lo < hi)
    {
        size_t mid = lo + (hi - lo) / 2;
        int diff = strcmp(entities[mid].name, name);
        if (diff == 0)
            return entities[mid].ch;
        if (diff < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return 0;
}

static int is_alnum(glui32 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9');
}

static int is_space(glui32 c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void feed(parser *p, glui32 c);

/* the entity's character, as text */
static void put_entity_char(parser *p, glui32 c)
{
    if (c != 0)
        put_html_text(p, c);
}

/* the character after "&...", which ends the entity */
static void end_entity(parser *p, glui32 c)
{
    char *e = p->entity;
    glui32 val = 0;
    size_t i;

    p->state = TOK_TEXT;

    /* &#nnn; or &#xhhh; */
    if (e[0] == '#')
    {
        int hex = (e[1] == 'x' || e[1] == 'X');
        for (i = hex ? 2 : 1 ; e[i] != '\0' ; ++i)
        {
            int d = (e[i] <= '9' ? e[i] - '0'
                     : e[i] <= 'F' ? e[i] - 'A' + 10 : e[i] - 'a' + 10);
            val = val * (hex ? 16 : 10) + d;
        }
        put_entity_char(p, val);
        if (c != ';')
            feed(p, c);
        return;
    }

    /* &name; -- unknown names are shown as they are */
    val = find_entity(e);
    if (val != 0)
    {
        put_entity_char(p, val);
        if (c != ';')
            feed(p, c);
        return;
    }
    put_html_text(p, '&');
    for (i = 0 ; e[i] != '\0' ; ++i)
        put_html_text(p, (unsigned char)e[i]);
    feed(p, c);
}

static void entity_char(parser *p, glui32 c)
{
    char *e = p->entity;
    size_t n = p->entitylen;
    int ok;

    if (n == 0 && c == '#')
        ok = 1;
    else if (e[0] == '#')
    {
        if (n == 1 && (c == 'x' || c == 'X'))
            ok = 1;
        else if (n >= 2 && (e[1] == 'x' || e[1] == 'X'))
            ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
                || (c >= 'A' && c <= 'F');
        else
            ok = (c >= '0' && c <= '9');
        /* a number too long to be a character ends here */
        if (n >= ENTITY_MAX + 1)
            ok = 0;
    }
    else
        ok = is_alnum(c) && n < ENTITY_MAX;

    if (ok)
    {
        e[n] = (char)c;
        e[n + 1] = '\0';
        p->entitylen = n + 1;
    }
    else
        end_entity(p, c);
}

/* ------------------------------------------------------------------------ */
/*
 *   Tags
 */

static int tag_is(const glui32 *name, size_t len, const char *s)
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

static void set_title(parser *p)
{
    char buf[TITLE_MAX * 4 + 1];
    char *d = buf;
    size_t i;

    /* UTF-8 */
    for (i = 0 ; i < p->titlelen ; ++i)
    {
        glui32 c = p->titlebuf[i];
        if (c < 0x80)
            *d++ = (char)c;
        else if (c < 0x800)
        {
            *d++ = (char)(0xC0 | (c >> 6));
            *d++ = (char)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            *d++ = (char)(0xE0 | (c >> 12));
            *d++ = (char)(0x80 | ((c >> 6) & 0x3F));
            *d++ = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            *d++ = (char)(0xF0 | (c >> 18));
            *d++ = (char)(0x80 | ((c >> 12) & 0x3F));
            *d++ = (char)(0x80 | ((c >> 6) & 0x3F));
            *d++ = (char)(0x80 | (c & 0x3F));
        }
    }
    *d = '\0';
    os_set_title(buf);
}

/* the tag in p->tag, between '<' and '>' */

/* the colour in a COLOR, BGCOLOR or TEXT attribute */
static const struct { const char *name; os_color_t color; } colors[] =
{
    { "black",   os_rgb_color(0x00, 0x00, 0x00) },
    { "white",   os_rgb_color(0xFF, 0xFF, 0xFF) },
    { "red",     os_rgb_color(0xFF, 0x00, 0x00) },
    { "blue",    os_rgb_color(0x00, 0x00, 0xFF) },
    { "green",   os_rgb_color(0x00, 0x80, 0x00) },
    { "yellow",  os_rgb_color(0xFF, 0xFF, 0x00) },
    { "cyan",    os_rgb_color(0x00, 0xFF, 0xFF) },
    { "aqua",    os_rgb_color(0x00, 0xFF, 0xFF) },
    { "magenta", os_rgb_color(0xFF, 0x00, 0xFF) },
    { "silver",  os_rgb_color(0xC0, 0xC0, 0xC0) },
    { "gray",    os_rgb_color(0x80, 0x80, 0x80) },
    { "maroon",  os_rgb_color(0x80, 0x00, 0x00) },
    { "purple",  os_rgb_color(0x80, 0x00, 0x80) },
    { "fuchsia", os_rgb_color(0xFF, 0x00, 0xFF) },
    { "lime",    os_rgb_color(0x00, 0xFF, 0x00) },
    { "olive",   os_rgb_color(0x80, 0x80, 0x00) },
    { "navy",    os_rgb_color(0x00, 0x00, 0x80) },
    { "teal",    os_rgb_color(0x00, 0x80, 0x80) },
    { "text",       OS_COLOR_P_TEXT },
    { "bgcolor",    OS_COLOR_P_TEXTBG },
    { "statustext", OS_COLOR_P_STATUSLINE },
    { "statusbg",   OS_COLOR_P_STATUSBG },
    { "input",      OS_COLOR_P_INPUT },
};

static int parse_color(const glui32 *v, size_t len, os_color_t *out)
{
    size_t i;

    if (len != 0 && v[0] == '#')
    {
        unsigned long val = 0;
        for (i = 1 ; i < len ; ++i)
        {
            glui32 c = v[i];
            if (c >= '0' && c <= '9')
                val = val * 16 + (c - '0');
            else if (c >= 'a' && c <= 'f')
                val = val * 16 + (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                val = val * 16 + (c - 'A' + 10);
            else
                break;
        }
        *out = (os_color_t)(val & 0x00FFFFFF);
        return 1;
    }
    for (i = 0 ; i < sizeof(colors) / sizeof(colors[0]) ; ++i)
        if (tag_is(v, len, colors[i].name))
        {
            *out = colors[i].color;
            return 1;
        }
    return 0;
}

static int name_in(const glui32 *name, size_t len, const char *const *list)
{
    for ( ; *list ; ++list)
        if (tag_is(name, len, *list))
            return 1;
    return 0;
}

static const char *const bold_tags[] = { "b", "strong", 0 };
static const char *const italic_tags[] =
    { "i", "em", "u", "cite", "var", "dfn", "address", 0 };
static const char *const fixed_tags[] =
    { "tt", "code", "kbd", "samp", "listing", "xmp", 0 };
static const char *const heading_tags[] =
    { "h1", "h2", "h3", "h4", "h5", "h6", 0 };

/* the blocks that an open <p> or <dd> doesn't reach out of */
static const char *const p_stops[] =
{
    "blockquote", "bq", "ul", "ol", "dl", "dd", "center", "div",
    "h1", "h2", "h3", "h4", "h5", "h6", "banner", 0
};
static const char *const dd_stops[] =
    { "blockquote", "bq", "ul", "ol", "dl", "banner", 0 };

/* open a tag, keeping the state to go back to at its end tag */
static void push_elem(parser *p, const glui32 *name, size_t len)
{
    elem *e;
    size_t k;

    if (len >= ELEM_NAME)
        return;

    /* a lost end tag: take everything down a level */
    if (p->nelems == ELEMS_MAX)
    {
        memmove(p->elems, p->elems + 1, (ELEMS_MAX - 1) * sizeof(elem));
        --p->nelems;
    }
    e = &p->elems[p->nelems++];
    for (k = 0 ; k < len ; ++k)
        e->name[k] = (char)(name[k] >= 'A' && name[k] <= 'Z'
                            ? name[k] + ('a' - 'A') : name[k]);
    e->name[len] = '\0';
    e->cur = p->cur;
    e->indent = p->indent;
    e->list = p->list;
    e->nitem = p->nitem;
    e->nobr = p->nobr;
    e->banner = p->banner;
}

/* close the open tag at 'i', and those inside it */
static void restore_elem(parser *p, int i)
{
    const elem *e = &p->elems[i];

    p->cur = e->cur;
    p->indent = e->indent;
    p->list = e->list;
    p->nitem = e->nitem;
    p->nobr = e->nobr;
    p->banner = e->banner;
    p->nelems = i;
}

/* an end tag closes its start tag, if it has one */
static int pop_elem(parser *p, const glui32 *name, size_t len)
{
    int i;

    for (i = p->nelems ; i > 0 ; --i)
        if (tag_is(name, len, p->elems[i - 1].name))
        {
            restore_elem(p, i - 1);
            return 1;
        }
    return 0;
}

/* close an open 'name', unless one of 'stops' is open inside it */
static void close_elem(parser *p, const char *name, const char *const *stops)
{
    int i;
    const char *const *s;

    for (i = p->nelems ; i > 0 ; --i)
    {
        const char *n = p->elems[i - 1].name;
        if (strcmp(n, name) == 0)
        {
            restore_elem(p, i - 1);
            return;
        }
        for (s = stops ; *s ; ++s)
            if (strcmp(n, *s) == 0)
                return;
    }
}

/* does the value contain 'sub', ignoring case? */
static int value_has(const glui32 *v, size_t len, const char *sub)
{
    size_t n = strlen(sub), i, k;

    for (i = 0 ; i + n <= len ; ++i)
    {
        for (k = 0 ; k < n ; ++k)
        {
            glui32 c = v[i + k];
            if (c >= 'A' && c <= 'Z')
                c += 'a' - 'A';
            if (c != (unsigned char)sub[k])
                break;
        }
        if (k == n)
            return 1;
    }
    return 0;
}

/*
 *   the combination for a FACE, as osstyles.c counts it: the TADS-Input
 *   font, a fixed one, or neither
 */
static int face_combo(const glui32 *v, size_t len)
{
    static const char *const mono[] =
        { "typewriter", "courier", "mono", "fixed", "console", 0 };
    const char *const *m;

    if (value_has(v, len, "input"))
        return OSSTY_INPUT;
    for (m = mono ; *m ; ++m)
        if (value_has(v, len, *m))
            return OSSTY_FIXED;
    return 0;
}

/* an <li>: "•", or the item's number, after the list's indent */
static void put_bullet(parser *p)
{
    char num[16];
    char *c;

    line_break(p);
    ++p->nitem;
    if (p->list == LIST_OL)
    {
        sprintf(num, "%d.", p->nitem);
        for (c = num ; *c ; ++c)
            put_visible(p, (unsigned char)*c);
    }
    else
        put_visible(p, 0x2022);
    add_pending(p, ' ');
}

/* TADS 3: the stop of a <TAB ID>, made if 'create'; the ids ignore case */
static int find_tab(parser *p, const glui32 *id, size_t len, int create)
{
    glui32 folded[TABID_MAX];
    size_t k;
    int i;

    if (len > TABID_MAX)
        len = TABID_MAX;
    for (k = 0 ; k < len ; ++k)
        folded[k] = id[k] >= 'A' && id[k] <= 'Z' ? id[k] + ('a' - 'A') : id[k];
    for (i = 0 ; i < p->ntabs ; ++i)
        if (p->tabs[i].len == len
            && memcmp(p->tabs[i].id, folded, len * sizeof(glui32)) == 0)
            return i;
    if (!create || p->ntabs == TABS_MAX)
        return -1;
    memcpy(p->tabs[i].id, folded, len * sizeof(glui32));
    p->tabs[i].len = len;
    p->tabs[i].col = 0;
    return p->ntabs++;
}

static int to_int(const glui32 *v, size_t len)
{
    int n = 0;
    size_t k;

    for (k = 0 ; k < len && v[k] >= '0' && v[k] <= '9' ; ++k)
        n = n * 10 + (int)(v[k] - '0');
    return n;
}

/*
 *   <BODY>: the window background.  A colour of the game's own behind the
 *   theme's text colour could hide the text, so the text gets black or
 *   white, whichever shows.  Black is 1/1/1: osglk_set_text_color takes 0
 *   on a transparent background for the theme's colour (Hill Ridge).
 */
static void set_body(parser *p, os_color_t bg)
{
    if (!os_color_is_param(bg) && os_color_is_param(p->cur.fg))
    {
        unsigned long lum = 299 * ((bg >> 16) & 0xFF)
            + 587 * ((bg >> 8) & 0xFF) + 114 * (bg & 0xFF);
        p->cur.fg = lum >= 128000 ? os_rgb_color(1, 1, 1)
            : os_rgb_color(255, 255, 255);
    }
    p->text = p->cur.fg;
    if (bg != p->screen)
    {
        flush_out(p);
        p->sink->set_screen_color(p->ctx, bg);
        p->screen = bg;
    }
}

/* the tag in p->tag, between '<' and '>' */
static void do_tag(parser *p)
{
    const glui32 *t = p->tag;
    size_t len = p->taglen, i = 0, name, namelen;
    int end = 0;
    int br = 0;                 /* 1: line break, 2: blank line */
    int alt_ok = 0;
    int color_ok = 0, body = 0; /* <font>, <body> */
    os_color_t body_bg = OS_COLOR_P_TEXTBG;
    int face_ok = 0, face = -1; /* <font FACE> */
    int align_ok = 0;           /* <p>, <div>, <hN>: 1 to open the tag */
    int align = -1;             /* their ALIGN: 0 left, 1 centred */
    int heading = 0;
    int tab = 0;                /* TADS 3: <tab> */
    int tab_to = -1;
    enum tab_align tab_align = TAB_NONE;
    glui32 tab_dp = 0;

    /* the name: "<b>", "</b>", "< / b >" */
    while (i < len && is_space(t[i]))
        ++i;
    if (i < len && (t[i] == '/' || t[i] == '\\'))
    {
        end = 1;
        ++i;
        while (i < len && is_space(t[i]))
            ++i;
    }
    for (name = i ; i < len && !is_space(t[i]) ; ++i)
        ;
    namelen = i - name;

#define TAG(s) tag_is(t + name, namelen, s)
#define TAG_IN(l) name_in(t + name, namelen, l)
    if (TAG("br"))
    {
        if (!p->ignore)
            br = (tads3 ? p->buffered
                  : p->col != 0 || p->npending != 0) ? 1 : 2;
    }
    else if (TAG_IN(bold_tags) || TAG_IN(italic_tags) || TAG_IN(fixed_tags))
    {
        if (p->ignore)
            ;
        else if (end)
            pop_elem(p, t + name, namelen);
        else
        {
            push_elem(p, t + name, namelen);
            if (TAG_IN(bold_tags))
                p->cur.attr |= OSSTY_BOLD;
            else if (TAG_IN(italic_tags))
                p->cur.attr |= OSSTY_ITALIC;
            else
                p->cur.attr = (p->cur.attr & ~OSSTY_INPUT) | OSSTY_FIXED;
        }
    }
    else if (TAG("p"))
    {
        if (!p->ignore)
        {
            close_elem(p, "p", p_stops);
            /* HTML TADS keeps the larger spacing (add_line_spacing) */
            para_break(p);
            align_ok = !end;
        }
    }
    else if (TAG("div"))
    {
        if (!p->ignore)
        {
            line_break(p);
            if (end)
                pop_elem(p, t + name, namelen);
            else
                align_ok = 1;
        }
    }
    else if (TAG_IN(heading_tags))
    {
        if (p->ignore)
            ;
        else if (end)
        {
            pop_elem(p, t + name, namelen);
            para_break(p);
        }
        else
        {
            para_break(p);
            align_ok = heading = 1;
        }
    }
    else if (TAG("center"))
    {
        /* a block, like <div> */
        if (p->ignore)
            ;
        else if (end)
        {
            line_break(p);
            pop_elem(p, t + name, namelen);
        }
        else
        {
            line_break(p);
            push_elem(p, t + name, namelen);
            p->cur.attr |= OSSTY_CENTER;
        }
    }
    else if (TAG("blockquote") || TAG("bq"))
    {
        if (!p->ignore)
        {
            if (end)
                pop_elem(p, t + name, namelen);
            para_break(p);
            if (!end)
            {
                push_elem(p, t + name, namelen);
                ++p->indent;
            }
        }
    }
    else if (TAG("ul") || TAG("ol") || TAG("dl"))
    {
        /* a list in a list goes on from the line of its item */
        if (p->ignore)
            ;
        else if (end)
        {
            pop_elem(p, t + name, namelen);
            if (p->list != LIST_NONE)
                line_break(p);
            else
                para_break(p);
        }
        else
        {
            if (p->list != LIST_NONE)
                line_break(p);
            else
                para_break(p);
            push_elem(p, t + name, namelen);
            if (!TAG("dl"))
            {
                ++p->indent;
                p->list = TAG("ol") ? LIST_OL : LIST_UL;
                p->nitem = 0;
            }
        }
    }
    else if (TAG("li"))
    {
        if (!p->ignore && !end)
            put_bullet(p);
    }
    else if (TAG("dt") || TAG("dd") || TAG("lh"))
    {
        if (!p->ignore && !end)
        {
            close_elem(p, "dd", dd_stops);
            line_break(p);
            if (TAG("dd"))
            {
                push_elem(p, t + name, namelen);
                ++p->indent;
            }
        }
        else if (!p->ignore && TAG("dd"))
            pop_elem(p, t + name, namelen);
    }
    else if (TAG("nobr"))
    {
        if (p->ignore)
            ;
        else if (end)
            pop_elem(p, t + name, namelen);
        else
        {
            push_elem(p, t + name, namelen);
            p->nobr = 1;
        }
    }
    else if (TAG("tab"))
    {
        if (p->ignore)
            ;
        else if (tads3)
        {
            expand_tab(p, 1);
            tab = 1;
        }
        else
            put_tab(p, 0, 4);
    }
    else if (TAG("img") || TAG("sound"))
        alt_ok = 1;
    else if (TAG("font"))
    {
        if (p->ignore)
            ;
        else if (!end)
        {
            color_ok = face_ok = 1;
            push_elem(p, t + name, namelen);
        }
        else
            pop_elem(p, t + name, namelen);
    }
    else if (TAG("body"))
    {
        /* as in HTML TADS, what a <BODY> leaves out is the default */
        body = !p->ignore && !end && !p->banner;
        if (body)
        {
            p->cur.fg = OS_COLOR_P_TEXT;
            body_bg = OS_COLOR_P_TEXTBG;
        }
    }
    else if (TAG("hr"))
    {
        if (!p->ignore)
        {
            int n;
            para_break(p);
            for (n = p->sink->width(p->ctx) - 1 - 2 * p->indent ; n > 0 ; --n)
                put_visible(p, 0x2500);
            para_break(p);
        }
    }
    else if (TAG("q"))
    {
        if (!p->ignore)
        {
            if (!end)
                ++p->quote_level;
            put_visible(p, (p->quote_level & 1)
                        ? (end ? 0x201D : 0x201C)
                        : (end ? 0x2019 : 0x2018));
            if (end)
                --p->quote_level;
        }
    }
    else if (TAG("title"))
    {
        if (end)
        {
            if (p->title > 0)
            {
                --p->ignore;
                if (--p->title == 0)
                    set_title(p);
            }
        }
        else
        {
            if (!p->title)
                p->titlelen = 0;
            ++p->ignore;
            ++p->title;
        }
    }
    else if (TAG("aboutbox") || (tads3 && TAG("banner")))
    {
        if (end)
        {
            if (p->ignore > p->title)
                --p->ignore;
        }
        else
            ++p->ignore;
    }
    else if (TAG("banner"))
    {
        /* TADS 2 shows a banner's text in line, but not its <body> */
        if (p->ignore)
            ;
        else if (end)
            pop_elem(p, t + name, namelen);
        else
        {
            push_elem(p, t + name, namelen);
            p->banner = 1;
        }
    }
    else if (TAG("pre"))
    {
        if (!end)
            ++p->pre;
        blank_line(p);
        if (end && p->pre > 0)
            --p->pre;
        if (p->ignore)
            ;
        else if (end)
            pop_elem(p, t + name, namelen);
        else
        {
            push_elem(p, t + name, namelen);
            p->cur.attr = (p->cur.attr & ~OSSTY_INPUT) | OSSTY_FIXED;
        }
    }
#undef TAG_IN
#undef TAG

    /*
     *   attributes: HEIGHT of <br>, ALT of <img> and <sound>, those of
     *   <font> and <body>, ALIGN of <p>, <div> and <hN>, and in TADS 3,
     *   those of <tab>
     */
    while (i < len)
    {
        size_t an, anlen, v = 0, vlen = 0;
        glui32 c = t[i];

        if (c == '"' || c == '\'')
        {
            for (++i ; i < len && t[i] != c ; ++i)
                ;
            ++i;
            continue;
        }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        {
            ++i;
            continue;
        }
        for (an = i ; i < len && is_alnum(t[i]) ; ++i)
            ;
        anlen = i - an;
        if (i < len && t[i] == '=')
        {
            ++i;
            if (i < len && (t[i] == '"' || t[i] == '\''))
            {
                glui32 qu = t[i++];
                for (v = i ; i < len && t[i] != qu ; ++i)
                    ;
                vlen = i - v;
                if (i < len)
                    ++i;
            }
            else if (tads3)
            {
                /* up to a space */
                for (v = i ; i < len && !is_space(t[i]) ; ++i)
                    ;
                vlen = i - v;
            }
            else
            {
                /* the first character, then letters and digits */
                v = i;
                if (i < len)
                    ++i;
                for ( ; i < len && is_alnum(t[i]) ; ++i)
                    ;
                vlen = i - v;
            }
        }

#define ATTR(s) tag_is(t + an, anlen, s)
        if (br && ATTR("height"))
        {
            int ht = to_int(t + v, vlen);
            if (ht == 0)
                br = 1;
            else if (ht > 1)
                for ( ; ht > 0 ; --ht)
                    blank_line(p);
        }
        else if (alt_ok && !p->ignore && ATTR("alt"))
        {
            size_t k;
            for (k = v ; k < v + vlen ; ++k)
                put_html_text(p, t[k]);
        }
        else if (align_ok && ATTR("align"))
            align = tag_is(t + v, vlen, "center")
                || tag_is(t + v, vlen, "right");
        else if (face_ok && ATTR("face"))
            face = face_combo(t + v, vlen);
        else if (color_ok && ATTR("color"))
            parse_color(t + v, vlen, &p->cur.fg);
        else if (color_ok && ATTR("bgcolor"))
            parse_color(t + v, vlen, &p->cur.bg);
        else if (body && ATTR("text"))
            parse_color(t + v, vlen, &p->cur.fg);
        else if (body && ATTR("bgcolor"))
            parse_color(t + v, vlen, &body_bg);
        else if (tab && ATTR("id"))
        {
            int k = find_tab(p, t + v, vlen, 1);
            if (k >= 0)
                p->tabs[k].col = p->col + (int)p->npending;
            tab = 0;
        }
        else if (tab && ATTR("to"))
        {
            int k = find_tab(p, t + v, vlen, 0);
            if (k >= 0)
                tab_to = p->tabs[k].col;
            else
                tab = 0;
        }
        else if (tab && ATTR("indent"))
        {
            put_tab(p, to_int(t + v, vlen), 0);
            tab = 0;
        }
        else if (tab && ATTR("multiple"))
        {
            put_tab(p, 0, to_int(t + v, vlen));
            tab = 0;
        }
        else if (tab && ATTR("align"))
        {
            if (tag_is(t + v, vlen, "left"))
                tab_align = TAB_LEFT;
            else if (tag_is(t + v, vlen, "right"))
                tab_align = TAB_RIGHT;
            else if (tag_is(t + v, vlen, "center"))
                tab_align = TAB_CENTER;
            else if (tag_is(t + v, vlen, "decimal"))
                tab_align = TAB_DECIMAL;
            else
                tab = 0;
        }
        else if (tab && ATTR("dp"))
            tab_dp = vlen ? t[v] : 0;
#undef ATTR
    }

    if (body)
        set_body(p, body_bg);

    if (br == 1)
        line_break(p);
    else if (br == 2)
        blank_line(p);

    /*
     *   <font FACE>, and the tags that ALIGN opens: a <p> only with it, as
     *   a <p> is seldom closed
     */
    if (face >= 0)
        p->cur.attr = (p->cur.attr & ~(OSSTY_FIXED | OSSTY_INPUT)) | face;
    if (align_ok && (align >= 0 || !tag_is(t + name, namelen, "p")))
    {
        push_elem(p, t + name, namelen);
        if (heading)
            p->cur.attr |= OSSTY_BOLD;
        if (align == 1)
            p->cur.attr |= OSSTY_CENTER;
        else if (align == 0)
            p->cur.attr &= ~OSSTY_CENTER;
    }

    /* TADS 3: what's left of the <TAB> */
    if (!tab)
        ;
    else if (tab_to < 0 && tab_align == TAB_NONE)
        put_tab(p, 0, 4);
    else if (tab_to < 0 && tab_align != TAB_RIGHT && tab_align != TAB_CENTER)
        ;
    else if (tab_align == TAB_LEFT || tab_align == TAB_NONE)
        put_tab(p, tab_to - (p->col + (int)p->npending), 0);
    else
    {
        /*
         *   the spaces go in when we know how much text follows, after
         *   those before the <TAB>, which the formatter would trim if
         *   nothing follows
         */
        p->tab_align = tab_align;
        p->tab_lead = p->npending;
        p->tab_start = p->col + (int)p->npending;
        p->tab_to = tab_to;
        p->tab_dp = (tab_align == TAB_DECIMAL && tab_dp == 0) ? '.' : tab_dp;
        p->tab_color = cur_color(p);
    }
}

static void tag_char(parser *p, glui32 c)
{
    if (p->tagquote)
    {
        if (c == p->tagquote)
            p->tagquote = 0;
    }
    else if (c == '"' || c == '\'')
        p->tagquote = c;
    else if (c == '>')
    {
        p->state = TOK_TEXT;
        do_tag(p);
        return;
    }
    if (p->taglen < TAG_MAX)
        p->tag[p->taglen++] = c;
}

/* ------------------------------------------------------------------------ */
/*
 *   The tokenizer
 */

static void feed(parser *p, glui32 c)
{
    switch (p->state)
    {
    case TOK_TAG:
        tag_char(p, c);
        return;

    case TOK_ENTITY:
        entity_char(p, c);
        return;

    case TOK_TEXT:
        break;
    }

    if (!p->html)
        put_plain_text(p, c);
    else if (c == '<')
    {
        p->state = TOK_TAG;
        p->taglen = 0;
        p->tagquote = 0;
    }
    else if (c == '&')
    {
        p->state = TOK_ENTITY;
        p->entitylen = 0;
        p->entity[0] = '\0';
    }
    else
        put_html_text(p, c);
}

static void feed_bytes(parser *p, const char *str, size_t len)
{
    glui32 *buf;
    glui32 n, i;

    if (len == 0)
        return;
    buf = malloc(sizeof(glui32) * (len + 1));
    if (!buf)
        return;

    /* as os_put_buffer decodes it; Latin-1 if the charset is unknown */
    n = os_parse_chars((unsigned char *)str, (glui32)len, buf, (glui32)len);
    if (n == 0)
        for (n = 0 ; n < len ; ++n)
            buf[n] = (unsigned char)str[n];

    for (i = 0 ; i < n ; ++i)
        feed(p, buf[i]);
    free(buf);
}

/* ------------------------------------------------------------------------ */
/*
 *   Interface
 */

void oshtml_set_tads3(int on)
{
    tads3 = on;
}

static int os_combo(int attr)
{
    return ((attr & OS_ATTR_BOLD) ? OSSTY_BOLD : 0)
        | ((attr & OS_ATTR_ITALIC) ? OSSTY_ITALIC : 0);
}

/* show the text of a centred line that we pad, as if the line ended */
static void show_held(parser *p)
{
    if (p->center_hold)
        expand_tab(p, 1);
}

oshtml_parser *oshtml_new(const oshtml_sink *sink, void *ctx)
{
    parser *p = calloc(1, sizeof(parser));

    if (!p)
        return 0;
    p->sink = sink;
    p->ctx = ctx;
    p->html = 1;
    p->cur.fg = OS_COLOR_P_TEXT;
    p->cur.bg = OS_COLOR_P_TRANSPARENT;
    p->sent = p->cur;
    p->screen = OS_COLOR_P_TEXTBG;
    p->text = OS_COLOR_P_TEXT;
    return p;
}

void oshtml_delete(oshtml_parser *p)
{
    free(p);
}

void oshtml_write(oshtml_parser *p, const char *str, size_t len)
{
    feed_bytes(p, str, len);
    flush_out(p);
}

/* the formatter flushed a line without ending it: show what it holds */
void oshtml_flush_parser(oshtml_parser *p)
{
    /* a centred line we pad waits for its end */
    if (!p->center_hold)
        expand_tab(p, 0);

    /*
     *   TADS 3 doesn't show the spaces at the end of the line until more
     *   text follows them; the formatter trims them itself, but not its
     *   special spaces, which reach us as &#nnn;
     */
    if (!tads3)
        print_pending(p);
    p->buffered = p->npending != 0;
    sync_color(p);
    flush_out(p);
}

/* the window was cleared */
/*
 *   HTML TADS clears the page by closing every tag (clear_page in
 *   htmlprs.cpp), which ends a <DIV> or <CENTER> a game left open
 */
static void close_all(parser *p)
{
    if (p->nelems)
        restore_elem(p, 0);
    p->cur.attr = 0;
    p->cur.fg = p->text;
    p->cur.bg = OS_COLOR_P_TRANSPARENT;
}

void oshtml_clear(oshtml_parser *p)
{
    p->tab_align = TAB_NONE;
    p->center_hold = 0;
    p->nhold = 0;
    p->npending = 0;
    p->col = 0;
    p->lastchar = 0;
    p->buffered = 0;
    p->just_nl = 0;
    p->nl_count = 0;
    p->line_open = 0;
    close_all(p);
}

void oshtml_clear_main(void)
{
    /* the TADS 3 formatter empties its buffers; TADS 2's goes on */
    if (tads3)
        oshtml_clear(&fmt);
    else
        close_all(&fmt);
}

void oshtml_set_attr(oshtml_parser *p, int attr)
{
    p->os_attr = os_combo(attr);
}

void oshtml_set_color(oshtml_parser *p, os_color_t fg, os_color_t bg)
{
    p->cur.fg = fg;
    p->cur.bg = bg;
}

void oshtml_start(void)
{
    fmt.html = 1;
}

void oshtml_end(void)
{
    /* the formatter flushed the line without ending it */
    show_held(&fmt);
    oshtml_flush_parser(&fmt);
    fmt.html = 0;
}

void oshtml_print_main(const char *str, size_t len)
{
    oshtml_write(&fmt, str, len);
}

size_t oshtml_print_status(const char *str, size_t len,
                           glui32 *out, size_t max)
{
    parser *p = &fmt;

    /* a centred line held for the main window stays there */
    show_held(p);
    p->dst = out;
    p->dstlen = 0;
    p->dstmax = max;
    feed_bytes(p, str, len);
    p->dst = 0;
    return p->dstlen;
}

void oshtml_put_plain(const char *str, size_t len)
{
    show_held(&fmt);
    oshtml_flush();

    /*
     *   TADS 3 would show the spaces it holds at the end of the line only
     *   if more text followed on that line: after the MORE prompt, nothing
     *   does
     */
    if (tads3)
        fmt.npending = 0;
    os_put_buffer((unsigned char *)str, len);

    /* the caller may set the style around this */
    fmt.sent.attr = -1;
}

void oshtml_flush(void)
{
    oshtml_flush_parser(&fmt);
}

void oshtml_input(void)
{
    /* the formatter shows the spaces at the end of the line before input */
    show_held(&fmt);
    print_pending(&fmt);
    fmt.buffered = 0;
    flush_out(&fmt);
}

void oshtml_input_done(void)
{
    /* the line input echoed a newline: TADS 3 starts over at column 0 */
    if (tads3)
    {
        fmt.npending = 0;
        fmt.buffered = 0;
        fmt.col = 0;
        fmt.lastchar = 0;
        fmt.just_nl = 1;
        fmt.nl_count = 1;
        fmt.line_open = 0;
    }
}

void oshtml_set_os_attr(int attr)
{
    color c;

    oshtml_set_attr(&fmt, attr);
    if (tads3)
        return;

    /* TADS 2 doesn't sync the window when it flushes */
    c = cur_color(&fmt);
    send_color(&fmt, &c);
    flush_out(&fmt);
}

void oshtml_set_os_color(os_color_t fg, os_color_t bg)
{
    oshtml_set_color(&fmt, fg, bg);
}

void oshtml_arrange(void)
{
    memset(fmt.centers, 0, sizeof(fmt.centers));
}

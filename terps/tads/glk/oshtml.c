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
 *   - in TADS 3 and in TADS 2's banners, <tab> takes ID, TO, ALIGN, DP,
 *     INDENT and MULTIPLE;
 *   - <a href> is a link (osglk.c), unless the href is a URL;
 *   - <img> draws its picture where the window can show one, and prints
 *     its ALT text where it can't; the ALT text of <sound> is printed;
 *   - Glk can't make parts of a picture clickable, so the <area>s of an
 *     <img usemap> are a row of links under the pictures, named by their
 *     ALT text or else their hrefs (which are commands); at the end of a
 *     banner, the picture goes in a graphics window, where they're clicked;
 *   - <title> sets the window title, and nothing inside <title> or
 *     <aboutbox> (or in TADS 3, <banner>) is shown;
 *   - in TADS 2, the text of a <banner> is kept until its </banner>, and
 *     then replaces what the banner's window had (osglkban.c), through a
 *     parser of the banner's own;
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
#include "osres.h"
#include "ossound.h"
#include "gi_blorb.h"

extern glui32 os_parse_chars(unsigned char *buf, glui32 buflen,
                             glui32 *out, glui32 outlen);

/* the longest tag and entity name we keep; the rest is skipped */
#define TAG_MAX     1024
#define LINK_MAX    256
#define ENTITY_MAX  9           /* as TADS 2 has it: "thetasym" fits */
#define PENDING_MAX 256
#define OUT_MAX     256
#define TITLE_MAX   256
#define HOLD_MAX    1024        /* text after a right, center or decimal tab */
#define ELEMS_MAX   64          /* open tags we keep */
#define ELEM_NAME   12
#define TABS_MAX    64          /* <TAB ID> stops */
#define TABID_MAX   32
#define BANNER_MAX  65536       /* the text of a <BANNER> we keep */
#define MAPS_MAX    32          /* <MAP>s */
#define AREAS_MAX   48          /* <AREA>s of a map, and links in a row */
#define MAPNAME_MAX 32
#define LABEL_MAX   80
#define CELL_GAP    3           /* the spaces between table cells */
#define CELL_INDENT 2           /* after empty cells, in steps of two spaces */

enum token { TOK_TEXT, TOK_TAG, TOK_ENTITY };

/* <TAB ALIGN> */
enum tab_align { TAB_NONE, TAB_LEFT, TAB_RIGHT, TAB_CENTER, TAB_DECIMAL };

/* text attributes, as OSSTY_* bits, and colours */
typedef struct color
{
    int attr;
    os_color_t fg, bg;
    int link;                   /* osglk_link_add's number, or 0 */
} color;

/* an <AREA> of an image map, as a link */
typedef struct maplink
{
    glui32 link;
    glui32 label[LABEL_MAX];
    size_t len;
} maplink;

/* the innermost list */
enum list_kind { LIST_NONE, LIST_UL, LIST_OL };

/* an open tag, and the state from before its start tag */
typedef struct elem
{
    char name[ELEM_NAME];
    color cur;
    int indent;
    int cell_indent;
    enum list_kind list;
    int nitem;
    int nobr;
    int table;
    int ncell;
    int table_box;
    int row_align;
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
    int cell_indent;            /* the part of it empty cells left */
    enum list_kind list;
    int nitem;                  /* the list's items so far */
    int nobr;                   /* inside <nobr> */
    int table;                  /* the <TABLE>s we're inside */
    int ncell;                  /* the cells of the row so far */
    int row_shown;              /* the row has put something out */
    int table_box;              /* the <TABLE> has a WIDTH and BGCOLOR */
    int row_align;              /* the <TR> has an ALIGN */

    /*
     *   A row's first cell holds back its first character (cell_hold 2)
     *   until we know whether the cell is just that one, a menu's ">" say.
     *   Then it and the gap after it are fixed-pitch, like the indent of
     *   the rows whose first cell is empty, so that the next cells line
     *   up in a proportional font.
     */
    int cell_hold;              /* 1: in a first cell, 2: holding held_c */
    glui32 held_c;
    color held_color;

    /*
     *   the main window: a picture alone so far in a table cell, held back
     *   until we know whether it goes beside the page (hold_cell_picture);
     *   cell_empty is set while nothing has come since the cell began
     */
    int cell_empty;
    int cell_pic;               /* 1: a first cell's, 2: a last cell's */
    oshtml_image cell_img;
    int cell_pic_space;         /* a space came after it */
    int side_left;              /* the row's first cell went beside it */

    /*
     *   TADS 2: a <BANNER>, whose text waits for its </BANNER>, and whether
     *   we're inside the tag of the text (and inside quotes in it)
     */
    int in_banner;              /* this is the banner's own parser */
    int bancap;                 /* reading the banner's text */
    oshtml_banner ban;
    glui32 *bantext;
    size_t banlen, banmax;
    size_t bantag;              /* where the tag began */
    int bantag_open;
    glui32 banquote;

    /* the open tags */
    elem elems[ELEMS_MAX];
    int nelems;

    /*
     *   the <MAP> whose <AREA>s we're reading, and the links of the
     *   pictures shown since the last text, which go under them
     */
    struct imagemap *map;
    maplink maprow[AREAS_MAX];
    int nmaprow;

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

static int main_rule_width(void *ctx, int combo)
{
    (void)ctx;
    if (!mainwin)
        return G_os_linewidth;
    return osglk_rule_width(mainwin, combo);
}

static winid_t main_window(void *ctx)
{
    (void)ctx;
    return mainwin;
}

static void main_set_link(void *ctx, glui32 link)
{
    (void)ctx;
    glk_set_hyperlink(link);
}

static const oshtml_sink main_sink =
{
    main_put, main_set_attr, main_set_color, main_set_screen_color,
    main_width, main_centers, main_window, main_set_link, NULL,
    main_rule_width
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
    return a->attr == b->attr && a->fg == b->fg && a->bg == b->bg
        && a->link == b->link;
}

static void release_held(parser *p, int fixed);

static void flush_out(parser *p)
{
    release_held(p, 0);
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
    if (s.link != p->sent.link && p->sink->set_link)
    {
        flush_out(p);
        p->sink->set_link(p->ctx, (glui32)s.link);
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
static void put_maprow(parser *p);
static void release_cell_picture(parser *p);

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
            p->row_shown = 1;
            return;
        }
    }
    emit_now(p, c, col);
}

/*
 *   put out the first cell's held character, fixed-pitch if the cell has
 *   no more than it
 */
static void release_held(parser *p, int fixed)
{
    color col = p->held_color;

    if (p->cell_hold != 2)
        return;
    p->cell_hold = 0;
    if (fixed)
        col.attr |= OSSTY_FIXED;
    emit_now(p, p->held_c, col);
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
    release_held(p, 0);
    send_color(p, &col);
    if (p->outlen == OUT_MAX)
        flush_out(p);
    p->out[p->outlen++] = c;
    p->row_shown = 1;
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
    int n, shown;

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
    shown = p->row_shown;
    for (n = 2 * p->indent ; n > 0 ; --n)
    {
        if (n == 2 * p->cell_indent)
            c.attr |= OSSTY_FIXED;
        emit_now(p, 0xA0, c);
        ++p->col;
    }
    p->row_shown = shown;
}

/* print a visible character */
static void put_visible(parser *p, glui32 c)
{
    put_maprow(p);
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
    if (p->cell_hold == 1 && !p->dst && p->tab_align == TAB_NONE
        && !p->line_center)
    {
        p->cell_hold = 2;
        p->held_c = c;
        p->held_color = cur_color(p);
        p->row_shown = 1;
    }
    else
    {
        if (p->cell_hold == 2)
            release_held(p, 0);
        p->cell_hold = 0;
        emit(p, c, cur_color(p));
    }
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
    p->row_shown = 1;
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
    put_maprow(p);
    line_break(p);
    put_newline(p);
}

/* the space between blocks: a blank line, unless there is one already */
static void para_break(parser *p)
{
    put_maprow(p);
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

/* 'n' spaces, fixed-pitch, to line up with a fixed-pitch cell */
static void put_fixed_tab(parser *p, int n)
{
    size_t i = p->npending;

    put_tab(p, n, 0);
    for ( ; i < p->npending ; ++i)
        p->pending_color[i].attr |= OSSTY_FIXED;
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

    /* a space waits with a picture held in a cell; anything else is text */
    if (p->cell_pic && is_space(c))
    {
        p->cell_pic_space = 1;
        return;
    }
    release_cell_picture(p);
    if (!is_space(c))
        p->cell_empty = 0;

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
    "h1", "h2", "h3", "h4", "h5", "h6", "banner",
    "table", "tr", "td", "th", 0
};
static const char *const dd_stops[] =
{
    "blockquote", "bq", "ul", "ol", "dl", "banner",
    "table", "tr", "td", "th", 0
};

/* and those that a row doesn't */
static const char *const tr_stops[] = { "table", 0 };

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
    e->cell_indent = p->cell_indent;
    e->list = p->list;
    e->nitem = p->nitem;
    e->nobr = p->nobr;
    e->table = p->table;
    e->ncell = p->ncell;
    e->table_box = p->table_box;
    e->row_align = p->row_align;
}

/* close the open tag at 'i', and those inside it */
static void restore_elem(parser *p, int i)
{
    const elem *e = &p->elems[i];

    p->cur = e->cur;
    p->indent = e->indent;
    p->cell_indent = e->cell_indent;
    p->list = e->list;
    p->nitem = e->nitem;
    p->nobr = e->nobr;
    p->table = e->table;
    p->ncell = e->ncell;
    p->table_box = e->table_box;
    p->row_align = e->row_align;
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

/* a right-aligned cell: a right <TAB> to the end of the line */
static void right_cell(parser *p)
{
    expand_tab(p, 1);
    add_pending(p, ' ');
    p->tab_align = TAB_RIGHT;
    p->tab_lead = p->npending;
    p->tab_start = p->col + (int)p->npending;
    p->tab_to = -1;
    p->tab_color = cur_color(p);
}

/* close the open cell of the innermost <TABLE>, if it has one */
static void close_cell(parser *p)
{
    int i;

    for (i = p->nelems ; i > 0 ; --i)
    {
        const char *n = p->elems[i - 1].name;
        if (strcmp(n, "td") == 0 || strcmp(n, "th") == 0)
        {
            restore_elem(p, i - 1);
            return;
        }
        if (strcmp(n, "tr") == 0 || strcmp(n, "table") == 0)
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
 *   A colour of the game's own behind the theme's text colour could hide
 *   the text, so the text gets black or white, whichever shows.  Black is
 *   1/1/1: osglk_set_text_color takes 0 on a transparent background for the
 *   theme's colour (Hill Ridge).
 */
static void contrast_text(parser *p, os_color_t bg)
{
    if (!os_color_is_param(bg) && os_color_is_param(p->cur.fg))
    {
        unsigned long lum = 299 * ((bg >> 16) & 0xFF)
            + 587 * ((bg >> 8) & 0xFF) + 114 * (bg & 0xFF);
        p->cur.fg = lum >= 128000 ? os_rgb_color(1, 1, 1)
            : os_rgb_color(255, 255, 255);
    }
}

/* <BODY>: the window background */
static void set_body(parser *p, os_color_t bg)
{
    contrast_text(p, bg);
    p->text = p->cur.fg;
    if (bg != p->screen)
    {
        flush_out(p);
        p->sink->set_screen_color(p->ctx, bg);
        p->screen = bg;
    }
}

/* <BR CLEAR>: the next text goes below the pictures in the margins */
static void flow_break(parser *p)
{
    winid_t win;

    flush_out(p);
    win = p->dst ? NULL : p->sink->window(p->ctx);
    if (win)
        glk_window_flow_break(win);
}

/* an <IMG> size: pixels, or with 'pct' set, per cent of the window */
typedef struct img_size
{
    glui32 num;
    int pct;
} img_size;

static void parse_img_size(const glui32 *v, size_t len, img_size *out)
{
    int n;

    out->pct = len != 0 && v[len - 1] == '%';
    n = to_int(v, out->pct ? len - 1 : len);
    out->num = n > 0 ? (glui32)n : 0;
}

/* a resource name in UTF-8, or 0 if it doesn't fit */
static size_t url_utf8(const glui32 *src, size_t srclen,
                       char *name, size_t size)
{
    size_t i, n = 0;

    for (i = 0 ; i < srclen ; ++i)
    {
        glui32 c = src[i];
        if (n + 4 >= size)
            return 0;
        if (c < 0x80)
            name[n++] = (char)c;
        else if (c < 0x800)
        {
            name[n++] = (char)(0xC0 | (c >> 6));
            name[n++] = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            name[n++] = (char)(0xE0 | ((c >> 12) & 0x0F));
            name[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
            name[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    return n;
}

/*
 *   Draw an <IMG>: return false if it can't be, and its ALT text is to be
 *   shown instead.  A picture in the line counts as a character on it; one
 *   in a margin is beside the text, and leaves the line alone.
 */
void oshtml_draw_image(winid_t win, const oshtml_image *img)
{
    glui32 id = img->id, align = img->align, iw, ih;
    glui32 w = img->width_pct ? 0 : img->width;
    glui32 h = img->height_pct ? 0 : img->height;

    if (img->width_pct && img->width
        && glk_gestalt(gestalt_DrawImageScale, wintype_TextBuffer))
    {
        /* a share of the window's width, the height to match */
        glk_image_draw_scaled_ext(win, id, align, 0,
                                  img->width * 0x10000 / 100, 0x10000,
                                  imagerule_WidthRatio
                                  | imagerule_AspectRatio, 0);
    }
    else if (w || h)
    {
        /* pixels; with only one of them, the other keeps the shape */
        if ((!w || !h) && glk_image_get_info(id, &iw, &ih) && iw && ih)
        {
            if (!w)
                w = iw * h / ih;
            else
                h = ih * w / iw;
        }
        if (w && h)
            glk_image_draw_scaled(win, id, align, 0, w, h);
        else
            glk_image_draw(win, id, align, 0);
    }
    else
        glk_image_draw(win, id, align, 0);
}

/*
 *   The size oshtml_draw_image draws a picture at, in pixels, in a window
 *   whose text is width pixels across: false if it's not known, as for a
 *   share of a width of 0
 */
int oshtml_image_size(const oshtml_image *img, glui32 width,
                      glui32 *w, glui32 *h)
{
    glui32 iw, ih;

    if (!glk_image_get_info(img->id, &iw, &ih) || !iw || !ih)
        return 0;

    if (img->width_pct && img->width
        && glk_gestalt(gestalt_DrawImageScale, wintype_TextBuffer))
    {
        *w = width * img->width / 100;
        *h = ih * *w / iw;
        return *w != 0;
    }

    *w = img->width_pct ? 0 : img->width;
    *h = img->height_pct ? 0 : img->height;
    if (!*w && !*h)
    {
        *w = iw;
        *h = ih;
    }
    else if (!*w)
        *w = iw * *h / ih;
    else if (!*h)
        *h = ih * *w / iw;
    return 1;
}

/* the picture of an <IMG>: false if there's none, or it can't be shown */
static int find_image(parser *p, const glui32 *src, size_t srclen,
                      glui32 align, img_size w, img_size h,
                      oshtml_image *img)
{
    char name[512];
    size_t n;

    if (p->dst || srclen == 0 || !glk_gestalt(gestalt_Graphics, 0)
        || !glk_gestalt(gestalt_DrawImage, wintype_TextBuffer)
        || !(p->sink->image ? p->sink->image(p->ctx, NULL)
             : p->sink->window(p->ctx) != NULL))
        return 0;

    n = url_utf8(src, srclen, name, sizeof(name));
    img->id = n ? osres_find(name, n, giblorb_ID_Pict) : 0;
    if (img->id == 0)
        return 0;
    img->align = align;
    img->width = w.num;
    img->width_pct = w.pct;
    img->height = h.num;
    img->height_pct = h.pct;
    return 1;
}

static int show_image(parser *p, const oshtml_image *img)
{
    int inline_img = img->align != imagealign_MarginLeft
        && img->align != imagealign_MarginRight;

    if (inline_img)
    {
        /*
         *   text held back for a <TAB> comes first, and the picture goes
         *   on the line in its style, which centres it as <DIV ALIGN> says
         *   though no text came after the tag yet (1893's title)
         */
        color c;

        expand_tab(p, 1);
        start_line(p);
        print_pending(p);
        c = cur_color(p);
        send_color(p, &c);
    }
    flush_out(p);

    if (p->sink->image)
    {
        if (!p->sink->image(p->ctx, img))
            return 0;
    }
    else
    {
        /* asking for the window again puts out what the sink holds back */
        oshtml_draw_image(p->sink->window(p->ctx), img);
    }

    if (inline_img)
    {
        ++p->col;
        p->buffered = 1;
        p->just_nl = 0;
        p->nl_count = 0;
        p->lastchar = 0xFFFC;
    }
    p->row_shown = 1;
    return 1;
}

/*
 *   A picture alone so far in a cell of a table in the main window, which
 *   may go beside the page, as the borders of 1893's pages do: a tall one
 *   in a row's first cell, or one in its last cell after text.  It waits
 *   for what follows: a tag that ends the cell goes on waiting, and the
 *   next cell, row or the end of the table decides (cell_picture_tag).
 *   Anything else puts it in the text after all (release_cell_picture).
 */
static int hold_cell_picture(parser *p, const oshtml_image *img)
{
    glui32 iw, ih;

    if (p != &fmt || p->table != 1 || p->cur.link
        || img->align == imagealign_MarginLeft
        || img->align == imagealign_MarginRight
        || !oshtml_image_size(img, 0, &iw, &ih))
        return 0;

    if (p->ncell == 1 && ih >= 2 * iw)
        p->cell_pic = 1;
    else if (p->ncell > 1 && p->row_shown)
        p->cell_pic = 2;
    else
        return 0;
    p->cell_img = *img;
    p->cell_pic_space = 0;
    return 1;
}

static void release_cell_picture(parser *p)
{
    if (!p->cell_pic)
        return;
    p->cell_pic = 0;
    show_image(p, &p->cell_img);
    if (p->cell_pic_space)
        put_space(p);
}

/* a tag after the held picture: is it beside the page? */
static void cell_picture_tag(parser *p, const glui32 *name, size_t len,
                             int end)
{
    int cell = tag_is(name, len, "td") || tag_is(name, len, "th");
    int right;

    if (cell && end)
        return;
    if (cell)
        right = 0;
    else if (tag_is(name, len, "tr") || (end && tag_is(name, len, "table")))
        right = 1;
    else
    {
        release_cell_picture(p);
        return;
    }

    if (p->cell_pic != (right ? 2 : 1)
        || !osglk_side_picture(right, &p->cell_img))
    {
        release_cell_picture(p);
        return;
    }

    /* the next cell is the row's first, as far as the text goes */
    p->cell_pic = 0;
    if (!right)
    {
        p->side_left = 1;
        --p->ncell;
    }
}

/* an <IMG>, 'alone' if first in its cell: false if it can't be shown */
static int put_image(parser *p, const glui32 *src, size_t srclen,
                     glui32 align, img_size w, img_size h, int alone)
{
    oshtml_image img;

    if (!find_image(p, src, srclen, align, w, h, &img))
        return 0;
    if (alone && hold_cell_picture(p, &img))
        return 1;
    return show_image(p, &img);
}

/* <SOUND LAYER> and <SOUND CANCEL=layer>; -1 if it's none of them */
static int sound_layer(const glui32 *v, size_t len)
{
    if (tag_is(v, len, "foreground"))
        return OSSND_FOREGROUND;
    if (tag_is(v, len, "ambient"))
        return OSSND_AMBIENT;
    if (tag_is(v, len, "bgambient"))
        return OSSND_BGAMBIENT;
    if (tag_is(v, len, "background"))
        return OSSND_BACKGROUND;
    return -1;
}

/* <SOUND FADEIN|FADEOUT=[CROSSFADE,]seconds>, in milliseconds */
static unsigned long sound_fade(const glui32 *v, size_t len, int *crossfade)
{
    unsigned long ms = 0, unit = 1000;
    size_t k;

    *crossfade = 0;
    for (k = 0 ; k < len && v[k] != ',' ; ++k)
        ;
    if (k < len && tag_is(v, k, "crossfade"))
    {
        *crossfade = 1;
        v += k + 1;
        len -= k + 1;
    }
    for (k = 0 ; k < len && v[k] >= '0' && v[k] <= '9' ; ++k)
        ms = ms * 10 + (v[k] - '0');
    ms *= 1000;
    if (k < len && v[k] == '.')
        for (++k ; k < len && v[k] >= '0' && v[k] <= '9' && unit > 1 ; ++k)
            ms += (v[k] - '0') * (unit /= 10);
    return ms;
}

/*
 *   Play, queue or cancel a <SOUND>: return false if it can't be, and its
 *   ALT text is to be shown instead.  As in HTML TADS, sounds go in the
 *   main window and in <BANNER>s, and not in the banners of the API.
 */
static int put_sound(parser *p, ossound_tag *tag,
                     const glui32 *src, size_t srclen)
{
    char name[512];

    if (p->dst || !p->sink->window(p->ctx))
        return 0;
    tag->srclen = url_utf8(src, srclen, name, sizeof(name));
    tag->src = name;
    if (!tag->cancel && tag->srclen == 0)
        return 0;
    return ossound_do_tag(tag);
}

/* the link for an HREF, with its &amp;s decoded; 0 if it's plain text */
static glui32 href_link(const glui32 *v, size_t len, int flags)
{
    char buf[LINK_MAX];
    size_t k, n = 0;

    for (k = 0 ; k < len && n < sizeof(buf) - 1 ; ++k)
    {
        glui32 c = v[k];
        if (c == '&' && k + 4 < len && tag_is(v + k, 5, "&amp;"))
            k += 4;
        buf[n++] = c < 256 ? (char)c : '?';
    }
    buf[n] = '\0';
    return osglk_link_add(buf, flags);
}

/* ------------------------------------------------------------------------ */
/*
 *   Image maps.  Glk can't make part of a picture a link, so the <AREA>s of
 *   an <IMG USEMAP> are shown as a row of links under it, each named by its
 *   ALT, or failing that its HREF, which is the command it types.  The maps
 *   are kept for every window, as HTML TADS keeps them for the document, and
 *   a <MAP> with a name we have replaces the old one.  In the corpus the
 *   <MAP> always comes before its <IMG>.
 *
 *   A picture can be one tile of several (Golden Skull's compass is nine,
 *   in rows with <BR> between them), so the row waits until the pictures
 *   end: until the next text, paragraph or input, and leaves out the links
 *   it has already.
 */
typedef struct imagemap
{
    glui32 name[MAPNAME_MAX];
    size_t namelen;
    int nareas;
    maplink areas[AREAS_MAX];

    /* where the areas are on the picture, unless one isn't a rectangle */
    int nrects;
    oshtml_tile rects[AREAS_MAX];
    int unplaced;
} imagemap;

static imagemap maps[MAPS_MAX];
static int nmaps, map_next;

/* the map with this name ("#name" or "name"), made if 'create' */
static imagemap *find_map(const glui32 *v, size_t len, int create)
{
    glui32 folded[MAPNAME_MAX];
    imagemap *m;
    size_t k;
    int i;

    if (len != 0 && v[0] == '#')
        ++v, --len;
    if (len == 0 || len > MAPNAME_MAX)
        return NULL;
    for (k = 0 ; k < len ; ++k)
        folded[k] = v[k] >= 'A' && v[k] <= 'Z' ? v[k] + ('a' - 'A') : v[k];
    for (i = 0 ; i < nmaps ; ++i)
        if (maps[i].namelen == len
            && memcmp(maps[i].name, folded, len * sizeof(glui32)) == 0)
            return &maps[i];
    if (!create)
        return NULL;

    /* when the table is full, the oldest map goes */
    if (nmaps < MAPS_MAX)
        m = &maps[nmaps++];
    else
    {
        m = &maps[map_next];
        map_next = (map_next + 1) % MAPS_MAX;
    }
    memcpy(m->name, folded, len * sizeof(glui32));
    m->namelen = len;
    return m;
}

/* an <AREA> of the map being read; a repeated link is left out */
static void add_area(imagemap *m, glui32 link, const glui32 *label,
                     size_t len)
{
    int i;

    /* the label without the spaces around it */
    while (len != 0 && is_space(*label))
        ++label, --len;
    while (len != 0 && is_space(label[len - 1]))
        --len;
    if (link == 0 || len == 0 || m->nareas == AREAS_MAX)
        return;
    for (i = 0 ; i < m->nareas ; ++i)
        if (m->areas[i].link == link)
            return;
    if (len > LABEL_MAX)
        len = LABEL_MAX;
    m->areas[i].link = link;
    memcpy(m->areas[i].label, label, len * sizeof(glui32));
    m->areas[i].len = len;
    ++m->nareas;
}

/* an <AREA>'s rectangle, "left,top,right,bottom" */
static void add_area_rect(imagemap *m, glui32 link, int rect,
                          const glui32 *v, size_t len)
{
    int n[4], i;
    size_t k = 0;
    oshtml_tile *r;

    for (i = 0 ; i < 4 ; ++i)
    {
        while (k < len && (is_space(v[k]) || v[k] == ','))
            ++k;
        if (k == len || v[k] < '0' || v[k] > '9')
            break;
        n[i] = 0;
        for ( ; k < len && v[k] >= '0' && v[k] <= '9' ; ++k)
            n[i] = n[i] * 10 + (int)(v[k] - '0');
    }
    if (!rect || i < 4 || n[2] <= n[0] || n[3] <= n[1])
    {
        m->unplaced = 1;
        return;
    }
    if (m->nrects == AREAS_MAX)
        return;
    r = &m->rects[m->nrects++];
    memset(r, 0, sizeof(*r));
    r->x = (glui32)n[0];
    r->y = (glui32)n[1];
    r->w = (glui32)(n[2] - n[0]);
    r->h = (glui32)(n[3] - n[1]);
    r->link = link;
    r->area = 1;
}

/* the links of an <IMG USEMAP>, for the row under the pictures */
static void add_map_links(parser *p, const imagemap *m)
{
    int i, j;

    if (p->dst)
        return;
    for (i = 0 ; i < m->nareas && p->nmaprow < AREAS_MAX ; ++i)
    {
        for (j = 0 ; j < p->nmaprow ; ++j)
            if (p->maprow[j].link == m->areas[i].link)
                break;
        if (j == p->nmaprow)
            p->maprow[p->nmaprow++] = m->areas[i];
    }
}

/* the row of links under the pictures, on a line of its own */
static void put_maprow(parser *p)
{
    int save = p->cur.link;
    int i, n = p->nmaprow;
    size_t k;

    if (n == 0 || p->dst)
        return;

    /* the row's own text doesn't put it out again */
    p->nmaprow = 0;
    line_break(p);
    for (i = 0 ; i < n ; ++i)
    {
        /* one space each side, even after a full stop in TADS 2 */
        if (i != 0)
        {
            add_pending(p, ' ');
            put_visible(p, 0x00B7);
            add_pending(p, ' ');
        }
        p->cur.link = (int)p->maprow[i].link;
        for (k = 0 ; k < p->maprow[i].len ; ++k)
            put_html_text(p, p->maprow[i].label[k]);
        p->cur.link = save;
    }
    line_break(p);
}

/* the tag in p->tag, between '<' and '>' */
static void do_tag(parser *p)
{
    const glui32 *t = p->tag;
    size_t len = p->taglen, i = 0, name, namelen;
    int end = 0;
    int br = 0;                 /* 1: line break, 2: blank line */
    int img = 0, clear = 0;     /* <img>, <br CLEAR> */
    int snd = 0;                /* <sound> */
    ossound_tag sndtag;
    size_t src = 0, srclen = 0, alt = 0, altlen = 0;
    glui32 img_align = imagealign_InlineUp;
    img_size img_w = { 0, 0 }, img_h = { 0, 0 };
    int color_ok = 0, body = 0; /* <font>, <body> */
    os_color_t body_bg = OS_COLOR_P_TEXTBG;
    int face_ok = 0, face = -1; /* <font FACE> */
    int align_ok = 0;           /* <p>, <div>, <hN>: 1 to open the tag */
    int align = -1;             /* their ALIGN: 0 left, 1 centred */
    int heading = 0;
    int tab = 0;                /* TADS 3: <tab> */
    int ban = 0, ban_remove = 0; /* TADS 2: <banner>, and REMOVE(ALL) */
    int link_ok = 0;            /* <a> */
    size_t href = 0, hreflen = 0;
    int have_href = 0, link_flags = 0;
    int map_ok = 0, area_ok = 0; /* <map>, <area> */
    size_t mapname = 0, mapnamelen = 0, usemap = 0, usemaplen = 0;
    size_t coords = 0, coordslen = 0;
    int nohref = 0, area_rect = 1;
    int cell_ok = 0, cell = 0;  /* <table>, <tr>, <td>; 1 for a <td> */
    int cell_first = 0;         /* the row's first cell */
    int held_one = 0;           /* the first cell was one character */
    int cell_align = -1;        /* ALIGN: 0 left, 1 centred, 2 right */
    int table_open = 0, row_open = 0;
    int cell_width = 0;         /* a WIDTH */
    int cell_bg_ok = 0;
    os_color_t cell_bg = OS_COLOR_P_TRANSPARENT;
    int tab_to = -1;
    enum tab_align tab_align = TAB_NONE;
    glui32 tab_dp = 0;
    int cell_empty = p->cell_empty; /* nothing yet in the cell */

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

    p->cell_empty = 0;
    if (p->cell_pic)
        cell_picture_tag(p, t + name, namelen, end);

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
    else if (TAG("table"))
    {
        /* a block, whose rows are lines */
        if (p->ignore)
            ;
        else if (end)
        {
            if (p->table)
            {
                release_held(p, 0);
                p->cell_hold = 0;
                pop_elem(p, t + name, namelen);
                line_break(p);
            }
        }
        else
        {
            release_held(p, 0);
            p->cell_hold = 0;
            line_break(p);
            push_elem(p, t + name, namelen);
            ++p->table;
            table_open = 1;
            p->ncell = 0;
            p->row_shown = 0;
            p->side_left = 0;
            cell_ok = 1;
        }
    }
    else if (TAG("tr"))
    {
        /* a row ends the row before it, which may not have been closed */
        if (!p->ignore && p->table)
        {
            release_held(p, 0);
            p->cell_hold = 0;
            close_cell(p);
            close_elem(p, "tr", tr_stops);
            line_break(p);
            if (!end)
            {
                push_elem(p, t + name, namelen);
                row_open = 1;
                p->ncell = 0;
                p->row_shown = 0;
                p->side_left = 0;
                cell_ok = 1;
            }
        }
    }
    else if (TAG("td") || TAG("th"))
    {
        /* so does a cell */
        if (!p->ignore && p->table)
        {
            close_cell(p);
            if (!end)
            {
                cell_first = p->ncell++ == 0;
                if (cell_first)
                    release_held(p, 0);
                held_one = !cell_first && p->cell_hold == 2;
                p->cell_hold = cell_first ? 1 : held_one ? 2 : 0;
                push_elem(p, t + name, namelen);
                if (TAG("th"))
                    p->cur.attr |= OSSTY_BOLD;
                cell_ok = cell = 1;
                p->cell_empty = 1;
            }
        }
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
        else if (tads3 || p->in_banner)
        {
            expand_tab(p, 1);
            tab = 1;
        }
        else
            put_tab(p, 0, 4);
    }
    else if (TAG("a"))
    {
        if (p->ignore)
            ;
        else if (end)
            pop_elem(p, t + name, namelen);
        else
        {
            push_elem(p, t + name, namelen);
            link_ok = 1;
        }
    }
    else if (TAG("img"))
        img = !p->ignore && !end;
    else if (TAG("map"))
    {
        p->map = NULL;
        map_ok = !p->ignore && !end;
    }
    else if (TAG("area"))
        area_ok = p->map && !end;
    else if (TAG("sound"))
    {
        snd = !p->ignore && !end;
        ossound_tag_init(&sndtag);
    }
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
        body = !p->ignore && !end;
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
            n = p->sink->rule_width(p->ctx, cur_color(p).attr)
                - 1 - 2 * p->indent;
            for ( ; n > 0 ; --n)
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
        /* TADS 2: the text waits for its </BANNER>; a stray one is dropped */
        if (!p->ignore && !end && !p->in_banner)
        {
            ban = 1;
            memset(&p->ban, 0, sizeof(p->ban));
            p->ban.align = OS_BANNER_ALIGN_TOP;
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
     *   attributes: HEIGHT of <br>, those of <img> and <sound>, those of
     *   <font> and <body>, ALIGN of <p>, <div> and <hN>, in TADS 3 and in
     *   banners, those of <tab>, and those of TADS 2's <banner>
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
            else if (tads3 || p->in_banner || ban || img || snd || link_ok
                     || map_ok || area_ok)
            {
                /* up to a space, as HTML TADS reads it */
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
        if (link_ok && ATTR("href"))
        {
            href = v;
            hreflen = vlen;
            have_href = 1;
        }
        else if (link_ok && ATTR("noenter"))
            link_flags |= OSLINK_NOENTER;
        else if (link_ok && ATTR("append"))
            link_flags |= OSLINK_APPEND;
        else if (map_ok && ATTR("name"))
        {
            mapname = v;
            mapnamelen = vlen;
        }
        else if (area_ok && ATTR("href"))
        {
            href = v;
            hreflen = vlen;
            have_href = 1;
        }
        else if (area_ok && ATTR("alt"))
        {
            alt = v;
            altlen = vlen;
        }
        else if (area_ok && ATTR("nohref"))
            nohref = 1;
        else if (area_ok && ATTR("shape"))
            area_rect = tag_is(t + v, vlen, "rect");
        else if (area_ok && ATTR("coords"))
        {
            coords = v;
            coordslen = vlen;
        }
        else if (img && ATTR("usemap"))
        {
            usemap = v;
            usemaplen = vlen;
        }
        else if (ban && ATTR("id"))
        {
            size_t k;
            for (k = 0 ; k < vlen && k < OSHTML_BANNER_ID - 1 ; ++k)
                p->ban.id[k] = t[v + k] < 0x80 ? (char)t[v + k] : '?';
            p->ban.id[k] = '\0';
        }
        else if (ban && (ATTR("height") || ATTR("width")))
        {
            int *num = ATTR("height") ? &p->ban.height : &p->ban.width;
            int *units = ATTR("height")
                ? &p->ban.height_units : &p->ban.width_units;

            /* a value HTML TADS can't read is 0 */
            *num = 0;
            *units = OSHTML_SIZE_PIX;
            if (tag_is(t + v, vlen, "previous"))
                *units = OSHTML_SIZE_PREV;
            else if (vlen != 0 && t[v + vlen - 1] == '%')
            {
                *units = OSHTML_SIZE_PCT;
                *num = to_int(t + v, vlen - 1);
            }
            else
                *num = to_int(t + v, vlen);
        }
        else if (ban && ATTR("align"))
        {
            if (tag_is(t + v, vlen, "top"))
                p->ban.align = OS_BANNER_ALIGN_TOP;
            else if (tag_is(t + v, vlen, "bottom"))
                p->ban.align = OS_BANNER_ALIGN_BOTTOM;
            else if (tag_is(t + v, vlen, "left"))
                p->ban.align = OS_BANNER_ALIGN_LEFT;
            else if (tag_is(t + v, vlen, "right"))
                p->ban.align = OS_BANNER_ALIGN_RIGHT;
        }
        else if (ban && ATTR("border"))
            p->ban.border = 1;
        else if (ban && ATTR("remove"))
            ban_remove |= 1;
        else if (ban && ATTR("removeall"))
            ban_remove |= 2;
        else if (br && ATTR("height"))
        {
            int ht = to_int(t + v, vlen);
            if (ht == 0)
                br = 1;
            else if (ht > 1)
                for ( ; ht > 0 ; --ht)
                    blank_line(p);
        }
        else if (br && ATTR("clear"))
            clear = 1;
        else if (img && ATTR("src"))
        {
            src = v;
            srclen = vlen;
        }
        else if (img && ATTR("alt"))
        {
            alt = v;
            altlen = vlen;
        }
        else if (img && ATTR("align"))
        {
            if (tag_is(t + v, vlen, "left"))
                img_align = imagealign_MarginLeft;
            else if (tag_is(t + v, vlen, "right"))
                img_align = imagealign_MarginRight;
            else if (tag_is(t + v, vlen, "top"))
                img_align = imagealign_InlineDown;
            else if (tag_is(t + v, vlen, "middle"))
                img_align = imagealign_InlineCenter;
            else
                img_align = imagealign_InlineUp;
        }
        else if (img && ATTR("width"))
            parse_img_size(t + v, vlen, &img_w);
        else if (img && ATTR("height"))
            parse_img_size(t + v, vlen, &img_h);
        else if (snd && (ATTR("src") || ATTR("alt")))
        {
            if (ATTR("src"))
            {
                src = v;
                srclen = vlen;
            }
            else
            {
                alt = v;
                altlen = vlen;
            }
        }
        else if (snd && ATTR("layer"))
            sndtag.layer = sound_layer(t + v, vlen);
        else if (snd && ATTR("cancel"))
        {
            sndtag.cancel = 1;
            sndtag.layer = sound_layer(t + v, vlen);
        }
        else if (snd && ATTR("interrupt"))
            sndtag.interrupt = 1;
        else if (snd && ATTR("repeat"))
        {
            sndtag.has_repeat = 1;
            sndtag.repeat = tag_is(t + v, vlen, "loop")
                ? 0 : to_int(t + v, vlen);
        }
        else if (snd && ATTR("random"))
            sndtag.random = to_int(t + v, vlen);
        else if (snd && ATTR("volume"))
            sndtag.volume = to_int(t + v, vlen) > 100
                ? 100 : to_int(t + v, vlen);
        else if (snd && ATTR("fadein"))
            sndtag.fadein = sound_fade(t + v, vlen, &sndtag.crossfade_in);
        else if (snd && ATTR("fadeout"))
            sndtag.fadeout = sound_fade(t + v, vlen, &sndtag.crossfade_out);
        else if (snd && ATTR("sequence"))
            sndtag.sequence = tag_is(t + v, vlen, "replace")
                ? OSSND_SEQ_REPLACE : tag_is(t + v, vlen, "random")
                ? OSSND_SEQ_RANDOM : tag_is(t + v, vlen, "cycle")
                ? OSSND_SEQ_CYCLE : OSSND_SEQ_NONE;
        else if (align_ok && ATTR("align"))
            align = tag_is(t + v, vlen, "center")
                || tag_is(t + v, vlen, "right");
        else if (cell_ok && ATTR("align") && table_open)
            /* a centred table centres its rows; a left or right one floats */
            cell_align = tag_is(t + v, vlen, "center") ? 1 : -1;
        else if (cell_ok && ATTR("align"))
            cell_align = tag_is(t + v, vlen, "center") ? 1
                : tag_is(t + v, vlen, "right") ? 2
                : tag_is(t + v, vlen, "left") ? 0 : -1;
        else if (cell_ok && ATTR("width"))
            cell_width = 1;
        else if (cell_ok && ATTR("bgcolor"))
            cell_bg_ok = parse_color(t + v, vlen, &cell_bg)
                && !os_color_is_param(cell_bg);
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

    /* <A HREF>: the command is the href; an <A NAME> is no link */
    if (link_ok && have_href && !p->dst)
        p->cur.link = (int)href_link(t + href, hreflen, link_flags);

    /* <MAP NAME>, and its <AREA HREF>s */
    if (map_ok && (p->map = find_map(t + mapname, mapnamelen, 1)) != NULL)
        p->map->nareas = p->map->nrects = p->map->unplaced = 0;
    if (area_ok && have_href && !nohref)
    {
        glui32 link = href_link(t + href, hreflen, 0);

        if (altlen != 0)
            add_area(p->map, link, t + alt, altlen);
        else
            add_area(p->map, link, t + href, hreflen);
        if (link)
            add_area_rect(p->map, link, area_rect, t + coords, coordslen);
    }

    /* the banner is whitespace in the text around it */
    if (ban)
    {
        put_html_text(p, ' ');
        if (ban_remove)
            osglk_tag_banner_remove((ban_remove & 2) ? 0 : p->ban.id);
        else
        {
            p->bancap = 1;
            p->banlen = 0;
            p->bantag_open = 0;
            p->banquote = 0;
        }
    }

    if (br == 1)
        line_break(p);
    else if (br == 2)
        blank_line(p);
    if (clear)
        flow_break(p);

    /* the picture, or if it can't be shown, its ALT text */
    if (img && !put_image(p, t + src, srclen, img_align, img_w, img_h,
                          cell_empty && usemaplen == 0))
    {
        size_t k;
        for (k = alt ; k < alt + altlen ; ++k)
            put_html_text(p, t[k]);
    }
    /* the sound, or if it can't be played, its ALT text */
    if (snd && !put_sound(p, &sndtag, t + src, srclen))
    {
        size_t k;
        for (k = alt ; k < alt + altlen ; ++k)
            put_html_text(p, t[k]);
    }
    if (img && usemaplen != 0)
    {
        const imagemap *m = find_map(t + usemap, usemaplen, 0);
        if (m)
            add_map_links(p, m);
    }

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

    /*
     *   <TABLE>, <TR> and <TD>: each row is a line, and a cell goes on it
     *   after a gap, so empty cells before a cell leave the gap as an
     *   indent.  Only the first cell's ALIGN centres the row.  Where a <TAB>
     *   can align right, a right-aligned cell goes at the end of the line,
     *   as on a status line.  Only a cell's own colour is a background: a
     *   theme colour is what the window shows already.
     */
    if (table_open)
        p->table_box = cell_width && cell_bg_ok;
    if (row_open)
        p->row_align = cell_align >= 0;
    if (cell && p->side_left && cell_align < 0)
    {
        /* beside a picture, the text isn't centred as the table is */
        cell_align = 0;
    }
    else if (cell && cell_align < 0 && !p->row_align
             && (p->table_box || (cell_width && cell_bg_ok)))
    {
        /*
         *   nor in a coloured box of a set width: HTML TADS centres the box,
         *   and the text starts at its left.  Centred lines would show the
         *   box ragged.  Without a colour, centring the lines looks as the
         *   centred box does.
         */
        cell_align = 0;
    }
    if (cell_bg_ok)
    {
        p->cur.bg = cell_bg;
        contrast_text(p, cell_bg);
    }
    if (!cell && cell_align >= 0)
    {
        if (cell_align)
            p->cur.attr |= OSSTY_CENTER;
        else
            p->cur.attr &= ~OSSTY_CENTER;
    }
    if (cell && !cell_first && !p->row_shown
        && !(cur_color(p).attr & OSSTY_CENTER))
    {
        /* after empty cells, the gap indents each line of the cell */
        p->npending = 0;
        if (p->line_open)
            put_fixed_tab(p, 2 * CELL_INDENT);
        p->indent += CELL_INDENT;
        p->cell_indent += CELL_INDENT;
    }
    else if (cell && !cell_first && p->line_open)
    {
        /* the gap takes the place of the spaces the cell before ends with */
        p->npending = 0;
        p->cur.attr = (p->cur.attr & ~OSSTY_CENTER) | p->line_center;
        if (cell_align == 2 && !p->line_center && (tads3 || p->in_banner))
            right_cell(p);
        else if (held_one && p->cell_hold == 2)
        {
            /* a first cell of one character, and its gap, are fixed-pitch */
            release_held(p, 1);
            put_fixed_tab(p, CELL_GAP);
        }
        else
            put_tab(p, CELL_GAP, 0);
    }
    else if (cell && !cell_first && cell_align == 2
             && !(p->cur.attr & OSSTY_CENTER) && (tads3 || p->in_banner))
        right_cell(p);
    else if (cell)
    {
        /* a right-aligned first cell is as wide as its column */
        if (cell_align == 1)
            p->cur.attr |= OSSTY_CENTER;
        else if (cell_align == 0)
            p->cur.attr &= ~OSSTY_CENTER;
        if (!cell_first)
            put_tab(p, CELL_GAP, 0);
    }
    if (held_one)
        release_held(p, 0);

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
 *   Picture tables.  A table shown as lines of text makes a jumble of one
 *   whose cells hold only pictures: 1893's compass is nine tiles in a cell,
 *   between cells of buttons, under its title in a table of its own.  Such
 *   tables at the end of a tag banner are laid out in pixels instead, as
 *   HTML TADS lays them out, and go under the banner's text as one picture,
 *   its links clicked by where they are.  So does a picture with an image
 *   map at the end of a banner (Six's introduction): its areas are clicked.
 */

#define PT_CELLS 24
#define PT_LINES 8              /* lines in a cell */
#define PT_SPACE 4              /* pixels for a space between two pictures */
#define PT_TABLES 4             /* tables, one under another */

typedef struct pt_cell
{
    int row, col, colspan;
    int align;                  /* 0 left, 1 centred, 2 right */
    int valign;                 /* 0 top, 1 middle, 2 bottom */
    glui32 minh;                /* its HEIGHT */
    int nlines;
    int first[PT_LINES + 1];    /* the first tile of each line */
    glui32 linew[PT_LINES], lineh[PT_LINES];
    glui32 w, h;
} pt_cell;

/*
 *   The tag at t[*i], a '<': the text between its brackets is t[*s..*e),
 *   and *i moves past it.  False if it has no '>'.
 */
static int raw_tag(const glui32 *t, size_t len, size_t *i,
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
static size_t raw_name(const glui32 *t, size_t s, size_t e,
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
 *   The value of an attribute of the tag in t[s..e), read as do_tag reads
 *   them in a banner: false if the tag hasn't got it
 */
static int raw_attr(const glui32 *t, size_t s, size_t e, const char *attr,
                    size_t *v, size_t *vlen)
{
    size_t name;
    int end;

    s = raw_name(t, s, e, &name, &end);
    s += name;
    while (s < e)
    {
        size_t an, anlen;
        glui32 c = t[s];

        if (!is_alnum(c))
        {
            ++s;
            continue;
        }
        for (an = s ; s < e && is_alnum(t[s]) ; ++s)
            ;
        anlen = s - an;
        while (s < e && is_space(t[s]))
            ++s;
        *v = s;
        *vlen = 0;
        if (s < e && t[s] == '=')
        {
            for (++s ; s < e && is_space(t[s]) ; ++s)
                ;
            if (s < e && (t[s] == '"' || t[s] == '\''))
            {
                glui32 qu = t[s++];
                for (*v = s ; s < e && t[s] != qu ; ++s)
                    ;
                *vlen = s - *v;
                if (s < e)
                    ++s;
            }
            else
            {
                for (*v = s ; s < e && !is_space(t[s]) ; ++s)
                    ;
                *vlen = s - *v;
            }
        }
        if (tag_is(t + an, anlen, attr))
            return 1;
    }
    return 0;
}

static int raw_int(const glui32 *t, size_t s, size_t e, const char *attr,
                   int dflt)
{
    size_t v, vlen;
    return raw_attr(t, s, e, attr, &v, &vlen) ? to_int(t + v, vlen) : dflt;
}

/*
 *   Is the last thing in t[0..len) a table, or tables with only line breaks
 *   between them, with only line breaks and the ends of elements after it?
 *   Their text is t[*s..*e).  1 if so, 2 if it's an <IMG USEMAP> instead.
 */
static int find_pictable(const glui32 *t, size_t len, size_t *s, size_t *e)
{
    size_t v, vlen;
    size_t i = 0, ts, te, name, namelen;
    int depth = 0, end, found = 0;

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
            found = 2;
        }
        else if (depth == 0 && !end && !tag_is(t + name, namelen, "br"))
            found = 0;
    }
    return depth == 0 ? found : 0;
}

/*
 *   Lay out the table in t[0..len) in pixels, adding its tiles to the
 *   picture's, and count its links, whether a row has more than one cell,
 *   and its size: false if it has anything but pictures, links and line
 *   breaks in its cells
 */
static int pt_table(parser *p, const glui32 *t, size_t len,
                    oshtml_pictable *pt, int *nlinks, int *wide,
                    glui32 *width, glui32 *height)
{
    pt_cell cells[PT_CELLS], *c = NULL;
    glui32 hs[OSHTML_TILES_MAX], vs[OSHTML_TILES_MAX];
    glui32 colw[PT_CELLS], rowh[PT_CELLS], colx[PT_CELLS + 1];
    glui32 rowy[PT_CELLS + 1];
    int ncells = 0, row = -1, col = 0, ncols = 0, nrows, links = 0;
    int space = 0, row_valign = 1, depth = 0, k, n;
    glui32 link = 0, pad = 1, spacing = 2, border = 0;
    size_t i = 0;

    while (i < len)
    {
        size_t ts, te, name, namelen, v, vlen;
        int end;

        if (t[i] != '<')
        {
            if (!is_space(t[i]))
                return 0;
            space = c != NULL;
            ++i;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            return 0;
        namelen = raw_name(t, ts, te, &name, &end);

#define RAW(s) tag_is(t + name, namelen, s)
#define RAWV(s) tag_is(t + v, vlen, s)
        if (RAW("table"))
        {
            if (end)
                --depth;
            else if (depth++ != 0)
                return 0;
            else
            {
                pad = (glui32)raw_int(t, ts, te, "cellpadding", 1);
                spacing = (glui32)raw_int(t, ts, te, "cellspacing", 2);
                border = (glui32)raw_int(t, ts, te, "border", 0);
            }
        }
        else if (RAW("tr"))
        {
            c = NULL;
            if (!end)
            {
                ++row;
                col = 0;
                row_valign = 1;
                if (raw_attr(t, ts, te, "valign", &v, &vlen))
                    row_valign = RAWV("top") ? 0 : RAWV("bottom") ? 2 : 1;
            }
        }
        else if (RAW("td"))
        {
            c = NULL;
            if (end)
                continue;
            if (row < 0 || ncells == PT_CELLS)
                return 0;
            c = &cells[ncells++];
            memset(c, 0, sizeof(*c));
            c->row = row;
            c->col = col;
            c->colspan = raw_int(t, ts, te, "colspan", 1);
            if (c->colspan < 1)
                c->colspan = 1;
            col += c->colspan;
            if (col > PT_CELLS)
                return 0;
            if (col > ncols)
                ncols = col;
            if (raw_attr(t, ts, te, "align", &v, &vlen))
                c->align = RAWV("center") ? 1 : RAWV("right") ? 2 : 0;
            c->valign = row_valign;
            if (raw_attr(t, ts, te, "valign", &v, &vlen))
                c->valign = RAWV("top") ? 0 : RAWV("bottom") ? 2 : 1;
            c->minh = (glui32)raw_int(t, ts, te, "height", 0);
            c->nlines = 1;
            c->first[0] = pt->ntiles;
            space = 0;
        }
        else if (RAW("a"))
        {
            link = 0;
            if (!end && raw_attr(t, ts, te, "href", &v, &vlen))
            {
                size_t fv, fvlen;
                int flags = 0;
                if (raw_attr(t, ts, te, "noenter", &fv, &fvlen))
                    flags |= OSLINK_NOENTER;
                if (raw_attr(t, ts, te, "append", &fv, &fvlen))
                    flags |= OSLINK_APPEND;
                link = href_link(t + v, vlen, flags);
            }
        }
        else if (RAW("br"))
        {
            if (c)
            {
                if (c->nlines == PT_LINES)
                    return 0;
                c->first[c->nlines++] = pt->ntiles;
            }
            space = 0;
        }
        else if (RAW("img") && !end)
        {
            oshtml_tile *tile = &pt->tiles[pt->ntiles];
            img_size w = { 0, 0 }, h = { 0, 0 };
            size_t sv, svlen;
            glui32 *lw, *lh;

            /* not a picture that floats beside the text, or a map */
            if (!c || pt->ntiles == OSHTML_TILES_MAX
                || (raw_attr(t, ts, te, "align", &sv, &svlen)
                    && (tag_is(t + sv, svlen, "left")
                        || tag_is(t + sv, svlen, "right")))
                || raw_attr(t, ts, te, "usemap", &sv, &svlen)
                || raw_attr(t, ts, te, "ismap", &sv, &svlen)
                || !raw_attr(t, ts, te, "src", &v, &vlen))
                return 0;
            if (raw_attr(t, ts, te, "width", &sv, &svlen))
                parse_img_size(t + sv, svlen, &w);
            if (raw_attr(t, ts, te, "height", &sv, &svlen))
                parse_img_size(t + sv, svlen, &h);
            if (w.pct || h.pct
                || !find_image(p, t + v, vlen, imagealign_InlineUp, w, h,
                               &tile->img)
                || !oshtml_image_size(&tile->img, 0, &tile->w, &tile->h)
                || tile->w == 0 || tile->h == 0)
                return 0;
            hs[pt->ntiles] = (glui32)raw_int(t, ts, te, "hspace", 0);
            vs[pt->ntiles] = (glui32)raw_int(t, ts, te, "vspace", 0);

            lw = &c->linew[c->nlines - 1];
            lh = &c->lineh[c->nlines - 1];
            tile->x = *lw + (space && *lw ? PT_SPACE : 0) + hs[pt->ntiles];
            *lw = tile->x + tile->w + hs[pt->ntiles];
            if (tile->h + 2 * vs[pt->ntiles] > *lh)
                *lh = tile->h + 2 * vs[pt->ntiles];
            tile->link = link;
            tile->area = 0;
            if (link)
                ++links;
            ++pt->ntiles;
            space = 0;
        }
        else
            return 0;
#undef RAWV
#undef RAW
    }

    nrows = row + 1;
    if (nrows > PT_CELLS)
        return 0;

    /* the cells' contents, and the columns' and rows' sizes around them */
    memset(colw, 0, sizeof(colw));
    memset(rowh, 0, sizeof(rowh));
    for (n = 0 ; n < ncells ; ++n)
    {
        c = &cells[n];
        c->first[c->nlines] = n + 1 < ncells ? cells[n + 1].first[0]
            : pt->ntiles;
        for (k = 0 ; k < c->nlines ; ++k)
        {
            if (c->linew[k] > c->w)
                c->w = c->linew[k];
            c->h += c->lineh[k];
        }
        if (c->colspan == 1 && c->w + 2 * pad > colw[c->col])
            colw[c->col] = c->w + 2 * pad;
        if (c->h + 2 * pad > rowh[c->row])
            rowh[c->row] = c->h + 2 * pad;
        if (c->minh > rowh[c->row])
            rowh[c->row] = c->minh;
    }
    for (n = 0 ; n < ncells ; ++n)
    {
        glui32 have = 0, need;

        c = &cells[n];
        if (c->colspan == 1)
            continue;
        for (k = c->col ; k < c->col + c->colspan ; ++k)
            have += colw[k];
        have += spacing * (glui32)(c->colspan - 1);
        need = c->w + 2 * pad;
        if (need > have)
            for (k = c->col ; k < c->col + c->colspan ; ++k)
                colw[k] += (need - have + (glui32)c->colspan - 1)
                    / (glui32)c->colspan;
    }
    colx[0] = border + spacing;
    for (k = 0 ; k < ncols ; ++k)
        colx[k + 1] = colx[k] + colw[k] + spacing;
    rowy[0] = border + spacing;
    for (k = 0 ; k < nrows ; ++k)
        rowy[k + 1] = rowy[k] + rowh[k] + spacing;
    *width = colx[ncols] + border;
    *height = rowy[nrows] + border;

    /* each line in its cell, each tile at the bottom of its line */
    for (n = 0 ; n < ncells ; ++n)
    {
        glui32 cw, ch, y;

        c = &cells[n];
        k = c->col + c->colspan;
        cw = colx[k] - spacing - colx[c->col];
        ch = rowh[c->row];
        y = rowy[c->row] + (c->valign == 0 ? pad
                            : c->valign == 2 ? ch - pad - c->h
                            : (ch - c->h) / 2);
        for (k = 0 ; k < c->nlines ; ++k)
        {
            glui32 lw = c->linew[k], x;
            int j;

            x = colx[c->col] + (c->align == 0 ? pad
                                : c->align == 2 ? cw - pad - lw
                                : (cw - lw) / 2);
            for (j = c->first[k] ; j < c->first[k + 1] ; ++j)
            {
                pt->tiles[j].x += x;
                pt->tiles[j].y = y + c->lineh[k] - pt->tiles[j].h - vs[j];
            }
            y += c->lineh[k];
        }
    }
    *nlinks += links;
    *wide |= ncols > 1;
    return 1;
}

/*
 *   Lay out the tables in t[0..len) in pixels, one under another, as one
 *   picture: 1893's title over its compass.  False if one can't be, or
 *   none of it is a link, or none is a grid.
 */
static int build_pictable(parser *p, const glui32 *t, size_t len,
                          oshtml_pictable *pt)
{
    int first[PT_TABLES + 1];
    glui32 tw[PT_TABLES], th[PT_TABLES], y = 0;
    int ntables = 0, links = 0, wide = 0, k, j;
    size_t i = 0;

    if (p->dst)
        return 0;
    pt->ntiles = 0;
    pt->width = 0;
    pt->page = 0;
    while (i < len)
    {
        size_t start = i, ts, te, name, namelen;
        int end, depth;

        /* only spaces and line breaks between them */
        if (t[i] != '<')
        {
            ++i;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            return 0;
        namelen = raw_name(t, ts, te, &name, &end);
        if (end || !tag_is(t + name, namelen, "table"))
            continue;
        for (depth = 1 ; depth && i < len ; )
        {
            if (t[i] != '<')
                ++i;
            else if (!raw_tag(t, len, &i, &ts, &te))
                return 0;
            else if (namelen = raw_name(t, ts, te, &name, &end),
                     tag_is(t + name, namelen, "table"))
                depth += end ? -1 : 1;
        }
        if (depth || ntables == PT_TABLES)
            return 0;
        first[ntables] = pt->ntiles;
        if (!pt_table(p, t + start, i - start, pt, &links, &wide,
                      &tw[ntables], &th[ntables]))
            return 0;
        if (tw[ntables] > pt->width)
            pt->width = tw[ntables];
        ++ntables;
    }
    first[ntables] = pt->ntiles;
    if (pt->ntiles < 2 || links == 0 || !wide)
        return 0;

    /* each centred in the widest, if they're centred */
    pt->center = (cur_color(p).attr & OSSTY_CENTER) != 0;
    for (k = 0 ; k < ntables ; ++k)
    {
        glui32 x = pt->center ? (pt->width - tw[k]) / 2 : 0;

        for (j = first[k] ; j < first[k + 1] ; ++j)
        {
            pt->tiles[j].x += x;
            pt->tiles[j].y += y;
        }
        y += th[k];
    }
    pt->height = y;
    return 1;
}

/*
 *   Lay out the <IMG USEMAP> in t[0..len) as a picture whose areas are its
 *   links: false if an area isn't a rectangle, or the picture floats
 */
static int build_mappic(parser *p, const glui32 *t, size_t len,
                        oshtml_pictable *pt)
{
    size_t i = 0, ts, te, v, vlen;
    img_size w = { 0, 0 }, h = { 0, 0 };
    const imagemap *m;
    oshtml_tile *tile = &pt->tiles[0];
    int k;

    if (p->dst || !raw_tag(t, len, &i, &ts, &te)
        || !raw_attr(t, ts, te, "usemap", &v, &vlen)
        || !(m = find_map(t + v, vlen, 0))
        || m->unplaced || m->nrects == 0 || m->nrects >= OSHTML_TILES_MAX
        || raw_attr(t, ts, te, "ismap", &v, &vlen)
        || (raw_attr(t, ts, te, "align", &v, &vlen)
            && (tag_is(t + v, vlen, "left") || tag_is(t + v, vlen, "right"))))
        return 0;
    if (raw_attr(t, ts, te, "width", &v, &vlen))
        parse_img_size(t + v, vlen, &w);
    if (raw_attr(t, ts, te, "height", &v, &vlen))
        parse_img_size(t + v, vlen, &h);
    memset(tile, 0, sizeof(*tile));
    if (w.pct || h.pct || !raw_attr(t, ts, te, "src", &v, &vlen)
        || !find_image(p, t + v, vlen, imagealign_InlineUp, w, h, &tile->img)
        || !oshtml_image_size(&tile->img, 0, &tile->w, &tile->h)
        || tile->w == 0 || tile->h == 0)
        return 0;

    /* the areas, as far as they're on it; the first one clicked wins */
    pt->ntiles = 1;
    for (k = 0 ; k < m->nrects ; ++k)
    {
        oshtml_tile *a = &pt->tiles[pt->ntiles];

        *a = m->rects[k];
        if (a->x >= tile->w || a->y >= tile->h)
            continue;
        if (a->w > tile->w - a->x)
            a->w = tile->w - a->x;
        if (a->h > tile->h - a->y)
            a->h = tile->h - a->y;
        ++pt->ntiles;
    }
    pt->width = tile->w;
    pt->height = tile->h;
    pt->center = (cur_color(p).attr & OSSTY_CENTER) != 0;
    pt->page = 0;
    return pt->ntiles > 1;
}

/* the tags a page of pictures can have, and those that lay nothing out */
static const char *const page_tags[] = {
    "body", "font", "map", "area", "center", "p", "br", "img", "bq",
    "blockquote", NULL
};

static int page_quiet_tag(const glui32 *t, size_t len)
{
    return tag_is(t, len, "body") || tag_is(t, len, "font")
        || tag_is(t, len, "map") || tag_is(t, len, "area");
}

/*
 *   Is t[0..len) only centred pictures and line breaks, on a <BODY
 *   BACKGROUND>?  Its picture is t[*bg..*bg+*bglen).  Six's introduction.
 */
static int find_picpage(const glui32 *t, size_t len, size_t *bg,
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

/*
 *   Lay out the page t[0..len) in pixels as the banner's text would flow:
 *   each line of pictures centred, under the blank lines before it.  False
 *   if a picture isn't centred, floats or can't be shown.
 */
static int build_picpage(parser *p, const glui32 *t, size_t len,
                         size_t bg, size_t bglen, oshtml_pictable *pt)
{
    glui32 linew[OSHTML_TILES_MAX], liney[OSHTML_TILES_MAX];
    glui32 lineh[OSHTML_TILES_MAX], vs[OSHTML_TILES_MAX];
    int line[OSHTML_TILES_MAX], own[OSHTML_TILES_MAX];
    glui32 blank = osglk_banner_line_pixels(), y = 0, lw = 0, lh = 0;
    img_size none = { 0, 0 };
    int nlines = 0, center = 0, j;
    size_t i = 0;

    if (p->dst || !find_image(p, t + bg, bglen, imagealign_InlineUp,
                              none, none, &pt->bg))
        return 0;
    pt->ntiles = 0;
    pt->width = 0;

#define END_LINE() \
    do { \
        if (lw) { \
            linew[nlines] = lw; \
            liney[nlines] = y; \
            lineh[nlines++] = lh; \
            y += lh; \
            lw = lh = 0; \
        } \
    } while (0)

    while (i < len)
    {
        size_t ts, te, name, namelen, v, vlen;
        int end;

        if (t[i] != '<')
        {
            ++i;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            return 0;
        namelen = raw_name(t, ts, te, &name, &end);

#define RAW(s) tag_is(t + name, namelen, s)
        if (page_quiet_tag(t + name, namelen))
            ;
        else if (RAW("br"))
        {
            int had = raw_attr(t, ts, te, "height", &v, &vlen);
            int ht = had ? to_int(t + v, vlen) : 1;

            if (lw)
            {
                END_LINE();
                --ht;
            }
            if (ht > 0)
                y += (glui32)ht * blank;
        }
        else if (RAW("p") || RAW("bq") || RAW("blockquote"))
        {
            END_LINE();
            if ((!end || !RAW("p")) && y > 0)
                y += blank;
        }
        else if (RAW("center"))
        {
            END_LINE();
            center += end ? -1 : 1;
        }
        else if (RAW("img") && !end)
        {
            oshtml_tile *tile = &pt->tiles[pt->ntiles];
            img_size w = { 0, 0 }, h = { 0, 0 };
            const imagemap *m = NULL;
            int pic = pt->ntiles, k;
            glui32 hs;

            if (center <= 0 || pt->ntiles == OSHTML_TILES_MAX
                || nlines == OSHTML_TILES_MAX
                || (raw_attr(t, ts, te, "align", &v, &vlen)
                    && (tag_is(t + v, vlen, "left")
                        || tag_is(t + v, vlen, "right")))
                || raw_attr(t, ts, te, "ismap", &v, &vlen))
                return 0;
            if (raw_attr(t, ts, te, "usemap", &v, &vlen)
                && (!(m = find_map(t + v, vlen, 0)) || m->unplaced))
                return 0;
            if (raw_attr(t, ts, te, "width", &v, &vlen))
                parse_img_size(t + v, vlen, &w);
            if (raw_attr(t, ts, te, "height", &v, &vlen))
                parse_img_size(t + v, vlen, &h);
            memset(tile, 0, sizeof(*tile));
            if (w.pct || h.pct || !raw_attr(t, ts, te, "src", &v, &vlen)
                || !find_image(p, t + v, vlen, imagealign_InlineUp, w, h,
                               &tile->img)
                || !oshtml_image_size(&tile->img, 0, &tile->w, &tile->h)
                || tile->w == 0 || tile->h == 0)
                return 0;
            hs = (glui32)raw_int(t, ts, te, "hspace", 0);
            vs[pic] = (glui32)raw_int(t, ts, te, "vspace", 0);
            tile->x = lw + hs;
            lw = tile->x + tile->w + hs;
            if (tile->h + 2 * vs[pic] > lh)
                lh = tile->h + 2 * vs[pic];
            line[pic] = nlines;
            own[pic] = -1;
            ++pt->ntiles;

            /* its areas, on it, placed with it below */
            for (k = 0 ; m && k < m->nrects ; ++k)
            {
                oshtml_tile *a = &pt->tiles[pt->ntiles];

                if (pt->ntiles == OSHTML_TILES_MAX)
                    return 0;
                *a = m->rects[k];
                if (a->x >= tile->w || a->y >= tile->h)
                    continue;
                if (a->w > tile->w - a->x)
                    a->w = tile->w - a->x;
                if (a->h > tile->h - a->y)
                    a->h = tile->h - a->y;
                own[pt->ntiles++] = pic;
            }
        }
#undef RAW
    }
    END_LINE();
#undef END_LINE

    for (j = 0 ; j < nlines ; ++j)
        if (linew[j] > pt->width)
            pt->width = linew[j];
    for (j = 0 ; j < pt->ntiles ; ++j)
    {
        oshtml_tile *tile = &pt->tiles[j];

        if (own[j] >= 0)
        {
            tile->x += pt->tiles[own[j]].x;
            tile->y += pt->tiles[own[j]].y;
            continue;
        }
        tile->x += (pt->width - linew[line[j]]) / 2;
        tile->y = liney[line[j]] + lineh[line[j]] - tile->h - vs[j];
    }
    pt->height = y;
    pt->center = 1;
    pt->page = 1;
    return pt->ntiles > 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   TADS 2's <BANNER>: its text, up to the </BANNER>, goes through a parser
 *   of its own into the banner's window, replacing what was there; as in
 *   HTML TADS, a banner with nothing but whitespace in it is left alone
 */

/*
 *   Give the banner's parser the tags of a page of pictures that lay
 *   nothing out, and lay the page out: false if it can't be
 */
static int banner_picpage(parser *b, void *ctx, const glui32 *t, size_t len,
                          size_t bg, size_t bglen, oshtml_pictable *pt)
{
    size_t i = 0, ts, te, name, namelen, k;
    int end;

    while (i < len)
    {
        size_t start = i;

        if (t[i] != '<')
        {
            ++i;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            return 0;
        namelen = raw_name(t, ts, te, &name, &end);
        if (page_quiet_tag(t + name, namelen))
            for (k = start ; k < i ; ++k)
                feed(b, t[k]);
    }
    flush_out(b);
    return build_picpage(b, t, len, bg, bglen, pt)
        && osglk_tag_banner_table(ctx, pt);
}

static void end_banner(parser *p)
{
    static oshtml_pictable pt;
    size_t i = 0, k, ts = 0, te = 0, bg = 0, bglen = 0;
    int table, page;

    p->bancap = 0;
    while (i < p->banlen && is_space(p->bantext[i]))
        ++i;
    page = find_picpage(p->bantext + i, p->banlen - i, &bg, &bglen);
    table = find_pictable(p->bantext + i, p->banlen - i, &ts, &te);
    ts += i;
    te += i;
    if (i < p->banlen)
    {
        const oshtml_sink *sink;
        void *ctx = osglk_tag_banner_open(&p->ban, &sink);

        if (ctx)
        {
            do
            {
                parser *b = oshtml_new(sink, ctx);
                size_t from = i, stop = p->banlen;

                if (!b)
                    break;
                b->in_banner = 1;
                if (page && banner_picpage(b, ctx, p->bantext + i,
                                           p->banlen - i, bg, bglen, &pt))
                    from = stop;
                else if (page)
                {
                    /* the text after all, from the start */
                    oshtml_delete(b);
                    if (!(b = oshtml_new(sink, ctx)))
                        break;
                    b->in_banner = 1;
                }
                if (from == stop)
                    ;
                else if (table)
                {
                    /* the text, then the table: the rest is only ends */
                    for (k = i ; k < ts ; ++k)
                        feed(b, p->bantext[k]);
                    put_maprow(b);
                    expand_tab(b, 1);
                    flush_out(b);
                    if ((table == 2
                         ? build_mappic(b, p->bantext + ts, te - ts, &pt)
                         : build_pictable(b, p->bantext + ts, te - ts, &pt))
                        && osglk_tag_banner_table(ctx, &pt))
                        stop = ts;
                    from = ts;
                }
                for (k = from ; k < stop ; ++k)
                    feed(b, p->bantext[k]);
                put_maprow(b);
                expand_tab(b, 1);
                flush_out(b);
                oshtml_delete(b);
            } while (osglk_tag_banner_done(ctx));
        }
    }
    p->banlen = 0;
    put_html_text(p, ' ');
}

/* is the tag in p->tag "</banner>"? */
static int is_banner_end(parser *p)
{
    const glui32 *t = p->tag;
    size_t len = p->taglen, i = 0, name;

    while (i < len && is_space(t[i]))
        ++i;
    if (i == len || (t[i] != '/' && t[i] != '\\'))
        return 0;
    for (++i ; i < len && is_space(t[i]) ; ++i)
        ;
    for (name = i ; i < len && !is_space(t[i]) ; ++i)
        ;
    return tag_is(t + name, i - name, "banner");
}

static void banner_char(parser *p, glui32 c)
{
    /* the tags in the text, to find the </BANNER> */
    if (!p->bantag_open)
    {
        if (c == '<')
        {
            p->bantag_open = 1;
            p->bantag = p->banlen;
            p->banquote = 0;
            p->taglen = 0;
        }
    }
    else if (p->banquote)
    {
        if (c == p->banquote)
            p->banquote = 0;
    }
    else if (c == '"' || c == '\'')
        p->banquote = c;
    else if (c == '>')
    {
        p->bantag_open = 0;
        if (is_banner_end(p))
        {
            p->banlen = p->bantag;
            end_banner(p);
            return;
        }
    }
    else if (p->taglen < TAG_MAX)
        p->tag[p->taglen++] = c;

    if (p->banlen == p->banmax && p->banmax < BANNER_MAX)
    {
        size_t n = p->banmax ? p->banmax * 2 : 256;
        glui32 *nb = realloc(p->bantext, n * sizeof(glui32));
        if (nb)
        {
            p->bantext = nb;
            p->banmax = n;
        }
    }
    if (p->banlen < p->banmax)
        p->bantext[p->banlen++] = c;
}

/* ------------------------------------------------------------------------ */
/*
 *   The tokenizer
 */

static void feed(parser *p, glui32 c)
{
    if (p->bancap)
    {
        banner_char(p, c);
        return;
    }

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

int oshtml_is_tads3(void)
{
    return tads3;
}

int oshtml_is_html(void)
{
    return fmt.html;
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
    free(p->bantext);
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
    p->nmaprow = 0;
    p->cell_pic = p->cell_empty = p->side_left = 0;
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
    p->cell_hold = 0;
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
    release_cell_picture(&fmt);
    show_held(&fmt);
    put_maprow(&fmt);
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

/* the OS layer's own text isn't part of a link */
static void drop_link(parser *p)
{
    if (p->sent.link && p->sink->set_link)
    {
        flush_out(p);
        p->sink->set_link(p->ctx, 0);
        p->sent.link = 0;
    }
}

void oshtml_put_plain(const char *str, size_t len)
{
    show_held(&fmt);
    oshtml_flush();
    drop_link(&fmt);

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
    release_cell_picture(&fmt);
    show_held(&fmt);
    put_maprow(&fmt);
    print_pending(&fmt);
    fmt.buffered = 0;
    flush_out(&fmt);
    drop_link(&fmt);
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

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
 *     the tags left open inside it; an end tag with no start tag open
 *     closes the innermost one instead, as HTML TADS's end_normal_tag
 *     does (Drool's "<b>pamphlets</A>"); clearing the window closes them
 *     all, as HTML TADS's clear_page does;
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
#include "oshtml_scan.h"
#include "oshtml_priv.h"
#include "osglk.h"
#include "osstyles.h"
#include "osres.h"
#include "ossound.h"
#include "gi_blorb.h"

static int tads3 = 0;

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
    osglk_set_text_color(osglk_body_color(fg), bg);
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

static int main_centers(void *ctx, int combo)
{
    (void)ctx;
    return osglk_centers(mainwin, combo);
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

void flush_out(parser *p)
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

color cur_color(parser *p)
{
    color c = p->cur;

    c.attr |= p->os_attr;
    return c;
}

static void line_break(parser *p);
static void release_cell_picture(parser *p);
static void end_row_capture(parser *p, int side);

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
void expand_tab(parser *p, int allow_anon)
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

    lead = p->tab_lead < p->npending ? p->tab_lead : p->npending;
    n = p->nhold;

    /*
     *   text too wide to fit after the text before the tab still gets a
     *   space between them, as on a status line in a narrow window
     */
    if ((p->tab_align == TAB_RIGHT || p->tab_align == TAB_CENTER)
        && spaces < 1 && n > 0 && lead == 0 && p->tab_start > 0)
        spaces = 1;

    p->tab_align = TAB_NONE;
    p->center_hold = 0;
    p->tab_lead = 0;
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
    if (p->tab_centered)
    {
        p->tab_centered = 0;
        p->cur.attr &= ~OSSTY_CENTER;
    }
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
void put_html_text(parser *p, glui32 c)
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
        /*
         *   a non-breaking space, the formatter's quoted space: a
         *   character in its own right, so that a line of nothing but
         *   "&nbsp;" and a break is a blank line, as it is in HTML TADS
         *   (the credits of Tomorrow Never Comes are padded with them)
         */
        put_visible(p, ' ');
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
    e->in_box = p->in_box;
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
    p->in_box = e->in_box;
    p->nelems = i;
}

/*
 *   An end tag closes its start tag, and the tags left open inside it.
 *   Without one open, it closes the innermost open tag: HTML TADS's
 *   end_normal_tag goes on "as though it had matched" and pops the current
 *   container, so Drool's "<b>pamphlets</A>" is bold only to the </A>.
 */
static int pop_elem(parser *p, const glui32 *name, size_t len)
{
    int i;

    for (i = p->nelems ; i > 0 ; --i)
        if (tag_is(name, len, p->elems[i - 1].name))
        {
            restore_elem(p, i - 1);
            return 1;
        }
    if (p->nelems > 0)
        restore_elem(p, p->nelems - 1);
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

/*
 *   A <BODY BACKGROUND> picture is not drawn behind the text, and the colour
 *   the game gave to show if the picture is missing might hide its text:
 *   Macrocosm's starfield has TEXT=WHITE on BGCOLOR=WHITE.  So text that
 *   doesn't show on the colour gets the opposite one, black for light text,
 *   as the starfield is dark.
 */
static os_color_t pic_page_color(os_color_t fg, os_color_t bg)
{
    unsigned long lf, lb;

    if (os_color_is_param(fg) || os_color_is_param(bg))
        return bg;
    lf = 299 * ((fg >> 16) & 0xFF) + 587 * ((fg >> 8) & 0xFF)
        + 114 * (fg & 0xFF);
    lb = 299 * ((bg >> 16) & 0xFF) + 587 * ((bg >> 8) & 0xFF)
        + 114 * (bg & 0xFF);
    if ((lf > lb ? lf - lb : lb - lf) >= 64000)
        return bg;
    return lf >= 128000 ? os_rgb_color(0, 0, 0) : os_rgb_color(255, 255, 255);
}

/* <BODY>: the window background */
static void set_body(parser *p, os_color_t bg)
{
    contrast_text(p, bg);
    p->text = p->cur.fg;
    if (p == &fmt)
    {
        flush_out(p);
        osglk_set_body_text(p->text);
    }
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

void parse_img_size(const glui32 *v, size_t len, img_size *out)
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
int find_image(parser *p, const glui32 *src, size_t srclen,
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
    else if (p->col > 0 && !p->dst)
    {
        /*
         *   A margin picture goes at the start of a line, and the window
         *   puts it where the text has got to.  After text in the line (the
         *   title of Tomorrow Never Comes) the first line ran under it, so
         *   it starts the next one
         */
        line_break(p);
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
 *   (or a big one, as Threnody's cat is) in a row's first cell, or one in
 *   its last cell after text.  It waits for what follows: a tag that ends
 *   the cell goes on waiting, and the next cell, row or the end of the
 *   table decides (cell_picture_tag).
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

    if (p->ncell == 1 && (ih >= 2 * iw || (iw >= 64 && ih >= 64)))
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

static void row_capture_add(parser *p, glui32 c)
{
    if (p->nrowcap == p->rowcapmax)
    {
        size_t max = p->rowcapmax ? p->rowcapmax * 2 : 1024;
        glui32 *buf = realloc(p->rowcap, max * sizeof(glui32));
        if (!buf)
        {
            end_row_capture(p, 0);
            return;
        }
        p->rowcap = buf;
        p->rowcapmax = max;
    }
    p->rowcap[p->nrowcap++] = c;
}

/* the row was read, or must be shown now: put out what waited of it */
static void end_row_capture(parser *p, int side)
{
    glui32 *buf = p->rowcap;
    size_t i, n = p->nrowcap;

    if (!p->capturing)
        return;
    p->capturing = 0;
    p->row_side = side == 1 ? 1 : 2;
    p->rowcap = 0;
    p->nrowcap = p->rowcapmax = 0;
    for (i = 0 ; i < n ; ++i)
        feed(p, buf[i]);
    free(buf);
}

/* the game's text while the row waits */
static int row_capture_char(parser *p, glui32 c)
{
    int side;

    if (!p->capturing || p->dst)
        return 0;
    row_capture_add(p, c);
    if (c == '>' && p->capturing
        && (side = cell_row_scan(p->rowcap, p->nrowcap)) != 0)
        end_row_capture(p, side);
    return 1;
}

/*
 *   a tag after the held picture: is it beside the page?  True if the tag
 *   waits for the rest of the row (t, len its text)
 */
static int cell_picture_tag(parser *p, const glui32 *name, size_t len,
                            int end, const glui32 *t, size_t tlen)
{
    int cell = tag_is(name, len, "td") || tag_is(name, len, "th");
    int right, side;
    size_t i;

    if (cell && end)
        return 0;
    if (cell)
        right = 0;
    else if (tag_is(name, len, "tr") || (end && tag_is(name, len, "table")))
        right = 1;
    else
    {
        release_cell_picture(p);
        return 0;
    }

    if (p->cell_pic != (right ? 2 : 1))
    {
        release_cell_picture(p);
        return 0;
    }

    /* a first cell's picture: read the row, from this tag on */
    side = right;
    if (!right && !p->row_side)
    {
        p->capturing = 1;
        row_capture_add(p, '<');
        for (i = 0 ; i < tlen && p->capturing ; ++i)
            row_capture_add(p, t[i]);
        if (p->capturing)
            row_capture_add(p, '>');
        return p->capturing;
    }
    if (!right)
        side = p->row_side == 1;
    p->row_side = 0;

    if (!side || !osglk_side_picture(right, &p->cell_img))
    {
        /*
         *   in a window too narrow for that, or in a row with no picture
         *   at its other end, a first cell's picture goes in the margin, so
         *   that the next cell's text runs beside it, and the next row
         *   starts below it (According to Cain's vignette)
         */
        if (right)
        {
            release_cell_picture(p);
            return 0;
        }
        p->cell_pic = 0;
        p->cell_img.align = imagealign_MarginLeft;
        show_image(p, &p->cell_img);
        p->margin_pic = 1;
    }

    /* the next cell is the row's first, as far as the text goes */
    p->cell_pic = 0;
    if (!right)
    {
        p->side_left = 1;
        --p->ncell;
    }
    return 0;
}

/* the row with a picture in the margin is over: what follows goes below */
static void end_margin_pic(parser *p)
{
    if (!p->margin_pic)
        return;
    p->margin_pic = 0;
    flow_break(p);
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
glui32 href_link(const glui32 *v, size_t len, int flags)
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
 *   it has already, or a link named the same but for case and spaces
 *   (School's "transport to Metalwork room" and "... Metalwork Room").
 *
 *   A long row is made easier to read.  The words every link starts with
 *   are put once in front of it ("Transport to: Room 1 · Room 2"), and a
 *   row from a single map with MAPROW_SORT links or more is sorted, so
 *   School's 45 rooms come in order rather than in the order the areas
 *   were drawn.  A shorter row, or one made of several maps' tiles, keeps
 *   the author's order, which for a menu (HELP10) or a compass (Golden
 *   Skull) means something.
 */
#define MAPROW_SORT 10

static imagemap maps[MAPS_MAX];
static int nmaps, map_next;

/* the map with this name ("#name" or "name"), made if 'create' */
imagemap *find_map(const glui32 *v, size_t len, int create)
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

static glui32 fold(glui32 c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

/* whether two labels are the same but for case and spaces */
static int same_label(const maplink *a, const maplink *b)
{
    size_t i = 0, j = 0;

    for (;;)
    {
        while (i < a->len && is_space(a->label[i]))
            ++i;
        while (j < b->len && is_space(b->label[j]))
            ++j;
        if (i == a->len || j == b->len)
            return i == a->len && j == b->len;
        if (fold(a->label[i++]) != fold(b->label[j++]))
            return 0;
    }
}

/* the links of an <IMG USEMAP>, for the row under the pictures */
static void add_map_links(parser *p, const imagemap *m)
{
    int i, j;

    if (p->dst)
        return;
    ++p->nmaprow_maps;
    for (i = 0 ; i < m->nareas && p->nmaprow < AREAS_MAX ; ++i)
    {
        for (j = 0 ; j < p->nmaprow ; ++j)
            if (p->maprow[j].link == m->areas[i].link
                || same_label(&p->maprow[j], &m->areas[i]))
                break;
        if (j == p->nmaprow)
            p->maprow[p->nmaprow++] = m->areas[i];
    }
}

/*
 *   How much of each label is the words they all start with, ending in a
 *   space, so that every link keeps a word of its own; 0 if they don't
 *   share a word, or there are too few links to be worth it.
 */
static size_t maprow_prefix(const maplink *row, int n)
{
    size_t len = row[0].len, k, word = 0;
    int i;

    if (n < 3)
        return 0;
    for (i = 1 ; i < n ; ++i)
    {
        for (k = 0 ; k < len && k < row[i].len
                     && fold(row[i].label[k]) == fold(row[0].label[k]) ; ++k)
            ;
        len = k;
    }

    /* back to the end of a word: a space every label has there */
    for (k = 0 ; k < len ; ++k)
        if (is_space(row[0].label[k]))
            word = k + 1;
    while (word != 0)
    {
        for (k = word ; k < len && is_space(row[0].label[k]) ; ++k)
            ;
        for (i = 0 ; i < n && k < row[i].len ; ++i)
            ;
        if (i == n)
            return k;
        while (--word != 0 && !is_space(row[0].label[word - 1]))
            ;
    }
    return 0;
}

/* the start of a label for sorting: past 'skip' and a leading "the " */
static size_t sort_start(const maplink *m, size_t skip)
{
    size_t k = skip;

    if (k + 4 <= m->len && fold(m->label[k]) == 't'
        && fold(m->label[k + 1]) == 'h' && fold(m->label[k + 2]) == 'e'
        && is_space(m->label[k + 3]))
    {
        for (k += 4 ; k < m->len && is_space(m->label[k]) ; ++k)
            ;
        if (k == m->len)
            k = skip;
    }
    return k;
}

/*
 *   Compare two labels for the sorted row: case and spaces don't count, and
 *   a run of digits counts as its number, so "Room 2" comes before "Room 10"
 *   and "Room19" between "Room 18" and "Room 20"
 */
static int label_cmp(const maplink *a, const maplink *b, size_t skip)
{
    size_t i = sort_start(a, skip), j = sort_start(b, skip);

    for (;;)
    {
        glui32 x, y;

        while (i < a->len && is_space(a->label[i]))
            ++i;
        while (j < b->len && is_space(b->label[j]))
            ++j;
        if (i == a->len || j == b->len)
            return (i < a->len) - (j < b->len);
        x = a->label[i];
        y = b->label[j];
        if (x >= '0' && x <= '9' && y >= '0' && y <= '9')
        {
            size_t xs, ys, xn, yn, k;

            while (i < a->len && a->label[i] == '0')
                ++i;
            while (j < b->len && b->label[j] == '0')
                ++j;
            for (xs = i ; i < a->len && a->label[i] >= '0'
                          && a->label[i] <= '9' ; ++i)
                ;
            for (ys = j ; j < b->len && b->label[j] >= '0'
                          && b->label[j] <= '9' ; ++j)
                ;
            xn = i - xs;
            yn = j - ys;
            if (xn != yn)
                return xn < yn ? -1 : 1;
            for (k = 0 ; k < xn ; ++k)
                if (a->label[xs + k] != b->label[ys + k])
                    return a->label[xs + k] < b->label[ys + k] ? -1 : 1;
            continue;
        }
        x = fold(x);
        y = fold(y);
        if (x != y)
            return x < y ? -1 : 1;
        ++i;
        ++j;
    }
}

/*
 *   A picture with a map in the page's text goes in a window above the
 *   page, where its areas can be clicked (osglkpage.c), rather than in the
 *   text, where it could be only one link.  Not one in a table, unless it
 *   starts the table's first cell (School's map, in a table that centres
 *   it), or that floats, or the tiles of a picture in several (Golden Skull's compass):
 *   when a second tile comes, the first goes back in the text after all,
 *   where it would have been.  The row of links is put out all the same,
 *   for the keyboard and VoiceOver.
 */
static int map_window(parser *p, const imagemap *m, const oshtml_image *img,
                      int cell_empty)
{
    oshtml_pictable pt;

    if (p != &fmt || p->dst)
        return 0;
    if (p->nmaprow_maps != 0)
    {
        if (p->map_win == 1)
        {
            p->map_win = 0;
            osglk_map_remove();
            show_image(p, &p->map_img);
        }
        return 0;
    }
    if ((p->table && (p->table > 1 || p->ncell != 1 || !cell_empty))
        || img->align == imagealign_MarginLeft
        || img->align == imagealign_MarginRight
        || img->width_pct || img->height_pct)
        return 0;
    memset(&pt, 0, sizeof(pt));
    if (!layout_mappic(p, m, img, &pt) || !osglk_map_show(&pt))
        return 0;
    p->map_win = 1;
    p->map_img = *img;
    return 1;
}

/*
 *   A table at the top of the main window's text may be only pictures with
 *   image maps, as Golden Skull's compass is: nine tiles, each with its own
 *   buttons.  As lines of text its pictures have gaps between them and
 *   can't be clicked, so it waits, from its <TABLE>, until its end or the
 *   first thing in it that isn't a picture tells whether it can go in the
 *   window above the page, laid out in pixels as HTML TADS lays it out
 *   (layout_maptable).  If it can't, the parser reads it after all.
 */
#define MAPCAP_MAX 8192

static int map_capture_add(parser *p, glui32 c)
{
    if (p->nmapcap == p->mapcapmax)
    {
        size_t max = p->mapcapmax ? p->mapcapmax * 2 : 1024;
        glui32 *buf;

        if (max > MAPCAP_MAX)
            return 0;
        buf = realloc(p->mapcaptext, max * sizeof(glui32));
        if (!buf)
            return 0;
        p->mapcaptext = buf;
        p->mapcapmax = max;
    }
    p->mapcaptext[p->nmapcap++] = c;
    return 1;
}

/* the table's <TABLE> tag, t[0..len): does the table wait? */
static int map_table_start(parser *p, const glui32 *t, size_t len)
{
    size_t i;

    if (p != &fmt || p->dst || p->table || p->mapcap || p->capturing
        || p->nmaprow_maps)
        return 0;
    p->mapcap = 1;
    p->nmapcap = 0;
    if (!map_capture_add(p, '<'))
        return p->mapcap = 0;
    for (i = 0 ; i < len ; ++i)
        if (!map_capture_add(p, t[i]))
            return p->mapcap = 0;
    if (!map_capture_add(p, '>'))
        return p->mapcap = 0;
    return 1;
}

/* the table isn't one, or must be shown now: the parser reads it */
static void end_map_capture(parser *p)
{
    glui32 *buf = p->mapcaptext;
    size_t i, n = p->nmapcap;

    if (p->mapcap != 1)
        return;
    p->mapcap = 2;
    p->mapcaptext = NULL;
    p->nmapcap = p->mapcapmax = 0;
    for (i = 0 ; i < n ; ++i)
        feed(p, buf[i]);
    free(buf);
    p->mapcap = 0;
}

/* the whole table is pictures: show it in the window, if it can be */
static int map_table_show(parser *p)
{
    const glui32 *t = p->mapcaptext;
    size_t len = p->nmapcap, i, ts, te, name, namelen, v, vlen, k;
    oshtml_pictable pt;
    const imagemap *m;
    int end;

    /* its maps first, as the parser would have read them */
    p->mapcap = 2;
    for (i = 0 ; i < len ; )
    {
        size_t start = i;

        if (t[i] != '<')
        {
            ++i;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            break;
        namelen = raw_name(t, ts, te, &name, &end);
        if (tag_is(t + name, namelen, "map")
            || tag_is(t + name, namelen, "area"))
            for (k = start ; k < i ; ++k)
                feed(p, t[k]);
    }
    p->mapcap = 1;

    if (!layout_maptable(p, t, len, &pt) || !osglk_map_show(&pt))
        return 0;
    p->map_win = 2;

    /* and their links, in the row under it */
    for (i = 0 ; i < len ; )
    {
        if (t[i] != '<')
        {
            ++i;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            break;
        namelen = raw_name(t, ts, te, &name, &end);
        if (!end && tag_is(t + name, namelen, "img")
            && raw_attr(t, ts, te, "usemap", &v, &vlen)
            && (m = find_map(t + v, vlen, 0)) != NULL)
            add_map_links(p, m);
    }
    return 1;
}

/* the game's text while the table waits */
static int map_capture_char(parser *p, glui32 c)
{
    int found;

    if (p->mapcap != 1)
        return 0;
    if (!map_capture_add(p, c))
    {
        end_map_capture(p);
        return 0;
    }
    if (c != '>')
        return 1;
    found = map_table_scan(p->mapcaptext, p->nmapcap);
    if (found == 2 || (found == 1 && !map_table_show(p)))
        end_map_capture(p);
    else if (found == 1)
    {
        free(p->mapcaptext);
        p->mapcaptext = NULL;
        p->nmapcap = p->mapcapmax = 0;
        p->mapcap = 0;
    }
    return 1;
}

/* the row of links under the pictures, on a line of its own */
void put_maprow(parser *p)
{
    int save = p->cur.link;
    int i, n = p->nmaprow;
    size_t k, skip;

    if (n == 0)
        p->nmaprow_maps = p->map_win = 0;
    if (n == 0 || p->dst)
        return;

    /* the row's own text doesn't put it out again */
    p->nmaprow = 0;
    skip = maprow_prefix(p->maprow, n);
    if (n >= MAPROW_SORT && p->nmaprow_maps == 1)
    {
        /* an insertion sort, which keeps equal labels in order */
        for (i = 1 ; i < n ; ++i)
        {
            maplink m = p->maprow[i];
            int j;

            for (j = i ; j > 0 && label_cmp(&p->maprow[j - 1], &m, skip) > 0
                 ; --j)
                p->maprow[j] = p->maprow[j - 1];
            p->maprow[j] = m;
        }
    }
    p->nmaprow_maps = p->map_win = 0;

    line_break(p);
    if (skip != 0)
    {
        /* the shared words, capitalised, as the row's heading */
        size_t end = skip;

        while (end != 0 && is_space(p->maprow[0].label[end - 1]))
            --end;
        for (k = 0 ; k < end ; ++k)
        {
            glui32 c = p->maprow[0].label[k];

            put_html_text(p, k == 0 && c >= 'a' && c <= 'z'
                          ? c - ('a' - 'A') : c);
        }
        put_html_text(p, ':');
        add_pending(p, ' ');
    }
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
        for (k = skip ; k < p->maprow[i].len ; ++k)
            put_html_text(p, p->maprow[i].label[k]);
        p->cur.link = save;
    }
    line_break(p);
}

/* ------------------------------------------------------------------------ */
/*
 *   The tags.  do_tag reads the name, and the tag's handler does the rest,
 *   reading what attributes it has.
 */

typedef struct tag_info
{
    const glui32 *t;            /* p->tag */
    size_t len;
    size_t name, namelen;       /* its name, t[name..name+namelen) */
    int end;                    /* an end tag */
    size_t attrs;               /* where its attributes start */
    int cell_empty;             /* nothing was in the cell before it */
} tag_info;

static int tag_named(const tag_info *tg, const char *s)
{
    return tag_is(tg->t + tg->name, tg->namelen, s);
}

static int tag_named_in(const tag_info *tg, const char *const *list)
{
    return name_in(tg->t + tg->name, tg->namelen, list);
}

static void tag_push(parser *p, const tag_info *tg)
{
    push_elem(p, tg->t + tg->name, tg->namelen);
}

static void tag_pop(parser *p, const tag_info *tg)
{
    pop_elem(p, tg->t + tg->name, tg->namelen);
}

/*
 *   Start on the attributes, read as ATTR_TO_SPACE if to_space, in TADS 3
 *   or in a banner, otherwise as ATTR_WORD
 */
static void tag_attrs(parser *p, const tag_info *tg, attr_iter *it,
                      int to_space)
{
    attr_start(it, tg->t, tg->attrs, tg->len,
               tads3 || p->in_banner || to_space ? ATTR_TO_SPACE : ATTR_WORD);
}

static int attr_named(const attr_iter *it, const char *s)
{
    return tag_is(it->t + it->name, it->namelen, s);
}

/* its value is s */
static int attr_value_is(const attr_iter *it, const char *s)
{
    return tag_is(it->t + it->v, it->vlen, s);
}

static int attr_int(const attr_iter *it)
{
    return to_int(it->t + it->v, it->vlen);
}

/* the text of t[s..s+n) */
static void put_tag_text(parser *p, const glui32 *t, size_t s, size_t n)
{
    size_t k;
    for (k = s ; k < s + n ; ++k)
        put_html_text(p, t[k]);
}

/* <BR>, a line break, or a blank line if the line is empty */
static void tag_br(parser *p, const tag_info *tg)
{
    attr_iter it;
    int br, clear = 0;          /* 1: line break, 2: blank line */

    if (p->ignore)
        return;
    br = (tads3 ? p->buffered : p->col != 0 || p->npending != 0) ? 1 : 2;
    tag_attrs(p, tg, &it, 0);
    while (attr_next(&it))
    {
        if (attr_named(&it, "height"))
        {
            int ht = attr_int(&it);
            if (ht == 0)
                br = 1;
            else if (ht > 1)
                for ( ; ht > 0 ; --ht)
                    blank_line(p);
        }
        else if (attr_named(&it, "clear"))
            clear = 1;
    }
    if (br == 1)
        line_break(p);
    else
        blank_line(p);
    if (clear)
        flow_break(p);
}

/* <IMG>: the picture, or if it can't be shown, its ALT text */
static void tag_img(parser *p, const tag_info *tg)
{
    const glui32 *t = tg->t;
    attr_iter it;
    size_t src = 0, srclen = 0, alt = 0, altlen = 0;
    size_t usemap = 0, usemaplen = 0;
    glui32 img_align = imagealign_InlineUp;
    img_size img_w = { 0, 0 }, img_h = { 0, 0 };

    if (p->ignore || tg->end)
        return;
    tag_attrs(p, tg, &it, 1);
    while (attr_next(&it))
    {
        if (attr_named(&it, "usemap"))
        {
            usemap = it.v;
            usemaplen = it.vlen;
        }
        else if (attr_named(&it, "src"))
        {
            src = it.v;
            srclen = it.vlen;
        }
        else if (attr_named(&it, "alt"))
        {
            alt = it.v;
            altlen = it.vlen;
        }
        else if (attr_named(&it, "align"))
        {
            if (attr_value_is(&it, "left"))
                img_align = imagealign_MarginLeft;
            else if (attr_value_is(&it, "right"))
                img_align = imagealign_MarginRight;
            else if (attr_value_is(&it, "top"))
                img_align = imagealign_InlineDown;
            else if (attr_value_is(&it, "middle"))
                img_align = imagealign_InlineCenter;
            else
                img_align = imagealign_InlineUp;
        }
        else if (attr_named(&it, "width"))
            parse_img_size(t + it.v, it.vlen, &img_w);
        else if (attr_named(&it, "height"))
            parse_img_size(t + it.v, it.vlen, &img_h);
    }

    if (usemaplen == 0)
    {
        if (!put_image(p, t + src, srclen, img_align, img_w, img_h,
                       tg->cell_empty))
            put_tag_text(p, t, alt, altlen);
    }
    else
    {
        const imagemap *m = find_map(t + usemap, usemaplen, 0);
        oshtml_image img;

        if (!find_image(p, t + src, srclen, img_align, img_w, img_h, &img)
            || (!(m && map_window(p, m, &img, tg->cell_empty)) && !show_image(p, &img)))
            put_tag_text(p, t, alt, altlen);
        if (m)
            add_map_links(p, m);
    }
}

/* <SOUND>: the sound, or if it can't be played, its ALT text */
static void tag_sound(parser *p, const tag_info *tg)
{
    const glui32 *t = tg->t;
    attr_iter it;
    ossound_tag sndtag;
    size_t src = 0, srclen = 0, alt = 0, altlen = 0;

    if (p->ignore || tg->end)
        return;
    ossound_tag_init(&sndtag);
    tag_attrs(p, tg, &it, 1);
    while (attr_next(&it))
    {
        const glui32 *v = t + it.v;
        size_t vlen = it.vlen;

        if (attr_named(&it, "src"))
        {
            src = it.v;
            srclen = vlen;
        }
        else if (attr_named(&it, "alt"))
        {
            alt = it.v;
            altlen = vlen;
        }
        else if (attr_named(&it, "layer"))
            sndtag.layer = sound_layer(v, vlen);
        else if (attr_named(&it, "cancel"))
        {
            sndtag.cancel = 1;
            sndtag.layer = sound_layer(v, vlen);
        }
        else if (attr_named(&it, "interrupt"))
            sndtag.interrupt = 1;
        else if (attr_named(&it, "repeat"))
        {
            sndtag.has_repeat = 1;
            sndtag.repeat = attr_value_is(&it, "loop") ? 0 : attr_int(&it);
        }
        else if (attr_named(&it, "random"))
            sndtag.random = attr_int(&it);
        else if (attr_named(&it, "volume"))
            sndtag.volume = attr_int(&it) > 100 ? 100 : attr_int(&it);
        else if (attr_named(&it, "fadein"))
            sndtag.fadein = sound_fade(v, vlen, &sndtag.crossfade_in);
        else if (attr_named(&it, "fadeout"))
            sndtag.fadeout = sound_fade(v, vlen, &sndtag.crossfade_out);
        else if (attr_named(&it, "sequence"))
            sndtag.sequence = attr_value_is(&it, "replace")
                ? OSSND_SEQ_REPLACE : attr_value_is(&it, "random")
                ? OSSND_SEQ_RANDOM : attr_value_is(&it, "cycle")
                ? OSSND_SEQ_CYCLE : OSSND_SEQ_NONE;
    }

    if (!put_sound(p, &sndtag, t + src, srclen))
        put_tag_text(p, t, alt, altlen);
}

/* <A HREF>: the command is the href; an <A NAME> is no link */
static void tag_a(parser *p, const tag_info *tg)
{
    attr_iter it;
    size_t href = 0, hreflen = 0;
    int have_href = 0, link_flags = 0;

    if (p->ignore)
        return;
    if (tg->end)
    {
        tag_pop(p, tg);
        return;
    }
    tag_push(p, tg);
    tag_attrs(p, tg, &it, 1);
    while (attr_next(&it))
    {
        if (attr_named(&it, "href"))
        {
            href = it.v;
            hreflen = it.vlen;
            have_href = 1;
        }
        else if (attr_named(&it, "noenter"))
            link_flags |= OSLINK_NOENTER;
        else if (attr_named(&it, "append"))
            link_flags |= OSLINK_APPEND;
    }
    if (have_href && !p->dst)
        p->cur.link = (int)href_link(tg->t + href, hreflen, link_flags);
}

/* <MAP NAME>: the <AREA>s that follow are its */
static void tag_map(parser *p, const tag_info *tg)
{
    attr_iter it;
    size_t mapname = 0, mapnamelen = 0;

    p->map = NULL;
    if (p->ignore || tg->end)
        return;
    tag_attrs(p, tg, &it, 1);
    while (attr_next(&it))
        if (attr_named(&it, "name"))
        {
            mapname = it.v;
            mapnamelen = it.vlen;
        }
    if ((p->map = find_map(tg->t + mapname, mapnamelen, 1)) != NULL)
        p->map->nareas = p->map->nrects = p->map->unplaced = 0;
}

/* <AREA HREF> of the <MAP> */
static void tag_area(parser *p, const tag_info *tg)
{
    const glui32 *t = tg->t;
    attr_iter it;
    size_t href = 0, hreflen = 0, alt = 0, altlen = 0;
    size_t coords = 0, coordslen = 0;
    int have_href = 0, nohref = 0, area_rect = 1;

    if (!p->map || tg->end)
        return;
    tag_attrs(p, tg, &it, 1);
    while (attr_next(&it))
    {
        if (attr_named(&it, "href"))
        {
            href = it.v;
            hreflen = it.vlen;
            have_href = 1;
        }
        else if (attr_named(&it, "alt"))
        {
            alt = it.v;
            altlen = it.vlen;
        }
        else if (attr_named(&it, "nohref"))
            nohref = 1;
        else if (attr_named(&it, "shape"))
            area_rect = attr_value_is(&it, "rect");
        else if (attr_named(&it, "coords"))
        {
            coords = it.v;
            coordslen = it.vlen;
        }
    }
    if (have_href && !nohref)
    {
        glui32 link = href_link(t + href, hreflen, 0);

        if (altlen != 0)
            add_area(p->map, link, t + alt, altlen);
        else
            add_area(p->map, link, t + href, hreflen);
        if (link)
            add_area_rect(p->map, link, area_rect, t + coords, coordslen);
    }
}

/* <ABOUTBOX>: its text isn't shown */
static void tag_aboutbox(parser *p, const tag_info *tg)
{
    if (tg->end)
    {
        if (p->ignore > p->title)
            --p->ignore;
    }
    else
        ++p->ignore;
}

/*
 *   TADS 2's <BANNER>: the text waits for its </BANNER>, and a stray one is
 *   dropped.  The banner is whitespace in the text around it.  TADS 3
 *   doesn't show them, as HTML TADS doesn't.
 */
static void tag_banner(parser *p, const tag_info *tg)
{
    attr_iter it;
    int ban_remove = 0;         /* 1: REMOVE, 2: REMOVEALL */

    if (tads3)
    {
        tag_aboutbox(p, tg);
        return;
    }
    if (p->ignore || tg->end || p->in_banner)
        return;
    memset(&p->ban, 0, sizeof(p->ban));
    p->ban.align = OS_BANNER_ALIGN_TOP;

    tag_attrs(p, tg, &it, 1);
    while (attr_next(&it))
    {
        if (attr_named(&it, "id"))
        {
            size_t k;
            for (k = 0 ; k < it.vlen && k < OSHTML_BANNER_ID - 1 ; ++k)
                p->ban.id[k] = tg->t[it.v + k] < 0x80
                    ? (char)tg->t[it.v + k] : '?';
            p->ban.id[k] = '\0';
        }
        else if (attr_named(&it, "height") || attr_named(&it, "width"))
        {
            int height = attr_named(&it, "height");
            int *num = height ? &p->ban.height : &p->ban.width;
            int *units = height ? &p->ban.height_units : &p->ban.width_units;

            /* a value HTML TADS can't read is 0 */
            *num = 0;
            *units = OSHTML_SIZE_PIX;
            if (attr_value_is(&it, "previous"))
                *units = OSHTML_SIZE_PREV;
            else if (it.vlen != 0 && tg->t[it.v + it.vlen - 1] == '%')
            {
                *units = OSHTML_SIZE_PCT;
                *num = to_int(tg->t + it.v, it.vlen - 1);
            }
            else
                *num = attr_int(&it);
        }
        else if (attr_named(&it, "align"))
        {
            if (attr_value_is(&it, "top"))
                p->ban.align = OS_BANNER_ALIGN_TOP;
            else if (attr_value_is(&it, "bottom"))
                p->ban.align = OS_BANNER_ALIGN_BOTTOM;
            else if (attr_value_is(&it, "left"))
                p->ban.align = OS_BANNER_ALIGN_LEFT;
            else if (attr_value_is(&it, "right"))
                p->ban.align = OS_BANNER_ALIGN_RIGHT;
        }
        else if (attr_named(&it, "border"))
            p->ban.border = 1;
        else if (attr_named(&it, "remove"))
            ban_remove |= 1;
        else if (attr_named(&it, "removeall"))
            ban_remove |= 2;
    }

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

/* <FONT FACE COLOR BGCOLOR> */
static void tag_font(parser *p, const tag_info *tg)
{
    attr_iter it;
    int face = -1;

    if (p->ignore)
        return;
    if (tg->end)
    {
        tag_pop(p, tg);
        return;
    }
    tag_push(p, tg);
    tag_attrs(p, tg, &it, 0);
    while (attr_next(&it))
    {
        const glui32 *v = tg->t + it.v;

        if (attr_named(&it, "face"))
            face = face_combo(v, it.vlen);
        else if (attr_named(&it, "color"))
            parse_color(v, it.vlen, &p->cur.fg);
        else if (attr_named(&it, "bgcolor"))
            parse_color(v, it.vlen, &p->cur.bg);
    }
    if (face >= 0)
        p->cur.attr = (p->cur.attr & ~(OSSTY_FIXED | OSSTY_INPUT)) | face;
}

/* <BODY TEXT BGCOLOR BACKGROUND>: as in HTML TADS, what it leaves out is
   the default */
static void tag_body(parser *p, const tag_info *tg)
{
    attr_iter it;
    int body_pic = 0;           /* it has a BACKGROUND picture */
    os_color_t body_bg = OS_COLOR_P_TEXTBG;

    if (p->ignore || tg->end)
        return;
    p->cur.fg = OS_COLOR_P_TEXT;
    tag_attrs(p, tg, &it, 0);
    while (attr_next(&it))
    {
        const glui32 *v = tg->t + it.v;

        if (attr_named(&it, "text"))
            parse_color(v, it.vlen, &p->cur.fg);
        else if (attr_named(&it, "bgcolor"))
            parse_color(v, it.vlen, &body_bg);
        else if (attr_named(&it, "background"))
            body_pic = it.vlen != 0;
    }
    if (body_pic)
        body_bg = pic_page_color(p->cur.fg, body_bg);
    set_body(p, body_bg);
}

/*
 *   Open a <P>, <DIV> or <hN> with its ALIGN: 0 left, 1 centred.  A <p>
 *   opens only with one, as a <p> is seldom closed.
 */
static void open_aligned(parser *p, const tag_info *tg, int heading)
{
    attr_iter it;
    int align = -1;

    tag_attrs(p, tg, &it, 0);
    while (attr_next(&it))
        if (attr_named(&it, "align"))
            align = attr_value_is(&it, "center")
                || attr_value_is(&it, "right");
    if (align < 0 && tag_named(tg, "p"))
        return;
    tag_push(p, tg);
    if (heading)
        p->cur.attr |= OSSTY_BOLD;
    if (align == 1 && !p->in_box)
        p->cur.attr |= OSSTY_CENTER;
    else if (align == 0)
        p->cur.attr &= ~OSSTY_CENTER;
}

static void tag_p(parser *p, const tag_info *tg)
{
    if (p->ignore)
        return;
    close_elem(p, "p", p_stops);
    /* HTML TADS keeps the larger spacing (add_line_spacing) */
    para_break(p);
    if (!tg->end)
        open_aligned(p, tg, 0);
}

static void tag_div(parser *p, const tag_info *tg)
{
    if (p->ignore)
        return;
    line_break(p);
    if (tg->end)
        tag_pop(p, tg);
    else
        open_aligned(p, tg, 0);
}

/* <H1> to <H6> */
static void tag_heading(parser *p, const tag_info *tg)
{
    if (p->ignore)
        return;
    if (tg->end)
    {
        tag_pop(p, tg);
        para_break(p);
    }
    else
    {
        para_break(p);
        open_aligned(p, tg, 1);
    }
}

/*
 *   <TABLE>, <TR> and <TD>: each row is a line, and a cell goes on it
 *   after a gap, so empty cells before a cell leave the gap as an
 *   indent.  Only the first cell's ALIGN centres the row.  Where a <TAB>
 *   can align right, a right-aligned cell goes at the end of the line,
 *   as on a status line.  Only a cell's own colour is a background: a
 *   theme colour is what the window shows already.
 */
typedef struct cell_attrs
{
    int align;                  /* ALIGN: 0 left, 1 centred, 2 right */
    int width;                  /* a WIDTH */
    int px;                     /* ... in pixels */
    int bg_ok;
    os_color_t bg;
} cell_attrs;

static void read_cell_attrs(parser *p, const tag_info *tg, int table,
                            cell_attrs *c)
{
    attr_iter it;

    c->align = -1;
    c->width = c->px = c->bg_ok = 0;
    c->bg = OS_COLOR_P_TRANSPARENT;
    tag_attrs(p, tg, &it, 0);
    while (attr_next(&it))
    {
        const glui32 *v = tg->t + it.v;
        size_t vlen = it.vlen;

        if (attr_named(&it, "align") && table)
            /* a centred table centres its rows; a left or right one floats */
            c->align = attr_value_is(&it, "center") ? 1 : -1;
        else if (attr_named(&it, "align"))
            c->align = attr_value_is(&it, "center") ? 1
                : attr_value_is(&it, "right") ? 2
                : attr_value_is(&it, "left") ? 0 : -1;
        else if (attr_named(&it, "width"))
        {
            c->width = 1;
            /* under 100 pixels: a marker's cell, not a box */
            c->px = vlen && vlen <= 2
                && v[0] >= '0' && v[0] <= '9'
                && v[vlen - 1] != '%';
        }
        else if (attr_named(&it, "bgcolor"))
            c->bg_ok = parse_color(v, vlen, &c->bg)
                && !os_color_is_param(c->bg);
    }
}

static void cell_background(parser *p, const cell_attrs *c)
{
    if (c->bg_ok)
    {
        p->cur.bg = c->bg;
        contrast_text(p, c->bg);
    }
}

/* the colour of a <TABLE> or <TR>, whose ALIGN aligns its rows */
static void row_style(parser *p, const cell_attrs *c)
{
    cell_background(p, c);
    if (c->align >= 0)
    {
        if (c->align)
            p->cur.attr |= OSSTY_CENTER;
        else
            p->cur.attr &= ~OSSTY_CENTER;
    }
}

/* <TABLE>: a block, whose rows are lines */
static void tag_table(parser *p, const tag_info *tg)
{
    cell_attrs c;

    if (p->ignore)
        return;
    if (tg->end)
    {
        if (p->table)
        {
            release_held(p, 0);
            p->cell_hold = 0;
            tag_pop(p, tg);
            line_break(p);
            end_margin_pic(p);
        }
        return;
    }
    if (map_table_start(p, tg->t, tg->len))
        return;
    release_held(p, 0);
    p->cell_hold = 0;
    line_break(p);
    tag_push(p, tg);
    ++p->table;
    p->ncell = 0;
    p->row_shown = 0;
    p->side_left = 0;
    read_cell_attrs(p, tg, 1, &c);
    p->table_box = c.width && c.bg_ok;
    row_style(p, &c);
}

/* <TR>: a row ends the row before it, which may not have been closed */
static void tag_tr(parser *p, const tag_info *tg)
{
    cell_attrs c;

    if (p->ignore || !p->table)
        return;
    release_held(p, 0);
    p->cell_hold = 0;
    close_cell(p);
    close_elem(p, "tr", tr_stops);
    line_break(p);
    end_margin_pic(p);
    if (tg->end)
        return;
    tag_push(p, tg);
    p->ncell = 0;
    p->row_shown = 0;
    p->side_left = 0;
    read_cell_attrs(p, tg, 0, &c);
    p->row_align = c.align >= 0;
    row_style(p, &c);
}

/* <TD> and <TH>: so does a cell */
static void tag_td(parser *p, const tag_info *tg)
{
    cell_attrs c;
    int cell_first;             /* the row's first cell */
    int held_one;               /* the first cell was one character */
    int after_marker;           /* the cell before had one */

    if (p->ignore || !p->table)
        return;
    close_cell(p);
    if (tg->end)
        return;
    cell_first = p->ncell++ == 0;
    if (cell_first)
        release_held(p, 0);
    held_one = !cell_first && p->cell_hold == 2;
    p->cell_hold = cell_first ? 1 : held_one ? 2 : 0;
    tag_push(p, tg);
    if (tag_named(tg, "th"))
        p->cur.attr |= OSSTY_BOLD;
    p->cell_empty = 1;
    read_cell_attrs(p, tg, 0, &c);

    if (p->side_left && c.align < 0)
    {
        /* beside a picture, the text isn't centred as the table is */
        c.align = 0;
    }
    else if (c.align < 0 && !p->row_align
             && (p->table_box || (c.width && c.bg_ok)))
    {
        /*
         *   nor in a coloured box of a set width: HTML TADS centres the box,
         *   and the text starts at its left.  Centred lines would show the
         *   box ragged.  Without a colour, centring the lines looks as the
         *   centred box does.  A <CENTER> or ALIGN inside the cell
         *   (Golden Skull's sign) would do the same, so it doesn't centre.
         */
        c.align = 0;
        p->in_box = 1;
    }
    cell_background(p, &c);
    after_marker = p->narrow_first;
    p->narrow_first = c.px;
    if (!cell_first && !p->row_shown
        && !(cur_color(p).attr & OSSTY_CENTER))
    {
        /* after empty cells, the gap indents each line of the cell */
        p->npending = 0;
        /* but a picture in a marker's cell fills the gap */
        if (p->line_open && !(after_marker && p->col <= 1))
            put_fixed_tab(p, 2 * CELL_INDENT);
        p->indent += CELL_INDENT;
        p->cell_indent += CELL_INDENT;
    }
    else if (!cell_first && p->line_open)
    {
        /* the gap takes the place of the spaces the cell before ends with */
        p->npending = 0;
        p->cur.attr = (p->cur.attr & ~OSSTY_CENTER) | p->line_center;
        if (c.align == 2 && !p->line_center && (tads3 || p->in_banner))
            right_cell(p);
        else if (held_one && p->cell_hold == 2)
        {
            /* a first cell of one character, and its gap, are fixed-pitch */
            release_held(p, 1);
            put_fixed_tab(p, CELL_GAP);
        }
        else if (!(after_marker && p->col <= 1))
            put_tab(p, CELL_GAP, 0);    /* a picture fills a marker's cell */
    }
    else if (!cell_first && c.align == 2
             && !(p->cur.attr & OSSTY_CENTER) && (tads3 || p->in_banner))
        right_cell(p);
    else
    {
        /*
         *   a right-aligned first cell is as wide as its column.  A
         *   centred one of a few pixels (Threnody's menu pointer) centres
         *   its picture in the cell, not the row, and the rest of the row
         *   starts at the left, where its cells' text is.
         */
        if (cell_first && c.px)
            p->cur.attr &= ~OSSTY_CENTER;
        else if (c.align == 1)
            p->cur.attr |= OSSTY_CENTER;
        else if (c.align == 0)
            p->cur.attr &= ~OSSTY_CENTER;
        if (!cell_first)
            put_tab(p, CELL_GAP, 0);
    }
    if (held_one)
        release_held(p, 0);
}

/* <TAB>: in TADS 3 and in banners, ID, TO, INDENT, MULTIPLE and ALIGN */
static void tag_tab(parser *p, const tag_info *tg)
{
    attr_iter it;
    int tab_to = -1;
    enum tab_align tab_align = TAB_NONE;
    glui32 tab_dp = 0;

    if (p->ignore)
        return;
    if (!tads3 && !p->in_banner)
    {
        put_tab(p, 0, 4);
        return;
    }
    expand_tab(p, 1);
    tag_attrs(p, tg, &it, 0);
    while (attr_next(&it))
    {
        const glui32 *v = tg->t + it.v;
        size_t vlen = it.vlen;

        if (attr_named(&it, "id"))
        {
            int k = find_tab(p, v, vlen, 1);
            if (k >= 0)
                p->tabs[k].col = p->col + (int)p->npending;
            return;
        }
        else if (attr_named(&it, "to"))
        {
            int k = find_tab(p, v, vlen, 0);
            if (k < 0)
                return;
            tab_to = p->tabs[k].col;
        }
        else if (attr_named(&it, "indent"))
        {
            put_tab(p, to_int(v, vlen), 0);
            return;
        }
        else if (attr_named(&it, "multiple"))
        {
            put_tab(p, 0, to_int(v, vlen));
            return;
        }
        else if (attr_named(&it, "align"))
        {
            if (attr_value_is(&it, "left"))
                tab_align = TAB_LEFT;
            else if (attr_value_is(&it, "right"))
                tab_align = TAB_RIGHT;
            else if (attr_value_is(&it, "center"))
                tab_align = TAB_CENTER;
            else if (attr_value_is(&it, "decimal"))
                tab_align = TAB_DECIMAL;
            else
                return;
        }
        else if (attr_named(&it, "dp"))
            tab_dp = vlen ? v[0] : 0;
    }

    if (tab_align == TAB_CENTER && tab_to < 0 && p->in_banner
        && !p->line_open && p->col == 0 && p->npending == 0
        && !(p->cur.attr & OSSTY_CENTER))
    {
        /* a centred tab at the start of a line centres the line, as
           <CENTER> does (start_line pads with spaces if the window can't) */
        p->cur.attr |= OSSTY_CENTER;
        p->tab_centered = 1;
    }
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

/* <B>, <I>, <TT> and their kin: a style, to their end tag */
static void phrase(parser *p, const tag_info *tg, int bold, int italic)
{
    if (p->ignore)
        return;
    if (tg->end)
    {
        tag_pop(p, tg);
        return;
    }
    tag_push(p, tg);
    if (bold)
        p->cur.attr |= OSSTY_BOLD;
    else if (italic)
        p->cur.attr |= OSSTY_ITALIC;
    else
        p->cur.attr = (p->cur.attr & ~OSSTY_INPUT) | OSSTY_FIXED;
}

static void tag_bold(parser *p, const tag_info *tg)
{
    phrase(p, tg, 1, 0);
}

static void tag_italic(parser *p, const tag_info *tg)
{
    phrase(p, tg, 0, 1);
}

static void tag_fixed(parser *p, const tag_info *tg)
{
    phrase(p, tg, 0, 0);
}

/* <CENTER>: a block, like <div> */
static void tag_center(parser *p, const tag_info *tg)
{
    if (p->ignore)
        return;
    line_break(p);
    if (tg->end)
        tag_pop(p, tg);
    else
    {
        tag_push(p, tg);
        if (!p->in_box)
            p->cur.attr |= OSSTY_CENTER;
    }
}

/* <BLOCKQUOTE> and <BQ> */
static void tag_blockquote(parser *p, const tag_info *tg)
{
    if (p->ignore)
        return;
    if (tg->end)
        tag_pop(p, tg);
    para_break(p);
    if (!tg->end)
    {
        tag_push(p, tg);
        ++p->indent;
    }
}

/* <UL>, <OL> and <DL>: a list in a list goes on from the line of its item */
static void tag_list(parser *p, const tag_info *tg)
{
    if (p->ignore)
        return;
    if (tg->end)
        tag_pop(p, tg);
    if (p->list != LIST_NONE)
        line_break(p);
    else
        para_break(p);
    if (tg->end)
        return;
    tag_push(p, tg);
    if (!tag_named(tg, "dl"))
    {
        ++p->indent;
        p->list = tag_named(tg, "ol") ? LIST_OL : LIST_UL;
        p->nitem = 0;
    }
}

static void tag_li(parser *p, const tag_info *tg)
{
    if (!p->ignore && !tg->end)
        put_bullet(p);
}

/* <DT>, <DD> and <LH>: only a <DD> is indented, and closed */
static void tag_dt(parser *p, const tag_info *tg)
{
    if (p->ignore)
        return;
    if (tg->end)
    {
        if (tag_named(tg, "dd"))
            tag_pop(p, tg);
        return;
    }
    close_elem(p, "dd", dd_stops);
    line_break(p);
    if (tag_named(tg, "dd"))
    {
        tag_push(p, tg);
        ++p->indent;
    }
}

static void tag_nobr(parser *p, const tag_info *tg)
{
    if (p->ignore)
        return;
    if (tg->end)
        tag_pop(p, tg);
    else
    {
        tag_push(p, tg);
        p->nobr = 1;
    }
}

/* <HR>: a line of box-drawing characters, as wide as the window */
static void tag_hr(parser *p, const tag_info *tg)
{
    int n;

    (void)tg;
    if (p->ignore)
        return;
    para_break(p);
    n = p->sink->rule_width(p->ctx, cur_color(p).attr) - 1 - 2 * p->indent;
    for ( ; n > 0 ; --n)
        put_visible(p, 0x2500);
    para_break(p);
}

/* <Q>: double quotes, single ones in them */
static void tag_q(parser *p, const tag_info *tg)
{
    int end = tg->end;

    if (p->ignore)
        return;
    if (!end)
        ++p->quote_level;
    put_visible(p, (p->quote_level & 1)
                ? (end ? 0x201D : 0x201C)
                : (end ? 0x2019 : 0x2018));
    if (end)
        --p->quote_level;
}

/* <TITLE>: its text is the window's title, not shown */
static void tag_title(parser *p, const tag_info *tg)
{
    if (tg->end)
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

/* <PRE>: fixed-pitch, its spaces and newlines kept */
static void tag_pre(parser *p, const tag_info *tg)
{
    if (!tg->end)
        ++p->pre;
    blank_line(p);
    if (tg->end && p->pre > 0)
        --p->pre;
    if (p->ignore)
        return;
    if (tg->end)
        tag_pop(p, tg);
    else
    {
        tag_push(p, tg);
        p->cur.attr = (p->cur.attr & ~OSSTY_INPUT) | OSSTY_FIXED;
    }
}

/* the tags we show, by name; a tag that isn't here isn't shown */
static const struct tag_handler
{
    const char *name;
    void (*fn)(parser *p, const tag_info *tg);
} tag_handlers[] =
{
    { "br", tag_br },
    { "b", tag_bold }, { "strong", tag_bold },
    { "i", tag_italic }, { "em", tag_italic }, { "u", tag_italic },
    { "cite", tag_italic }, { "var", tag_italic }, { "dfn", tag_italic },
    { "address", tag_italic },
    { "tt", tag_fixed }, { "code", tag_fixed }, { "kbd", tag_fixed },
    { "samp", tag_fixed }, { "listing", tag_fixed }, { "xmp", tag_fixed },
    { "p", tag_p },
    { "div", tag_div },
    { "h1", tag_heading }, { "h2", tag_heading }, { "h3", tag_heading },
    { "h4", tag_heading }, { "h5", tag_heading }, { "h6", tag_heading },
    { "center", tag_center },
    { "blockquote", tag_blockquote }, { "bq", tag_blockquote },
    { "ul", tag_list }, { "ol", tag_list }, { "dl", tag_list },
    { "li", tag_li },
    { "dt", tag_dt }, { "dd", tag_dt }, { "lh", tag_dt },
    { "table", tag_table },
    { "tr", tag_tr },
    { "td", tag_td }, { "th", tag_td },
    { "nobr", tag_nobr },
    { "tab", tag_tab },
    { "a", tag_a },
    { "img", tag_img },
    { "map", tag_map },
    { "area", tag_area },
    { "sound", tag_sound },
    { "font", tag_font },
    { "body", tag_body },
    { "hr", tag_hr },
    { "q", tag_q },
    { "title", tag_title },
    { "aboutbox", tag_aboutbox },
    { "banner", tag_banner },
    { "pre", tag_pre },
};

/* the tag in p->tag, between '<' and '>' */
static void do_tag(parser *p)
{
    const glui32 *t = p->tag;
    size_t len = p->taglen, i = 0, name, namelen;
    tag_info tg;
    size_t k;
    int end = 0;
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

    tg.t = t;
    tg.len = len;
    tg.name = name;
    tg.namelen = namelen;
    tg.end = end;
    tg.attrs = i;
    tg.cell_empty = cell_empty;

    p->cell_empty = 0;
    if (p->cell_pic && cell_picture_tag(p, t + name, namelen, end, t, len))
    {
        p->cell_empty = cell_empty;
        return;
    }

    for (k = 0 ; k < sizeof(tag_handlers) / sizeof(tag_handlers[0]) ; ++k)
        if (tag_named(&tg, tag_handlers[k].name))
        {
            tag_handlers[k].fn(p, &tg);
            return;
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

void feed(parser *p, glui32 c)
{
    if (map_capture_char(p, c))
        return;
    if (row_capture_char(p, c))
        return;
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
    free(p->rowcap);
    free(p->mapcaptext);
    free(p);
}

/*
 *   What the main window was sent since it was last cleared or waited on,
 *   up to a size, to tell a page of pictures from the text of a game
 */
#define MAINPAGE_MAX 4096
static glui32 mainpage[MAINPAGE_MAX];
static size_t mainpage_len;
static int mainpage_over;

void oshtml_write(oshtml_parser *p, const char *str, size_t len)
{
    if (p == &fmt)
    {
        size_t k;

        for (k = 0 ; k < len ; ++k)
        {
            if (mainpage_len < MAINPAGE_MAX)
                mainpage[mainpage_len++] = (unsigned char)str[k];
            else
                mainpage_over = 1;
        }
    }
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
    end_map_capture(p);
    end_row_capture(p, 0);
    if (p->nelems)
        restore_elem(p, 0);
    p->nmaprow = p->nmaprow_maps = p->map_win = 0;
    p->cell_pic = p->cell_empty = p->side_left = p->margin_pic = 0;
    p->row_side = 0;
    p->cur.attr = 0;
    p->cur.fg = p->text;
    p->cur.bg = OS_COLOR_P_TRANSPARENT;
}

void oshtml_clear(oshtml_parser *p)
{
    end_row_capture(p, 0);
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
    mainpage_len = 0;
    mainpage_over = 0;
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
    end_map_capture(&fmt);
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
    end_row_capture(&fmt, 0);
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
    end_map_capture(&fmt);
    end_row_capture(&fmt, 0);
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

int oshtml_main_page(oshtml_pictable *pt)
{
    size_t i = 0, bg, bglen;
    int ok = 0;

    if (!mainpage_over)
    {
        while (i < mainpage_len && is_space(mainpage[i]))
            ++i;
        ok = find_picpage(mainpage + i, mainpage_len - i, &bg, &bglen)
            && build_picpage(&fmt, mainpage + i, mainpage_len - i, bg, bglen,
                             pt);
    }
    mainpage_len = 0;
    mainpage_over = 0;
    return ok;
}

/* oshtml.c -- HTML TADS markup to Glk
 *
 * The TADS 2 formatter is built with USE_HTML, so once a game turns on HTML
 * mode with \H+, its tags and entities reach os_print unmodified, along with
 * markup of the formatter's own: <BR HEIGHT=0> after each newline, <BR> for
 * a blank line, <TAB MULTIPLE=4> for \t, &nbsp; for "\ " and <B>/</B> for
 * \( and \).  This file parses that markup.
 *
 * For now it does what the formatter's own text-mode filter
 * (tads2/output.c) did when the formatter was built without USE_HTML, so
 * games look as they did, with one difference: entities come out as
 * Unicode instead of ASCII approximations.  In particular:
 *
 *   - whitespace is collapsed, dropped at the start of a line and before a
 *     line break, and doubled after . ? ! and : (looking back past quotes
 *     and closing brackets), as the formatter does it for plain text;
 *   - <br>, <p>, <tab>, <hr>, <pre> and <q> do what the text-mode filter
 *     does, and <b>, <i>, <em> and <strong> set OS_ATTR_BOLD,
 *     OS_ATTR_ITALIC, OS_ATTR_EM and OS_ATTR_STRONG;
 *   - the ALT text of <img> and <sound> is printed;
 *   - <title> sets the window title, and nothing inside <title> or
 *     <aboutbox> is shown;
 *   - every other tag is dropped and its contents shown.
 *
 * The main window and the status line share one parser, as they shared the
 * formatter's text-mode filter: the status line's closing newline puts the
 * main window's text at the start of a line too, so the formatter's newline
 * after a command doesn't make a blank line, and a <b> left open in one is
 * closed in the other.  Text the formatter writes while the status line is
 * hidden (os_status(2)) goes through the parser too, and is thrown away.
 * Text reaches the parser in Unicode: each os_print chunk is decoded with
 * os_parse_chars first, exactly as os_put_buffer decoded it, so the
 * character set is guessed from the same text as before.
 */

#include <stdlib.h>
#include <string.h>

#include "os.h"
#include "glk.h"
#include "oshtml.h"

extern glui32 os_parse_chars(unsigned char *buf, glui32 buflen,
                             glui32 *out, glui32 outlen);

/* the longest tag and entity name we keep; the rest is skipped */
#define TAG_MAX     1024
#define ENTITY_MAX  9           /* as TADS 2 has it: "thetasym" fits */
#define PENDING_MAX 256
#define OUT_MAX     256
#define TITLE_MAX   256

enum token { TOK_TEXT, TOK_TAG, TOK_ENTITY };

typedef struct parser
{
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
    int pending_attr[PENDING_MAX]; /* ...and their attributes */
    size_t npending;

    /* tag state */
    int ignore;                 /* inside <title> or <aboutbox> */
    int title;                  /* inside <title> */
    int pre;                    /* inside <pre> */
    int quote_level;            /* <q> nesting */
    glui32 titlebuf[TITLE_MAX];
    size_t titlelen;

    /*
     *   output: Glk calls for the main window, or 'dst' while
     *   oshtml_print_status is running
     */
    glui32 out[OUT_MAX];
    size_t outlen;
    glui32 *dst;
    size_t dstlen, dstmax;
} parser;

static parser fmt;

/* \H+ is on */
static int html_mode = 0;

/* the formatter's attributes, from os_set_text_attr */
static int os_attr = 0;

/* OS_ATTR_xxx from <b> <i> <em> <strong> */
static int tag_attr = 0;

/* the attributes of the main window's current Glk style */
static int glk_attr = 0;

static void put_char(parser *p, glui32 c);
static int is_space(glui32 c);

/* ------------------------------------------------------------------------ */
/*
 *   Output
 */

static void apply_style(int attr)
{
    if (attr & OS_ATTR_BOLD && attr & OS_ATTR_ITALIC)
        glk_set_style(style_Alert);
    else if (attr & OS_ATTR_BOLD)
        glk_set_style(style_Subheader);
    else if (attr & OS_ATTR_ITALIC)
        glk_set_style(style_Emphasized);
    else
        glk_set_style(style_Normal);
    glk_attr = attr;
}

static void flush_out(parser *p)
{
    if (p->outlen)
        glk_put_buffer_uni(p->out, (glui32)p->outlen);
    p->outlen = 0;
}

/* print one character with the given attributes */
static void emit(parser *p, glui32 c, int attr)
{
    if (p->dst)
    {
        if (p->dstlen < p->dstmax)
            p->dst[p->dstlen++] = c;
        return;
    }
    if (attr != glk_attr)
    {
        flush_out(p);
        apply_style(attr);
    }
    if (p->outlen == OUT_MAX)
        flush_out(p);
    p->out[p->outlen++] = c;
}

static int cur_attr(parser *p)
{
    (void)p;
    return os_attr | tag_attr;
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

    for (i = 0 ; i < p->npending ; ++i)
        emit(p, p->pending[i], p->pending_attr[i]);
    if (p->npending)
    {
        p->col += (int)p->npending;
        p->lastchar = ' ';
    }
    p->npending = 0;
}

static void add_pending(parser *p, glui32 c)
{
    if (p->npending == PENDING_MAX)
        print_pending(p);
    p->pending[p->npending] = c;
    p->pending_attr[p->npending++] = cur_attr(p);
}

/* print a visible character */
static void put_visible(parser *p, glui32 c)
{
    print_pending(p);
    emit(p, c, cur_attr(p));
    ++p->col;
    if (!is_quote(c))
        p->lastchar = c;
}

/* end the line, in the style of the text before it */
static void put_newline(parser *p)
{
    p->npending = 0;
    emit(p, '\n', glk_attr);
    p->col = 0;
    p->lastchar = 0;
}

/*
 *   The formatter's flush with a newline: drop the trailing spaces, and end
 *   the line unless nothing is on it (in <pre>, end it anyway).  Then the
 *   formatter brings the style up to date.
 */
static void line_break(parser *p)
{
    p->npending = 0;
    if (p->col != 0 || p->pre)
        put_newline(p);
    if (!p->dst && cur_attr(p) != glk_attr)
    {
        flush_out(p);
        apply_style(cur_attr(p));
    }
}

/* the formatter's blank line: a line break, then a newline */
static void blank_line(parser *p)
{
    line_break(p);
    put_newline(p);
}

/* a tab: spaces to the next multiple of 4, as the formatter's \t */
static void put_tab(parser *p)
{
    int linecol = p->col + (int)p->npending;

    do
    {
        add_pending(p, ' ');
        ++linecol;
    } while (((linecol + 1) & 3) != 0 && linecol < OS_MAXWIDTH);
}

/* an ordinary space in HTML mode */
static void put_space(parser *p)
{
    glui32 prev;

    if (p->pre)
    {
        add_pending(p, ' ');
        return;
    }

    /* none at the start of a line, and never two in a row... */
    if (p->col == 0 && p->npending == 0)
        return;
    if (p->npending != 0)
        return;

    /* ...except after the end of a sentence */
    add_pending(p, ' ');
    prev = p->lastchar;
    if (prev == '.' || prev == '?' || prev == '!' || prev == ':')
        add_pending(p, ' ');
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
        add_pending(p, ' ');
        break;

    default:
        put_visible(p, c);
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
static void do_tag(parser *p)
{
    const glui32 *t = p->tag;
    size_t len = p->taglen, i = 0, name, namelen;
    int end = 0;
    int br = 0;                 /* 1: line break, 2: blank line */
    int alt_ok = 0;

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
    if (TAG("br"))
    {
        if (!p->ignore)
            br = (p->col != 0 || p->npending != 0) ? 1 : 2;
    }
    else if (TAG("b") || TAG("i") || TAG("em") || TAG("strong"))
    {
        int attr = TAG("b") ? OS_ATTR_BOLD
            : TAG("i") ? OS_ATTR_ITALIC
            : TAG("em") ? OS_ATTR_EM : OS_ATTR_STRONG;
        if (p->ignore)
            ;
        else if (!end)
            tag_attr |= attr;
        else
            tag_attr &= ~attr;
    }
    else if (TAG("p"))
    {
        if (!p->ignore)
            blank_line(p);
    }
    else if (TAG("tab"))
    {
        if (!p->ignore)
            put_tab(p);
    }
    else if (TAG("img") || TAG("sound"))
        alt_ok = 1;
    else if (TAG("hr"))
    {
        if (!p->ignore)
        {
            int n;
            line_break(p);
            for (n = G_os_linewidth - 1 ; n > 0 ; --n)
                put_visible(p, '_');
            blank_line(p);
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
    else if (TAG("aboutbox"))
    {
        if (end)
        {
            if (p->ignore > p->title)
                --p->ignore;
        }
        else
            ++p->ignore;
    }
    else if (TAG("pre"))
    {
        if (!end)
            ++p->pre;
        blank_line(p);
        if (end && p->pre > 0)
            --p->pre;
    }
#undef TAG

    /* attributes: HEIGHT of <br>, ALT of <img> and <sound> */
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

        if (br && tag_is(t + an, anlen, "height"))
        {
            int ht = 0;
            size_t k;
            for (k = v ; k < v + vlen && t[k] >= '0' && t[k] <= '9' ; ++k)
                ht = ht * 10 + (int)(t[k] - '0');
            if (ht == 0)
                br = 1;
            else if (ht > 1)
                for ( ; ht > 0 ; --ht)
                    blank_line(p);
        }
        else if (alt_ok && !p->ignore && tag_is(t + an, anlen, "alt"))
        {
            size_t k;
            for (k = v ; k < v + vlen ; ++k)
                put_html_text(p, t[k]);
        }
    }

    if (br == 1)
        line_break(p);
    else if (br == 2)
        blank_line(p);
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

    if (!html_mode)
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

void oshtml_start(void)
{
    html_mode = 1;
}

void oshtml_end(void)
{
    /* the formatter flushed the line without ending it */
    print_pending(&fmt);
    flush_out(&fmt);
    html_mode = 0;
}

void oshtml_print_main(const char *str, size_t len)
{
    feed_bytes(&fmt, str, len);
    flush_out(&fmt);
}

size_t oshtml_print_status(const char *str, size_t len,
                           glui32 *out, size_t max)
{
    parser *p = &fmt;

    p->dst = out;
    p->dstlen = 0;
    p->dstmax = max;
    feed_bytes(p, str, len);
    p->dst = 0;
    return p->dstlen;
}

void oshtml_put_plain(const char *str, size_t len)
{
    oshtml_flush();
    os_put_buffer((unsigned char *)str, len);
}

/* the formatter flushed a line without ending it: show what it holds */
void oshtml_flush(void)
{
    print_pending(&fmt);
    flush_out(&fmt);
}

void oshtml_set_os_attr(int attr)
{
    os_attr = attr;
    flush_out(&fmt);
    apply_style(os_attr | tag_attr);
}

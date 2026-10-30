/* osstyles.c -- the 11 Glk styles, chosen per game
 *
 * HTML TADS can combine bold, italic, fixed pitch, the input font and
 * centring in any way, and Glk has 11 styles whose hints must be set before
 * a window opens.  So before the first window opens, we read the game's
 * strings straight from the game file, follow the tags in each one, and
 * count how often text shows in each combination of those five attributes.
 * The combinations used most get a style each, and the hints that make the
 * style look like its combination.  See README.md, "Styles".
 *
 * The count works as test/.work/bin/stylecount.py does, on the same blobs:
 * the data of each object and function, and the format strings, in a TADS 2
 * game; each constant and code pool page in a TADS 3 game.  Each blob has a
 * tag stack of its own, and a run of text counts only while some tag is
 * open.  TADS 2's \( and \) count as <b> and </b>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "osstyles.h"

#define NSTYLES 11

/* the combination in each style, or -1 if the style is unused */
static int style_combo[NSTYLES];

/* the style for each combination, nearest one included */
static glui32 combo_style[OSSTY_COMBOS];

/* the styles have been chosen */
static int chosen = 0;

/* ------------------------------------------------------------------------ */
/*
 *   Following the tags
 */

/* what a tag sets */
enum { SET_B = 1, SET_I = 2, SET_U = 4, SET_FAM = 8, SET_ALIGN = 16 };
enum { FAM_NONE, FAM_MONO, FAM_INPUT, FAM_OTHER };

#define NAME_MAX 16

typedef struct entry
{
    char name[NAME_MAX];        /* the tag, in upper case; "\\(" for \( */
    int set;
    int fam;
} entry;

typedef struct scan
{
    entry *stack;
    size_t depth, max;
    unsigned long *counts;
    int escapes;
    int tabc;               /* a <TAB ALIGN=center> centres the next text */
} scan;

static int is_alpha(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_alnum(int c)
{
    return is_alpha(c) || (c >= '0' && c <= '9');
}

/* Python's whitespace in a bytes regex */
static int is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r'
        || c == '\v' || c == '\f';
}

static void push(scan *sc, const char *name, int set, int fam)
{
    entry *e;

    if (sc->depth == sc->max)
    {
        size_t max = sc->max ? sc->max * 2 : 64;
        entry *stack = realloc(sc->stack, max * sizeof(entry));
        if (!stack)
            return;
        sc->stack = stack;
        sc->max = max;
    }
    e = &sc->stack[sc->depth++];
    strncpy(e->name, name, NAME_MAX - 1);
    e->name[NAME_MAX - 1] = '\0';
    e->set = set;
    e->fam = fam;
}

/* an end tag takes the stack back to below its start tag, if it has one */
static void pop(scan *sc, const char *name)
{
    size_t i;

    for (i = sc->depth ; i > 0 ; --i)
        if (strcmp(sc->stack[i - 1].name, name) == 0)
        {
            sc->depth = i - 1;
            return;
        }
}

/* a run of text that isn't all spaces */
static void count_text(scan *sc)
{
    int combo = 0, fam = FAM_NONE;
    size_t i;

    if (sc->depth == 0)
        return;
    if (sc->tabc)
        combo |= OSSTY_CENTER;
    sc->tabc = 0;
    for (i = 0 ; i < sc->depth ; ++i)
    {
        const entry *e = &sc->stack[i];
        if (e->set & SET_B)
            combo |= OSSTY_BOLD;
        if (e->set & (SET_I | SET_U))
            combo |= OSSTY_ITALIC;
        if (e->set & SET_ALIGN)
            combo |= OSSTY_CENTER;
        if (e->set & SET_FAM)
            fam = e->fam;
    }
    if (fam == FAM_MONO)
        combo |= OSSTY_FIXED;
    else if (fam == FAM_INPUT)
        combo |= OSSTY_INPUT;
    ++sc->counts[combo];
}

/* does 's' contain 'sub', ignoring case? */
static int contains(const char *s, size_t len, const char *sub)
{
    size_t n = strlen(sub), i, k;

    for (i = 0 ; i + n <= len ; ++i)
    {
        for (k = 0 ; k < n ; ++k)
        {
            int c = (unsigned char)s[i + k];
            if (c >= 'A' && c <= 'Z')
                c += 'a' - 'A';
            if (c != sub[k])
                break;
        }
        if (k == n)
            return 1;
    }
    return 0;
}

static int font_family(const char *face, size_t len)
{
    static const char *const mono[] =
    {
        "typewriter", "courier", "mono", "fixed", "console"
    };
    size_t i;

    if (contains(face, len, "input"))
        return FAM_INPUT;
    for (i = 0 ; i < sizeof(mono) / sizeof(mono[0]) ; ++i)
        if (contains(face, len, mono[i]))
            return FAM_MONO;
    return FAM_OTHER;
}

static int value_is(const char *v, size_t len, const char *s)
{
    return len == strlen(s) && contains(v, len, s);
}

/*
 *   The attributes of a tag, between its name and the '>'.  Only FACE and
 *   ALIGN matter; the last of each wins.
 */
static void tag_attrs(const char *s, size_t len,
                      const char **face, size_t *facelen,
                      const char **align, size_t *alignlen)
{
    size_t i = 0;

    while (i < len)
    {
        size_t name, namelen, k, v = 0, vlen = 0;
        int has_value = 0;

        if (!is_alpha((unsigned char)s[i]))
        {
            ++i;
            continue;
        }
        for (name = i++ ; i < len && (is_alnum((unsigned char)s[i])
                                      || s[i] == '-') ; ++i)
            ;
        namelen = i - name;

        /* "=value", "='value'" or "=\"value\"" */
        for (k = i ; k < len && is_space((unsigned char)s[k]) ; ++k)
            ;
        if (k < len && s[k] == '=')
        {
            for (++k ; k < len && is_space((unsigned char)s[k]) ; ++k)
                ;
            if (k < len && (s[k] == '"' || s[k] == '\''))
            {
                const char *q = memchr(s + k + 1, s[k], len - k - 1);
                if (q)
                {
                    v = k;
                    vlen = (size_t)(q - s) + 1 - k;
                    has_value = 1;
                }
            }
            if (!has_value && k < len && !is_space((unsigned char)s[k])
                && s[k] != '>')
            {
                for (v = k ; k < len && !is_space((unsigned char)s[k])
                         && s[k] != '>' ; ++k)
                    ;
                vlen = k - v;
                has_value = 1;
            }
            if (has_value)
                i = v + vlen;
        }

        /* the value without its quotes */
        while (vlen && (s[v] == '"' || s[v] == '\''))
            ++v, --vlen;
        while (vlen && (s[v + vlen - 1] == '"' || s[v + vlen - 1] == '\''))
            --vlen;

        if (namelen == 4 && contains(s + name, 4, "face"))
            *face = s + v, *facelen = vlen;
        else if (namelen == 5 && contains(s + name, 5, "align"))
            *align = s + v, *alignlen = vlen;
    }
}

/* a tag: what it sets goes on the stack, or comes off it */
static void do_tag(scan *sc, int end, const char *name, size_t namelen,
                   const char *attrs, size_t attrlen)
{
    static const char *const bold[] = { "B", "STRONG", 0 };
    static const char *const italic[] =
        { "I", "EM", "CITE", "VAR", "DFN", "ADDRESS", 0 };
    static const char *const fixed[] =
        { "TT", "CODE", "KBD", "SAMP", "PRE", "LISTING", "XMP", 0 };
    static const char *const other[] =
    {
        "S", "STRIKE", "DEL", "BIG", "SMALL", "SUP", "SUB",
        "BLOCKQUOTE", "BQ", "UL", "OL", "DL", 0
    };
    const char *face = 0, *align = 0;
    size_t facelen = 0, alignlen = 0;
    char n[NAME_MAX];
    int centred;
    size_t i;

    /* the name in upper case; one too long is no tag we know */
    if (namelen >= NAME_MAX)
        return;
    for (i = 0 ; i < namelen ; ++i)
        n[i] = (char)(name[i] >= 'a' && name[i] <= 'z'
                      ? name[i] - ('a' - 'A') : name[i]);
    n[namelen] = '\0';

    if (end)
    {
        pop(sc, n);
        return;
    }

    tag_attrs(attrs, attrlen, &face, &facelen, &align, &alignlen);
    centred = value_is(align, alignlen, "center")
        || value_is(align, alignlen, "right");

    for (i = 0 ; bold[i] ; ++i)
        if (strcmp(n, bold[i]) == 0)
        {
            push(sc, n, SET_B, 0);
            return;
        }
    for (i = 0 ; italic[i] ; ++i)
        if (strcmp(n, italic[i]) == 0)
        {
            push(sc, n, SET_I, 0);
            return;
        }
    for (i = 0 ; fixed[i] ; ++i)
        if (strcmp(n, fixed[i]) == 0)
        {
            push(sc, n, SET_FAM, FAM_MONO);
            return;
        }
    for (i = 0 ; other[i] ; ++i)
        if (strcmp(n, other[i]) == 0)
        {
            push(sc, n, 0, 0);
            return;
        }
    if (strcmp(n, "TAB") == 0)
    {
        if (value_is(align, alignlen, "center"))
            sc->tabc = 1;
    }
    else if (strcmp(n, "U") == 0)
        push(sc, n, SET_U, 0);
    else if (strcmp(n, "FONT") == 0)
        push(sc, n, face ? SET_FAM : 0,
             face ? font_family(face, facelen) : 0);
    else if (n[0] == 'H' && n[1] >= '1' && n[1] <= '6' && n[2] == '\0')
        push(sc, n, SET_B | (centred ? SET_ALIGN : 0), 0);
    else if (strcmp(n, "CENTER") == 0)
        push(sc, n, SET_ALIGN, 0);
    else if ((strcmp(n, "P") == 0 || strcmp(n, "DIV") == 0) && centred)
        push(sc, n, SET_ALIGN, 0);
}

/*
 *   A tag at s[pos], which is '<': if there is one, act on it and return the
 *   position after its '>'; if not, return 0
 */
static size_t tag_at(scan *sc, const char *s, size_t len, size_t pos)
{
    size_t i = pos + 1, name, namelen, attrs;
    int end = 0;

    while (i < len && is_space((unsigned char)s[i]))
        ++i;
    if (i < len && s[i] == '/')
    {
        end = 1;
        ++i;
    }
    while (i < len && is_space((unsigned char)s[i]))
        ++i;
    if (i >= len || !is_alpha((unsigned char)s[i]))
        return 0;
    for (name = i ; i < len && is_alnum((unsigned char)s[i]) ; ++i)
        ;
    namelen = i - name;

    /* the attributes, with quoted values, up to the '>' */
    for (attrs = i ; ; )
    {
        if (i >= len || s[i] == '<')
            return 0;
        if (s[i] == '>')
            break;
        if (s[i] == '"' || s[i] == '\'')
        {
            const char *q = memchr(s + i + 1, s[i], len - i - 1);
            if (!q)
                return 0;
            i = (size_t)(q - s) + 1;
        }
        else
            ++i;
    }

    do_tag(sc, end, s + name, namelen, s + attrs, i - attrs);
    return i + 1;
}

/* a blob of game text */
static void scan_blob(scan *sc, const char *s, size_t len)
{
    size_t pos = 0, next;
    int text = 0;               /* the run of text so far isn't all spaces */

    sc->depth = 0;
    while (pos < len)
    {
        int c = (unsigned char)s[pos];

        if (c == '<')
        {
            if (text)
                count_text(sc);
            text = 0;
            next = tag_at(sc, s, len, pos);
            pos = next ? next : pos + 1;
            continue;
        }
        if (c == '\\' && sc->escapes && pos + 1 < len)
        {
            if (s[pos + 1] == '(' || s[pos + 1] == ')')
            {
                if (text)
                    count_text(sc);
                text = 0;
                if (s[pos + 1] == '(')
                    push(sc, "\\(", SET_B, 0);
                else
                    pop(sc, "\\(");
                pos += 2;
                continue;
            }

            /* "\\(" is a backslash, then a parenthesis */
            if (s[pos + 1] != '<')
            {
                text = 1;
                pos += 2;
                continue;
            }
        }
        if (!is_space(c))
            text = 1;
        ++pos;
    }
    if (text)
        count_text(sc);
}

/* ------------------------------------------------------------------------ */
/*
 *   Reading the game file
 */

static const char t2sig[] = "TADS2 bin\n\r\x1a";
static const char t3sig[] = "T3-image\r\n\x1a";

static unsigned long u2(const unsigned char *b)
{
    return b[0] | ((unsigned long)b[1] << 8);
}

static unsigned long u4(const unsigned char *b)
{
    return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16)
        | ((unsigned long)b[3] << 24);
}

/* read 'len' bytes at 'pos', or return null */
static unsigned char *read_at(FILE *fp, long pos, size_t len)
{
    unsigned char *buf = malloc(len ? len : 1);

    if (!buf)
        return 0;
    if (fseek(fp, pos, SEEK_SET) != 0 || fread(buf, 1, len, fp) != len)
    {
        free(buf);
        return 0;
    }
    return buf;
}

static void unxor(unsigned char *b, size_t len, int seed, int inc)
{
    size_t i;

    for (i = 0 ; i < len ; ++i, seed += inc)
        b[i] ^= (unsigned char)seed;
}

/*
 *   TADS 2: the objects and functions in the OBJ section and the format
 *   strings in FMTSTR, which are encrypted if header flag 8 is set, with the
 *   seed and increment from XSI
 */
static int scan_t2(scan *sc, FILE *fp, long size)
{
    unsigned char hdr[48], sec[1 + 255 + 4], *body;
    int crypt, seed = 17, inc = 29;
    long pos = 48, end, start;
    size_t p, n;

    if (fseek(fp, 0, SEEK_SET) != 0 || fread(hdr, 1, 48, fp) != 48)
        return 0;
    crypt = (u2(hdr + 20) & 0x08) != 0;

    while (pos < size)
    {
        if (fseek(fp, pos, SEEK_SET) != 0 || fread(sec, 1, 1, fp) != 1
            || fread(sec + 1, 1, sec[0] + 4, fp) != (size_t)sec[0] + 4)
            break;
        n = sec[0];
        end = (long)u4(sec + 1 + n);
        start = pos + 1 + (long)n + 4;

#define SECTION(s) (n == strlen(s) && memcmp(sec + 1, s, n) == 0)
        if (SECTION("$EOF"))
            break;
        if (end > size || end < start)
            break;
        if (SECTION("XSI") && end - start >= 2
            && (body = read_at(fp, start, 2)) != 0)
        {
            seed = body[0];
            inc = body[1];
            free(body);
        }
        else if (SECTION("OBJ")
                 && (body = read_at(fp, start, (size_t)(end - start))) != 0)
        {
            size_t len = (size_t)(end - start);
            for (p = 0 ; p < len ; )
            {
                int t = body[p];
                if ((t == 1 || t == 2) && p + 7 <= len)
                {
                    size_t dsz = u2(body + p + 5);
                    if (p + 7 + dsz > len)
                        break;
                    if (crypt)
                        unxor(body + p + 7, dsz, seed, inc);
                    scan_blob(sc, (char *)body + p + 7, dsz);
                    p += 7 + dsz;
                }
                else if ((t == 7 || t == 8) && p + 5 <= len)
                    p += 5 + u2(body + p + 3);
                else if (t == 10 && p + 4 <= len)
                    p += 4 + body[p + 3];
                else
                    break;
            }
            free(body);
        }
        else if (SECTION("FMTSTR") && end - start >= 2
                 && (body = read_at(fp, start, (size_t)(end - start))) != 0)
        {
            size_t len = u2(body);
            if (len > (size_t)(end - start) - 2)
                len = (size_t)(end - start) - 2;
            if (crypt)
                unxor(body + 2, len, seed, inc);
            scan_blob(sc, (char *)body + 2, len);
            free(body);
        }
#undef SECTION
        pos = end;
    }
    return 1;
}

/* TADS 3: every constant and code pool page, XOR'd with its mask */
static int scan_t3(scan *sc, FILE *fp, long size)
{
    unsigned char hdr[10], *body;
    long pos = 69;
    unsigned long sz;
    size_t i;

    while (pos + 10 <= size)
    {
        if (fseek(fp, pos, SEEK_SET) != 0 || fread(hdr, 1, 10, fp) != 10)
            break;
        sz = u4(hdr + 4);
        if (memcmp(hdr, "EOF ", 4) == 0 || (long)sz > size - pos - 10)
            break;
        if (memcmp(hdr, "CPPG", 4) == 0 && sz >= 7
            && (body = read_at(fp, pos + 10, sz)) != 0)
        {
            unsigned char mask = body[6];
            if (mask)
                for (i = 7 ; i < sz ; ++i)
                    body[i] ^= mask;
            scan_blob(sc, (char *)body + 7, sz - 7);
            free(body);
        }
        pos += 10 + (long)sz;
    }
    return 1;
}

int osstyles_count(const char *filename, unsigned long *counts, int escapes)
{
    char sig[11];
    FILE *fp;
    long size;
    scan sc;
    int ok = 0;

    memset(counts, 0, OSSTY_COMBOS * sizeof(unsigned long));
    if (!filename || !(fp = fopen(filename, "rb")))
        return 0;
    if (fseek(fp, 0, SEEK_END) == 0 && (size = ftell(fp)) > 0
        && fseek(fp, 0, SEEK_SET) == 0 && fread(sig, 1, 11, fp) == 11)
    {
        memset(&sc, 0, sizeof(sc));
        sc.counts = counts;
        if (memcmp(sig, t2sig, 11) == 0)
        {
            sc.escapes = escapes;
            ok = scan_t2(&sc, fp, size);
        }
        else if (memcmp(sig, t3sig, 11) == 0)
            ok = scan_t3(&sc, fp, size);
        free(sc.stack);
    }
    fclose(fp);
    return ok;
}

/* ------------------------------------------------------------------------ */
/*
 *   Choosing the styles
 */

/* the style whose meaning is closest to a combination */
static int natural_style(int combo)
{
    switch (combo)
    {
    case OSSTY_ITALIC:                      return style_Emphasized;
    case OSSTY_BOLD:                        return style_Subheader;
    case OSSTY_BOLD | OSSTY_ITALIC:         return style_Alert;
    case OSSTY_FIXED:                       return style_Preformatted;
    case OSSTY_CENTER | OSSTY_BOLD:         return style_Header;
    case OSSTY_CENTER:                      return style_BlockQuote;
    default:                                return -1;
    }
}

/* the styles left for the other combinations, in the order they're given */
static const int spare_styles[] =
{
    style_User1, style_User2, style_Note,
    style_Emphasized, style_Subheader, style_Alert, style_Preformatted,
    style_Header, style_BlockQuote
};

/* the combinations a game with no tags can show, most used first */
static const int default_rank[] =
{
    OSSTY_ITALIC, OSSTY_BOLD, OSSTY_BOLD | OSSTY_ITALIC, OSSTY_FIXED,
    OSSTY_CENTER, OSSTY_CENTER | OSSTY_BOLD, OSSTY_CENTER | OSSTY_ITALIC,
    OSSTY_CENTER | OSSTY_BOLD | OSSTY_ITALIC, OSSTY_CENTER | OSSTY_FIXED
};

static void assign(const int *rank, int n)
{
    int i, k, s;

    for (s = 0 ; s < NSTYLES ; ++s)
        style_combo[s] = -1;
    style_combo[style_Normal] = 0;
    style_combo[style_Input] = OSSTY_INPUT;

    /* a combination's own style first... */
    for (i = 0 ; i < n ; ++i)
    {
        s = natural_style(rank[i]);
        if (s >= 0)
            style_combo[s] = rank[i];
    }

    /* ...then whatever is left */
    for (i = 0 ; i < n ; ++i)
    {
        if (natural_style(rank[i]) >= 0)
            continue;
        for (k = 0 ; k < (int)(sizeof(spare_styles) / sizeof(int)) ; ++k)
        {
            s = spare_styles[k];
            if (style_combo[s] < 0)
            {
                style_combo[s] = rank[i];
                break;
            }
        }
    }

    /* the style for each combination, or the nearest one's */
    for (i = 0 ; i < OSSTY_COMBOS ; ++i)
    {
        int c = i, step;

        /* drop bold on fixed or Input text, then italic, then centring */
        for (step = 0 ; step < 5 ; ++step)
        {
            for (s = 0 ; s < NSTYLES ; ++s)
                if (style_combo[s] == c)
                    break;
            if (s < NSTYLES)
                break;
            switch (step)
            {
            case 0:
                if (c & (OSSTY_FIXED | OSSTY_INPUT))
                    c &= ~OSSTY_BOLD;
                break;
            case 1: c &= ~OSSTY_ITALIC; break;
            case 2: c &= ~OSSTY_CENTER; break;
            case 3: c &= ~OSSTY_BOLD; break;
            }
        }
        combo_style[i] = s < NSTYLES ? (glui32)s : style_Normal;
    }
    chosen = 1;
}

void osstyles_choose_counts(const unsigned long *counts)
{
    int rank[OSSTY_COMBOS];
    int n = 0, i, k, total = 0;

    for (i = 0 ; i < OSSTY_COMBOS ; ++i)
        if (counts[i])
            total = 1;
    if (!total)
    {
        assign(default_rank, sizeof(default_rank) / sizeof(int));
        return;
    }

    /*
     *   Plain text and Input have styles of their own.  Bold, italic and
     *   bold italic are always in, as the formatter can make them without
     *   tags, and so is fixed, which a table's marker column uses; but they
     *   only win a style on their count.
     */
    for (i = 0 ; i < OSSTY_COMBOS ; ++i)
        if (i != 0 && i != OSSTY_INPUT
            && (counts[i] || i == OSSTY_BOLD || i == OSSTY_ITALIC
                || i == (OSSTY_BOLD | OSSTY_ITALIC) || i == OSSTY_FIXED))
            rank[n++] = i;

    /* most used first; ties by combination, so the choice is stable */
    for (i = 1 ; i < n ; ++i)
        for (k = i ; k > 0 && (counts[rank[k]] > counts[rank[k - 1]]
                               || (counts[rank[k]] == counts[rank[k - 1]]
                                   && rank[k] < rank[k - 1])) ; --k)
        {
            int t = rank[k];
            rank[k] = rank[k - 1];
            rank[k - 1] = t;
        }

    assign(rank, n < NSTYLES - 2 ? n : NSTYLES - 2);
}

void osstyles_choose(const char *filename)
{
    unsigned long counts[OSSTY_COMBOS];

    if (!osstyles_count(filename, counts, 1))
        memset(counts, 0, sizeof(counts));
    osstyles_choose_counts(counts);
}

/* ------------------------------------------------------------------------ */
/*
 *   Hints
 */

/* the default table, if no game chose its own */
static void choose_default(void)
{
    if (!chosen)
        assign(default_rank, sizeof(default_rank) / sizeof(int));
}

/* the styles whose look we set: every style in use but Normal and Input */
static int hinted(int s)
{
    choose_default();
    return style_combo[s] >= 0 && s != style_Normal && s != style_Input;
}

void osstyles_set_hints(glui32 wintype)
{
    int s, c;

    for (s = 0 ; s < NSTYLES ; ++s)
    {
        if (!hinted(s))
            continue;
        c = style_combo[s];
        glk_stylehint_set(wintype, s, stylehint_Weight,
                          (c & OSSTY_BOLD) ? 1 : 0);
        glk_stylehint_set(wintype, s, stylehint_Oblique,
                          (c & OSSTY_ITALIC) ? 1 : 0);
        glk_stylehint_set(wintype, s, stylehint_Justification,
                          (c & OSSTY_CENTER) ? stylehint_just_Centered
                          : stylehint_just_LeftFlush);
        if (!(c & OSSTY_INPUT))
            glk_stylehint_set(wintype, s, stylehint_Proportional,
                              (c & OSSTY_FIXED) ? 0 : 1);
        glk_stylehint_set(wintype, s, stylehint_Size, 0);
        glk_stylehint_set(wintype, s, stylehint_Indentation, 0);
        glk_stylehint_set(wintype, s, stylehint_ParaIndentation, 0);
    }
}

void osstyles_clear_hints(glui32 wintype)
{
    static const glui32 hints[] =
    {
        stylehint_Weight, stylehint_Oblique, stylehint_Justification,
        stylehint_Proportional, stylehint_Size, stylehint_Indentation,
        stylehint_ParaIndentation
    };
    int s;
    size_t h;

    for (s = 0 ; s < NSTYLES ; ++s)
        if (hinted(s))
            for (h = 0 ; h < sizeof(hints) / sizeof(hints[0]) ; ++h)
                glk_stylehint_clear(wintype, s, hints[h]);
}

glui32 osstyles_style(int combo)
{
    choose_default();
    if (combo < 0 || combo >= OSSTY_COMBOS)
        return style_Normal;
    return combo_style[combo];
}

int osstyles_combo(glui32 style)
{
    choose_default();
    return style < NSTYLES ? style_combo[style] : -1;
}

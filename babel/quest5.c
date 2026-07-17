/* quest5.c  Treaty of Babel module for Quest 5 files
 * 2026 By Petter Sjölund.
 * GPL license.
 *
 * Quest 5 games are either .quest packages (a zip archive whose directory
 * contains game.aslx plus resources) or raw .aslx XML whose root element is
 * <asl version="NNN">.  Inside game.aslx the <game>...</game> element carries
 * the bibliographic data: the name= attribute (title), <gameid> (IFID),
 * <author>, <subtitle>, <category>, <firstpublished>, <description> and a
 * <cover> naming a packaged image.  game.aslx is normally deflated inside the
 * zip, so we inflate it with zlib to read that data and to pull the cover art.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT quest5
#define HOME_PAGE "https://textadventures.co.uk/quest"
#define FORMAT_EXT ".quest,.aslx"

#include "treaty_builder.h"
#include "md5.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* If string is found, returns a pointer to the char AFTER it */
static unsigned char *find_string(unsigned char *storyvp, int32 extent, const char *string, int32 stringlength)
{
    for (int i = 0; i < extent - stringlength - 1; i++)
        if (memcmp(string, storyvp + i, (size_t) stringlength) == 0)
            return storyvp + i + stringlength;
    return NULL;
}

/* A .quest package is a zip whose local file headers name game.aslx.  The
 * entry names are stored in plaintext even when the data is deflated. */
static bool quest_package_found(unsigned char *story_file, int32 extent)
{
    if (story_file == NULL || extent < 30)
        return false;
    if (memcmp(story_file, "PK\003\004", 4) != 0)
        return false;
    return find_string(story_file, extent, "game.aslx", 9) != NULL;
}

/* Raw .aslx: after an optional BOM and whitespace the content must be markup,
 * with an <asl ...> root tag near the top (an <?xml?> declaration and
 * comments may precede it). */
static bool aslx_header_found(unsigned char *story_file, int32 extent)
{
    if (story_file == NULL)
        return false;
    int32 i = 0;
    if (extent >= 3 && story_file[0] == 0xef && story_file[1] == 0xbb &&
        story_file[2] == 0xbf)
        i = 3;
    while (i < extent && isspace(story_file[i]))
        i++;
    if (i >= extent || story_file[i] != '<')
        return false;
    int32 limit = extent < 2048 ? extent : 2048;
    for (; i + 4 < limit; i++)
        if (memcmp(story_file + i, "<asl", 4) == 0 &&
            (story_file[i + 4] == ' ' || story_file[i + 4] == '>' ||
             story_file[i + 4] == '\t' || story_file[i + 4] == '\r' ||
             story_file[i + 4] == '\n'))
            return true;
    return false;
}

static int32 claim_story_file(void *storyvp, int32 extent)
{
    if (extent > 30 && quest_package_found(storyvp, extent))
        return VALID_STORY_FILE_RV;
    if (extent > 10 && aslx_header_found(storyvp, extent))
        return VALID_STORY_FILE_RV;
    return INVALID_STORY_FILE_RV;
}

/* -------------------------------------------------------------------------
 * Minimal zip reader (central directory) + zlib raw inflate.
 * ---------------------------------------------------------------------- */

static uint32_t rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const unsigned char *p)
{
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) |
           ((uint32_t) p[3] << 24);
}

/* Locate an entry by name (exact, then case-insensitive) in a zip and report
 * its deflate method, sizes and the offset of the raw payload. */
static bool zip_find_entry(const unsigned char *zip, int32 extent,
                           const char *name, int32 *off, int32 *comp,
                           int32 *raw, int *method)
{
    if (extent < 22 || memcmp(zip, "PK\003\004", 4) != 0)
        return false;

    /* Find the End Of Central Directory record (PK\05\06), scanning back. */
    const unsigned char *eocd = NULL;
    int32 maxback = extent < 65557 ? extent : 65557;
    for (int32 i = extent - 22; i >= 0 && i + maxback + 22 >= extent; --i)
        if (zip[i] == 'P' && zip[i + 1] == 'K' && zip[i + 2] == 5 &&
            zip[i + 3] == 6) {
            eocd = zip + i;
            break;
        }
    if (!eocd)
        return false;

    int count = (int) rd16(eocd + 10);
    uint32_t cd_off = rd32(eocd + 16);
    const unsigned char *p = zip + cd_off;
    size_t namelen = strlen(name);

    const unsigned char *ci_p = NULL; /* case-insensitive fallback */
    int ci_method = 0;
    int32 ci_comp = 0, ci_raw = 0;
    uint32_t ci_local = 0;

    for (int e = 0; e < count && p + 46 <= zip + extent; ++e) {
        if (!(p[0] == 'P' && p[1] == 'K' && p[2] == 1 && p[3] == 2))
            break;
        int m = (int) rd16(p + 10);
        int32 c = (int32) rd32(p + 20);
        int32 r = (int32) rd32(p + 24);
        int nl = (int) rd16(p + 28);
        int el = (int) rd16(p + 30);
        int cl = (int) rd16(p + 32);
        uint32_t local = rd32(p + 42);
        const unsigned char *ename = p + 46;

        bool exact = (size_t) nl == namelen &&
                     memcmp(ename, name, namelen) == 0;
        bool ci = false;
        if (!exact && (size_t) nl == namelen) {
            ci = true;
            for (size_t k = 0; k < namelen; k++)
                if (tolower(ename[k]) != tolower((unsigned char) name[k])) {
                    ci = false;
                    break;
                }
        }

        if (exact || ci) {
            /* The local header repeats name/extra with a possibly different
             * extra length, so the payload offset comes from it. */
            const unsigned char *lh = zip + local;
            if (lh + 30 <= zip + extent && lh[0] == 'P' && lh[1] == 'K' &&
                lh[2] == 3 && lh[3] == 4) {
                int32 payload = (int32) (lh - zip) + 30 +
                                (int32) rd16(lh + 26) + (int32) rd16(lh + 28);
                if (payload + c <= extent) {
                    if (exact) {
                        *off = payload;
                        *comp = c;
                        *raw = r;
                        *method = m;
                        return true;
                    }
                    if (!ci_p) {
                        ci_p = lh;
                        ci_method = m;
                        ci_comp = c;
                        ci_raw = r;
                        ci_local = (uint32_t) payload;
                    }
                }
            }
        }
        p += 46 + nl + el + cl;
    }

    if (ci_p) {
        *off = (int32) ci_local;
        *comp = ci_comp;
        *raw = ci_raw;
        *method = ci_method;
        return true;
    }
    return false;
}

/* Inflate a raw DEFLATE payload; caller frees the returned buffer. */
static unsigned char *inflate_raw(const unsigned char *src, int32 srclen,
                                  int32 rawlen)
{
    if (rawlen <= 0)
        return NULL;
    unsigned char *out = malloc((size_t) rawlen);
    if (!out)
        return NULL;
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
        free(out);
        return NULL;
    }
    zs.next_in = (Bytef *) src;
    zs.avail_in = (uInt) srclen;
    zs.next_out = (Bytef *) out;
    zs.avail_out = (uInt) rawlen;
    int r = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (r == Z_STREAM_END || (r == Z_OK && zs.avail_out == 0))
        return out;
    free(out);
    return NULL;
}

/* Extract a named zip entry; caller frees.  *outlen gets the byte count. */
static unsigned char *zip_extract(const unsigned char *zip, int32 extent,
                                  const char *name, int32 *outlen)
{
    int32 off, comp, raw;
    int method;
    if (!zip_find_entry(zip, extent, name, &off, &comp, &raw, &method))
        return NULL;
    if (method == 0) { /* stored */
        unsigned char *out = malloc((size_t) raw + 1);
        if (!out)
            return NULL;
        memcpy(out, zip + off, (size_t) raw);
        *outlen = raw;
        return out;
    }
    if (method == 8) { /* deflate */
        unsigned char *out = inflate_raw(zip + off, comp, raw);
        if (out)
            *outlen = raw;
        return out;
    }
    return NULL;
}

/* Return game.aslx as a malloc'd, NUL-terminated string (caller frees), from a
 * package or a raw .aslx file. */
static char *get_aslx_text(void *storyvp, int32 extent, int32 *len)
{
    unsigned char *story = storyvp;
    if (extent >= 4 && memcmp(story, "PK\003\004", 4) == 0) {
        int32 l = 0;
        unsigned char *raw = zip_extract(story, extent, "game.aslx", &l);
        if (!raw)
            return NULL;
        char *s = realloc(raw, (size_t) l + 1);
        if (!s) {
            free(raw);
            return NULL;
        }
        s[l] = '\0';
        *len = l;
        return s;
    }
    /* raw .aslx: copy and NUL-terminate */
    char *s = malloc((size_t) extent + 1);
    if (!s)
        return NULL;
    memcpy(s, story, (size_t) extent);
    s[extent] = '\0';
    *len = extent;
    return s;
}

/* -------------------------------------------------------------------------
 * Metadata extraction from game.aslx.
 * ---------------------------------------------------------------------- */

/* Find the <game ...>...</game> range in the aslx text. */
static bool find_game_range(const char *text, const char **gs, const char **ge)
{
    const char *p = text;
    while ((p = strstr(p, "<game")) != NULL) {
        char after = p[5];
        if (after == ' ' || after == '\t' || after == '\r' || after == '\n' ||
            after == '>') {
            const char *end = strstr(p, "</game>");
            if (!end)
                return false;
            *gs = p;
            *ge = end;
            return true;
        }
        p += 5;
    }
    return false;
}

/* Copy the value of attribute `attr` from an opening tag (up to the first '>')
 * into a malloc'd string, or NULL. */
static char *get_attr(const char *tag, const char *tagend, const char *attr)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "%s=\"", attr);
    const char *p = tag;
    size_t patlen = strlen(pat);
    while (p < tagend) {
        const char *hit = strstr(p, pat);
        if (!hit || hit >= tagend)
            return NULL;
        /* require a preceding space so name="" doesn't match e.g. surname="" */
        if (hit == tag || isspace((unsigned char) hit[-1])) {
            const char *v = hit + patlen;
            const char *q = strchr(v, '"');
            if (!q)
                return NULL;
            size_t n = (size_t) (q - v);
            char *out = malloc(n + 1);
            if (!out)
                return NULL;
            memcpy(out, v, n);
            out[n] = '\0';
            return out;
        }
        p = hit + patlen;
    }
    return NULL;
}

/* Return the inner text of the first <tag>...</tag> child within [lo,hi) as a
 * malloc'd string (CDATA unwrapped), or NULL. */
static char *get_child(const char *lo, const char *hi, const char *tag)
{
    char open[64];
    snprintf(open, sizeof(open), "<%s", tag);
    size_t openlen = strlen(open);
    const char *p = lo;
    while (p < hi) {
        const char *hit = strstr(p, open);
        if (!hit || hit >= hi)
            return NULL;
        char after = hit[openlen];
        if (after == ' ' || after == '\t' || after == '\r' || after == '\n' ||
            after == '>' || after == '/') {
            const char *gt = strchr(hit, '>');
            if (!gt || gt >= hi)
                return NULL;
            if (gt[-1] == '/') /* self-closing, no text */
                return NULL;
            char close[64];
            snprintf(close, sizeof(close), "</%s>", tag);
            const char *end = strstr(gt + 1, close);
            if (!end || end > hi)
                return NULL;
            const char *v = gt + 1;
            size_t n = (size_t) (end - v);
            /* unwrap a single CDATA section */
            if (n >= 12 && memcmp(v, "<![CDATA[", 9) == 0) {
                const char *cend = strstr(v, "]]>");
                if (cend && cend < end) {
                    v += 9;
                    n = (size_t) (cend - v);
                }
            }
            char *out = malloc(n + 1);
            if (!out)
                return NULL;
            memcpy(out, v, n);
            out[n] = '\0';
            return out;
        }
        p = hit + openlen;
    }
    return NULL;
}

/* Decode HTML/XML entities in place. */
static void decode_entities(char *s)
{
    char *w = s;
    const char *r = s;
    while (*r) {
        if (*r == '&') {
            const char *sc = strchr(r, ';');
            if (sc && sc - r <= 10) {
                size_t n = (size_t) (sc - r - 1);
                char ent[11];
                memcpy(ent, r + 1, n);
                ent[n] = '\0';
                int code = -1;
                if (ent[0] == '#') {
                    code = (ent[1] == 'x' || ent[1] == 'X')
                               ? (int) strtol(ent + 2, NULL, 16)
                               : (int) strtol(ent + 1, NULL, 10);
                } else if (strcmp(ent, "lt") == 0) code = '<';
                else if (strcmp(ent, "gt") == 0) code = '>';
                else if (strcmp(ent, "amp") == 0) code = '&';
                else if (strcmp(ent, "quot") == 0) code = '"';
                else if (strcmp(ent, "apos") == 0) code = '\'';
                else if (strcmp(ent, "nbsp") == 0) code = ' ';
                if (code > 0) {
                    if (code < 0x80) {
                        *w++ = (char) code;
                    } else if (code < 0x800) {
                        *w++ = (char) (0xC0 | (code >> 6));
                        *w++ = (char) (0x80 | (code & 0x3F));
                    } else {
                        *w++ = (char) (0xE0 | (code >> 12));
                        *w++ = (char) (0x80 | ((code >> 6) & 0x3F));
                        *w++ = (char) (0x80 | (code & 0x3F));
                    }
                    r = sc + 1;
                    continue;
                }
            }
        }
        *w++ = *r++;
    }
    *w = '\0';
}

/* Strip HTML tags in place; <br> variants become '\n', other tags dropped. */
static void strip_tags(char *s)
{
    char *w = s;
    const char *r = s;
    while (*r) {
        if (*r == '<') {
            const char *gt = strchr(r, '>');
            if (!gt) /* not a well-formed tag; keep the literal '<' */
                break;
            if ((r[1] == 'b' || r[1] == 'B') && (r[2] == 'r' || r[2] == 'R'))
                *w++ = '\n';
            r = gt + 1;
            continue;
        }
        *w++ = *r++;
    }
    while (*r)
        *w++ = *r++;
    *w = '\0';
}

/* Quest treats these fields as HTML.  Decode entities first (so a
 * double-encoded tag like &lt;b&gt; becomes a real tag) then strip tags.
 * When collapse is true, runs of whitespace fold to a single space and the
 * result is trimmed. */
static void clean_html(char *s, bool collapse)
{
    decode_entities(s);
    strip_tags(s);

    if (collapse) {
        /* fold whitespace to single spaces, trim ends */
        char *out = s;
        const char *in = s;
        while (isspace((unsigned char) *in))
            in++;
        int pending = 0;
        while (*in) {
            if (isspace((unsigned char) *in)) {
                pending = 1;
                in++;
            } else {
                if (pending) {
                    *out++ = ' ';
                    pending = 0;
                }
                *out++ = *in++;
            }
        }
        *out = '\0';
    }
}

/* -------------------------------------------------------------------------
 * iFiction synthesis.
 * ---------------------------------------------------------------------- */

typedef struct {
    char *buf;
    int32 cap;
    int32 total;
} sink;

static void put(sink *s, const char *src, size_t n)
{
    int32 room = s->cap - s->total;
    if (room > 0 && s->buf) {
        int32 copy = (int32) n < room ? (int32) n : room;
        memcpy(s->buf + s->total, src, (size_t) copy);
    }
    s->total += (int32) n;
}
static void putz(sink *s, const char *src) { put(s, src, strlen(src)); }

/* Emit XML-escaped PCDATA; when as_desc is true, newlines become <br/>. */
static void put_escaped(sink *s, const char *src, bool as_desc)
{
    for (const char *p = src; *p; p++) {
        switch (*p) {
        case '&': putz(s, "&amp;"); break;
        case '<': putz(s, "&lt;"); break;
        case '>': putz(s, "&gt;"); break;
        case '\n':
            if (as_desc) putz(s, "<br/>");
            else put(s, " ", 1);
            break;
        case '\r': break;
        default: put(s, p, 1); break;
        }
    }
}

static void put_field(sink *s, const char *tag, const char *val, bool as_desc)
{
    if (!val || !*val)
        return;
    putz(s, "      <");
    putz(s, tag);
    putz(s, ">");
    put_escaped(s, val, as_desc);
    putz(s, "</");
    putz(s, tag);
    putz(s, ">\n");
}

/* Detect cover format from magic bytes: 1 = PNG, 2 = JPEG, 0 = neither. */
static int cover_format_of(const unsigned char *d, int32 n)
{
    if (n >= 8 && d[0] == 137 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G')
        return PNG_COVER_FORMAT;
    if (n >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF)
        return JPEG_COVER_FORMAT;
    return 0;
}

static void png_dim(const unsigned char *d, int32 *w, int32 *h)
{
    *w = (int32) ((d[16] << 24) | (d[17] << 16) | (d[18] << 8) | d[19]);
    *h = (int32) ((d[20] << 24) | (d[21] << 16) | (d[22] << 8) | d[23]);
}

static bool jpeg_dim(const unsigned char *d, int32 n, int32 *w, int32 *h)
{
    const unsigned char *p = d + 2, *end = d + n;
    while (p + 9 < end) {
        if (*p != 0xFF) {
            p++;
            continue;
        }
        int marker = p[1];
        if ((marker & 0xF0) == 0xC0 && marker != 0xC4 && marker != 0xC8 &&
            marker != 0xCC) {
            *h = (p[5] << 8) | p[6];
            *w = (p[7] << 8) | p[8];
            return true;
        }
        if (p[1] == 0xD8 || p[1] == 0xD9 ||
            (p[1] >= 0xD0 && p[1] <= 0xD7)) {
            p += 2;
            continue;
        }
        int seg = (p[2] << 8) | p[3];
        p += 2 + seg;
    }
    return false;
}

/* Pull the cover image out of a package; caller frees.  Sets fmt and len. */
static unsigned char *get_cover(void *storyvp, int32 extent, int32 *len,
                                int *fmt)
{
    unsigned char *story = storyvp;
    if (!(extent >= 4 && memcmp(story, "PK\003\004", 4) == 0))
        return NULL; /* raw .aslx keeps resources as external files */

    int32 atext_len = 0;
    char *atext = get_aslx_text(storyvp, extent, &atext_len);
    if (!atext)
        return NULL;
    const char *gs, *ge;
    char *name = NULL;
    if (find_game_range(atext, &gs, &ge))
        name = get_child(gs, ge, "cover");
    free(atext);
    if (!name || !*name) {
        free(name);
        return NULL;
    }

    int32 clen = 0;
    unsigned char *img = zip_extract(story, extent, name, &clen);
    free(name);
    if (!img)
        return NULL;
    int f = cover_format_of(img, clen);
    if (f == 0) {
        free(img);
        return NULL;
    }
    *fmt = f;
    *len = clen;
    return img;
}

/* Build the iFiction record into buf (NULL to just measure); returns the
 * exact byte length that a full record needs. */
static int32 synth_ifiction(void *storyvp, int32 extent, char *buf,
                            int32 bufsize)
{
    int32 atext_len = 0;
    char *atext = get_aslx_text(storyvp, extent, &atext_len);
    if (!atext)
        return NO_REPLY_RV;

    const char *gs, *ge;
    if (!find_game_range(atext, &gs, &ge)) {
        free(atext);
        return NO_REPLY_RV;
    }
    const char *tagend = strchr(gs, '>');
    if (!tagend)
        tagend = ge;

    char *title = get_attr(gs, tagend, "name");
    char *gameid = get_child(gs, ge, "gameid");
    char *author = get_child(gs, ge, "author");
    char *subtitle = get_child(gs, ge, "subtitle");
    char *genre = get_child(gs, ge, "category");
    char *published = get_child(gs, ge, "firstpublished");
    char *desc = get_child(gs, ge, "description");

    if (title) clean_html(title, true);
    if (author) clean_html(author, true);
    if (subtitle) clean_html(subtitle, true);
    if (genre) clean_html(genre, true);
    if (gameid) clean_html(gameid, true);
    if (published) clean_html(published, true);
    if (desc) clean_html(desc, false);

    /* IFID: the game's gameid, else the MD5 of the whole story file (matching
     * babel_handler's fallback). */
    char ifid[40];
    if (gameid && strlen(gameid) >= 8) {
        size_t n = strlen(gameid);
        if (n > 38) n = 38;
        memcpy(ifid, gameid, n);
        ifid[n] = '\0';
        for (char *p = ifid; *p; p++)
            *p = (char) toupper((unsigned char) *p);
    } else {
        md5_state_t md5;
        unsigned char ob[16];
        md5_init(&md5);
        md5_append(&md5, storyvp, extent);
        md5_finish(&md5, ob);
        for (int i = 0; i < 16; i++)
            sprintf(ifid + 2 * i, "%02X", ob[i]);
        ifid[32] = '\0';
    }

    /* Cover art, if any (for the <cover> dimensions block). */
    int32 cover_len = 0, cw = 0, ch = 0;
    int cover_fmt = 0;
    unsigned char *cover = get_cover(storyvp, extent, &cover_len, &cover_fmt);
    if (cover) {
        if (cover_fmt == PNG_COVER_FORMAT && cover_len >= 24)
            png_dim(cover, &cw, &ch);
        else if (cover_fmt == JPEG_COVER_FORMAT)
            jpeg_dim(cover, cover_len, &cw, &ch);
    }

    sink s;
    s.buf = buf;
    s.cap = bufsize;
    s.total = 0;

    putz(&s,
         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<ifindex version=\"1.0\" "
         "xmlns=\"http://babel.ifarchive.org/protocol/iFiction/\">\n"
         "  <story>\n"
         "    <identification>\n"
         "      <ifid>");
    putz(&s, ifid);
    putz(&s, "</ifid>\n"
             "      <format>quest5</format>\n"
             "    </identification>\n"
             "    <bibliographic>\n");
    put_field(&s, "title", title ? title : "An Interactive Fiction", false);
    put_field(&s, "author", author ? author : "Anonymous", false);
    put_field(&s, "headline", subtitle, false);
    put_field(&s, "genre", genre, false);
    put_field(&s, "firstpublished", published, false);
    put_field(&s, "description", desc, true);
    putz(&s, "    </bibliographic>\n");

    if (cover && (cover_fmt == PNG_COVER_FORMAT ||
                  cover_fmt == JPEG_COVER_FORMAT) && cw > 0 && ch > 0) {
        char b[160];
        snprintf(b, sizeof(b),
                 "    <cover>\n"
                 "      <format>%s</format>\n"
                 "      <height>%ld</height>\n"
                 "      <width>%ld</width>\n"
                 "    </cover>\n",
                 cover_fmt == PNG_COVER_FORMAT ? "png" : "jpg", (long) ch,
                 (long) cw);
        putz(&s, b);
    }

    putz(&s, "  </story>\n</ifindex>\n");

    free(cover);
    free(title);
    free(gameid);
    free(author);
    free(subtitle);
    free(genre);
    free(published);
    free(desc);
    free(atext);

    return s.total;
}

/* -------------------------------------------------------------------------
 * Treaty entry points.
 * ---------------------------------------------------------------------- */

static int32 get_story_file_metadata_extent(void *story_file, int32 extent)
{
    int32 need = synth_ifiction(story_file, extent, NULL, 0);
    if (need < 0)
        return need;
    /* Reserve one byte for the terminating NUL: babel's consumers treat the
     * returned buffer as a C string (strstr/fputs). */
    return need + 1;
}

static int32 get_story_file_metadata(void *story_file, int32 extent,
                                     char *output, int32 output_extent)
{
    int32 need = synth_ifiction(story_file, extent, output, output_extent);
    if (need < 0)
        return need;
    if (need + 1 > output_extent)
        return INVALID_USAGE_RV;
    output[need] = '\0';
    return need + 1;
}

static int32 get_story_file_cover_extent(void *story_file, int32 extent)
{
    int32 len = 0;
    int fmt = 0;
    unsigned char *c = get_cover(story_file, extent, &len, &fmt);
    if (!c)
        return NO_REPLY_RV;
    free(c);
    return len;
}

static int32 get_story_file_cover_format(void *story_file, int32 extent)
{
    int32 len = 0;
    int fmt = 0;
    unsigned char *c = get_cover(story_file, extent, &len, &fmt);
    if (!c)
        return NO_REPLY_RV;
    free(c);
    return fmt;
}

static int32 get_story_file_cover(void *story_file, int32 extent, void *output,
                                  int32 output_extent)
{
    int32 len = 0;
    int fmt = 0;
    unsigned char *c = get_cover(story_file, extent, &len, &fmt);
    if (!c)
        return NO_REPLY_RV;
    if (len > output_extent) {
        free(c);
        return INVALID_USAGE_RV;
    }
    memcpy(output, c, (size_t) len);
    free(c);
    return len;
}

static int32 get_story_file_IFID(void *storyvp, int32 extent, char *output, int32 output_extent)
{
    if (claim_story_file(storyvp, extent) != VALID_STORY_FILE_RV)
        return INVALID_STORY_FILE_RV;

    /* The <gameid> GUID is the real IFID.  Raw .aslx keeps it in plaintext; a
     * package needs game.aslx inflated first. */
    char *atext = NULL;
    unsigned char *search = storyvp;
    int32 search_len = extent;
    int32 atext_len = 0;
    if (memcmp(storyvp, "PK\003\004", 4) == 0) {
        atext = get_aslx_text(storyvp, extent, &atext_len);
        if (atext) {
            search = (unsigned char *) atext;
            search_len = atext_len;
        }
    }

    int32 rv = INCOMPLETE_REPLY_RV;
    unsigned char *p = find_string(search, search_len, "<gameid>", 8);
    if (p != NULL) {
        char guid[64];
        int n = 0;
        int32 left = search_len - (int32) (p - search);
        while (n < 63 && n < left && p[n] != '<' &&
               (isxdigit(p[n]) || p[n] == '-')) {
            guid[n] = (char) toupper(p[n]);
            n++;
        }
        guid[n] = 0;
        if (n == 36 && (n < left && p[n] == '<')) {
            ASSERT_OUTPUT_SIZE(n + 1);
            memcpy(output, guid, (size_t) (n + 1));
            free(atext);
            return 1;
        }
    }

    free(atext);
    ASSERT_OUTPUT_SIZE(1);
    output[0] = '\0';
    return rv;
}

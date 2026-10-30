/* oshtml_banner.c -- a TADS 2 <BANNER>'s text, laid out when its
   </BANNER> comes: as pictures in pixels where its tables hold only
   pictures, as a line of cells over them where that fits, or else as
   text fed to the banner's own parser */

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
    int row, col, colspan, rowspan;
    glui32 minw;                /* its WIDTH, in pixels */
    int aligned;                /* its ALIGN, or a picture's, was set */
    int align;                  /* 0 left, 1 centred, 2 right */
    int valign;                 /* 0 top, 1 middle, 2 bottom */
    glui32 minh;                /* its HEIGHT */
    int nlines;
    int first[PT_LINES + 1];    /* the first tile of each line */
    glui32 linew[PT_LINES], lineh[PT_LINES];
    glui32 w, h;
} pt_cell;

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
    int until[PT_CELLS];        /* the row a column is taken up to */
    glui32 rowy[PT_CELLS + 1];
    int ncells = 0, row = -1, col = 0, ncols = 0, nrows, links = 0;
    int space = 0, row_valign = 1, depth = 0, k, n;
    glui32 link = 0, pad = 1, spacing = 2, border = 0;
    size_t i = 0;

    memset(until, 0, sizeof(until));
    while (i < len)
    {
        size_t ts, te, name, namelen, v, vlen;
        int end;

        if (t[i] != '<')
        {
            /* a &nbsp; holds an empty cell open, and adds nothing */
            if (t[i] == '&' && i + 6 <= len && t[i + 1] == 'n'
                && t[i + 2] == 'b' && t[i + 3] == 's' && t[i + 4] == 'p'
                && t[i + 5] == ';')
            {
                i += 6;
                continue;
            }
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
            /* columns an earlier row's cell takes up are skipped */
            while (col < PT_CELLS && until[col] > row)
                ++col;
            c->col = col;
            c->colspan = raw_int(t, ts, te, "colspan", 1);
            if (c->colspan < 1)
                c->colspan = 1;
            c->rowspan = raw_int(t, ts, te, "rowspan", 1);
            if (c->rowspan < 1)
                c->rowspan = 1;
            col += c->colspan;
            if (col > PT_CELLS)
                return 0;
            for (k = c->col ; k < col ; ++k)
                until[k] = row + c->rowspan;
            if (col > ncols)
                ncols = col;
            if (raw_attr(t, ts, te, "width", &v, &vlen)
                && !(vlen && t[v + vlen - 1] == '%'))
                c->minw = (glui32)raw_int(t, ts, te, "width", 0);
            if (raw_attr(t, ts, te, "align", &v, &vlen))
            {
                c->align = RAWV("center") ? 1 : RAWV("right") ? 2 : 0;
                c->aligned = 1;
            }
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
        else if (RAW("center"))
        {
            /* a cell's picture in <CENTER> is centred in it */
            if (c && !end && !c->aligned)
            {
                c->align = 1;
                c->aligned = 1;
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

            /* not a map */
            if (!c || pt->ntiles == OSHTML_TILES_MAX
                || raw_attr(t, ts, te, "usemap", &sv, &svlen)
                || raw_attr(t, ts, te, "ismap", &sv, &svlen)
                || !raw_attr(t, ts, te, "src", &v, &vlen))
                return 0;
            if (raw_attr(t, ts, te, "width", &sv, &svlen))
                parse_img_size(t + sv, svlen, &w);
            if (raw_attr(t, ts, te, "height", &sv, &svlen))
                parse_img_size(t + sv, svlen, &h);
            if (!c->aligned && raw_attr(t, ts, te, "align", &sv, &svlen)
                && (tag_is(t + sv, svlen, "left")
                    || tag_is(t + sv, svlen, "right")))
            {
                /* in a cell, a picture goes to the side it names */
                c->align = tag_is(t + sv, svlen, "right") ? 2 : 0;
                c->aligned = 1;
            }
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
        if (c->row + c->rowspan > nrows)
            c->rowspan = nrows - c->row;
        if (c->w < c->minw)
            c->w = c->minw;
        if (c->colspan == 1 && c->w + 2 * pad > colw[c->col])
            colw[c->col] = c->w + 2 * pad;
        if (c->rowspan > 1)
            continue;
        if (c->h + 2 * pad > rowh[c->row])
            rowh[c->row] = c->h + 2 * pad;
        if (c->minh > rowh[c->row])
            rowh[c->row] = c->minh;
    }
    /* a cell over rows makes them taller, if they aren't enough */
    for (n = 0 ; n < ncells ; ++n)
    {
        glui32 have = 0, need;

        c = &cells[n];
        if (c->rowspan == 1)
            continue;
        for (k = c->row ; k < c->row + c->rowspan ; ++k)
            have += rowh[k];
        have += spacing * (glui32)(c->rowspan - 1);
        need = c->h + 2 * pad;
        if (c->minh > need)
            need = c->minh;
        if (need > have)
            for (k = c->row ; k < c->row + c->rowspan ; ++k)
                rowh[k] += (need - have + (glui32)c->rowspan - 1)
                    / (glui32)c->rowspan;
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
        ch = rowy[c->row + c->rowspan] - spacing - rowy[c->row];
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
 *   Lay out the <IMG USEMAP> in t[0..len) as a picture whose areas are its
 *   links: false if an area isn't a rectangle, or the picture floats
 */
static int build_mappic(parser *p, const glui32 *t, size_t len,
                        oshtml_pictable *pt)
{
    size_t i = 0, ts, te, v, vlen;
    img_size w = { 0, 0 }, h = { 0, 0 };
    const imagemap *m;
    oshtml_image img;

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
    if (w.pct || h.pct || !raw_attr(t, ts, te, "src", &v, &vlen)
        || !find_image(p, t + v, vlen, imagealign_InlineUp, w, h, &img))
        return 0;
    return layout_mappic(p, m, &img, pt);
}

/*
 *   Lay out the picture 'img' with the areas of 'm' on it as its links:
 *   false if an area isn't a rectangle, or the picture has no size
 */
int layout_mappic(parser *p, const imagemap *m, const oshtml_image *img,
                  oshtml_pictable *pt)
{
    oshtml_tile *tile = &pt->tiles[0];
    int k;

    if (m->unplaced || m->nrects == 0 || m->nrects >= OSHTML_TILES_MAX)
        return 0;
    memset(tile, 0, sizeof(*tile));
    tile->img = *img;
    if (!oshtml_image_size(&tile->img, 0, &tile->w, &tile->h)
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
    pt->above = 0;
    return pt->ntiles > 1;
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
    size_t i = 0, ts, te, name, namelen;
    int end, depth;

    if (p->dst)
        return 0;
    pt->ntiles = 0;
    pt->width = 0;
    pt->page = 0;
    pt->above = 0;

    /* a picture with areas, as the first of them: its areas are links */
    while (i < len && is_space(t[i]))
        ++i;
    if (i < len && t[i] == '<' && raw_tag(t, len, &i, &ts, &te)
        && (namelen = raw_name(t, ts, te, &name, &end),
            !end && tag_is(t + name, namelen, "img")))
    {
        if (!build_mappic(p, t, i, pt))
            return 0;
        first[0] = 0;
        tw[0] = pt->width;
        th[0] = pt->height;
        ntables = 1;
        links = 1;
        wide = 1;
    }
    else
        i = 0;
    while (i < len)
    {
        size_t start = i;

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
    if (pt->ntiles < 1 || (pt->ntiles < 2 && links) || !wide)
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
 *   Lay out the <IMG> in t[0..len), with no map, as the table's only tile:
 *   false if it has no known size or floats
 */
static int build_cellpic(parser *p, const glui32 *t, size_t len,
                         oshtml_pictable *pt)
{
    size_t i = 0, ts, te, v, vlen;
    img_size w = { 0, 0 }, h = { 0, 0 };
    oshtml_tile *tile = &pt->tiles[0];

    if (p->dst || !raw_tag(t, len, &i, &ts, &te)
        || raw_attr(t, ts, te, "usemap", &v, &vlen)
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
    pt->ntiles = 1;
    pt->width = tile->w;
    pt->height = tile->h;
    pt->page = 0;
    pt->above = 0;
    return 1;
}

/*
 *   Is the table in t[0..len) one cell that starts with an <IMG USEMAP>, or
 *   a plain <IMG> in a centred cell of a full-height table (a location card)?
 *   Lay that out as build_mappic does, to go over the rest of the cell,
 *   which is t[*s..*e).  False if anything but spaces is outside the cell,
 *   or the rest has a table or picture of its own.
 */
static int build_mapcell(parser *p, const glui32 *t, size_t len,
                         oshtml_pictable *pt, size_t *s, size_t *e,
                         int *centered)
{
    static const char *const outer[] = { "table", "tr", "td", NULL };
    size_t i = 0, ts, te, name, namelen, v, vlen, img;
    int end, k, valign = 1, full = 0;

    *centered = 0;

    /* <TABLE>, <TR>, <TD>, the picture */
    for (k = 0 ; outer[k] ; ++k)
    {
        while (i < len && is_space(t[i]))
            ++i;
        if (i == len || t[i] != '<' || !raw_tag(t, len, &i, &ts, &te))
            return 0;
        namelen = raw_name(t, ts, te, &name, &end);
        if (end || !tag_is(t + name, namelen, outer[k]))
            return 0;
        if (k == 0 && raw_attr(t, ts, te, "height", &v, &vlen))
            full = tag_is(t + v, vlen, "100%");
        if (k > 0 && raw_attr(t, ts, te, "valign", &v, &vlen))
            valign = tag_is(t + v, vlen, "top") ? 0
                : tag_is(t + v, vlen, "bottom") ? 2 : 1;
        if (k == 2 && raw_attr(t, ts, te, "align", &v, &vlen))
            *centered = tag_is(t + v, vlen, "center");
    }
    while (i < len && is_space(t[i]))
        ++i;
    img = i;
    if (i == len || t[i] != '<' || !raw_tag(t, len, &i, &ts, &te)
        || !(raw_attr(t, ts, te, "usemap", &v, &vlen)
             ? build_mappic(p, t + img, i - img, pt)
             : full && *centered && build_cellpic(p, t + img, i - img, pt)))
        return 0;
    if (*centered)
        pt->center = 1;

    /* the rest of the cell, up to its end, the row's or the table's */
    *s = i;
    for (;;)
    {
        size_t start = i;

        if (i == len)
            return 0;
        if (t[i] != '<')
        {
            ++i;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            return 0;
        namelen = raw_name(t, ts, te, &name, &end);
        if (tag_is(t + name, namelen, "td") || tag_is(t + name, namelen, "tr")
            || tag_is(t + name, namelen, "table"))
        {
            if (!end)
                return 0;
            *e = start;
            break;
        }
        if (tag_is(t + name, namelen, "img"))
            return 0;
    }

    /* and nothing but those ends after it */
    while (i < len)
    {
        if (t[i] != '<')
        {
            if (!is_space(t[i++]))
                return 0;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            return 0;
        namelen = raw_name(t, ts, te, &name, &end);
        if (!end || !(tag_is(t + name, namelen, "td")
                      || tag_is(t + name, namelen, "tr")
                      || tag_is(t + name, namelen, "table")))
            return 0;
    }
    pt->above = 1;
    pt->valign = valign;
    return 1;
}

/*
 *   Lay out the page t[0..len) in pixels as the banner's text would flow:
 *   each line of pictures centred, under the blank lines before it.  False
 *   if a picture isn't centred, floats or can't be shown.
 */
int build_picpage(parser *p, const glui32 *t, size_t len,
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
    pt->above = 0;
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

/*
 *   Give the picture table the <BODY BACKGROUND> picture that t[0..len)
 *   sets, to tile behind it
 */
static void pt_body_background(parser *p, const glui32 *t, size_t len,
                               oshtml_pictable *pt)
{
    img_size none = { 0, 0 };
    size_t i = 0, ts, te, name, namelen, v, vlen;
    int end;

    memset(&pt->bg, 0, sizeof(pt->bg));
    while (i < len)
    {
        if (t[i] != '<')
        {
            ++i;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            return;
        namelen = raw_name(t, ts, te, &name, &end);
        if (!end && tag_is(t + name, namelen, "body")
            && raw_attr(t, ts, te, "background", &v, &vlen) && vlen)
        {
            find_image(p, t + v, vlen, imagealign_InlineUp, none, none,
                       &pt->bg);
            return;
        }
    }
}

/*
 *   Lay out the table t[0..len) as a line of cells of text over a strip of
 *   pictures, as Threnody's status line: its first row is text, its first
 *   column the title, which takes the room the other columns leave, as the
 *   table is the window's width; under the others, rows of pictures.  Each
 *   text cell's text is t[s[n]..e[n]).  False if the table is anything
 *   else, so that it goes as a table always does.
 */
static int build_celltable(parser *p, const glui32 *t, size_t len,
                           oshtml_celltable *ct, size_t *s, size_t *e)
{
    oshtml_pictable *pt = &ct->pics;
    glui32 colw[OSHTML_CELLS_MAX], colx[OSHTML_CELLS_MAX + 1];
    glui32 rowh[OSHTML_CELLS_MAX], rowy[OSHTML_CELLS_MAX + 1];
    int tilerow[OSHTML_TILES_MAX], tilecol[OSHTML_TILES_MAX];
    int cellcol[OSHTML_CELLS_MAX], colalign[OSHTML_CELLS_MAX];
    int until[OSHTML_CELLS_MAX];
    int row = -1, col = 0, ncols = 0, nrows, depth = 0, intext = -1;
    int incell = 0, k, j, n;
    glui32 pad = 1, spacing = 2, border = 0, line;
    size_t i = 0;

    if (p->ban.align == OS_BANNER_ALIGN_LEFT
        || p->ban.align == OS_BANNER_ALIGN_RIGHT)
        return 0;
    memset(ct, 0, sizeof(*ct));
    memset(colw, 0, sizeof(colw));
    memset(rowh, 0, sizeof(rowh));
    memset(until, 0, sizeof(until));

    while (i < len)
    {
        size_t start = i, ts, te, name, namelen, v, vlen;
        int end;

        if (t[i] != '<')
        {
            /* text is only in the first row's cells */
            if (!is_space(t[i]) && (!incell || row != 0))
                return 0;
            ++i;
            continue;
        }
        if (!raw_tag(t, len, &i, &ts, &te))
            return 0;
        namelen = raw_name(t, ts, te, &name, &end);

#define RAW(s) tag_is(t + name, namelen, s)
#define RAWV(s) tag_is(t + v, vlen, s)
        /* a cell of text ends at the next cell, row or the table's end */
        if (intext >= 0 && (RAW("td") || RAW("tr") || RAW("table")))
        {
            e[intext] = start;
            intext = -1;
        }
        if (RAW("table"))
        {
            if (end)
                --depth;
            else if (depth++ != 0)
                return 0;
            else
            {
                if (!raw_attr(t, ts, te, "width", &v, &vlen)
                    || !RAWV("100%"))
                    return 0;
                pad = (glui32)raw_int(t, ts, te, "cellpadding", 1);
                spacing = (glui32)raw_int(t, ts, te, "cellspacing", 2);
                border = (glui32)raw_int(t, ts, te, "border", 0);
            }
        }
        else if (RAW("tr"))
        {
            incell = 0;
            if (!end)
            {
                if (++row == OSHTML_CELLS_MAX)
                    return 0;
                col = 0;
            }
        }
        else if (RAW("td"))
        {
            int rowspan, align = -1;
            glui32 minw = 0, minh;

            incell = 0;
            if (end)
                continue;
            if (row < 0)
                return 0;
            while (col < OSHTML_CELLS_MAX && until[col] > row)
                ++col;
            if (col == OSHTML_CELLS_MAX
                || raw_int(t, ts, te, "colspan", 1) > 1)
                return 0;
            rowspan = raw_int(t, ts, te, "rowspan", 1);
            if (rowspan < 1)
                rowspan = 1;

            /* only the title goes down past its row */
            if (rowspan > 1 && (row != 0 || col != 0))
                return 0;
            until[col] = row + rowspan;
            if (raw_attr(t, ts, te, "width", &v, &vlen))
            {
                if (vlen && t[v + vlen - 1] == '%')
                    return 0;
                minw = (glui32)to_int(t + v, vlen);
            }
            if (raw_attr(t, ts, te, "align", &v, &vlen))
                align = RAWV("center") ? 1 : RAWV("right") ? 2 : 0;
            minh = (glui32)raw_int(t, ts, te, "height", 0);

            if (minw > colw[col])
                colw[col] = minw;
            if (row == 0)
            {
                /* a cell of text, or an empty one: it is told below */
                if (ct->ncells == OSHTML_CELLS_MAX)
                    return 0;
                if (col == 0 && minw)
                    return 0;
                cellcol[ct->ncells] = col;
                ct->cells[ct->ncells].align = align < 0 ? 0 : align;
                s[ct->ncells] = i;
                intext = ct->ncells++;
            }
            else
            {
                /* a cell of pictures: the title is beside them */
                if (col == 0)
                    return 0;
                colalign[col] = align < 0 ? 0 : align;
                if (minh > rowh[row])
                    rowh[row] = minh;
            }
            incell = 1;
            ++col;
            if (col > ncols)
                ncols = col;
        }
        else if (RAW("img") && !end && incell && row > 0)
        {
            oshtml_tile *tile = &pt->tiles[pt->ntiles];
            img_size w = { 0, 0 }, h = { 0, 0 };
            size_t sv, svlen;
            glui32 x = 0;

            if (pt->ntiles == OSHTML_TILES_MAX
                || raw_attr(t, ts, te, "usemap", &sv, &svlen)
                || raw_attr(t, ts, te, "ismap", &sv, &svlen)
                || !raw_attr(t, ts, te, "src", &v, &vlen))
                return 0;
            if (raw_attr(t, ts, te, "width", &sv, &svlen))
                parse_img_size(t + sv, svlen, &w);
            if (raw_attr(t, ts, te, "height", &sv, &svlen))
                parse_img_size(t + sv, svlen, &h);
            memset(tile, 0, sizeof(*tile));
            if (w.pct || h.pct
                || !find_image(p, t + v, vlen, imagealign_InlineUp, w, h,
                               &tile->img)
                || !oshtml_image_size(&tile->img, 0, &tile->w, &tile->h)
                || tile->w == 0 || tile->h == 0)
                return 0;

            /* after the cell's pictures before it, on one line */
            for (j = 0 ; j < pt->ntiles ; ++j)
                if (tilerow[j] == row && tilecol[j] == col - 1
                    && pt->tiles[j].x + pt->tiles[j].w > x)
                    x = pt->tiles[j].x + pt->tiles[j].w;
            tile->x = x;
            tilerow[pt->ntiles] = row;
            tilecol[pt->ntiles] = col - 1;
            if (x + tile->w + 2 * pad > colw[col - 1])
                colw[col - 1] = x + tile->w + 2 * pad;
            if (tile->h + 2 * pad > rowh[row])
                rowh[row] = tile->h + 2 * pad;
            ++pt->ntiles;
        }
        else if (!(RAW("font") || RAW("b") || RAW("i") || RAW("br")))
            return 0;
#undef RAWV
#undef RAW
    }
    if (intext >= 0)
        e[intext] = len;
    nrows = row + 1;
    if (depth != 0 || nrows < 2 || pt->ntiles == 0)
        return 0;

    /* the cells with text in them; the title is the first */
    line = osglk_px_grid_line();
    for (n = 0, k = 0 ; n < ct->ncells ; ++n)
    {
        int chars = cell_chars(t, s[n], e[n]);
        glui32 w;

        if (chars < 0 || (cellcol[n] == 0) != (k == 0))
            return 0;
        if (chars == 0 && cellcol[n] != 0)
            continue;
        w = osglk_px_grid_width(chars) + 2 * pad;
        if (w > colw[cellcol[n]] && cellcol[n] != 0)
            colw[cellcol[n]] = w;
        ct->cells[k] = ct->cells[n];
        ct->cells[k].chars = chars;
        cellcol[k] = cellcol[n];
        s[k] = s[n];
        e[k] = e[n];
        ++k;
    }
    ct->ncells = k;
    if (k < 2 || cellcol[0] != 0)
        return 0;

    /* the columns after the title from the left of the first, and the rows
       of pictures from the top of the second */
    colx[1] = 0;
    for (k = 1 ; k < ncols ; ++k)
    {
        if (colw[k] < 2 * pad)
            colw[k] = 2 * pad;
        colx[k + 1] = colx[k] + colw[k] + spacing;
    }
    pt->width = colx[ncols] + border;
    rowy[1] = 0;
    for (k = 1 ; k < nrows ; ++k)
        rowy[k + 1] = rowy[k] + rowh[k] + (k + 1 < nrows ? spacing : 0);
    pt->height = rowy[nrows];

    for (n = 0 ; n < ct->ncells ; ++n)
    {
        oshtml_textcell *c = &ct->cells[n];

        if (n == 0)
            continue;
        c->w = colw[cellcol[n]];
        c->right = pt->width - colx[cellcol[n]] - c->w;
    }

    /* each picture's line in its cell, as the cell aligns it, in the
       middle of its row */
    for (j = 0 ; j < pt->ntiles ; ++j)
    {
        oshtml_tile *tile = &pt->tiles[j];
        glui32 lw = 0, cw;
        int m;

        k = tilecol[j];
        for (m = 0 ; m < pt->ntiles ; ++m)
            if (tilerow[m] == tilerow[j] && tilecol[m] == k
                && pt->tiles[m].x + pt->tiles[m].w > lw)
                lw = pt->tiles[m].x + pt->tiles[m].w;
        cw = colw[k];
        tile->x += colx[k] + (colalign[k] == 1 ? (cw - lw) / 2
                              : colalign[k] == 2 ? cw - pad - lw : pad);
        tile->y = rowy[tilerow[j]] + (rowh[tilerow[j]] - tile->h) / 2;
    }
    pt->center = 0;
    pt->page = 0;
    pt->above = 0;
    pt->right = 1;
    ct->height = line + pt->height;
    return 1;
}

/*
 *   Give the banner the text before the table, then each cell's in its
 *   own grid: where the rest of the banner's text starts, at the table if
 *   the window can't show it so
 */
static size_t banner_cells(parser *b, void *ctx, const glui32 *t,
                           size_t from, size_t ts, size_t stop,
                           const oshtml_celltable *ct, const size_t *s,
                           const size_t *e)
{
    size_t k;
    int c;

    for (k = from ; k < ts ; ++k)
        feed(b, t[k]);
    flush_out(b);
    if (!osglk_tag_banner_cells(ctx, ct))
        return ts;
    for (c = 0 ; c < ct->ncells ; ++c)
    {
        osglk_tag_banner_cell(ctx, c);
        for (k = ts + s[c] ; k < ts + e[c] ; ++k)
            feed(b, t[k]);
        put_maprow(b);
        expand_tab(b, 1);
        flush_out(b);
    }
    osglk_tag_banner_cell(ctx, -1);
    return stop;
}

/* what a banner's text was found to be */
typedef struct banner_scan
{
    size_t i;                   /* where its text starts, after spaces */
    int page, table;            /* find_picpage's and find_pictable's */
    size_t bg, bglen, ts, te, ms;
    size_t cs[OSHTML_CELLS_MAX], ce[OSHTML_CELLS_MAX];
} banner_scan;

/* look the banner's text over, and lay out a table of text over pictures */
static void scan_banner(parser *p, banner_scan *sc, oshtml_celltable *ct)
{
    size_t i = 0;

    sc->bg = sc->bglen = sc->ts = sc->te = 0;
    sc->ms = (size_t)-1;
    while (i < p->banlen && is_space(p->bantext[i]))
        ++i;
    sc->i = i;
    sc->page = find_picpage(p->bantext + i, p->banlen - i,
                            &sc->bg, &sc->bglen);
    sc->table = find_pictable(p->bantext + i, p->banlen - i,
                              &sc->ts, &sc->te, &sc->ms);
    sc->ts += i;
    sc->te += i;
    if (sc->ms != (size_t)-1)
        sc->ms += i;

    /* a table of text over pictures, after only tags */
    ct->ncells = 0;
    if (sc->table == 1 && sc->ms == (size_t)-1 && !sc->page
        && only_tags(p->bantext, i, sc->ts))
    {
        /* the pictures are looked up as the page's, even from the status
           line's text */
        glui32 *dst = p->dst;

        p->dst = NULL;
        if (build_celltable(p, p->bantext + sc->ts, sc->te - sc->ts, ct,
                            sc->cs, sc->ce))
            pt_body_background(p, p->bantext + i, sc->ts - i, &ct->pics);
        else
            ct->ncells = 0;
        p->dst = dst;
    }
}

/* one pass of writing the banner's text to its window: false on failure */
static int write_banner(parser *p, const oshtml_sink *sink, void *ctx,
                        const banner_scan *sc, const oshtml_celltable *ct,
                        oshtml_pictable *pt)
{
    size_t i = sc->i, ts = sc->ts, te = sc->te, ms = sc->ms;
    size_t bg = sc->bg, bglen = sc->bglen, k;
    int table = sc->table, page = sc->page;

    parser *b = oshtml_new(sink, ctx);
    size_t from = i, stop = p->banlen;

    if (!b)
        return 0;
    b->in_banner = 1;
    if (page && banner_picpage(b, ctx, p->bantext + i,
                               p->banlen - i, bg, bglen, pt))
        from = stop;
    else if (page)
    {
        /* the text after all, from the start */
        oshtml_delete(b);
        if (!(b = oshtml_new(sink, ctx)))
            return 0;
        b->in_banner = 1;
    }
    if (from == stop)
        ;
    else if (ct->ncells)
        from = banner_cells(b, ctx, p->bantext, i, ts, stop,
                            ct, sc->cs, sc->ce);
    else if (table)
    {
        size_t cs = 0, ce = 0;
        int centered = 0;

        /* the text, then the table: the rest is only ends */
        for (k = i ; k < (table == 1 ? ms : ts) && k < ts ; ++k)
            feed(b, p->bantext[k]);
        put_maprow(b);
        expand_tab(b, 1);
        flush_out(b);
        from = ts;

        /* a picture with areas over the tables, one picture */
        if (table == 1 && ms != (size_t)-1)
        {
            if (build_pictable(b, p->bantext + ms, te - ms, pt)
                && osglk_tag_banner_table(ctx, pt))
                from = stop = ts;
            else
            {
                for (k = ms ; k < ts ; ++k)
                    feed(b, p->bantext[k]);
                put_maprow(b);
                expand_tab(b, 1);
                flush_out(b);
            }
        }
        if (stop == ts)
            ;
        else if ((table == 2
             ? build_mappic(b, p->bantext + ts, te - ts, pt)
             : build_pictable(b, p->bantext + ts, te - ts, pt))
            && (pt_body_background(b, p->bantext + i, ts - i, pt),
                osglk_tag_banner_table(ctx, pt)))
            stop = ts;

        /* or a map over the rest of its cell, which is text */
        else if (table == 1
                 && build_mapcell(b, p->bantext + ts, te - ts,
                                  pt, &cs, &ce, &centered)
                 && osglk_tag_banner_table(ctx, pt))
        {
            static const char cen[] = "<CENTER>";
            size_t bi, bs, be, bn, bl;
            int bend;

            from = ts + cs;
            stop = ts + ce;

            /* the <BR> under the picture only ends its line */
            for (bi = from ; bi < stop
                 && is_space(p->bantext[bi]) ; ++bi)
                ;
            if (centered && bi < stop && p->bantext[bi] == '<'
                && raw_tag(p->bantext, stop, &bi, &bs, &be)
                && (bl = raw_name(p->bantext, bs, be, &bn, &bend),
                    !bend && tag_is(p->bantext + bn, bl, "br")))
                from = bi;

            /* a centred cell centres its text, which starts
             * under the picture */
            if (centered)
                for (k = 0 ; cen[k] ; ++k)
                    feed(b, (glui32)cen[k]);
        }
    }
    for (k = from ; k < stop ; ++k)
        feed(b, p->bantext[k]);
    put_maprow(b);
    expand_tab(b, 1);
    flush_out(b);
    oshtml_delete(b);
    return 1;
}

void end_banner(parser *p)
{
    static oshtml_pictable pt;
    static oshtml_celltable ct;
    banner_scan sc;

    p->bancap = 0;
    scan_banner(p, &sc, &ct);
    p->ban.cells = ct.ncells ? ct.height : 0;
    if (sc.i < p->banlen)
    {
        const oshtml_sink *sink;
        void *ctx = osglk_tag_banner_open(&p->ban, &sink);

        /* the first pass may only measure the text, for its window's size,
           and then it is written again */
        if (ctx)
            while (write_banner(p, sink, ctx, &sc, &ct, &pt)
                   && osglk_tag_banner_done(ctx))
                ;
    }
    p->banlen = 0;
    put_html_text(p, ' ');
}

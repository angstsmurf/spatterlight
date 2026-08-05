/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * SVG emitter for the Glk map-document extension (gestalt_Map).
 * Walks the same map_t / map_view_t model as map_render().
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#include "mapdraw.h"

#define SVG_SCALE 20
#define SVG_PAD 24
#define SVG_INOUT_R 8

/* #RRGGBB for SVG attributes (buf must hold at least 8 chars). */
static void
svg_hex (char *buf, size_t buflen, unsigned int rgb)
{
  snprintf (buf, buflen, "#%06x", rgb & 0xFFFFFFu);
}
static void
svg_expand_xy (int *min_x, int *min_y, int *max_x, int *max_y, int x, int y)
{
  if (x < *min_x) *min_x = x;
  if (y < *min_y) *min_y = y;
  if (x > *max_x) *max_x = x;
  if (y > *max_y) *max_y = y;
}

static int
svg_seen (const map_view_t *v, const char *key)
{
  if (v == NULL || v->seen == NULL || key == NULL)
    return 0;
  return v->seen (v->ctx, key);
}

static const map_node_t *
svg_page_node (const map_page_t *page, const char *key)
{
  int i;
  if (page == NULL || key == NULL)
    return NULL;
  for (i = 0; i < page->n_nodes; i++)
    if (page->nodes[i].key != NULL
        && strcmp (page->nodes[i].key, key) == 0)
      return &page->nodes[i];
  return NULL;
}

static const map_page_t *
svg_page_for_player (const map_t *map, const char *player_key)
{
  int p, n;
  if (map == NULL)
    return NULL;
  if (player_key != NULL)
    {
      for (p = 0; p < map->n_pages; p++)
        for (n = 0; n < map->pages[p].n_nodes; n++)
          if (map->pages[p].nodes[n].key != NULL
              && strcmp (map->pages[p].nodes[n].key, player_key) == 0)
            return &map->pages[p];
    }
  return map->n_pages > 0 ? &map->pages[0] : NULL;
}

static void
edge_point (const map_node_t *n, int dir, int *x, int *y)
{
  int sx = n->x * SVG_SCALE;
  int sy = n->y * SVG_SCALE;
  int w = n->w * SVG_SCALE;
  int h = n->h * SVG_SCALE;
  switch (dir)
    {
    case DIR_N:  *x = sx + w / 2; *y = sy;         break;
    case DIR_NE: *x = sx + w;     *y = sy;         break;
    case DIR_E:  *x = sx + w;     *y = sy + h / 2; break;
    case DIR_SE: *x = sx + w;     *y = sy + h;     break;
    case DIR_S:  *x = sx + w / 2; *y = sy + h;     break;
    case DIR_SW: *x = sx;         *y = sy + h;     break;
    case DIR_W:  *x = sx;         *y = sy + h / 2; break;
    case DIR_NW: *x = sx;         *y = sy;         break;
    case DIR_UP:
    case DIR_DOWN:
      /* Fallback only — A5 badge connectors use badge_site_xy instead. */
      *x = sx + w / 2; *y = sy + h / 2;
      break;
    default:     *x = sx + w / 2; *y = sy + h / 2; break;
    }
}

static int
opp_dir (int dir)
{
  switch (dir)
    {
    case DIR_N:  return DIR_S;
    case DIR_E:  return DIR_W;
    case DIR_S:  return DIR_N;
    case DIR_W:  return DIR_E;
    case DIR_UP: return DIR_DOWN;
    case DIR_DOWN: return DIR_UP;
    case DIR_IN: return DIR_OUT;
    case DIR_OUT: return DIR_IN;
    case DIR_NE: return DIR_SW;
    case DIR_SE: return DIR_NW;
    case DIR_SW: return DIR_NE;
    case DIR_NW: return DIR_SE;
    default: return dir;
    }
}

static void
stub_end (const map_node_t *n, int dir, int *x, int *y)
{
  int ox = n->w > 0 ? 400 / n->w : 50;
  int oy = n->h > 0 ? 400 / n->h : 50;
  int xp, yp;
  switch (dir)
    {
    case DIR_N: case DIR_UP:   xp = 50; yp = -oy; break;
    case DIR_E:                xp = 100 + ox; yp = 50; break;
    case DIR_S: case DIR_DOWN: xp = 50; yp = 100 + oy; break;
    case DIR_W:                xp = -ox; yp = 50; break;
    case DIR_NE:               xp = 100 + 3 * ox / 4; yp = -(oy / 2); break;
    case DIR_SE:               xp = 100 + 3 * ox / 4; yp = 100 + oy / 2; break;
    case DIR_SW:               xp = -(3 * ox / 4); yp = 100 + oy / 2; break;
    case DIR_NW:               xp = -(3 * ox / 4); yp = -(oy / 2); break;
    default:                   xp = 50; yp = 50; break;
    }
  *x = n->x * SVG_SCALE + n->w * SVG_SCALE * xp / 100;
  *y = n->y * SVG_SCALE + n->h * SVG_SCALE * yp / 100;
}

/* Facing edge for a badge (Map.vb GetLinkPoint / eInEdge). Same geometry as
   mapdraw.cpp's inout_edge. */
static int
inout_edge (const map_node_t *n, const map_node_t *dn)
{
  if (dn == NULL)
    return DIR_N;
  if (dn->x > n->x + n->w)
    return DIR_E;
  if (dn->x + dn->w < n->x)
    return DIR_W;
  if (dn->y > n->y)
    return DIR_S;
  return DIR_N;
}

/* Badge sites: half-winds between compass stubs (mapdraw.cpp). */
enum {
  BADGE_NNE, BADGE_ENE, BADGE_ESE, BADGE_SSE,
  BADGE_SSW, BADGE_WSW, BADGE_WNW, BADGE_NNW,
  BADGE_N, BADGE_E, BADGE_S, BADGE_W,
  BADGE_NE, BADGE_SE, BADGE_SW, BADGE_NW
};

static void
badge_pct (int site, double *xp, double *yp)
{
  switch (site)
    {
    case BADGE_NNE: *xp = 75;  *yp = 0;   break;
    case BADGE_ENE: *xp = 100; *yp = 25;  break;
    case BADGE_ESE: *xp = 100; *yp = 75;  break;
    case BADGE_SSE: *xp = 75;  *yp = 100; break;
    case BADGE_SSW: *xp = 25;  *yp = 100; break;
    case BADGE_WSW: *xp = 0;   *yp = 75;  break;
    case BADGE_WNW: *xp = 0;   *yp = 25;  break;
    case BADGE_NNW: *xp = 25;  *yp = 0;   break;
    case BADGE_N:   *xp = 50;  *yp = 0;   break;
    case BADGE_E:   *xp = 100; *yp = 50;  break;
    case BADGE_S:   *xp = 50;  *yp = 100; break;
    case BADGE_W:   *xp = 0;   *yp = 50;  break;
    case BADGE_NE:  *xp = 100; *yp = 0;   break;
    case BADGE_SE:  *xp = 100; *yp = 100; break;
    case BADGE_SW:  *xp = 0;   *yp = 100; break;
    case BADGE_NW:  *xp = 0;   *yp = 0;   break;
    default:        *xp = 25;  *yp = 0;   break;
    }
}

static int
compass_site (int dir)
{
  switch (dir)
    {
    case DIR_N:  return BADGE_N;
    case DIR_E:  return BADGE_E;
    case DIR_S:  return BADGE_S;
    case DIR_W:  return BADGE_W;
    case DIR_NE: return BADGE_NE;
    case DIR_SE: return BADGE_SE;
    case DIR_SW: return BADGE_SW;
    case DIR_NW: return BADGE_NW;
    default:     return -1;
    }
}

static int
inout_site (int dir, int edge)
{
  int in = (dir == DIR_IN);
  switch (edge)
    {
    case DIR_E: return in ? BADGE_ENE : BADGE_ESE;
    case DIR_W: return in ? BADGE_WSW : BADGE_WNW;
    case DIR_S: return in ? BADGE_SSE : BADGE_SSW;
    default:    return in ? BADGE_NNW : BADGE_NNE;
    }
}

/* Up/Down primary half-wind on a facing edge (never the cardinal midpoint). */
static int
ud_site_primary (int dir, int edge)
{
  int up = (dir == DIR_UP);
  switch (edge)
    {
    case DIR_E: return up ? BADGE_ENE : BADGE_ESE;
    case DIR_W: return up ? BADGE_WNW : BADGE_WSW;
    case DIR_S: return up ? BADGE_SSE : BADGE_SSW;
    default:    return up ? BADGE_NNE : BADGE_NNW;
    }
}

static int
ud_site_alt (int site)
{
  switch (site)
    {
    case BADGE_ENE: return BADGE_NNE;
    case BADGE_ESE: return BADGE_SSE;
    case BADGE_WNW: return BADGE_NNW;
    case BADGE_WSW: return BADGE_SSW;
    case BADGE_NNE: return BADGE_ENE;
    case BADGE_NNW: return BADGE_WNW;
    case BADGE_SSE: return BADGE_ESE;
    default:        return BADGE_WSW;
    }
}

static void
badge_site_xy (const map_node_t *n, int site, int *x, int *y)
{
  double xp, yp;
  badge_pct (site, &xp, &yp);
  *x = (int) (n->x * SVG_SCALE + n->w * SVG_SCALE * xp / 100.0 + 0.5);
  *y = (int) (n->y * SVG_SCALE + n->h * SVG_SCALE * yp / 100.0 + 0.5);
}

/* ADRIFT 3/4 fixed badge sites (mapdraw a4_badge_site): opposite corners,
   Up at NNE and Down at SSW, In at WNW and Out at ESE. */
static void
a4_badge_sites (int *up, int *down, int *in, int *out)
{
  *up = BADGE_NNE;
  *down = BADGE_SSW;
  *in = BADGE_WNW;
  *out = BADGE_ESE;
}

static const map_link_t *
find_dir_link (const map_node_t *n, int dir)
{
  int l;
  if (n == NULL)
    return NULL;
  for (l = 0; l < n->n_links; l++)
    if (n->links[l].dir == dir)
      return &n->links[l];
  return NULL;
}

static int
badge_opposite (int dir)
{
  switch (dir)
    {
    case DIR_UP:   return DIR_DOWN;
    case DIR_DOWN: return DIR_UP;
    case DIR_IN:   return DIR_OUT;
    case DIR_OUT:  return DIR_IN;
    default:       return -1;
    }
}

static int
compass_opposite (int dir)
{
  switch (dir)
    {
    case DIR_N:  return DIR_S;
    case DIR_E:  return DIR_W;
    case DIR_S:  return DIR_N;
    case DIR_W:  return DIR_E;
    case DIR_NE: return DIR_SW;
    case DIR_SE: return DIR_NW;
    case DIR_SW: return DIR_NE;
    case DIR_NW: return DIR_SE;
    default:     return -1;
    }
}

/* Aim badge at dest on this page, else closest same-page reverse badge. */
static const map_node_t *
badge_face_node (const map_page_t *page, const map_node_t *n,
                 const map_link_t *link)
{
  const map_node_t *dn, *best = NULL;
  int want, i, best_d2 = -1;

  if (page == NULL || n == NULL || n->key == NULL || link == NULL)
    return NULL;
  dn = (link->dest != NULL) ? svg_page_node (page, link->dest) : NULL;
  if (dn != NULL)
    return dn;

  want = link->dst_anchor;
  if (want != DIR_UP && want != DIR_DOWN && want != DIR_IN && want != DIR_OUT)
    want = badge_opposite (link->dir);
  if (want < 0)
    return NULL;

  for (i = 0; i < page->n_nodes; i++)
    {
      const map_node_t *m = &page->nodes[i];
      const map_link_t *back;
      int dx, dy, d2;

      if (m == n || m->key == NULL)
        continue;
      back = find_dir_link (m, want);
      if (back == NULL || back->dest == NULL
          || strcmp (back->dest, n->key) != 0)
        continue;
      dx = m->x - n->x;
      dy = m->y - n->y;
      d2 = dx * dx + dy * dy;
      if (best == NULL || d2 < best_d2)
        {
          best = m;
          best_d2 = d2;
        }
    }
  return best;
}

/*
 * Per-node In/Out/Up/Down badge layout (mapdraw inout_layout).  Far-end badges
 * and compass-twin parking match the raster drawer.  Returns NULL for pages
 * with nothing to record (ADRIFT 4 badge-only links skip the walk).
 */
typedef struct {
  int in_edge, out_edge, up_edge, down_edge;
  int in_site, out_site, up_site, down_site;
  unsigned char far_in, far_out;
  unsigned char far_up, far_down;
  unsigned char has_in, has_out, has_up, has_down;
  unsigned char on_compass_in, on_compass_out;
  unsigned char on_compass_up, on_compass_down;
  unsigned char in_site_fixed, out_site_fixed;
  unsigned char up_site_fixed, down_site_fixed;
} inout_badge_t;

static int
site_taken_io (const inout_badge_t *b, int site)
{
  return (b->has_in && b->in_site == site)
      || (b->has_out && b->out_site == site);
}

static int
badge_compass_arrival (const map_node_t *n, const map_link_t *link)
{
  const map_link_t *twin_lk;
  int twin;

  if (n == NULL || link == NULL || link->dest == NULL)
    return -1;
  if (!link->has_compass_twin)
    return -1;
  twin = link->compass_twin;
  twin_lk = find_dir_link (n, twin);
  if (twin_lk != NULL && twin_lk->dst_anchor >= 0
      && compass_site (twin_lk->dst_anchor) >= 0)
    return twin_lk->dst_anchor;
  return compass_opposite (twin);
}

static void
fix_far_compass_site (inout_badge_t *b, int dst_anchor, int arrival_dir)
{
  int cs = compass_site (arrival_dir);

  if (cs < 0 || b == NULL)
    return;
  switch (dst_anchor)
    {
    case DIR_IN:
      b->in_site = cs;
      b->in_site_fixed = 1;
      break;
    case DIR_OUT:
      b->out_site = cs;
      b->out_site_fixed = 1;
      break;
    case DIR_UP:
      b->up_site = cs;
      b->up_site_fixed = 1;
      break;
    case DIR_DOWN:
      b->down_site = cs;
      b->down_site_fixed = 1;
      break;
    default:
      break;
    }
}

static void
try_compass_port (inout_badge_t *x, const map_node_t *n, int dir,
                  int *site, unsigned char *on_compass)
{
  const map_link_t *lk = find_dir_link (n, dir);
  int twin, cs;

  (void) x;
  if (lk == NULL || lk->dest == NULL || !lk->has_compass_twin)
    return;
  twin = lk->compass_twin;
  cs = compass_site (twin);
  if (cs < 0)
    return;
  *site = cs;
  *on_compass = 1;
}

static void
finalize_badge_sites (inout_badge_t *b, const map_page_t *page)
{
  int i;

  for (i = 0; i < page->n_nodes; i++)
    {
      inout_badge_t *x = &b[i];
      const map_node_t *n = &page->nodes[i];

      if (x->has_in && !x->in_site_fixed)
        {
          x->in_site = inout_site (DIR_IN, x->in_edge);
          try_compass_port (x, n, DIR_IN, &x->in_site, &x->on_compass_in);
        }
      if (x->has_out && !x->out_site_fixed)
        {
          x->out_site = inout_site (DIR_OUT, x->out_edge);
          try_compass_port (x, n, DIR_OUT, &x->out_site, &x->on_compass_out);
        }
      if (x->has_up && !x->up_site_fixed)
        {
          x->up_site = ud_site_primary (DIR_UP, x->up_edge);
          if (site_taken_io (x, x->up_site))
            {
              int alt = ud_site_alt (x->up_site);
              if (!site_taken_io (x, alt))
                x->up_site = alt;
            }
          try_compass_port (x, n, DIR_UP, &x->up_site, &x->on_compass_up);
        }
      if (x->has_down && !x->down_site_fixed)
        {
          x->down_site = ud_site_primary (DIR_DOWN, x->down_edge);
          if (site_taken_io (x, x->down_site)
              || (x->has_up && x->up_site == x->down_site))
            {
              int alt = ud_site_alt (x->down_site);
              if (!site_taken_io (x, alt)
                  && !(x->has_up && x->up_site == alt))
                x->down_site = alt;
            }
          try_compass_port (x, n, DIR_DOWN, &x->down_site,
                            &x->on_compass_down);
        }
    }
}

static void
inout_mark (inout_badge_t *b, int dir, int edge, int far)
{
  if (dir == DIR_IN)
    {
      b->in_edge = edge;
      b->has_in = 1;
      b->far_in |= (unsigned char) (far != 0);
    }
  else
    {
      b->out_edge = edge;
      b->has_out = 1;
      b->far_out |= (unsigned char) (far != 0);
    }
}

static void
ud_mark (inout_badge_t *b, int dir, int edge, int far)
{
  if (dir == DIR_UP)
    {
      b->up_edge = edge;
      b->has_up = 1;
      b->far_up |= (unsigned char) (far != 0);
    }
  else if (dir == DIR_DOWN)
    {
      b->down_edge = edge;
      b->has_down = 1;
      b->far_down |= (unsigned char) (far != 0);
    }
}

static int
node_has_badge_dir (const map_node_t *n, int dir)
{
  if (n == NULL)
    return 0;
  switch (dir)
    {
    case DIR_IN:   return n->has_badge[MAP_BADGE (DIR_IN)];
    case DIR_OUT:  return n->has_badge[MAP_BADGE (DIR_OUT)];
    case DIR_UP:   return n->has_badge[MAP_BADGE (DIR_UP)];
    case DIR_DOWN:  return n->has_badge[MAP_BADGE (DIR_DOWN)];
    default:       return 0;
    }
}

static int
node_has_link_dir (const map_node_t *n, int dir)
{
  int l;
  if (n == NULL)
    return 0;
  for (l = 0; l < n->n_links; l++)
    if (n->links[l].dir == dir)
      return 1;
  return 0;
}

static int
arrival_badge_site (const inout_badge_t *b, const map_node_t *dn,
                    const map_node_t *src, int dst_anchor)
{
  int edge = inout_edge (dn, src);

  switch (dst_anchor)
    {
    case DIR_IN:
      return (b != NULL && b->has_in) ? b->in_site : inout_site (DIR_IN, edge);
    case DIR_OUT:
      return (b != NULL && b->has_out) ? b->out_site
                                      : inout_site (DIR_OUT, edge);
    case DIR_UP:
      return (b != NULL && b->has_up) ? b->up_site
                                     : ud_site_primary (DIR_UP, edge);
    case DIR_DOWN:
      return (b != NULL && b->has_down) ? b->down_site
                                       : ud_site_primary (DIR_DOWN, edge);
    default:
      return BADGE_NNE;
    }
}

static inout_badge_t *
inout_badge_buf (inout_badge_t *b, const map_page_t *page)
{
  if (b != NULL)
    return b;
  return (inout_badge_t *) calloc ((size_t) page->n_nodes,
                                   sizeof (inout_badge_t));
}

static inout_badge_t *
inout_layout (const map_page_t *page, const map_view_t *view)
{
  inout_badge_t *b = NULL;
  int i, l;

  for (i = 0; i < page->n_nodes; i++)
    {
      const map_node_t *n = &page->nodes[i];
      if (!svg_seen (view, n->key))
        continue;

      for (l = 0; l < n->n_links; l++)
        {
          const map_link_t *link = &n->links[l];
          const map_node_t *dn;
          int dst_anchor;

          if (link->badge)
            continue;
          if (link->dir != DIR_IN && link->dir != DIR_OUT
              && link->dir != DIR_UP && link->dir != DIR_DOWN)
            continue;
          b = inout_badge_buf (b, page);
          if (b == NULL)
            return NULL;

          dn = (link->dest != NULL) ? svg_page_node (page, link->dest) : NULL;
          {
            const map_node_t *face = badge_face_node (page, n, link);

            if (link->dir == DIR_IN || link->dir == DIR_OUT)
              inout_mark (&b[i], link->dir, inout_edge (n, face), 0);
            else if (link->dir == DIR_UP || link->dir == DIR_DOWN)
              ud_mark (&b[i], link->dir, inout_edge (n, face), 0);
          }
          if (dn == NULL || !svg_seen (view, dn->key))
            continue;
          dst_anchor = link->dst_anchor;
          if (!node_has_badge_dir (dn, dst_anchor))
            continue;

          if (dst_anchor == DIR_IN || dst_anchor == DIR_OUT)
            inout_mark (&b[dn - page->nodes], dst_anchor,
                        inout_edge (dn, n), 1);
          else if (dst_anchor == DIR_UP || dst_anchor == DIR_DOWN)
            ud_mark (&b[dn - page->nodes], dst_anchor,
                     inout_edge (dn, n), 1);
          else
            continue;
          {
            int arrival = badge_compass_arrival (n, link);
            if (arrival >= 0)
              fix_far_compass_site (&b[dn - page->nodes], dst_anchor, arrival);
          }
        }

      /* Movement-only badge toward an unseen room (Map.vb DrawNode). */
      if (view != NULL && view->exit_dest != NULL)
        {
          static const int badge_dirs[] = {
            DIR_IN, DIR_OUT, DIR_UP, DIR_DOWN
          };
          int di;

          for (di = 0; di < 4; di++)
            {
              int dir = badge_dirs[di];
              const char *dest;
              const map_node_t *face;
              int already;

              if (!node_has_badge_dir (n, dir))
                continue;
              if (node_has_link_dir (n, dir))
                continue;
              if (b != NULL)
                {
                  already = (dir == DIR_IN) ? b[i].has_in
                          : (dir == DIR_OUT) ? b[i].has_out
                          : (dir == DIR_UP) ? b[i].has_up
                          : b[i].has_down;
                  if (already)
                    continue;
                }
              dest = view->exit_dest (view->ctx, n->key, dir);
              if (dest == NULL || dest[0] == '\0')
                continue;
              if (svg_seen (view, dest))
                continue;
              b = inout_badge_buf (b, page);
              if (b == NULL)
                return NULL;
              face = svg_page_node (page, dest);
              if (dir == DIR_IN || dir == DIR_OUT)
                inout_mark (&b[i], dir, inout_edge (n, face), 0);
              else
                ud_mark (&b[i], dir, inout_edge (n, face), 0);
            }
        }
    }
  if (b != NULL)
    finalize_badge_sites (b, page);
  return b;
}

static int
badge_dir_site (const inout_badge_t *b, int dir)
{
  if (b == NULL)
    return BADGE_NNE;
  switch (dir)
    {
    case DIR_IN:   return b->in_site;
    case DIR_OUT:  return b->out_site;
    case DIR_UP:   return b->up_site;
    case DIR_DOWN: return b->down_site;
    default:       return BADGE_NNE;
    }
}

static int
badge_on_compass (const inout_badge_t *b, int dir)
{
  if (b == NULL)
    return 0;
  switch (dir)
    {
    case DIR_IN:   return b->on_compass_in;
    case DIR_OUT:  return b->on_compass_out;
    case DIR_UP:   return b->on_compass_up;
    case DIR_DOWN: return b->on_compass_down;
    default:       return 0;
    }
}

static void
append_badge_disc (std::string &out, int cx, int cy, int dir)
{
  /* Fills match mapdraw's ICON_* (darkened so the letter reads).  Letters are
     drawn as shapes, not <text>: AppKit's SVG renderer mishandles text-anchor
     / baseline on these tiny discs.  Integer coords avoid half-pixel drift. */
  const char *fill, *stroke;
  switch (dir)
    {
    case DIR_IN:
      fill = "#007000"; stroke = "#003800";
      break;
    case DIR_OUT:
      fill = "#b03060"; stroke = "#601028";
      break;
    case DIR_UP:
      /* ICON_UP is the Finder yellow (#febc2e); darken so the white U reads. */
      fill = "#c49018"; stroke = "#8a6408";
      break;
    case DIR_DOWN:
      fill = "#2840a0"; stroke = "#142050";
      break;
    default:
      return;
    }
  out += "<circle cx=\"";
  out += std::to_string (cx);
  out += "\" cy=\"";
  out += std::to_string (cy);
  out += "\" r=\"";
  out += std::to_string (SVG_INOUT_R);
  out += "\" fill=\"";
  out += fill;
  out += "\" stroke=\"";
  out += stroke;
  out += "\" stroke-width=\"1.25\"/>";

  switch (dir)
    {
    case DIR_IN:  /* I */
      out += "<rect x=\"";
      out += std::to_string (cx - 1);
      out += "\" y=\"";
      out += std::to_string (cy - 4);
      out += "\" width=\"2\" height=\"8\" fill=\"#ffffff\"/>";
      break;
    case DIR_OUT:  /* O */
      out += "<circle cx=\"";
      out += std::to_string (cx);
      out += "\" cy=\"";
      out += std::to_string (cy);
      out += "\" r=\"3\" fill=\"none\" stroke=\"#ffffff\" stroke-width=\"2\"/>";
      break;
    case DIR_UP: {
      /* U — stems + bottom semicircle.  AppKit flattens C/Q/A, so build the
         bowl from a stroked circle and hide its upper half with the badge
         fill, then redraw the stems on top. */
      int bowl_cy = cy;
      out += "<circle cx=\"";
      out += std::to_string (cx);
      out += "\" cy=\"";
      out += std::to_string (bowl_cy);
      out += "\" r=\"3\" fill=\"none\" stroke=\"#ffffff\" stroke-width=\"2\"/>";
      /* Hide everything above the bowl's equator so a deep semicircle remains. */
      out += "<rect x=\"";
      out += std::to_string (cx - 4);
      out += "\" y=\"";
      out += std::to_string (cy - 5);
      out += "\" width=\"8\" height=\"";
      out += std::to_string ((bowl_cy - 1) - (cy - 5));
      out += "\" fill=\"";
      out += fill;
      out += "\"/>";
      /* Stems shorter than I; meet the raised bowl. */
      out += "<rect x=\"";
      out += std::to_string (cx - 4);
      out += "\" y=\"";
      out += std::to_string (cy - 4);
      out += "\" width=\"2\" height=\"5\" fill=\"#ffffff\"/>";
      out += "<rect x=\"";
      out += std::to_string (cx + 2);
      out += "\" y=\"";
      out += std::to_string (cy - 4);
      out += "\" width=\"2\" height=\"5\" fill=\"#ffffff\"/>";
      break;
    }
    case DIR_DOWN: {
      /* D — rects only.  Circles leave an antialiased hump on the stem in
         AppKit's SVG renderer, so approximate the bowl with stepped bars. */
      out += "<rect x=\"";
      out += std::to_string (cx - 3);
      out += "\" y=\"";
      out += std::to_string (cy - 4);
      out += "\" width=\"2\" height=\"8\" fill=\"#ffffff\"/>";  /* stem */
      out += "<rect x=\"";
      out += std::to_string (cx - 1);
      out += "\" y=\"";
      out += std::to_string (cy - 4);
      out += "\" width=\"3\" height=\"2\" fill=\"#ffffff\"/>";  /* top */
      out += "<rect x=\"";
      out += std::to_string (cx + 1);
      out += "\" y=\"";
      out += std::to_string (cy - 3);
      out += "\" width=\"2\" height=\"2\" fill=\"#ffffff\"/>";
      out += "<rect x=\"";
      out += std::to_string (cx + 2);
      out += "\" y=\"";
      out += std::to_string (cy - 2);
      out += "\" width=\"2\" height=\"4\" fill=\"#ffffff\"/>";  /* right */
      out += "<rect x=\"";
      out += std::to_string (cx + 1);
      out += "\" y=\"";
      out += std::to_string (cy + 1);
      out += "\" width=\"2\" height=\"2\" fill=\"#ffffff\"/>";
      out += "<rect x=\"";
      out += std::to_string (cx - 1);
      out += "\" y=\"";
      out += std::to_string (cy + 2);
      out += "\" width=\"3\" height=\"2\" fill=\"#ffffff\"/>";  /* bottom */
      break;
    }
    default:
      break;
    }
}

/* Filled triangle at (tip_x, tip_y) pointing along the vector from (from_*)
   to the tip.  Drawn as geometry (not SVG marker) so NSImage can render it. */
static void
append_arrowhead (std::string &out, int tip_x, int tip_y,
                  int from_x, int from_y, const char *fill)
{
  double dx = (double) tip_x - (double) from_x;
  double dy = (double) tip_y - (double) from_y;
  double len = sqrt (dx * dx + dy * dy);
  double ux, uy, nx, ny;
  double size = 8.0;
  double half = size * 0.45;
  double bx, by;
  char buf[160];

  if (len < 0.001)
    return;
  ux = dx / len;
  uy = dy / len;
  nx = -uy;
  ny = ux;
  bx = (double) tip_x - ux * size;
  by = (double) tip_y - uy * size;
  snprintf (buf, sizeof buf,
            "<polygon fill=\"%s\" points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f\"/>",
            fill,
            (double) tip_x, (double) tip_y,
            bx + nx * half, by + ny * half,
            bx - nx * half, by - ny * half);
  out += buf;
}

static std::string
xml_escape (const char *s)
{
  std::string out;
  if (s == NULL)
    return out;
  for (; *s; s++)
    {
      unsigned char c = (unsigned char) *s;
      /* XML 1.0 forbids most C0 controls; A5_ALR_MARK (\x03) left by
         a5text_render_plain after stripping tags like <1> would make the
         whole SVG fail to decode in NSImage. */
      if (c < 0x20 && c != '\t' && c != '\n' && c != '\r')
        continue;
      switch (c)
        {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += (char) c; break;
        }
    }
  return out;
}

/* Approx. advance for weight-600 system UI sans (SVG user units ≈ CSS px). */
#define LABEL_CHAR_EM 0.56
#define LABEL_LINE_EM 1.15
#define LABEL_PAD_PX 8

static size_t
utf8_nchars (const std::string &s)
{
  size_t n = 0;
  for (size_t i = 0; i < s.size (); i++)
    if (((unsigned char) s[i] & 0xC0) != 0x80)
      n++;
  return n;
}

static void
split_overlong (const std::string &word, size_t max_chars,
                std::vector<std::string> &chunks)
{
  size_t n = utf8_nchars (word);
  if (n <= max_chars)
    {
      chunks.push_back (word);
      return;
    }
  size_t i = 0;
  while (i < word.size ())
    {
      size_t start = i;
      size_t count = 0;
      while (i < word.size () && count < max_chars)
        {
          unsigned char c = (unsigned char) word[i];
          if ((c & 0x80) == 0)
            i += 1;
          else if ((c & 0xE0) == 0xC0)
            i += 2;
          else if ((c & 0xF0) == 0xE0)
            i += 3;
          else
            i += 4;
          if (i > word.size ())
            i = word.size ();
          count++;
        }
      chunks.push_back (word.substr (start, i - start));
    }
}

static void
wrap_label (const char *text, int box_w, int box_h, double font_px,
            std::vector<std::string> &lines)
{
  int max_w = box_w - LABEL_PAD_PX;
  size_t max_chars;
  size_t max_lines;
  std::string cur;
  const char *p;

  lines.clear ();
  if (text == NULL || text[0] == '\0')
    return;
  if (max_w < 24)
    max_w = 24;
  if (font_px < 8.0)
    font_px = 8.0;
  max_chars = (size_t) (max_w / (font_px * LABEL_CHAR_EM));
  if (max_chars < 4)
    max_chars = 4;
  max_lines = (size_t) ((box_h - LABEL_PAD_PX) / (font_px * LABEL_LINE_EM));
  if (max_lines < 1)
    max_lines = 1;
  if (max_lines > 4)
    max_lines = 4;

  p = text;
  while (*p)
    {
      std::string word;
      std::vector<std::string> chunks;
      size_t ci;

      while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
      if (!*p)
        break;
      while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
        word += *p++;

      chunks.clear ();
      split_overlong (word, max_chars, chunks);
      for (ci = 0; ci < chunks.size (); ci++)
        {
          std::string candidate;
          if (cur.empty ())
            candidate = chunks[ci];
          else
            candidate = cur + " " + chunks[ci];
          if (utf8_nchars (candidate) <= max_chars)
            cur = candidate;
          else
            {
              if (!cur.empty ())
                lines.push_back (cur);
              cur = chunks[ci];
            }
        }
    }
  if (!cur.empty ())
    lines.push_back (cur);

  if (lines.size () > max_lines)
    {
      lines.resize (max_lines);
      if (!lines.empty ())
        {
          std::string &last = lines.back ();
          size_t n = utf8_nchars (last);
          if (n > 1)
            {
              size_t i = last.size ();
              while (i > 0)
                {
                  i--;
                  if (((unsigned char) last[i] & 0xC0) != 0x80)
                    break;
                }
              last.resize (i);
              last += "\xE2\x80\xA6";
            }
        }
    }
}

/* Map.vb GetFont starts large; keep the old 16px default and only bump
   slightly for oversized nodes (e.g. Wide Room). */
static double
label_font_px (int box_w, int box_h)
{
  double em = 16.0;
  double area_scale = sqrt (((double) box_w * box_h) / (120.0 * 80.0));
  if (area_scale > 1.05)
    {
      em = 16.0 * (1.0 + (area_scale - 1.0) * 0.5);
      if (em > 20.0)
        em = 20.0;
    }
  return em;
}

static void
append_label (std::string &out, const char *label, int cx, int cy,
              int box_w, int box_h, unsigned int fill_rgb)
{
  std::vector<std::string> lines;
  size_t i;
  double font_px;
  double line_h;
  double start_y;
  char ybuf[32];
  char fbuf[32];
  char fillbuf[8];

  font_px = label_font_px (box_w, box_h);
  wrap_label (label, box_w, box_h, font_px, lines);
  if (lines.empty ())
    return;

  line_h = font_px * LABEL_LINE_EM;
  start_y = (double) cy - ((double) lines.size () - 1.0) * line_h / 2.0;
  snprintf (fbuf, sizeof fbuf, "%.1f", font_px);
  svg_hex (fillbuf, sizeof fillbuf, fill_rgb);
  for (i = 0; i < lines.size (); i++)
    {
      snprintf (ybuf, sizeof ybuf, "%.1f", start_y + (double) i * line_h);
      out += "<text fill=\"";
      out += fillbuf;
      out += "\" font-family=\"system-ui,-apple-system,Segoe UI,"
             "Roboto,Helvetica,Arial,sans-serif\" font-size=\"";
      out += fbuf;
      out += "\" font-weight=\"500\" text-anchor=\"middle\" "
             "dominant-baseline=\"middle\" x=\"";
      out += std::to_string (cx);
      out += "\" y=\"";
      out += ybuf;
      out += "\">";
      out += xml_escape (lines[i].c_str ());
      out += "</text>";
    }
}

typedef struct { double x, y; } svg_pt_t;

typedef struct {
  svg_pt_t p0, c0, c1, p1;
} svg_cubic_t;

/* GetRelativePoint percentages → SVG user units. */
static void
svg_rel_point (const map_node_t *n, double xp, double yp, double *x, double *y)
{
  *x = n->x * SVG_SCALE + n->w * SVG_SCALE * xp / 100.0;
  *y = n->y * SVG_SCALE + n->h * SVG_SCALE * yp / 100.0;
}

/* GetBezierAssister (Map.vb:1592) in SVG user units for compass links.
   Badge connectors (In/Out/Up/Down) are straight and skip this.
   Cap the relative offsets: on long mismatched links (Hub SE→Disagree sits
   NW of a SE exit) the raw Map.vb formula pushes controls so far that the
   cubic shoots out and back, reading as a polygonal spike under NSImage. */
static void
svg_bezier_assister (const map_node_t *n, int dir, int edge, double dist,
                     double *x, double *y)
{
  double ox, oy;
  int nw = n->w > 0 ? n->w : 1;
  int nh = n->h > 0 ? n->h : 1;

  (void) edge;
  if (dist < 1.0)
    dist = 1.0;
  ox = dist * 40.0 / SVG_SCALE / nw;
  oy = dist * 40.0 / SVG_SCALE / nh;
  if (ox > 60.0) ox = 60.0;
  if (oy > 60.0) oy = 60.0;
  switch (dir)
    {
    case DIR_N:  svg_rel_point (n, 50, -oy, x, y); break;
    case DIR_NE: svg_rel_point (n, 100 + 3 * ox / 4, -oy / 2, x, y); break;
    case DIR_E:  svg_rel_point (n, 100 + ox, 50, x, y); break;
    case DIR_SE: svg_rel_point (n, 100 + 3 * ox / 4, 100 + oy / 2, x, y); break;
    case DIR_S:  svg_rel_point (n, 50, 100 + oy, x, y); break;
    case DIR_SW: svg_rel_point (n, -3 * ox / 4, 100 + oy / 2, x, y); break;
    case DIR_W:  svg_rel_point (n, -ox, 50, x, y); break;
    case DIR_NW: svg_rel_point (n, -3 * ox / 4, -oy / 2, x, y); break;
    default:
      svg_rel_point (n, 50, 50, x, y);
      break;
    }
}

static void
sample_cubic (std::vector<svg_pt_t> &pts,
              double x0, double y0, double x1, double y1,
              double x2, double y2, double x3, double y3, int steps)
{
  int s;
  if (steps < 4)
    steps = 4;
  for (s = 0; s <= steps; s++)
    {
      double t = (double) s / (double) steps;
      double u = 1.0 - t;
      svg_pt_t p;
      p.x = u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2
            + t * t * t * x3;
      p.y = u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2
            + t * t * t * y3;
      if (pts.empty ()
          || fabs (p.x - pts.back ().x) > 0.01
          || fabs (p.y - pts.back ().y) > 0.01)
        pts.push_back (p);
    }
}

/* Author Map Link Anchors → one cubic Bezier (start, mid0, mid1, end).
   Map.vb DrawCurve interpolates the anchors as knots, which keeps acute
   corners (Hub→North, No Link→Wide) looking like a polyline. Treating the
   anchors as Bezier controls matches the intended bent shape as a smooth
   curve and renders cleanly via SVG C under NSImage. */
static void
mids_to_cubics (std::vector<svg_cubic_t> &out,
                const svg_pt_t &start, const svg_pt_t &end,
                const map_pt_t *mids, int n_mids)
{
  svg_cubic_t c;
  out.clear ();
  c.p0 = start;
  c.p1 = end;
  if (n_mids <= 0 || mids == NULL)
    {
      c.c0 = start;
      c.c1 = end;
    }
  else if (n_mids == 1)
    {
      c.c0.x = mids[0].x * SVG_SCALE;
      c.c0.y = mids[0].y * SVG_SCALE;
      c.c1 = c.c0;
    }
  else
    {
      c.c0.x = mids[0].x * SVG_SCALE;
      c.c0.y = mids[0].y * SVG_SCALE;
      c.c1.x = mids[n_mids - 1].x * SVG_SCALE;
      c.c1.y = mids[n_mids - 1].y * SVG_SCALE;
    }
  out.push_back (c);
}

static void
sample_cubics (std::vector<svg_pt_t> &pts, const std::vector<svg_cubic_t> &cubics,
               int steps_per_seg)
{
  size_t i;
  for (i = 0; i < cubics.size (); i++)
    {
      const svg_cubic_t &c = cubics[i];
      if (i > 0 && !pts.empty ())
        pts.pop_back ();
      sample_cubic (pts, c.p0.x, c.p0.y, c.c0.x, c.c0.y,
                    c.c1.x, c.c1.y, c.p1.x, c.p1.y, steps_per_seg);
    }
  if (!pts.empty () && !cubics.empty ())
    {
      pts.front () = cubics.front ().p0;
      pts.back () = cubics.back ().p1;
    }
}

/* Stroke a polyline. Dashes are explicit segments — NSImage often ignores
   stroke-dasharray. */
static void
append_poly_stroke (std::string &out, const std::vector<svg_pt_t> &pts,
                    int dash, const char *stroke)
{
  size_t i;
  if (pts.size () < 2)
    return;
  if (!dash)
    {
      char buf[64];
      out += "<path fill=\"none\" stroke=\"";
      out += stroke;
      out += "\" stroke-width=\"1.5\" stroke-linejoin=\"round\" "
             "stroke-linecap=\"round\" d=\"M ";
      snprintf (buf, sizeof buf, "%.2f %.2f", pts[0].x, pts[0].y);
      out += buf;
      for (i = 1; i < pts.size (); i++)
        {
          snprintf (buf, sizeof buf, " L %.2f %.2f", pts[i].x, pts[i].y);
          out += buf;
        }
      out += "\"/>";
      return;
    }
  {
    const double dash_on = 7.0;
    const double dash_off = 5.0;
    double phase = 0.0;
    int on = 1;
    for (i = 0; i + 1 < pts.size (); i++)
      {
        double dx = pts[i + 1].x - pts[i].x;
        double dy = pts[i + 1].y - pts[i].y;
        double seg = sqrt (dx * dx + dy * dy);
        double pos = 0.0;
        if (seg < 0.001)
          continue;
        dx /= seg;
        dy /= seg;
        while (pos < seg)
          {
            double remain = (on ? dash_on : dash_off) - phase;
            double take = remain;
            if (take > seg - pos)
              take = seg - pos;
            if (on && take > 0.05)
              {
                char buf[160];
                double x0 = pts[i].x + dx * pos;
                double y0 = pts[i].y + dy * pos;
                double x1 = pts[i].x + dx * (pos + take);
                double y1 = pts[i].y + dy * (pos + take);
                snprintf (buf, sizeof buf,
                          "<line fill=\"none\" stroke=\"%s\" stroke-width=\"1.5\" "
                          "stroke-linecap=\"butt\" x1=\"%.2f\" y1=\"%.2f\" "
                          "x2=\"%.2f\" y2=\"%.2f\"/>",
                          stroke, x0, y0, x1, y1);
                out += buf;
              }
            pos += take;
            phase += take;
            if (phase >= (on ? dash_on : dash_off) - 0.001)
              {
                phase = 0.0;
                on = !on;
              }
          }
      }
  }
}

/* Stroke mid-point links as native SVG cubics (smoother under NSImage than
   a dense polyline of L segments). */
static void
append_curve_stroke (std::string &out, const std::vector<svg_cubic_t> &cubics,
                     int dash, const char *stroke)
{
  size_t i;
  char buf[96];
  if (cubics.empty ())
    return;
  if (dash)
    {
      std::vector<svg_pt_t> sampled;
      sample_cubics (sampled, cubics, 12);
      append_poly_stroke (out, sampled, 1, stroke);
      return;
    }
  out += "<path fill=\"none\" stroke=\"";
  out += stroke;
  out += "\" stroke-width=\"1.5\" stroke-linecap=\"round\" d=\"M ";
  snprintf (buf, sizeof buf, "%.2f %.2f", cubics[0].p0.x, cubics[0].p0.y);
  out += buf;
  for (i = 0; i < cubics.size (); i++)
    {
      const svg_cubic_t &c = cubics[i];
      snprintf (buf, sizeof buf, " C %.2f %.2f %.2f %.2f %.2f %.2f",
                c.c0.x, c.c0.y, c.c1.x, c.c1.y, c.p1.x, c.p1.y);
      out += buf;
    }
  out += "\"/>";
}

map_svg_t *
map_render_svg (const map_t *map, const map_view_t *view,
                const char *player_key)
{
  const map_page_t *page;
  const map_node_t *focus_node = NULL;
  std::vector<const map_node_t *> visible;
  inout_badge_t *badges = NULL;
  int min_x, min_y, max_x, max_y;
  int i, l, d;
  int focus_z = 0;
  std::string out;
  map_svg_t *result;
  map_palette_t pal;
  char hex_bg[8], hex_room_fill[8], hex_room_stroke[8];
  char hex_here_fill[8], hex_here_stroke[8];
  char hex_link[8], hex_stub[8];

  if (map == NULL)
    return NULL;
  page = svg_page_for_player (map, player_key);
  if (page == NULL || page->n_nodes == 0)
    return NULL;

  map_get_palette (&pal);
  svg_hex (hex_bg, sizeof hex_bg, pal.background);
  svg_hex (hex_room_fill, sizeof hex_room_fill, pal.room_fill);
  svg_hex (hex_room_stroke, sizeof hex_room_stroke, pal.room_stroke);
  svg_hex (hex_here_fill, sizeof hex_here_fill, pal.here_fill);
  svg_hex (hex_here_stroke, sizeof hex_here_stroke, pal.here_stroke);
  svg_hex (hex_link, sizeof hex_link, pal.link);
  svg_hex (hex_stub, sizeof hex_stub, pal.stub);

  for (i = 0; i < page->n_nodes; i++)
    {
      const map_node_t *n = &page->nodes[i];
      if (!svg_seen (view, n->key))
        continue;
      visible.push_back (n);
      if (player_key != NULL && n->key != NULL
          && strcmp (n->key, player_key) == 0)
        focus_node = n;
    }
  if (visible.empty ())
    return NULL;
  if (focus_node == NULL)
    focus_node = visible[0];
  focus_z = focus_node->z;
  badges = inout_layout (page, view);

  /* Lone visible room with nowhere to go isn't useful as an automap
     (e.g. Skybreak's "The Beginning", or Notebook).  Match adrift-5-rs
     exit_info: only restriction-checked exits count, not raw Map Links. */
  if (visible.size () == 1)
    {
      const map_node_t *n = visible[0];
      int has_exit = 0;
      if (view != NULL && view->exit_dest != NULL)
        {
          for (d = 0; d < MAP_N_DIRS; d++)
            {
              const char *dest = view->exit_dest (view->ctx, n->key, d);
              if (dest != NULL && dest[0] != '\0')
                {
                  has_exit = 1;
                  break;
                }
            }
        }
      if (!has_exit)
        {
          free (badges);
          return NULL;
        }
    }

  min_x = min_y = 0x7fffffff;
  max_x = max_y = -0x7fffffff;
  for (i = 0; i < (int) visible.size (); i++)
    {
      const map_node_t *n = visible[(size_t) i];
      int ni = (int) (n - page->nodes);
      int x = n->x * SVG_SCALE;
      int y = n->y * SVG_SCALE;
      int x2 = x + n->w * SVG_SCALE;
      int y2 = y + n->h * SVG_SCALE;
      if (x < min_x) min_x = x;
      if (y < min_y) min_y = y;
      if (x2 > max_x) max_x = x2;
      if (y2 > max_y) max_y = y2;
      /* Badge discs (In/Out/Up/Down) can sit on the box edge. */
      if (badges != NULL && ni >= 0 && ni < page->n_nodes)
        {
          static const int bdirs[] = { DIR_IN, DIR_OUT, DIR_UP, DIR_DOWN };
          int di;
          for (di = 0; di < 4; di++)
            {
              int bd = bdirs[di];
              int sx, sy, site;
              int show = 0;
              if (bd == DIR_IN)
                show = badges[ni].has_in;
              else if (bd == DIR_OUT)
                show = badges[ni].has_out;
              else if (bd == DIR_UP)
                show = badges[ni].has_up;
              else
                show = badges[ni].has_down;
              if (!show)
                continue;
              site = badge_dir_site (&badges[ni], bd);
              badge_site_xy (n, site, &sx, &sy);
              if (sx - SVG_INOUT_R < min_x) min_x = sx - SVG_INOUT_R;
              if (sy - SVG_INOUT_R < min_y) min_y = sy - SVG_INOUT_R;
              if (sx + SVG_INOUT_R > max_x) max_x = sx + SVG_INOUT_R;
              if (sy + SVG_INOUT_R > max_y) max_y = sy + SVG_INOUT_R;
            }
        }
      else if (map->line_links)
        {
          /* A4 fixed sites when there is no A5 badge layout. */
          int up_s, dn_s, in_s, out_s, sx, sy;
          a4_badge_sites (&up_s, &dn_s, &in_s, &out_s);
          badge_site_xy (n, in_s, &sx, &sy);
          svg_expand_xy (&min_x, &min_y, &max_x, &max_y,
                         sx - SVG_INOUT_R, sy - SVG_INOUT_R);
          svg_expand_xy (&min_x, &min_y, &max_x, &max_y,
                         sx + SVG_INOUT_R, sy + SVG_INOUT_R);
          badge_site_xy (n, out_s, &sx, &sy);
          svg_expand_xy (&min_x, &min_y, &max_x, &max_y,
                         sx - SVG_INOUT_R, sy - SVG_INOUT_R);
          svg_expand_xy (&min_x, &min_y, &max_x, &max_y,
                         sx + SVG_INOUT_R, sy + SVG_INOUT_R);
          badge_site_xy (n, up_s, &sx, &sy);
          svg_expand_xy (&min_x, &min_y, &max_x, &max_y,
                         sx - SVG_INOUT_R, sy - SVG_INOUT_R);
          svg_expand_xy (&min_x, &min_y, &max_x, &max_y,
                         sx + SVG_INOUT_R, sy + SVG_INOUT_R);
          badge_site_xy (n, dn_s, &sx, &sy);
          svg_expand_xy (&min_x, &min_y, &max_x, &max_y,
                         sx - SVG_INOUT_R, sy - SVG_INOUT_R);
          svg_expand_xy (&min_x, &min_y, &max_x, &max_y,
                         sx + SVG_INOUT_R, sy + SVG_INOUT_R);
        }
      for (d = 0; d < MAP_N_DIRS; d++)
        {
          int sx, sy, ex, ey;
          if (map_is_badge_dir (d))
            continue;
          if (view == NULL || view->exit_dest == NULL)
            continue;
          {
            const char *dest = view->exit_dest (view->ctx, n->key, d);
            if (dest == NULL || dest[0] == '\0')
              continue;
            /* Self-links stay as stubs even when "seen". */
            if (svg_seen (view, dest) && strcmp (dest, n->key) != 0)
              continue;
          }
          edge_point (n, d, &sx, &sy);
          stub_end (n, d, &ex, &ey);
          if (sx < min_x) min_x = sx;
          if (sy < min_y) min_y = sy;
          if (sx > max_x) max_x = sx;
          if (sy > max_y) max_y = sy;
          if (ex < min_x) min_x = ex;
          if (ey < min_y) min_y = ey;
          if (ex > max_x) max_x = ex;
          if (ey > max_y) max_y = ey;
        }
    }
  /* Author midpoints and Bezier bulges can sit outside the room AABB
     (e.g. No Link→Wide Anchors at X=-18 while rooms start at X=-16). */
  for (i = 0; i < (int) visible.size (); i++)
    {
      const map_node_t *n = visible[(size_t) i];
      int ni = (int) (n - page->nodes);
      for (l = 0; l < n->n_links; l++)
        {
          const map_link_t *link = &n->links[l];
          const map_node_t *dn;
          int x1, y1, x2, y2, dst_anchor;
          double dist, ax, ay, bx, by;

          if (link->badge || link->dest == NULL)
            continue;
          if (n->key != NULL && strcmp (link->dest, n->key) == 0)
            continue;
          if (map_is_badge_dir (link->dir)
              && (link->has_compass_twin
                  || (badges != NULL
                      && badge_on_compass (&badges[ni], link->dir))))
            continue;
          dn = svg_page_node (page, link->dest);
          if (dn == NULL || !svg_seen (view, dn->key))
            continue;

          dst_anchor = link->dst_anchor;
          if (dst_anchor < 0)
            dst_anchor = opp_dir (link->dir);
          if (map_is_badge_dir (link->dir) || map_is_badge_dir (dst_anchor))
            {
              int site;
              if (badges == NULL)
                continue;
              if (map_is_badge_dir (link->dir))
                {
                  site = badge_dir_site (&badges[ni], link->dir);
                  badge_site_xy (n, site, &x1, &y1);
                }
              else
                edge_point (n, link->dir, &x1, &y1);
              if (map_is_badge_dir (dst_anchor))
                {
                  int j = (int) (dn - page->nodes);
                  site = arrival_badge_site (&badges[j], dn, n, dst_anchor);
                  badge_site_xy (dn, site, &x2, &y2);
                }
              else
                edge_point (dn, dst_anchor, &x2, &y2);
            }
          else
            {
              edge_point (n, link->dir, &x1, &y1);
              edge_point (dn, dst_anchor, &x2, &y2);
            }
          svg_expand_xy (&min_x, &min_y, &max_x, &max_y, x1, y1);
          svg_expand_xy (&min_x, &min_y, &max_x, &max_y, x2, y2);

          if (link->n_mids > 0 && link->mids != NULL)
            {
              std::vector<svg_pt_t> curved;
              std::vector<svg_cubic_t> cubics;
              svg_pt_t s, e;
              s.x = x1; s.y = y1;
              e.x = x2; e.y = y2;
              mids_to_cubics (cubics, s, e, link->mids, link->n_mids);
              sample_cubics (curved, cubics, 12);
              for (size_t k = 0; k < curved.size (); k++)
                svg_expand_xy (&min_x, &min_y, &max_x, &max_y,
                               (int) curved[k].x, (int) curved[k].y);
            }
          else if (!map_is_badge_dir (link->dir) && !map_is_badge_dir (dst_anchor))
            {
              dist = sqrt ((double) (x2 - x1) * (x2 - x1)
                           + (double) (y2 - y1) * (y2 - y1));
              svg_bezier_assister (n, link->dir, -1, dist, &ax, &ay);
              svg_bezier_assister (dn, dst_anchor, -1, dist, &bx, &by);
              svg_expand_xy (&min_x, &min_y, &max_x, &max_y, (int) ax, (int) ay);
              svg_expand_xy (&min_x, &min_y, &max_x, &max_y, (int) bx, (int) by);
            }
        }
    }
  min_x -= SVG_PAD;
  min_y -= SVG_PAD;
  max_x += SVG_PAD;
  max_y += SVG_PAD;
  {
    int vb_w = max_x - min_x;
    int vb_h = max_y - min_y;
    if (vb_w < 1) vb_w = 1;
    if (vb_h < 1) vb_h = 1;

    out += "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 ";
    out += std::to_string (vb_w);
    out += " ";
    out += std::to_string (vb_h);
    out += "\" width=\"";
    out += std::to_string (vb_w);
    out += "\" height=\"";
    out += std::to_string (vb_h);
    out += "\">";
    /* Absolute size: AppKit's SVG renderer ignores width/height="100%", which
       left the canvas transparent (host window white) with a junk corner tile. */
    out += "<rect x=\"0\" y=\"0\" width=\"";
    out += std::to_string (vb_w);
    out += "\" height=\"";
    out += std::to_string (vb_h);
    out += "\" fill=\"";
    out += hex_bg;
    out += "\"/>";
  }
  out += "<g transform=\"translate(";
  out += std::to_string (-min_x);
  out += ",";
  out += std::to_string (-min_y);
  out += ")\">";

  /* Links between seen rooms (including badge-to-badge In/Out/Up/Down). */
  for (i = 0; i < (int) visible.size (); i++)
    {
      const map_node_t *n = visible[(size_t) i];
      int ni = (int) (n - page->nodes);
      for (l = 0; l < n->n_links; l++)
        {
          const map_link_t *link = &n->links[l];
          const map_node_t *dn;
          int x1, y1, x2, y2;
          int dash = 0;
          int dst_anchor;

          if (link->badge)
            continue;           /* ADRIFT 4 Up/Down/In/Out: icons only */
          if (link->dest == NULL)
            continue;
          /* Self-link: out-arrow stub, not a loop (Map.vb:1474). Restricted
             self-exits that currently fail are omitted (Map.vb:1429) — e.g.
             Grandpa Ranch Western Enclosure North is Destination=self with
             Expression 1=0, so ADRIFT draws nothing.  Skip self-Down; In/Out
             are badge-only (no stub). */
          if (n->key != NULL && strcmp (link->dest, n->key) == 0)
            {
              if (view != NULL && view->exit_dest != NULL
                  && view->exit_dest (view->ctx, n->key, link->dir) == NULL)
                continue;
              if (link->dir == DIR_DOWN
                  || link->dir == DIR_IN || link->dir == DIR_OUT)
                continue;
              edge_point (n, link->dir, &x1, &y1);
              stub_end (n, link->dir, &x2, &y2);
              out += "<line fill=\"none\" stroke=\"";
              out += hex_stub;
              out += "\" stroke-width=\"1.5\" x1=\"";
              out += std::to_string (x1);
              out += "\" y1=\"";
              out += std::to_string (y1);
              out += "\" x2=\"";
              out += std::to_string (x2);
              out += "\" y2=\"";
              out += std::to_string (y2);
              out += "\"/>";
              append_arrowhead (out, x2, y2, x1, y1, hex_stub);
              continue;
            }
          dn = svg_page_node (page, link->dest);
          if (dn == NULL || !svg_seen (view, dn->key))
            continue;

          /* Badge exits only draw while the route is open. */
          if (map_is_badge_dir (link->dir)
              && view != NULL && view->exit_dest != NULL
              && view->exit_dest (view->ctx, n->key, link->dir) == NULL)
            continue;

          /* Compass twin already draws the line; park the badge on that port.
             Prefer the link flag so a far-end site_fixed does not leave
             on_compass unset and redraw the same segment. */
          if (map_is_badge_dir (link->dir)
              && (link->has_compass_twin
                  || (badges != NULL
                      && badge_on_compass (&badges[ni], link->dir))))
            continue;

          if (link->dotted)
            {
              if (view != NULL && view->ever_blocked != NULL)
                {
                  if (view->exit_dest == NULL
                      || view->exit_dest (view->ctx, n->key, link->dir) == NULL)
                    continue;
                  if (view->ever_blocked (view->ctx, n->key, link->dir))
                    dash = 1;
                  else
                    {
                      /* Reciprocal end may hold bEverBeenBlocked (REVEAL probes
                         Cellar-East; Door-West may be the drawn Map Link). */
                      int rl;
                      for (rl = 0; rl < dn->n_links; rl++)
                        {
                          const map_link_t *rev = &dn->links[rl];
                          if (rev->dest != NULL
                              && strcmp (rev->dest, n->key) == 0
                              && view->ever_blocked (view->ctx, dn->key,
                                                     rev->dir))
                            {
                              dash = 1;
                              break;
                            }
                        }
                    }
                }
              else
                dash = 1;
            }
          else if (view != NULL && view->exit_dest != NULL
                   && view->ever_blocked != NULL)
            {
              /* Restricted-but-failing routes are omitted above via dotted.
                 Unrestricted exits that currently fail also omit. */
              if (view->exit_dest (view->ctx, n->key, link->dir) == NULL)
                continue;
            }

          /* Each Map Link is drawn (Map.vb). Reciprocal anchors that disagree
             (Hub SE→Disagree East vs Disagree NW→Hub SE; Hub North midpoints
             vs North South) are separate geometries — do not pair-dedup. */

          dst_anchor = link->dst_anchor;
          if (dst_anchor < 0)
            dst_anchor = opp_dir (link->dir);
          if (map_is_badge_dir (link->dir) || map_is_badge_dir (dst_anchor))
            {
              int site;
              if (badges == NULL)
                continue;
              if (map_is_badge_dir (link->dir))
                {
                  site = badge_dir_site (&badges[ni], link->dir);
                  badge_site_xy (n, site, &x1, &y1);
                }
              else
                edge_point (n, link->dir, &x1, &y1);
              if (map_is_badge_dir (dst_anchor))
                {
                  int j = (int) (dn - page->nodes);
                  site = arrival_badge_site (&badges[j], dn, n, dst_anchor);
                  badge_site_xy (dn, site, &x2, &y2);
                }
              else
                edge_point (dn, dst_anchor, &x2, &y2);
            }
          else
            {
              edge_point (n, link->dir, &x1, &y1);
              edge_point (dn, dst_anchor, &x2, &y2);
            }

          {
            std::vector<svg_pt_t> sampled;
            std::vector<svg_cubic_t> cubics;
            double dist = sqrt ((double) (x2 - x1) * (x2 - x1)
                                + (double) (y2 - y1) * (y2 - y1));
            double ax, ay, bx, by;

            if (link->n_mids > 0 && link->mids != NULL)
              {
                svg_pt_t s, e;
                s.x = x1; s.y = y1;
                e.x = x2; e.y = y2;
                mids_to_cubics (cubics, s, e, link->mids, link->n_mids);
                sample_cubics (sampled, cubics, 12);
                append_curve_stroke (out, cubics, dash, hex_link);
              }
            else if (map_is_badge_dir (link->dir) || map_is_badge_dir (dst_anchor))
              {
                /* Straight badge-to-badge run (mapdraw / run500). */
                svg_pt_t a, b;
                a.x = x1; a.y = y1;
                b.x = x2; b.y = y2;
                sampled.push_back (a);
                sampled.push_back (b);
                append_poly_stroke (out, sampled, dash, hex_link);
              }
            else
              {
                /* Map.vb DrawBezier with GetBezierAssister control points.
                   Emit a native SVG cubic — sampled L polylines look faceted
                   (e.g. Hub SE→Disagree near the source corner). */
                svg_cubic_t cubic;
                svg_bezier_assister (n, link->dir, -1, dist, &ax, &ay);
                svg_bezier_assister (dn, dst_anchor, -1, dist, &bx, &by);
                cubic.p0.x = x1; cubic.p0.y = y1;
                cubic.c0.x = ax; cubic.c0.y = ay;
                cubic.c1.x = bx; cubic.c1.y = by;
                cubic.p1.x = x2; cubic.p1.y = y2;
                cubics.push_back (cubic);
                sample_cubics (sampled, cubics, 16);
                append_curve_stroke (out, cubics, dash, hex_link);
              }
            if (!link->duplex && sampled.size () >= 2)
              {
                const svg_pt_t &tip = sampled.back ();
                const svg_pt_t &prev = sampled[sampled.size () - 2];
                append_arrowhead (out, (int) tip.x, (int) tip.y,
                                  (int) prev.x, (int) prev.y, hex_link);
              }
          }
        }
    }

  /* Stubs to unseen destinations (and self-links via Movement without Map Link). */
  if (view != NULL && view->exit_dest != NULL)
    {
      for (i = 0; i < (int) visible.size (); i++)
        {
          const map_node_t *n = visible[(size_t) i];
          for (d = 0; d < MAP_N_DIRS; d++)
            {
              const char *dest;
              int x1, y1, x2, y2;
              int is_self;
              if (d == DIR_IN || d == DIR_OUT || d == DIR_UP || d == DIR_DOWN)
                continue;
              dest = view->exit_dest (view->ctx, n->key, d);
              if (dest == NULL || dest[0] == '\0')
                continue;
              is_self = (n->key != NULL && strcmp (dest, n->key) == 0);
              if (!is_self && svg_seen (view, dest))
                continue;
              /* Map Link self-stubs already drawn above. */
              if (is_self)
                {
                  int has_link = 0;
                  for (l = 0; l < n->n_links; l++)
                    if (n->links[l].dir == d
                        && n->links[l].dest != NULL
                        && strcmp (n->links[l].dest, n->key) == 0)
                      {
                        has_link = 1;
                        break;
                      }
                  if (has_link)
                    continue;
                }
              edge_point (n, d, &x1, &y1);
              stub_end (n, d, &x2, &y2);
              out += "<line fill=\"none\" stroke=\"";
              out += hex_stub;
              out += "\" stroke-width=\"1.5\" x1=\"";
              out += std::to_string (x1);
              out += "\" y1=\"";
              out += std::to_string (y1);
              out += "\" x2=\"";
              out += std::to_string (x2);
              out += "\" y2=\"";
              out += std::to_string (y2);
              out += "\"/>";
              append_arrowhead (out, x2, y2, x1, y1, hex_stub);
            }
        }
    }

  /* Rooms. */
  for (i = 0; i < (int) visible.size (); i++)
    {
      const map_node_t *n = visible[(size_t) i];
      int x = n->x * SVG_SCALE;
      int y = n->y * SVG_SCALE;
      int w = n->w * SVG_SCALE;
      int h = n->h * SVG_SCALE;
      int is_here = (player_key != NULL && n->key != NULL
                     && strcmp (n->key, player_key) == 0);
      int dim = (n->z != focus_z);
      const char *label = NULL;

      out += "<rect x=\"";
      out += std::to_string (x);
      out += "\" y=\"";
      out += std::to_string (y);
      out += "\" width=\"";
      out += std::to_string (w);
      out += "\" height=\"";
      out += std::to_string (h);
      if (is_here)
        {
          out += "\" rx=\"3\" fill=\"";
          out += hex_here_fill;
          out += "\" stroke=\"";
          out += hex_here_stroke;
          out += "\" stroke-width=\"2.5\"";
        }
      else
        {
          out += "\" rx=\"3\" fill=\"";
          out += hex_room_fill;
          out += "\" stroke=\"";
          out += hex_room_stroke;
          out += "\" stroke-width=\"1.5\"";
        }
      if (dim)
        out += " opacity=\"0.35\"";
      out += "/>";

      if (view != NULL && view->name != NULL)
        label = view->name (view->ctx, n->key);
      append_label (out, label, x + w / 2, y + h / 2, w, h,
                    is_here ? pal.here_label : pal.label);
    }

  /* In/Out/Up/Down badge discs — A5 from inout_layout sites; A4 fixed sites. */
  for (i = 0; i < (int) visible.size (); i++)
    {
      const map_node_t *n = visible[(size_t) i];
      int ni = (int) (n - page->nodes);
      const map_link_t *b_in = NULL, *b_out = NULL;
      const map_link_t *b_up = NULL, *b_down = NULL;
      int a4_up, a4_dn, a4_in, a4_out;

      for (l = 0; l < n->n_links; l++)
        {
          const map_link_t *lk = &n->links[l];
          if (lk->dir == DIR_IN)
            b_in = lk;
          else if (lk->dir == DIR_OUT)
            b_out = lk;
          else if (lk->dir == DIR_UP)
            b_up = lk;
          else if (lk->dir == DIR_DOWN)
            b_down = lk;
        }
      /* Hide own-link badges while the route currently fails (A5). */
      if (view != NULL && view->ever_blocked != NULL && view->exit_dest != NULL)
        {
          if (b_in != NULL
              && view->exit_dest (view->ctx, n->key, DIR_IN) == NULL)
            b_in = NULL;
          if (b_out != NULL
              && view->exit_dest (view->ctx, n->key, DIR_OUT) == NULL)
            b_out = NULL;
          if (b_up != NULL
              && view->exit_dest (view->ctx, n->key, DIR_UP) == NULL)
            b_up = NULL;
          if (b_down != NULL
              && view->exit_dest (view->ctx, n->key, DIR_DOWN) == NULL)
            b_down = NULL;
        }

      if (badges == NULL
          && (b_up != NULL || b_down != NULL
              || (b_in != NULL && b_in->badge)
              || (b_out != NULL && b_out->badge)))
        {
          int cx, cy;
          a4_badge_sites (&a4_up, &a4_dn, &a4_in, &a4_out);
          if (b_in != NULL)
            {
              badge_site_xy (n, a4_in, &cx, &cy);
              append_badge_disc (out, cx, cy, DIR_IN);
            }
          if (b_out != NULL)
            {
              badge_site_xy (n, a4_out, &cx, &cy);
              append_badge_disc (out, cx, cy, DIR_OUT);
            }
          if (b_up != NULL)
            {
              badge_site_xy (n, a4_up, &cx, &cy);
              append_badge_disc (out, cx, cy, DIR_UP);
            }
          if (b_down != NULL)
            {
              badge_site_xy (n, a4_dn, &cx, &cy);
              append_badge_disc (out, cx, cy, DIR_DOWN);
            }
        }
      else if (badges != NULL)
        {
          int cx, cy;
          /* Own Link, far badge from another's Link, or Movement-only toward
             an unseen room — same gates as mapdraw pass 3. */
          if (b_in != NULL || badges[ni].far_in
              || (badges[ni].has_in && !node_has_link_dir (n, DIR_IN)))
            {
              badge_site_xy (n, badges[ni].in_site, &cx, &cy);
              append_badge_disc (out, cx, cy, DIR_IN);
            }
          if (b_out != NULL || badges[ni].far_out
              || (badges[ni].has_out && !node_has_link_dir (n, DIR_OUT)))
            {
              badge_site_xy (n, badges[ni].out_site, &cx, &cy);
              append_badge_disc (out, cx, cy, DIR_OUT);
            }
          if (b_up != NULL || badges[ni].far_up
              || (badges[ni].has_up && !node_has_link_dir (n, DIR_UP)))
            {
              badge_site_xy (n, badges[ni].up_site, &cx, &cy);
              append_badge_disc (out, cx, cy, DIR_UP);
            }
          if (b_down != NULL || badges[ni].far_down
              || (badges[ni].has_down && !node_has_link_dir (n, DIR_DOWN)))
            {
              badge_site_xy (n, badges[ni].down_site, &cx, &cy);
              append_badge_disc (out, cx, cy, DIR_DOWN);
            }
        }
    }

  out += "</g></svg>";
  free (badges);

  result = (map_svg_t *) calloc (1, sizeof (map_svg_t));
  if (result == NULL)
    return NULL;
  result->svg = (char *) malloc (out.size () + 1);
  if (result->svg == NULL)
    {
      free (result);
      return NULL;
    }
  memcpy (result->svg, out.c_str (), out.size () + 1);
  result->origin_x = min_x;
  result->origin_y = min_y;
  result->focus_left = focus_node->x * SVG_SCALE - min_x;
  result->focus_top = focus_node->y * SVG_SCALE - min_y;
  result->focus_width = (unsigned int) (focus_node->w * SVG_SCALE);
  result->focus_height = (unsigned int) (focus_node->h * SVG_SCALE);
  if (result->focus_width < 1)
    result->focus_width = 1;
  if (result->focus_height < 1)
    result->focus_height = 1;

  /* Clickable room boxes as 4-point polygons in document space. */
  {
    int link_count = 0;
    for (i = 0; i < (int) visible.size (); i++)
      {
        const map_node_t *node = visible[(size_t) i];
        if (node->hidden)
          continue;
        link_count++;
      }
    if (link_count > 0)
      {
        result->hyperlinks = (map_svg_hyperlink_t *) calloc ((size_t) link_count,
                                                         sizeof (map_svg_hyperlink_t));
        if (result->hyperlinks == NULL)
          {
            map_svg_free (result);
            return NULL;
          }
        result->nhyperlinks = link_count;
        link_count = 0;
        for (i = 0; i < (int) visible.size (); i++)
          {
            const map_node_t *node = visible[(size_t) i];
            map_svg_hyperlink_t *link;
            int x0, y0, x1, y1;
            const char *label = NULL;

            if (node->hidden)
              continue;
            link = &result->hyperlinks[link_count];
            link->id = (unsigned int) (link_count + 1);
            if (node->key != NULL)
              link->key = strdup (node->key);
            if (view != NULL && view->name != NULL)
              label = view->name (view->ctx, node->key);
            if (label != NULL && label[0] != '\0')
              link->label = strdup (label);
            x0 = node->x * SVG_SCALE - min_x;
            y0 = node->y * SVG_SCALE - min_y;
            x1 = x0 + node->w * SVG_SCALE;
            y1 = y0 + node->h * SVG_SCALE;
            link->npoints = 4;
            link->xy = (int *) malloc (8 * sizeof (int));
            if (link->xy == NULL)
              {
                map_svg_free (result);
                return NULL;
              }
            link->xy[0] = x0; link->xy[1] = y0;
            link->xy[2] = x1; link->xy[3] = y0;
            link->xy[4] = x1; link->xy[5] = y1;
            link->xy[6] = x0; link->xy[7] = y1;
            link_count++;
          }
      }
  }

  return result;
}

void
map_svg_free (map_svg_t *svg)
{
  int i;
  if (svg == NULL)
    return;
  free (svg->svg);
  for (i = 0; i < svg->nhyperlinks; i++)
    {
      free (svg->hyperlinks[i].key);
      free (svg->hyperlinks[i].label);
      free (svg->hyperlinks[i].xy);
    }
  free (svg->hyperlinks);
  free (svg);
}

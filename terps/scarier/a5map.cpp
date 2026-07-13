/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjolund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* The ADRIFT 5 map loader.  See a5map.h.  This reads the authored <Map> block
   into the engine-neutral map_t that mapdraw.cpp draws. */

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "a5map.h"
#include "a5xml.h"
#include "a5model.h"

static int
dir_index (const char *s)
{
  int i;
  if (s == NULL)
    return -1;
  for (i = 0; i < 12; i++)
    if (strcmp (s, map_dirs[i]) == 0)
      return i;
  return -1;
}

/*
 * ---------------------------------------------------------------- parsing
 */

/* The destination of an exit, straight from the location's Movement table --
   the map XML records only which anchors the connector was drawn between, so
   the runner re-derives the target from arlDirections (FileIO.vb:4100). */
static const char *
movement_dest (const a5_location_t *loc, const char *dir, int *dotted)
{
  const a5_xml_node_t *c;
  if (dotted != NULL)
    *dotted = 0;
  if (loc == NULL || loc->node == NULL)
    return NULL;
  for (c = loc->node->first_child; c != NULL; c = c->next)
    {
      const char *d, *dest;
      const a5_xml_node_t *r;
      if (strcmp (c->name, "Movement") != 0)
        continue;
      d = a5xml_child_text (c, "Direction");
      if (d == NULL || strcmp (d, dir) != 0)
        continue;
      dest = a5xml_child_text (c, "Destination");
      if (dest == NULL || dest[0] == '\0')
        return NULL;
      /* A restricted movement is drawn dotted (Map.vb:1441, DottedLink). */
      r = a5xml_child (c, "Restrictions");
      if (dotted != NULL && r != NULL && r->n_children > 0)
        *dotted = 1;
      return dest;
    }
  return NULL;
}

static int
node_int (const a5_xml_node_t *n, const char *name, int dflt)
{
  const char *s = a5xml_child_text (n, name);
  if (s == NULL || s[0] == '\0')
    return dflt;
  return atoi (s);
}

map_t *
a5map_load (const a5_adventure_t *adv)
{
  const a5_xml_node_t *map, *pg;
  map_t *m;
  int total_nodes = 0;

  if (adv == NULL || adv->root == NULL)
    return NULL;
  map = a5xml_child (adv->root, "Map");
  if (map == NULL)
    return NULL;

  m = (map_t *) calloc (1, sizeof (map_t));
  if (m == NULL)
    return NULL;

  m->n_pages = a5xml_count (map, "Page");
  if (m->n_pages <= 0)
    {
      free (m);
      return NULL;
    }
  m->pages = (map_page_t *) calloc ((size_t) m->n_pages,
                                      sizeof (map_page_t));
  if (m->pages == NULL)
    {
      free (m);
      return NULL;
    }

  {
    int ip = 0;
    for (pg = map->first_child; pg != NULL; pg = pg->next)
      {
        map_page_t *page;
        const a5_xml_node_t *nd;
        int in;

        if (strcmp (pg->name, "Page") != 0)
          continue;
        if (ip >= m->n_pages)
          break;
        page = &m->pages[ip];
        page->key = node_int (pg, "Key", ip);
        page->label = a5xml_child_text (pg, "Label");
        page->n_nodes = a5xml_count (pg, "Node");
        if (page->n_nodes > 0)
          page->nodes = (map_node_t *) calloc ((size_t) page->n_nodes,
                                                 sizeof (map_node_t));

        in = 0;
        for (nd = pg->first_child; nd != NULL; nd = nd->next)
          {
            map_node_t *node;
            const a5_location_t *loc;
            const a5_xml_node_t *lk;
            int il, n_links;

            if (strcmp (nd->name, "Node") != 0)
              continue;
            if (page->nodes == NULL || in >= page->n_nodes)
              break;
            node = &page->nodes[in];
            node->key = a5xml_child_text (nd, "Key");
            node->x = node_int (nd, "X", 0);
            node->y = node_int (nd, "Y", 0);
            node->z = node_int (nd, "Z", 0);
            node->w = node_int (nd, "Width", MAP_NODE_W);
            node->h = node_int (nd, "Height", MAP_NODE_H);
            if (node->w <= 0)
              node->w = MAP_NODE_W;
            if (node->h <= 0)
              node->h = MAP_NODE_H;
            node->page = page->key;

            loc = a5model_location (adv, node->key);

            n_links = a5xml_count (nd, "Link");
            if (n_links > 0)
              node->links = (map_link_t *) calloc ((size_t) n_links,
                                                     sizeof (map_link_t));
            il = 0;
            for (lk = nd->first_child; lk != NULL && node->links != NULL;
                 lk = lk->next)
              {
                map_link_t *link;
                const a5_xml_node_t *an;
                const char *src;
                int d, n_mids, im;

                if (strcmp (lk->name, "Link") != 0)
                  continue;
                if (il >= n_links)
                  break;
                src = a5xml_child_text (lk, "SourceAnchor");
                d = dir_index (src);
                if (d < 0)
                  continue;

                link = &node->links[il];
                link->dir = d;
                link->dst_anchor = dir_index (a5xml_child_text
                                              (lk, "DestinationAnchor"));
                link->dest = movement_dest (loc, src, &link->dotted);

                n_mids = a5xml_count (lk, "Anchor");
                if (n_mids > 0)
                  {
                    link->mids = (map_pt_t *) calloc ((size_t) n_mids,
                                                        sizeof (map_pt_t));
                    im = 0;
                    for (an = lk->first_child;
                         an != NULL && link->mids != NULL; an = an->next)
                      {
                        if (strcmp (an->name, "Anchor") != 0)
                          continue;
                        if (im >= n_mids)
                          break;
                        link->mids[im].x = node_int (an, "X", 0);
                        link->mids[im].y = node_int (an, "Y", 0);
                        link->mids[im].z = node_int (an, "Z", 0);
                        im++;
                      }
                    link->n_mids = im;
                  }
                il++;
              }
            node->n_links = il;
            in++;
            total_nodes++;
          }
        page->n_nodes = in;
        ip++;
      }
    m->n_pages = ip;
  }

  if (total_nodes == 0)
    {
      map_free (m);
      return NULL;
    }
  return m;
}

/* Does any task command consist of the bare word "map"?  Task commands are
   slash-separated alternatives ("map/chart"), so each alternative is tested;
   surrounding space is ignored, and the match is case-insensitive.  Anything
   with more to it ("look at map", "map %object%") is not a bare MAP and does
   not conflict with a host MAP command. */
int
a5map_command_taken (const a5_adventure_t *adv)
{
  int t, c;

  if (adv == NULL)
    return 0;
  for (t = 0; t < adv->n_tasks; t++)
    {
      const a5_task_t *task = &adv->tasks[t];
      for (c = 0; c < task->n_commands; c++)
        {
          const char *s = task->commands[c];
          while (s != NULL && *s != '\0')
            {
              const char *end = strchr (s, '/');
              size_t len = (end != NULL) ? (size_t) (end - s) : strlen (s);
              size_t b = 0;

              while (b < len && (s[b] == ' ' || s[b] == '\t'))
                b++;
              while (len > b && (s[len - 1] == ' ' || s[len - 1] == '\t'))
                len--;
              if (len - b == 3
                  && (s[b] == 'm' || s[b] == 'M')
                  && (s[b + 1] == 'a' || s[b + 1] == 'A')
                  && (s[b + 2] == 'p' || s[b + 2] == 'P'))
                return 1;
              if (end == NULL)
                break;
              s = end + 1;
            }
        }
    }
  return 0;
}

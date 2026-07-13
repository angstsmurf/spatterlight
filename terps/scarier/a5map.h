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

/*
 * The ADRIFT 5 map (Adrift 5 runner: clsMap.vb, Map.vb).
 *
 * The room layout is authored data, not something the interpreter derives:
 * the Developer writes an <Adventure><Map> block of <Page>s, each holding
 * <Node>s with the location key and its X/Y/Z position in map units.  All we
 * have to do is read it and draw it.
 *
 * The runner's map control can rotate into a pseudo-3D view, but its default
 * offsets (iOffsetX=200, iOffsetY=40) make all three rotation angles exactly
 * zero -- Convert3DtoScreen is then the identity and Z drops out.  The view a
 * player actually sees unless they drag the map is therefore a plain
 * orthographic plan view, which is what we render: X to the right, Y downward,
 * one map unit = `scale` pixels (the runner's iScale, default 10).
 *
 * This module is deliberately free of Glk: it rasterises into a plain RGB
 * surface, so it can be rendered and diffed headlessly (test/a5map_dump.cpp).
 */

#ifndef A5MAP_H
#define A5MAP_H

#include "a5model.h"
#include "a5xml.h"

/* A point in map units. */
typedef struct a5map_pt_s {
  int x, y, z;
} a5map_pt_t;

/* One drawn connector leaving a node (clsMap.MapLink).  The XML stores only
   the two anchors; the destination is re-derived from the location's Movement
   table, and the link is dotted when that movement has restrictions. */
typedef struct a5map_link_s {
  int dir;                    /* source direction, DirectionsEnum 0..11      */
  int dst_anchor;             /* direction the connector enters the dest by  */
  const char *dest;           /* destination location key (aliases the doc)  */
  int dotted;                 /* the movement is restricted                  */
  a5map_pt_t *mids;           /* author-dragged waypoints (<Anchor>)         */
  int n_mids;
} a5map_link_t;

/* One room box on a page (clsMap.MapNode). */
typedef struct a5map_node_s {
  const char *key;            /* location key (aliases the doc)              */
  int x, y, z;                /* top-left corner, in map units               */
  int w, h;                   /* size in map units (runner defaults: 6 x 4)  */
  int page;
  a5map_link_t *links;
  int n_links;
} a5map_node_t;

typedef struct a5map_page_s {
  int key;
  const char *label;          /* <Label> ("Page 1", ...)                     */
  a5map_node_t *nodes;
  int n_nodes;
} a5map_page_t;

typedef struct a5map_s {
  a5map_page_t *pages;
  int n_pages;
} a5map_t;

/* Parse <Map> out of a loaded adventure.  Returns NULL if the game has no map
   nodes at all.  The result aliases the adventure's XML document, so it must
   not outlive `adv`. */
extern a5map_t *a5map_load (const a5_adventure_t *adv);
extern void a5map_free (a5map_t *map);

/* Find the node for a location.  Returns NULL if the room was never placed on
   the map (common: procedurally generated rooms have no node). */
extern const a5map_node_t *a5map_find (const a5map_t *map, const char *lockey);

/* Does the game define a task command of its own that a bare "map" would
   match?  The runner's map is GUI chrome, not a game command, so ADRIFT
   authors are free to use MAP as a verb (Lost Coastlines does, for its sea
   chart).  A host that offers a "map" command must let the game's win. */
extern int a5map_command_taken (const a5_adventure_t *adv);

/* --- rendering ---------------------------------------------------------- */

/* A 24-bit RGB surface, 0x00RRGGBB per pixel, row-major. */
typedef struct a5map_surface_s {
  int w, h;
  unsigned int *px;
} a5map_surface_t;

extern a5map_surface_t *a5map_surface_new (int w, int h);
extern void a5map_surface_free (a5map_surface_t *s);

/* What the renderer needs to know about the run.  Keeping this a callback
   table is what lets the map be drawn from the headless harness (and diffed)
   without linking the Glk layer. */
typedef struct a5map_view_s {
  /* Has the viewpoint character seen this room?  (a5state loc_seen) */
  int (*seen) (void *ctx, const char *lockey);
  /* The room's short description, for the label. */
  const char *(*name) (void *ctx, const char *lockey);
  /* Where a usable exit in `dir` leads (restrictions applied, as in the
     runner's HasRouteInDirection), or NULL for no exit.  An exit whose
     destination has not been seen is drawn as a stub arrow.  May be NULL. */
  const char *(*exit_dest) (void *ctx, const char *lockey, int dir);
  void *ctx;
} a5map_view_t;

/* Where the map is currently looking. */
typedef struct a5map_camera_s {
  int page;                   /* page to draw                                */
  int scale;                  /* pixels per map unit (runner default 10)     */
  int cx, cy;                 /* centre of the view, in map units * scale    */
} a5map_camera_t;

/* Pick the page the player is on and frame it: fit the seen nodes to `dst` if
   they will fit, and otherwise centre on the player and let the map run past
   the edges, as the runner does (it never shrinks to fit -- iScale stays 10
   and LockPlayerCentre pans).  Keeping 10 as the floor is also what keeps a
   room name legible: below it a box is too small for the label. */
#define A5MAP_SCALE_MIN 10
#define A5MAP_SCALE_MAX 16
extern void a5map_frame (const a5map_t *map, const a5map_view_t *view,
                         const char *player_key, const a5map_surface_t *dst,
                         a5map_camera_t *cam);

/* Draw the map.  Only rooms the player has seen are drawn (as in the runner);
   the player's own room is highlighted. */
extern void a5map_render (const a5map_t *map, const a5map_view_t *view,
                          const char *player_key, const a5map_camera_t *cam,
                          a5map_surface_t *dst);

/* Which room is at pixel (px,py) of a `w` x `h` map view?  NULL if none.
   Lets a click walk there. */
extern const char *a5map_hit (const a5map_t *map, const a5map_view_t *view,
                              const a5map_camera_t *cam, int w, int h,
                              int px, int py);

/* The first step of the shortest route from `from` to `to`: a direction index
   (0..11), or -1 if there is no route.  This is the runner's map-click walk
   (clsCharacter.Dijkstra / DoWalk): edges are the restriction-checked exits,
   and only rooms the player has already seen may be walked through.  The
   caller submits that direction as an ordinary command, one room per turn,
   exactly as DoWalk does. */
extern int a5map_walk_step (const a5map_view_t *view, const char *from,
                            const char *to);

/* The twelve directions, DirectionsEnum order (North=0 .. NorthWest=11). */
extern const char *const a5map_dirs[12];

#endif /* A5MAP_H */

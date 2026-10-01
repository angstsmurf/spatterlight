/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

/*
 * os_glk_map.cpp: the Glk map pane, shared by both engines -- the map
 * window and its preferences, the "glk map" and "glk zoom" commands, and
 * the map-click walks.  Split out of os_glk.cpp; see os_glk_internal.h.
 */

#include "os_glk_internal.h"

/* The map, shown in a graphics window to the right of the story on request
   ("map").  Both engines use this pane; what differs is where the map comes
   from.  An ADRIFT 5 map is authored data, parsed once per adventure, aliasing
   its XML, and so dropped whenever the adventure is.  An ADRIFT 4 map has to be
   derived from the exit graph, and it depends on the room you are standing in
   and on how much you have explored -- so it is rebuilt on every redraw, as the
   ADRIFT 4 runner rebuilt it every time it drew (drawmap with mode = 1). */
winid_t gsc_map_window = NULL;
map_t *gsc_map = NULL;
int gsc_map_shown = FALSE;
/* Whether the map pane is wanted at all: the player's remembered choice, or
   failing that the layout the game shipped (gsc_map_auto_reveal).  Wanted is
   not the same as shown -- a map with nothing on it yet is held back until
   there is something to see (gsc_map_worth_opening), and the per-turn redraw
   opens it then.  FALSE until something decides. */
int gsc_map_want = FALSE;
/* Where the map pane sits: the standard 40% pane to the right of the story,
   or -- for games with wide maps -- a 30% band across the top of the whole
   screen, above the status line ("glk map top").  The choice is kept, so
   hiding and re-showing the map does not move it, and it is remembered with
   the visibility (gsc_map_pref_write) so neither does restarting. */
int gsc_map_at_top = FALSE;
/* How the two colours the map is drawn in are spent ("glk map colour"):
   paper/ink cards by default, or the same cards with a you-are-here amber
   (MAP_SCHEME_DERIVED in mapdraw.h).  Kept with the visibility and the
   placement (gsc_map_pref_write). */
int gsc_map_colourful = FALSE;
/* Set when the game defines a MAP command of its own (Lost Coastlines has a
   sea chart): the game's command wins, and the pane is reached with the
   "glk map" escape instead. */
int gsc_map_taken = FALSE;
/* The restart the pane was last set up for (run_get_restart_count), so that an
   ADRIFT 4 restart -- which happens entirely inside the interpreter -- is
   noticed as the replayed opening arrives (gsc_map_notice_restart). */
static scr_int gsc_map_restarts = 0;

/* The camera and pixel size of the last map redraw, so a mouse click can be
   hit-tested against exactly what is on screen. */
static map_camera_t gsc_map_cam;
static int gsc_map_px_w = 0, gsc_map_px_h = 0;

/* A manual zoom ("glk zoom in/out"), as pixels per map unit; 0 while the map
   is fitting itself to its window ("glk zoom auto", the default).  A manual
   zoom is kept until "auto" puts it back; meanwhile the view pans to keep the
   player on-screen. */
int gsc_map_zoom = 0;

/* The pixels currently on screen, kept so that a redraw need only send the rows
   that have changed.  The map is redrawn at every prompt, but most turns do not
   move the player and so do not change a single pixel; a turn that does move him
   usually changes only part of the pane.  Sending the whole surface regardless
   is what costs: Glk has no blit, so each row of the map arrives at the display
   as a run of glk_window_fill_rect() calls, and Spatterlight turns every one of
   them into an NSRectFill.  Comparing against these pixels first is much cheaper
   than drawing them again.
   The comparison is only valid while the window still holds what we last put
   there, so gsc_map_full_flush forces the whole surface out again whenever it
   may not: after an arrange or redraw event (the window is resized, and its
   backing image with it), and when the pane is opened or cleared. */
static map_surface_t *gsc_map_screen = NULL;
int gsc_map_full_flush = TRUE;

void
gsc_map_screen_drop (void)
{
  map_surface_free (gsc_map_screen);
  gsc_map_screen = NULL;
  gsc_map_full_flush = TRUE;
}

/* A walk in progress, from clicking a room on the map (clsCharacter.WalkTo /
   DoWalk): the target room, and the room we set off from on the last step so a
   step that fails to move us can abort the walk, as DoWalk's sLastPosition
   does.  Each step is submitted as an ordinary direction command, one room per
   turn. */
char gsc_a5_walk_to[256] = "";
static char gsc_a5_walk_last[256] = "";
int gsc_a5_walk_clicked = FALSE;
static int gsc_a5_walk_steps = 0;
/* A walk is bounded: a game that shuffles the player around could otherwise
   keep the route recomputing for ever. */
#define GSC_A5_WALK_MAX 100
static void gsc_a5_map_view (a5_state_t *st, map_view_t *view);

void
gsc_a5_walk_stop (void)
{
  gsc_a5_walk_to[0] = '\0';
  gsc_a5_walk_last[0] = '\0';
  gsc_a5_walk_steps = 0;
}

/* The same walk, for ADRIFT 4.  Room keys here are just room numbers, and the
   step is submitted as a direction word through SCARE's own parser. */
static char gsc_sc_walk_to[16] = "";
static char gsc_sc_walk_last[16] = "";
static int gsc_sc_walk_steps = 0;
#define GSC_SC_WALK_MAX 100

static int gsc_map_available (void);

static void
gsc_sc_walk_stop (void)
{
  gsc_sc_walk_to[0] = '\0';
  gsc_sc_walk_last[0] = '\0';
  gsc_sc_walk_steps = 0;
}


/*
 * gsc_map_click()
 *
 * Act on a mouse click that arrived while a line of input was pending.  A
 * click on a room of the map walks the player there, one room per turn
 * (Map.vb imgMap_MouseDown: Player.WalkTo = node; DoWalk()): the pending
 * line request is cancelled so that the next line read can issue the first
 * step in its place, and whatever the player had half-typed is discarded.
 * Returns TRUE when a walk was started, and `event` is then the LineInput
 * event the cancel produced.  A click on empty map re-arms the mouse request
 * and returns FALSE, as does a click anywhere but the map.
 */
int
gsc_map_click (event_t *event)
{
  map_view_t view;
  char here_room[16];
  const char *hit, *here;

  if (event->win != gsc_map_window)
    return FALSE;

  if (gsc_is_a5)
    {
      a5_state_t *st;

      if (gsc_a5_run == NULL)
        return FALSE;
      st = a5run_state (gsc_a5_run);
      gsc_a5_map_view (st, &view);
      here = a5state_player_location (st);
    }
  else
    {
      if (gsc_map == NULL || gsc_game == NULL)
        return FALSE;
      scmap_view ((scr_gameref_t) gsc_game, &view);
      snprintf (here_room, sizeof here_room, "%ld",
                (long) gs_playerroom ((scr_gameref_t) gsc_game));
      here = here_room;
    }

  hit = map_hit (gsc_map, &view, &gsc_map_cam, gsc_map_px_w, gsc_map_px_h,
                 (int) event->val1, (int) event->val2);
  if (hit != NULL && (here == NULL || strcmp (hit, here) != 0))
    {
      if (gsc_is_a5)
        {
          gsc_a5_walk_stop ();
          snprintf (gsc_a5_walk_to, sizeof gsc_a5_walk_to, "%s", hit);
          gsc_a5_walk_clicked = TRUE;
        }
      else
        {
          gsc_sc_walk_stop ();
          snprintf (gsc_sc_walk_to, sizeof gsc_sc_walk_to, "%s", hit);
        }
      glk_cancel_line_event (gsc_main_window, event);
      return TRUE;
    }

  /* A click on empty map: re-arm and keep waiting. */
  if (glk_gestalt (gestalt_MouseInput, wintype_Graphics))
    glk_request_mouse_event (gsc_map_window);
  return FALSE;
}


/*---------------------------------------------------------------------*/
/*  ADRIFT 5 map window                                                */
/*---------------------------------------------------------------------*/

/*
 * The map view callbacks: what a5map needs to know about this run.  They read
 * the same state the runner's map does -- the player's seen-set, room short
 * names, and the restriction-checked exits.
 */
static int
gsc_a5_map_seen (void *ctx, const char *lockey)
{
  a5_state_t *st = (a5_state_t *) ctx;
  int li = a5state_location_index (st, lockey);

  return li >= 0 && st->loc_seen != NULL && st->loc_seen[li];
}

/*
 * Room labels.  a5text_location_short_plain allocates, but a5map wants a
 * borrowed string, so cache the names for the lifetime of one redraw.
 */
#define GSC_A5_MAP_NAMES 512
static char *gsc_a5_map_name_cache[GSC_A5_MAP_NAMES];
static const char *gsc_a5_map_name_key[GSC_A5_MAP_NAMES];
static int gsc_a5_map_n_names = 0;

void
gsc_a5_map_names_clear (void)
{
  int i;

  for (i = 0; i < gsc_a5_map_n_names; i++)
    free (gsc_a5_map_name_cache[i]);
  gsc_a5_map_n_names = 0;
}

static const char *
gsc_a5_map_name (void *ctx, const char *lockey)
{
  a5_state_t *st = (a5_state_t *) ctx;
  char *name;
  int i;

  for (i = 0; i < gsc_a5_map_n_names; i++)
    if (gsc_a5_map_name_key[i] == lockey)
      return gsc_a5_map_name_cache[i];

  name = a5text_location_short_plain (st, lockey);
  if (name == NULL)
    return NULL;
  /* Map labels are drawn outside the story window, where the turn loop's own
     strip pass never reaches: a room whose name lost a tag would otherwise
     carry the stripped-tag sentinel into the box. */
  a5text_strip_pres_marks (name);
  if (gsc_a5_map_n_names >= GSC_A5_MAP_NAMES)
    {
      /* Cache full (a huge page): draw this one and drop it. */
      static char scratch[128];
      snprintf (scratch, sizeof scratch, "%s", name);
      free (name);
      return scratch;
    }
  gsc_a5_map_name_key[gsc_a5_map_n_names] = lockey;
  gsc_a5_map_name_cache[gsc_a5_map_n_names] = name;
  gsc_a5_map_n_names++;
  return name;
}

static const char *
gsc_a5_map_exit_dest (void *ctx, const char *lockey, int dir)
{
  a5_state_t *st = (a5_state_t *) ctx;

  return a5restr_exit_in_direction (st, a5state_player_key (st), lockey,
                                    map_dirs[dir], NULL);
}

static int
gsc_a5_map_ever_blocked (void *ctx, const char *lockey, int dir)
{
  a5_state_t *st = (a5_state_t *) ctx;

  return a5restr_ever_blocked (st, lockey, map_dirs[dir]);
}

static void
gsc_a5_map_view (a5_state_t *st, map_view_t *view)
{
  /* The runner draws its map only after PrepareForNextTurn has emptied the
     per-turn route memo, so a route un-blocked late in the turn is re-checked
     fresh by the draw; drop the memo here to match.  (The turn loop clears it
     again at the next command, so entries seeded by drawing never leak into
     game-visible restriction checks.) */
  a5restr_route_cache_clear (st);
  view->seen = gsc_a5_map_seen;
  view->name = gsc_a5_map_name;
  view->exit_dest = gsc_a5_map_exit_dest;
  view->ever_blocked = gsc_a5_map_ever_blocked;
  view->ctx = st;
}

/*
 * gsc_map_current()
 *
 * The map to draw, and the room to centre it on, for whichever engine is
 * running.  For ADRIFT 5 that is the map parsed at load; for ADRIFT 4 there is
 * nothing to parse, so the layout is recomputed here -- it is derived from the
 * room you are in and the rooms you have seen, both of which change as you
 * play, and the runner likewise recomputed it every time it drew.
 *
 * Returns FALSE if there is no map to show.
 */
static int
gsc_map_current (map_view_t *view, const char **player, char *keybuf,
                 int keysize)
{
  if (gsc_is_a5)
    {
      a5_state_t *st;

      if (gsc_a5_run == NULL || gsc_map == NULL)
        return FALSE;
      st = a5run_state (gsc_a5_run);
      gsc_a5_map_view (st, view);
      *player = a5state_player_location (st);
      return TRUE;
    }

  if (gsc_game == NULL)
    return FALSE;

  scmap_view ((scr_gameref_t) gsc_game, view);
  map_free (gsc_map);
  gsc_map = scmap_build ((scr_gameref_t) gsc_game, view);
  if (gsc_map == NULL)
    return FALSE;

  snprintf (keybuf, (size_t) keysize, "%ld",
            (long) gs_playerroom ((scr_gameref_t) gsc_game));
  *player = keybuf;
  return TRUE;
}

/*
 * gsc_map_worth_opening()
 *
 * Whether opening the pane right now would show the player anything.
 *
 * ADRIFT 5 games routinely run their title, credits and options screens from a
 * staging room the map hides (Location <Hide>), and a page whose only seen room
 * is hidden renders as an empty rectangle -- The Axe of Kolt spends three
 * screens there before the game proper begins.  Rather than put up a blank
 * pane, opening waits; every redraw asks again, so the map arrives with the
 * first real room.
 *
 * ADRIFT 4 hides its map the same way, but a step earlier: the layout is
 * derived from the room you are standing in, and from a room the author flagged
 * HideOnMap the runner drew nothing at all (Form29.drawmap bails, so scmap_build
 * hands back no map).  The PK Girl opens in such a room and plays its whole
 * introduction there.  That is the same "wait for a real room" case, not a
 * failure, so it defers too.
 *
 * A map that cannot be built for any other reason answers TRUE, because that is
 * a different complaint and one the pane makes for itself: the game may have no
 * map to show, or ADRIFT 4's layout may have given up ("too complex"), and the
 * player asking for the map deserves to be told so.
 */
static int
gsc_map_worth_opening (void)
{
  map_view_t view;
  const char *ploc = NULL;
  char keybuf[16];

  if (!gsc_map_current (&view, &ploc, keybuf, sizeof keybuf))
    return gsc_is_a5 || !gsc_map_available () || scmap_failed () != 0;
  return map_has_content (gsc_map, &view, ploc);
}

/* One run of a colour, for rgbsurf_send. */
static void
gsc_map_span (void *ctx, unsigned int rgb, int x, int y, int len)
{
  (void) ctx;
  glk_window_fill_rect (gsc_map_window, rgb, x, y, (glui32) len, 1);
}

/*
 * gsc_map_redraw()
 *
 * Rasterise the map and blit it into the graphics window.  Glk has no drawing
 * primitives beyond a filled rectangle, so -- as in the Comprehend port -- the
 * page is rendered into an RGB surface and flushed as run-length-encoded
 * horizontal spans, which is far cheaper than one fill_rect per pixel.
 *
 * Only the rows that differ from what is already on screen (gsc_map_screen) are
 * flushed, so a turn that leaves the map alone -- most of them -- sends nothing
 * at all.
 */
void
gsc_map_redraw (void)
{
  map_surface_t *surf;
  map_view_t view;
  const char *ploc = NULL;
  char keybuf[16];
  glui32 w, h;

  /* A map that was asked for while there was nothing on it: try again now
     that the game has moved on.  (gsc_map_show comes straight back here, but
     with gsc_map_shown set, so this runs once.) */
  if (gsc_map_want && !gsc_map_shown && gsc_map_worth_opening ())
    gsc_map_show ();

  if (!gsc_map_window)
    return;

  glk_window_get_size (gsc_map_window, &w, &h);
  if (w == 0 || h == 0)
    return;

  /* The map is drawn in the story's colours: the buffer's normal style
     supplies the background and text colour.  Colour mode publishes those as
     Normal stylehints (and rebuilds windows so they stick); libraries that
     ignore author styles leave measure on the theme, matching the transcript.
     A Glk that cannot measure leaves the palette at its black-on-white
     default.  The measurement comes from gsc_normal_measure's cache -- the
     map redraws at every prompt, and each miss is two synchronous
     round-trips to the application. */
  {
    glui32 bg, fg;

    if (gsc_normal_measure (&fg, &bg))
      {
        map_set_palette (bg, fg);
        /* So the clear below, and any exposed edge, match the surface. */
        glk_window_set_background_color (gsc_map_window, bg);
      }
  }

  /* Nothing to draw: an ADRIFT 4 layout that gave up, or a player standing in a
     room hidden from the map -- where the runner showed an empty map too.  Wipe
     the pane rather than leave the last one up; it will come back by itself. */
  if (!gsc_map_current (&view, &ploc, keybuf, sizeof keybuf))
    {
      glk_window_clear (gsc_map_window);
      gsc_map_screen_drop ();
      return;
    }

  surf = map_surface_new ((int) w, (int) h);
  if (surf == NULL)
    return;

  map_frame (gsc_map, &view, ploc, surf, gsc_map_zoom, &gsc_map_cam);
  map_render (gsc_map, &view, ploc, &gsc_map_cam, surf);
  gsc_map_px_w = surf->w;
  gsc_map_px_h = surf->h;
  if (gsc_is_a5)
    gsc_a5_map_names_clear ();

  /* The window was resized under us, so the pixels we think are on screen are
     not the ones that are. */
  if (gsc_map_screen != NULL
      && (gsc_map_screen->w != surf->w || gsc_map_screen->h != surf->h))
    gsc_map_screen_drop ();

  /* Untouched rows: what is on screen is already right. */
  rgbsurf_send (surf, gsc_map_full_flush ? NULL : gsc_map_screen,
                gsc_map_span, NULL);

  /* These pixels are the screen now. */
  map_surface_free (gsc_map_screen);
  gsc_map_screen = surf;
  gsc_map_full_flush = FALSE;

  /* Arm the click that walks the player to a room.  Mouse requests are
     one-shot, so this is re-armed after every redraw. */
  if (glk_gestalt (gestalt_MouseInput, wintype_Graphics))
    glk_request_mouse_event (gsc_map_window);
}

/*
 * gsc_sc_walk_next()
 *
 * The next step of a map-click walk in an ADRIFT 4 game, as the direction
 * command that makes it.  The route only runs through rooms already visited,
 * and only along exits whose restrictions are currently satisfied, so it can
 * always be walked.  Gives up on arrival, when no route remains, or when the
 * last step did not actually move us -- something in the game blocked it.
 */
int
gsc_sc_walk_next (scr_char *buffer, scr_int length)
{
  map_view_t view;
  char from[16];
  const scr_char *word;
  int dir;

  if (gsc_sc_walk_to[0] == '\0' || gsc_game == NULL)
    return FALSE;
  if (++gsc_sc_walk_steps > GSC_SC_WALK_MAX)
    {
      gsc_sc_walk_stop ();
      return FALSE;
    }

  snprintf (from, sizeof from, "%ld",
            (long) gs_playerroom ((scr_gameref_t) gsc_game));
  if (strcmp (from, gsc_sc_walk_to) == 0
      || (gsc_sc_walk_last[0] != '\0' && strcmp (from, gsc_sc_walk_last) == 0))
    {
      gsc_sc_walk_stop ();
      return FALSE;
    }

  scmap_view ((scr_gameref_t) gsc_game, &view);
  dir = map_walk_step (&view, from, gsc_sc_walk_to);
  word = (dir >= 0) ? lib_direction_name (dir) : NULL;
  if (word == NULL)
    {
      gsc_sc_walk_stop ();
      return FALSE;
    }

  snprintf (buffer, (size_t) length, "%s", word);
  snprintf (gsc_sc_walk_last, sizeof gsc_sc_walk_last, "%s", from);
  return TRUE;
}

/*
 * gsc_a5_walk_next()
 *
 * The next step of a map-click walk, as the game command that makes it (the
 * localized direction word, so the game's own parser takes it).  Returns FALSE
 * when the walk is over -- we have arrived, no route remains, or the last step
 * did not move us (something blocked the way), which is DoWalk's
 * sLastPosition bail-out.
 */
int
gsc_a5_walk_next (a5_run_t *run, char *buf, int bufsize)
{
  map_view_t view;
  a5_state_t *st;
  const char *from, *word;
  int dir;

  if (gsc_a5_walk_to[0] == '\0' || run == NULL)
    return FALSE;
  if (a5run_is_over (run))
    return FALSE;                       /* the walk ended the game */
  if (++gsc_a5_walk_steps > GSC_A5_WALK_MAX)
    return FALSE;

  st = a5run_state (run);
  from = a5state_player_location (st);
  if (from == NULL)
    return FALSE;

  if (strcmp (from, gsc_a5_walk_to) == 0)
    return FALSE;                       /* arrived */
  if (gsc_a5_walk_last[0] != '\0' && strcmp (from, gsc_a5_walk_last) == 0)
    return FALSE;                       /* the last step did not move us */

  gsc_a5_map_view (st, &view);
  dir = map_walk_step (&view, from, gsc_a5_walk_to);
  if (dir < 0)
    return FALSE;

  word = a5parse_direction_name (map_dirs[dir]);
  if (word == NULL)
    return FALSE;

  snprintf (buf, (size_t) bufsize, "%s", word);
  snprintf (gsc_a5_walk_last, sizeof gsc_a5_walk_last, "%s", from);
  return TRUE;
}

/*
 * gsc_map_available()
 *
 * Whether this game has a map at all.  In ADRIFT 5 that is settled at load:
 * it either shipped map data or it did not.  In ADRIFT 4 every game with
 * rooms has one, unless the author switched it off -- and gsc_map itself is
 * no guide there, being built lazily at the first redraw.  Whether there is
 * anything to draw *right now* is a separate question (map_has_content).
 */
static int
gsc_map_available (void)
{
  if (gsc_is_a5)
    return gsc_map != NULL;
  return gsc_game != NULL && scmap_available ((scr_gameref_t) gsc_game);
}

/*
 * gsc_map_default_shown()
 *
 * Whether this game would start with the map showing if the player had never
 * said otherwise: what the author's shipped Runner layout asks for in ADRIFT
 * 5, and nothing at all in ADRIFT 4, whose Runner opened with the map closed
 * however the last game left it.
 */
int
gsc_map_default_shown (void)
{
  return gsc_is_a5 && gsc_a5_adv != NULL && gsc_a5_adv->map_pane_open == 1;
}

/*
 * gsc_map_pref_ref()
 *
 * A fileref for this game's remembered map choice, or NULL when there is
 * nothing to remember or nowhere to keep it.
 *
 * The ADRIFT 5 Runner keeps the player's window layout per game, in
 * RunnerLayout-<IFID>.xml, and writes it out on quit; that file is also what
 * overrides the layout an author shipped inside the Blorb, because the Runner
 * only unpacks the shipped one when no file for that IFID exists yet
 * (Blorb.vb:343).  This is the same store, under the same key -- two bytes,
 * '1' or '0' for whether the map is wanted and 't' or 'r' for where it goes,
 * rather than a window layout.
 *
 * Games the Runner has no key for get one of ours (gsc_game_key): ADRIFT 4
 * never had an IFID, and its Runner kept the map as a global View-menu
 * setting in the registry rather than per game, so there is nothing to be
 * compatible with -- and a few ADRIFT 5 .taf files were built without an
 * <ifid> block too.
 *
 * Games with no map and hosts with no graphics have nothing to record, and
 * must not leave a file behind for it.
 */
static frefid_t
gsc_map_pref_ref (void)
{
  const char *key;
  char name[128];

  if (!gsc_map_available ())
    return NULL;
  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    return NULL;

  if (gsc_is_a5 && gsc_a5_adv != NULL
      && gsc_a5_adv->ifid != NULL && gsc_a5_adv->ifid[0] != '\0')
    key = gsc_a5_adv->ifid;
  else if (gsc_game_key[0] != '\0')
    key = gsc_game_key;
  else
    return NULL;

  snprintf (name, sizeof name, "scarier-map-%s", key);
  return glk_fileref_create_by_name (fileusage_Data | fileusage_TextMode,
                                     name, 0);
}

/*
 * gsc_map_pref_read()
 *
 * The player's remembered choice for this game: 1 to show the map, 0 to hide
 * it, -1 when they have never said.  Where they last put it is returned
 * through at_top on the same terms -- 1 for the top band, 0 for the pane at
 * the right, -1 for never said -- and which colour scheme they last had
 * through colourful, 1 for the derived colours and 0 for the flat ones.
 * Files written before the map could be moved hold only the first byte, and
 * ones written before it could be recoloured only the first two, so a missing
 * byte reads as never said rather than as a choice.
 */
int
gsc_map_pref_read (int *at_top, int *colourful)
{
  frefid_t fileref;
  int value = -1;

  if (at_top != NULL)
    *at_top = -1;
  if (colourful != NULL)
    *colourful = -1;

  fileref = gsc_map_pref_ref ();
  if (fileref == NULL)
    return -1;

  if (glk_fileref_does_file_exist (fileref))
    {
      strid_t stream = glk_stream_open_file (fileref, filemode_Read, 0);

      if (stream)
        {
          glui32 c = glk_get_char_stream (stream);

          if (c == '0' || c == '1')
            value = (int) (c - '0');

          c = glk_get_char_stream (stream);
          if (at_top != NULL && (c == 't' || c == 'r'))
            *at_top = (c == 't');

          c = glk_get_char_stream (stream);
          if (colourful != NULL && (c == 'c' || c == 'p'))
            *colourful = (c == 'c');

          glk_stream_close (stream, NULL);
        }
    }

  glk_fileref_destroy (fileref);
  return value;
}

/*
 * gsc_map_pref_write()
 *
 * Remember that the player asked for the map to be shown or hidden in this
 * game, where they had it, and which colours they drew it in, so that the next
 * session opens the way they left it.  The position and the colour scheme are
 * recorded even when the map is off, so that turning it back on later still
 * puts it where they last had it, looking the way it did.
 *
 * Only a game whose map the player has actually moved away from its default
 * gets a file; one they have put back where it started has theirs removed
 * again.  So the common case -- ADRIFT 4, where the map starts hidden and
 * most players leave it that way -- costs nothing at all, and the files that
 * do accumulate are only for games the player made a decision about.
 */
static void
gsc_map_pref_write (int shown, int at_top, int colourful)
{
  frefid_t fileref;
  strid_t stream;

  fileref = gsc_map_pref_ref ();
  if (fileref == NULL)
    return;

  if (!shown == !gsc_map_default_shown () && !at_top && !colourful)
    {
      if (glk_fileref_does_file_exist (fileref))
        glk_fileref_delete_file (fileref);
      glk_fileref_destroy (fileref);
      return;
    }

  stream = glk_stream_open_file (fileref, filemode_Write, 0);
  if (stream)
    {
      glk_put_char_stream (stream, (unsigned char) (shown ? '1' : '0'));
      glk_put_char_stream (stream, (unsigned char) (at_top ? 't' : 'r'));
      glk_put_char_stream (stream, (unsigned char) (colourful ? 'c' : 'p'));
      glk_stream_close (stream, NULL);
    }
  glk_fileref_destroy (fileref);
}

/*
 * gsc_map_show()
 *
 * Open the map pane, in whichever position gsc_map_at_top asks for: the
 * standard pane to the right of the story, or a band across the top of the
 * screen.  The top band splits the root window, not the story, so that it
 * sits above the status line and spans the display's full width.
 */
void
gsc_map_show (void)
{
  if (!gsc_map_available ())
    {
      gsc_normal_string ("This game has no map.\n");
      return;
    }

  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    {
      gsc_normal_string ("This interpreter cannot display the map.\n");
      return;
    }

  if (gsc_map_at_top)
    gsc_map_window = glk_window_open (glk_window_get_root (),
                                         winmethod_Above
                                         | winmethod_Proportional,
                                         30, wintype_Graphics, 0);
  else
    gsc_map_window = glk_window_open (gsc_main_window,
                                         winmethod_Right
                                         | winmethod_Proportional,
                                         40, wintype_Graphics, 0);
  if (!gsc_map_window)
    {
      gsc_normal_string ("Sorry, the map window could not be opened.\n");
      return;
    }
  gsc_map_shown = TRUE;
  gsc_map_screen_drop ();       /* a fresh window holds nothing */
  gsc_map_redraw ();

  /* The ADRIFT 4 layout can give up ("Cannot draw map - too complex.",
     Form29.showmapfail).  Say so here, once, rather than at every prompt: the
     pane stays open, because a layout that fails from one room may well succeed
     from the next. */
  if (!gsc_is_a5 && gsc_map == NULL && scmap_failed () != 0)
    gsc_normal_string ("Sorry, this game's map is too complex to draw.\n");

  glk_set_window (gsc_main_window);
}

/*
 * gsc_map_hide()
 *
 * Close the map pane, dropping the record of what was drawn there.
 */
void
gsc_map_hide (void)
{
  if (gsc_map_window)
    {
      glk_window_close (gsc_map_window, NULL);
      gsc_map_window = NULL;
    }
  gsc_map_shown = FALSE;
  gsc_map_screen_drop ();
}

/*
 * gsc_map_set()
 *
 * Show or hide the map pane at the player's request, and remember the choice
 * for this game.  The window is opened on first use (and only for games that
 * actually have map data), then kept, so toggling is cheap.
 *
 * Remembering matters because a game can ask to start with its map showing
 * (gsc_map_auto_reveal); the player's own last word has to outlast the
 * session, or "glk map off" would be undone by the next launch.
 */
static void
gsc_map_set (int shown)
{
  int deferred = FALSE;

  if (shown)
    {
      if (!gsc_map_shown)
        {
          if (gsc_map_worth_opening ())
            gsc_map_show ();
          else
            {
              deferred = TRUE;
              gsc_normal_string ("There is nothing to put on the map from"
                                 " here; it will open by itself as soon as"
                                 " there is.\n");
            }
        }
    }
  else if (gsc_map_shown)
    {
      gsc_map_hide ();
      gsc_normal_string ("Map hidden.\n");
    }
  else if (gsc_map_want)
    /* Called off before it ever got to open. */
    gsc_normal_string ("Map hidden.\n");

  /* What actually happened, not what was asked for: opening can fail outright
     (no map, no graphics), and a pane that cannot exist is not a preference
     worth keeping.  A deferred open is the opposite case -- the player did
     ask, and the map is on its way. */
  gsc_map_want = gsc_map_shown || deferred;
  gsc_map_pref_write (gsc_map_want, gsc_map_at_top, gsc_map_colourful);
}

/*
 * gsc_map_toggle()
 *
 * Show the map pane if it is hidden, hide it if it is shown -- or cancel it if
 * it is merely on its way.
 */
void
gsc_map_toggle (void)
{
  gsc_map_set (!(gsc_map_shown || gsc_map_want));
}

/*
 * gsc_map_auto_reveal()
 *
 * Open the map pane at the start of a game that asks for it.
 *
 * ADRIFT 5 games can carry the author's own Runner window layout in their
 * Blorb (a5blorb_find_layout), and run500.exe starts with whatever panes that
 * layout has open -- which is the whole of why a handful of games "come with
 * the map already showing" and the rest do not.  There is no authored
 * map-visibility setting, and ADRIFT 4 has no equivalent at all: run400's map
 * was a View-menu toggle stored globally in the registry, never per game.
 *
 * A choice the player made themselves wins over the author's, exactly as the
 * Runner's own saved RunnerLayout-<IFID>.xml wins over the shipped one -- in
 * either direction, so a player who once opened the map in this game gets it
 * back too.  That part applies to ADRIFT 4 as well, which is why this runs on
 * both paths: the game has nothing to say about the map, but the player does.
 * Silent throughout: a host that cannot draw a map, or a game with none, just
 * gets no pane.
 */
void
gsc_map_auto_reveal (void)
{
  int pref, at_top, colourful;

  if (gsc_map_shown)
    return;

  gsc_map_want = FALSE;
  if (!gsc_map_available ())
    return;
  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    return;

  pref = gsc_map_pref_read (&at_top, &colourful);

  /* Where the map goes, and what it is drawn in, are remembered whether or not
     it is opened now, so that a later "glk map on" puts it back the way the
     player last had it. */
  if (at_top >= 0)
    gsc_map_at_top = at_top;
  if (colourful >= 0)
    gsc_map_set_colourful (colourful);

  if (pref < 0)
    {
      /* Never asked: follow the layout the author shipped, if any. */
      if (!gsc_is_a5 || gsc_a5_adv == NULL || gsc_a5_adv->map_pane_open != 1)
        return;
    }
  else if (pref == 0)
    return;

  /* Wanted -- but an opening played out from a room the map hides has nothing
     to put in the pane, so the map may have to wait for the first real room
     (gsc_map_worth_opening, and the retry in gsc_map_redraw). */
  gsc_map_want = TRUE;
  if (gsc_map_worth_opening ())
    gsc_map_show ();
}

/*
 * gsc_map_notice_restart()
 *
 * Take the map pane down for a replayed ADRIFT 4 opening, and let it be decided
 * again from scratch.
 *
 * The ADRIFT 5 path does this where the restart happens (gsc_a5_restart_run);
 * ADRIFT 4 has nowhere to do it, because the restart never surfaces here.  Both
 * RESTART at the prompt and the restart offered when the game ends are handled
 * inside scr_interpret_game, which just replays the opening -- so the pane goes
 * into the new game still showing the old one's layout, and for a game whose
 * opening plays out in a room the map hides, like The PK Girl, that means the
 * empty rectangle gsc_map_worth_opening exists to keep off the screen.  Watch
 * the interpreter's restart count for a change instead (run_get_restart_count).
 *
 * The zoom goes with the old run for the same reason it does in ADRIFT 5: the
 * new one is back to a single room, which a scale chosen for a whole map would
 * show far too close in.
 *
 * Called at the first sign of the replayed opening -- its first line of output,
 * or failing that its first prompt -- so that the opening is laid out at the
 * width it will keep.  Opening and closing windows moves the current stream
 * about (gsc_map_show leaves output pointed at the story window), which the
 * caller mid-print is not expecting, so put it back.
 */
void
gsc_map_notice_restart (void)
{
  strid_t stream;
  scr_int restarts;

  if (gsc_is_a5 || gsc_game == NULL)
    return;

  restarts = run_get_restart_count ();
  if (restarts == gsc_map_restarts)
    return;
  gsc_map_restarts = restarts;

  stream = glk_stream_get_current ();
  gsc_map_hide ();
  gsc_map_zoom = 0;
  gsc_map_auto_reveal ();
  glk_stream_set_current (stream);
}

/*
 * gsc_map_place()
 *
 * Put the map pane at the top of the screen ("glk map top", for games with
 * wide maps) or back at the right of the story ("glk map right").  A map
 * already shown in the other position is closed and reopened in the new one.
 */
static void
gsc_map_place (int at_top)
{
  if (gsc_map_shown && gsc_map_at_top == at_top)
    {
      gsc_normal_string (at_top
                         ? "The map is already at the top of the screen.\n"
                         : "The map is already at the right.\n");
      return;
    }

  gsc_map_hide ();
  gsc_map_at_top = at_top;

  /* Asking where the map goes is asking for a map -- through gsc_map_set, so
     that the request is remembered and honoured later even if there is nothing
     to draw from where the player is standing. */
  gsc_map_set (TRUE);
}

/*
 * gsc_map_set_colourful()
 *
 * Pick the scheme the renderer spends the story's two colours in.  Silent, and
 * it draws nothing: gsc_map_auto_reveal calls this before there is a pane.
 */
void
gsc_map_set_colourful (int colourful)
{
  gsc_map_colourful = colourful;
  map_set_colour_scheme (colourful ? MAP_SCHEME_DERIVED
                                   : MAP_SCHEME_STANDARD);
}

/*
 * gsc_map_colour()
 *
 * "glk map colour": draw the map with a you-are-here amber, or back in the
 * paper-and-ink cards that are the default.  Unlike placement this does not
 * ask for a map: recolouring one that is hidden is a preference for next
 * time, not a request to see it.
 *
 * The pixels we think are on screen were drawn in the old scheme, so they are
 * dropped before the redraw; otherwise the row comparison would find them
 * unchanged and send nothing.  Redrawing here rather than leaving it to the
 * next prompt is what gsc_set_colour does, and for the same reason: the
 * glk-command loop never reaches the turn loop's prompt.
 */
static void
gsc_map_colour (int colourful)
{
  if (gsc_map_colourful == colourful)
    {
      gsc_normal_string (colourful
                         ? "The map is already drawn in colour.\n"
                         : "The map is already drawn in the standard"
                           " colours.\n");
      return;
    }

  gsc_map_set_colourful (colourful);
  gsc_map_screen_drop ();
  gsc_map_redraw ();
  gsc_normal_string (colourful
                     ? "The map is now drawn in colour.\n"
                     : "The map is now drawn in the standard colours.\n");
  gsc_map_pref_write (gsc_map_want, gsc_map_at_top, gsc_map_colourful);
}

/*
 * gsc_map_colour_word()
 *
 * True if the argument to "glk map" starts with the word colour, in any of the
 * four spellings "glk colour" itself answers to; *arg is then advanced past it
 * to whatever followed, with the leading space eaten.
 *
 * Matched at a word boundary and longest first, so that "colours" is not read
 * as "colour" with a stray "s" argument.
 */
static int
gsc_map_colour_word (const char **arg)
{
  static const char * const words[] = {
    "colours", "colors", "colour", "color", NULL
  };
  const char *s = *arg;
  int i;

  for (i = 0; words[i] != NULL; i++)
    {
      size_t len = strlen (words[i]);

      if (scr_strncasecmp (s, words[i], len) == 0
          && (s[len] == '\0' || s[len] == ' ' || s[len] == '\t'))
        {
          s += len;
          *arg = s + strspn (s, " \t");
          return TRUE;
        }
    }
  return FALSE;
}

/*
 * gsc_command_map()
 *
 * "glk map [on|off]".  Always available, even for the rare game that defines a
 * MAP command of its own (see gsc_map_taken).
 */
void
gsc_command_map (const char *argument)
{
  if (gsc_is_a5 ? gsc_a5_run == NULL : gsc_game == NULL)
    return;

  if (strlen (argument) == 0)
    {
      gsc_map_toggle ();
      return;
    }
  /* Explicit on/off go through gsc_map_set even when they change nothing, so
     that "glk map off" in a game that opens with its map showing is recorded
     as a veto rather than shrugged off as a no-op. */
  if (scr_strcasecmp (argument, "on") == 0)
    {
      gsc_map_set (TRUE);
    }
  else if (scr_strcasecmp (argument, "off") == 0)
    {
      gsc_map_set (FALSE);
    }
  else if (scr_strcasecmp (argument, "top") == 0
           || scr_strcasecmp (argument, "above") == 0)
    {
      gsc_map_place (TRUE);
    }
  else if (scr_strcasecmp (argument, "right") == 0
           || scr_strcasecmp (argument, "side") == 0)
    {
      gsc_map_place (FALSE);
    }
  /* "glk map colour" on its own toggles, so that one command both tries the
     alternative colours and puts them away again; "on"/"off" are there for a
     player who would rather say which they mean.  All four spellings of the
     word that "glk colour" answers to are accepted here too. */
  else if (gsc_map_colour_word (&argument))
    {
      if (*argument == '\0')
        gsc_map_colour (!gsc_map_colourful);
      else if (scr_strcasecmp (argument, "on") == 0)
        gsc_map_colour (TRUE);
      else if (scr_strcasecmp (argument, "off") == 0)
        gsc_map_colour (FALSE);
      else if (scr_strcasecmp (argument, "status") == 0)
        gsc_normal_string (gsc_map_colourful
                           ? "The map is drawn in colour.\n"
                           : "The map is drawn in the standard colours.\n");
      else
        gsc_command_usage ("map");
    }
  else if (scr_strncasecmp (argument, "zoom", 4) == 0
           && (argument[4] == '\0' || argument[4] == ' '
               || argument[4] == '\t'))
    {
      /* "glk map zoom ..." is "glk zoom ..." by another name. */
      gsc_command_zoom (argument + 4 + strspn (argument + 4, "\t "));
    }
  else
    gsc_command_usage ("map");
}

/*
 * gsc_command_zoom()
 *
 * "glk zoom [in | out | auto]".  Plain "glk zoom" zooms in, and "default" is a
 * synonym for "auto".  A manual zoom is kept until "auto" puts the map back
 * to fitting itself to its window; meanwhile the view pans to keep the
 * player on-screen (map_frame).
 */
void
gsc_command_zoom (const char *argument)
{
  int in, scale, stepped;

  if (gsc_is_a5 ? gsc_a5_run == NULL : gsc_game == NULL)
    return;

  /* "glk map zoom help" arrives here directly; the dispatcher catches the
     plain "glk zoom help" before the handler is ever called. */
  if (scr_strcasecmp (argument, "help") == 0)
    {
      gsc_command_help ("zoom");
      return;
    }

  if (scr_strcasecmp (argument, "auto") == 0
      || scr_strcasecmp (argument, "default") == 0)
    {
      if (gsc_map_zoom == 0)
        gsc_normal_string ("The map is already zooming to fit its window.\n");
      else
        {
          gsc_map_zoom = 0;
          gsc_map_redraw ();
          gsc_normal_string ("Map zoom returned to automatic.\n");
        }
      return;
    }

  if (strlen (argument) == 0 || scr_strcasecmp (argument, "in") == 0)
    in = TRUE;
  else if (scr_strcasecmp (argument, "out") == 0)
    in = FALSE;
  else
    {
      gsc_command_usage ("zoom");
      return;
    }

  if (!gsc_map_shown)
    {
      gsc_normal_string ("The map is not open.  Use ");
      gsc_standout_string ("glk map on");
      gsc_normal_string (" to open it first.\n");
      return;
    }

  /* Step from the manual zoom, or from wherever the automatic fit last
     landed; a map with nothing drawn yet steps from the runner's default. */
  scale = gsc_map_zoom > 0 ? gsc_map_zoom : gsc_map_cam.scale;
  if (scale < MAP_SCALE_MIN)
    scale = 10;
  stepped = map_zoom_step (scale, in ? 1 : -1);
  if (stepped == scale)
    {
      gsc_normal_string (in ? "The map is already at its maximum zoom.\n"
                            : "The map is already at its minimum zoom.\n");
      return;
    }
  gsc_map_zoom = stepped;
  gsc_map_redraw ();
}

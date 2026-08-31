/* vi: set ts=8:
 *
 * Headless ADRIFT 4 map renderer: loads a .taf, optionally plays a command
 * script (to build up the visited set), then lays the rooms out the way the
 * ADRIFT 4 runner would and rasterises the map to a PPM.  Lets the layout be
 * eyeballed and diffed without the Glk layer -- the v4 counterpart of
 * a5map_dump.
 *
 *   ./scmap_dump <game.taf> [script.txt] [-o out.ppm] [-w W] [-h H] [-all]
 *                [-grid] [-colour]
 *
 *   -all      mark every room seen (whole-map view, for development)
 *   -grid     print the grid as ASCII, the way Form29.display_grid did
 *   -colour   draw in the alternative scheme "glk map colour" selects
 *   -bg/-fg   the host text style the palette is built from, RRGGBB hex
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scarier.h"
#include "scprotos.h"
#include "scmap.h"

static FILE *g_script = NULL;
static int g_reveal_all = 0;
static scr_gameref_t g_game = NULL;
static const char *g_out = "scmap.ppm";
static int g_width = 480, g_height = 480, g_grid = 0;
/* -colour: render in the alternative scheme "glk map colour" selects.
   -bg/-fg stand in for the host's text style, which the Glk frontend measures
   off the story window; the derived scheme reads very differently on dark
   paper, so it needs to be eyeballable here too. */
static int g_colour = 0;
static long g_bg = -1, g_fg = -1;

static void draw_and_exit (void);

/* --- the os layer, such as it is ----------------------------------------
   The do-nothing stubs live in harness_os_stubs.cpp; os_read_line is ours. */

/*
 * Feed the script; when it runs out, draw the map and stop.
 *
 * Drawing from here, rather than after scr_interpret_game() returns, is what
 * makes this reliable: there is no way to make the interpreter's read loop give
 * up (it re-asks until it gets a line, scinterf.c if_read_line_common), and
 * QUIT is not a way out either -- a game is free to define a task of its own
 * that matches "quit" and swallows it, and some do, whereupon the harness would
 * sit there feeding QUIT for ever.  At this point the game state is exactly
 * what the script left it as, which is what we want to map.
 */
scr_bool
os_read_line (scr_char *buffer, scr_int length)
{
  char line[1024];

  if (g_script != NULL)
    {
      while (fgets (line, sizeof line, g_script) != NULL)
        {
          char *nl = strchr (line, '\n');
          if (nl != NULL)
            *nl = '\0';
          if (line[0] == '\0' || line[0] == '#')
            continue;
          snprintf ((char *) buffer, (size_t) length, "%s", line);
          return TRUE;
        }
    }
  draw_and_exit ();
  return FALSE;                 /* not reached */
}

/* --- the map ------------------------------------------------------------ */

/* -all: pretend the player has been everywhere. */
static int
cb_seen_all (void *ctx, const char *lockey)
{
  (void) ctx; (void) lockey;
  return 1;
}

static void
print_grid (const map_t *map, const map_view_t *view)
{
  const map_page_t *page;
  int i, minx = 0, miny = 0, maxx = 0, maxy = 0, first = 1, x, y;

  if (map == NULL || map->n_pages == 0)
    return;
  page = &map->pages[0];

  for (i = 0; i < page->n_nodes; i++)
    {
      const map_node_t *n = &page->nodes[i];
      if (first)
        { minx = maxx = n->x; miny = maxy = n->y; first = 0; }
      if (n->x < minx) minx = n->x;
      if (n->x > maxx) maxx = n->x;
      if (n->y < miny) miny = n->y;
      if (n->y > maxy) maxy = n->y;
    }
  if (first)
    return;

  /* Nodes are spaced 1.5 boxes apart; step by that to walk grid cells. */
  for (y = miny; y <= maxy; y += MAP_NODE_H * 3 / 2)
    {
      for (x = minx; x <= maxx; x += MAP_NODE_W * 3 / 2)
        {
          const map_node_t *hit = NULL;
          for (i = 0; i < page->n_nodes; i++)
            if (page->nodes[i].x == x && page->nodes[i].y == y)
              { hit = &page->nodes[i]; break; }
          if (hit == NULL)
            printf (" . ");
          else if (view->seen (view->ctx, hit->key))
            printf ("%2s ", hit->key);
          else
            printf ("(%s)", hit->key);
        }
      printf ("\n");
    }
}

static void
draw_and_exit (void)
{
  map_surface_t *surf;
  map_camera_t cam = { 0 };
  map_view_t view;
  map_t *map;
  char player[16];
  FILE *f;
  int i;

  scmap_view (g_game, &view);
  if (g_reveal_all)
    view.seen = cb_seen_all;

  map = scmap_build (g_game, &view);
  if (map == NULL)
    {
      int why = scmap_failed ();
      fprintf (stderr, "nothing to draw (%s)\n",
               why == 1 ? "layout ran off the grid"
               : why == 2 ? "layout stuck"
               : why == 3 ? "layout made too many shifts"
               : !scmap_available (g_game)
                 ? "no rooms, or the author turned the map off"
                 : "the player is in a room hidden from the map");
      exit (1);
    }

  snprintf (player, sizeof player, "%ld", (long) gs_playerroom (g_game));
  fprintf (stderr, "rooms=%d placed=%d player=%s\n",
           (int) gs_room_count (g_game), map->pages[0].n_nodes, player);
  if (g_grid)
    print_grid (map, &view);

  if (g_bg >= 0 || g_fg >= 0)
    map_set_palette ((unsigned int) (g_bg >= 0 ? g_bg : 0xFFFFFF),
                     (unsigned int) (g_fg >= 0 ? g_fg : 0x000000));
  map_set_colour_scheme (g_colour ? MAP_SCHEME_DERIVED : MAP_SCHEME_STANDARD);
  surf = map_surface_new (g_width, g_height);
  map_frame (map, &view, player, surf, 0, 1, 0, &cam, NULL);
  map_render (map, &view, player, &cam, surf);
  fprintf (stderr, "scale=%d\n", cam.scale);

  f = fopen (g_out, "wb");
  if (f == NULL)
    {
      fprintf (stderr, "%s: cannot write\n", g_out);
      exit (1);
    }
  fprintf (f, "P6\n%d %d\n255\n", surf->w, surf->h);
  for (i = 0; i < surf->w * surf->h; i++)
    {
      unsigned int p = surf->px[i];
      unsigned char rgb[3];
      rgb[0] = (unsigned char) ((p >> 16) & 0xFF);
      rgb[1] = (unsigned char) ((p >> 8) & 0xFF);
      rgb[2] = (unsigned char) (p & 0xFF);
      fwrite (rgb, 1, 3, f);
    }
  fclose (f);
  fprintf (stderr, "wrote %s (%dx%d)\n", g_out, g_width, g_height);

  map_surface_free (surf);
  map_free (map);
  exit (0);
}

int
main (int argc, char **argv)
{
  scr_game g0;
  const char *script = NULL;
  int i;

  if (argc < 2)
    {
      fprintf (stderr, "usage: %s <game.taf> [script] [-o out.ppm] [-w W]"
               " [-h H] [-all] [-grid] [-colour]\n"
               "       [-bg RRGGBB] [-fg RRGGBB]\n", argv[0]);
      return 1;
    }
  for (i = 2; i < argc; i++)
    {
      if (strcmp (argv[i], "-o") == 0 && i + 1 < argc)
        g_out = argv[++i];
      else if (strcmp (argv[i], "-w") == 0 && i + 1 < argc)
        g_width = atoi (argv[++i]);
      else if (strcmp (argv[i], "-h") == 0 && i + 1 < argc)
        g_height = atoi (argv[++i]);
      else if (strcmp (argv[i], "-all") == 0)
        g_reveal_all = 1;
      else if (strcmp (argv[i], "-grid") == 0)
        g_grid = 1;
      else if (strcmp (argv[i], "-colour") == 0
               || strcmp (argv[i], "-color") == 0)
        g_colour = 1;
      else if (strcmp (argv[i], "-bg") == 0 && i + 1 < argc)
        g_bg = strtol (argv[++i], NULL, 16);
      else if (strcmp (argv[i], "-fg") == 0 && i + 1 < argc)
        g_fg = strtol (argv[++i], NULL, 16);
      else if (argv[i][0] != '-')
        script = argv[i];
    }

  g0 = scr_game_from_filename (argv[1]);
  if (g0 == NULL)
    {
      fprintf (stderr, "%s: could not load\n", argv[1]);
      return 1;
    }
  g_game = (scr_gameref_t) g0;

  if (script != NULL)
    {
      g_script = fopen (script, "r");
      if (g_script == NULL)
        {
          fprintf (stderr, "%s: cannot open\n", script);
          return 1;
        }
    }

  /* Runs until the script is exhausted, at which point os_read_line draws the
     map and exits. */
  scr_interpret_game (g0);
  return 0;
}

/* vi: set ts=8:
 *
 * Headless map renderer: loads an ADRIFT 5 game, optionally plays a command
 * script (to build up the visited set), then rasterises the map the player
 * would see and writes it out as a PPM.  Lets the map be eyeballed and diffed
 * without the Glk layer.
 *
 *   ./map_dump <game.blorb> [script.txt] [-o out.ppm] [-w W] [-h H] [-all]
 *
 *   -all   mark every room seen (whole-page view, for development)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>

#include "a5map.h"
#include "a5model.h"
#include "a5parse.h"
#include "a5restr.h"
#include "a5run.h"
#include "a5state.h"
#include "a5text.h"

struct ctx_t {
  a5_state_t *st;
  int reveal_all;
  std::map<std::string, std::string> names;
};

static int
cb_seen (void *vp, const char *lockey)
{
  ctx_t *c = (ctx_t *) vp;
  int li;
  if (c->reveal_all)
    return 1;
  li = a5state_location_index (c->st, lockey);
  return li >= 0 && c->st->loc_seen != NULL && c->st->loc_seen[li];
}

static const char *
cb_name (void *vp, const char *lockey)
{
  ctx_t *c = (ctx_t *) vp;
  std::map<std::string, std::string>::iterator it = c->names.find (lockey);
  if (it != c->names.end ())
    return it->second.c_str ();
  {
    char *s = a5text_location_short_plain (c->st, lockey);
    std::string v = (s != NULL) ? s : lockey;
    free (s);
    c->names[lockey] = v;
    return c->names[lockey].c_str ();
  }
}

static const char *
cb_exit_dest (void *vp, const char *lockey, int dir)
{
  ctx_t *c = (ctx_t *) vp;
  return a5restr_exit_in_direction (c->st, a5state_player_key (c->st),
                                    lockey, map_dirs[dir], NULL);
}

static int
cb_ever_blocked (void *vp, const char *lockey, int dir)
{
  ctx_t *c = (ctx_t *) vp;
  return a5restr_ever_blocked (c->st, lockey, map_dirs[dir]);
}

int
main (int argc, char **argv)
{
  a5_adventure_t *a;
  a5_run_t *run;
  map_t *map;
  map_surface_t *surf;
  map_camera_t cam;
  ctx_t ctx;
  map_view_t view;
  const char *out = "map.ppm";
  const char *script = NULL;
  const char *walk_to = NULL;
  int W = 480, H = 480, reveal = 0, i;
  FILE *f;

  if (argc < 2)
    {
      fprintf (stderr, "usage: %s <game> [script] [-o out.ppm] [-w W] [-h H]"
               " [-all]\n", argv[0]);
      return 1;
    }
  for (i = 2; i < argc; i++)
    {
      if (strcmp (argv[i], "-o") == 0 && i + 1 < argc)
        out = argv[++i];
      else if (strcmp (argv[i], "-w") == 0 && i + 1 < argc)
        W = atoi (argv[++i]);
      else if (strcmp (argv[i], "-h") == 0 && i + 1 < argc)
        H = atoi (argv[++i]);
      else if (strcmp (argv[i], "-all") == 0)
        reveal = 1;
      else if (strcmp (argv[i], "-walk") == 0 && i + 1 < argc)
        walk_to = argv[++i];
      else if (argv[i][0] != '-')
        script = argv[i];
    }

  a = a5model_load (argv[1]);
  if (a == NULL)
    {
      fprintf (stderr, "%s: could not load\n", argv[1]);
      return 1;
    }
  run = a5run_new (a);
  if (run == NULL)
    {
      fprintf (stderr, "could not start run\n");
      return 1;
    }
  free (a5run_intro (run));

  if (script != NULL)
    {
      FILE *s = fopen (script, "r");
      char line[1024];
      if (s == NULL)
        {
          fprintf (stderr, "%s: cannot open\n", script);
          return 1;
        }
      while (fgets (line, sizeof line, s) != NULL)
        {
          char *nl = strchr (line, '\n');
          if (nl != NULL)
            *nl = '\0';
          if (line[0] == '\0' || line[0] == '#')
            continue;
          free (a5run_input (run, line));
        }
      fclose (s);
    }

  map = a5map_load (a);
  if (map == NULL)
    {
      fprintf (stderr, "%s: no <Map> data\n", argv[1]);
      return 1;
    }

  ctx.st = a5run_state (run);
  ctx.reveal_all = reveal;
  view.seen = cb_seen;
  view.name = cb_name;
  view.exit_dest = cb_exit_dest;
  view.ever_blocked = cb_ever_blocked;
  view.ctx = &ctx;

  /* -walk <roomkey>: the map-click walk, driven headlessly.  Each step is
     submitted as a direction command, exactly as the Glk layer does. */
  if (walk_to != NULL)
    {
      int step;

      for (step = 0; step < 100; step++)
        {
          const char *from = a5state_player_location (ctx.st);
          const char *word;
          char *out;
          int dir;

          if (from == NULL || strcmp (from, walk_to) == 0)
            break;
          dir = map_walk_step (&view, from, walk_to);
          if (dir < 0)
            {
              fprintf (stderr, "walk: no route from %s to %s\n", from, walk_to);
              break;
            }
          word = a5parse_direction_name (map_dirs[dir]);
          printf ("[walk %2d] %-22s -> %s\n", step + 1, from, word);
          out = a5run_input (run, word);
          free (out);
          ctx.st = a5run_state (run);
        }
      printf ("[walk] ended at %s (target %s)\n",
              a5state_player_location (ctx.st), walk_to);
    }

  surf = map_surface_new (W, H);
  {
    const char *ploc = a5state_player_location (ctx.st);
    map_frame (map, &view, ploc, surf, 0, &cam);
    map_render (map, &view, ploc, &cam, surf);
    fprintf (stderr, "player=%s page=%d scale=%d\n",
             ploc ? ploc : "(none)", cam.page, cam.scale);
  }

  f = fopen (out, "wb");
  if (f == NULL)
    {
      fprintf (stderr, "%s: cannot write\n", out);
      return 1;
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
  fprintf (stderr, "wrote %s (%dx%d)\n", out, W, H);

  map_surface_free (surf);
  map_free (map);
  return 0;
}

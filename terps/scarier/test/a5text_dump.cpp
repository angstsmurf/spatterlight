/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- Phase 2 harness (static-world text engine).
 *
 * Loads an ADRIFT 5 game, builds the runtime state, and renders:
 *   - the Introduction
 *   - LOOK at the player's start location
 *   - LOOK at a named location (argv[2], to exercise restricted descriptions)
 *   - a few object descriptions
 *   - the player's inventory
 *
 * so the text engine (descriptions, restrictions, ALRs, functions, tags) can be
 * eyeballed against frankendrift / the official Runner.
 *
 *   ./a5text_dump <game.blorb> [LocationKey]
 *
 * Set A5_TRACE_RESTR=1 to trace restriction evaluation on stderr.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../adrift5/a5model.h"
#include "../adrift5/a5restr.h"
#include "../adrift5/a5state.h"
#include "../adrift5/a5text.h"

static void
rule (const char *title)
{
  printf ("\n========== %s ==========\n", title);
}

static void
describe_object (a5_state_t *st, const char *key)
{
  const a5_object_t *o = a5model_object (st->adv, key);
  char *name, *desc;
  if (o == NULL)
    {
      printf ("  (no object '%s')\n", key);
      return;
    }
  name = a5text_object_name (st, o, A5_ART_NONE);
  desc = a5text_describe (st, a5xml_child (o->node, "Description"));
  printf ("  [%s] \"%s\"\n%s\n", key, name, desc);
  free (name);
  free (desc);
}

int
main (int argc, char **argv)
{
  a5_adventure_t *a;
  a5_state_t *st;
  char *txt;
  int i;

  if (argc < 2)
    {
      fprintf (stderr, "usage: %s <game.blorb|game.taf> [LocationKey]\n", argv[0]);
      return 2;
    }
  if (getenv ("A5_TRACE_RESTR") != NULL)
    a5restr_trace = 1;

  a = a5model_load (argv[1]);
  if (a == NULL)
    {
      fprintf (stderr, "a5text_dump: failed to load %s\n", argv[1]);
      return 1;
    }
  st = a5state_new (a);
  if (st == NULL)
    {
      fprintf (stderr, "a5text_dump: out of memory\n");
      a5model_free (a);
      return 1;
    }

  printf ("Title:  %s\n", a->title ? a->title : "(none)");
  printf ("Author: %s\n", a->author ? a->author : "(none)");

  rule ("Introduction");
  txt = a5text_describe (st, a->introduction);
  printf ("%s\n", txt);
  free (txt);

  rule ("LOOK (start location)");
  printf ("[at %s]\n", a5state_player_location (st) ? a5state_player_location (st) : "?");
  txt = a5text_view_location (st);
  printf ("%s\n", txt);
  free (txt);

  if (argc >= 3)
    {
      int pi = a5state_character_index (st, "Player");
      rule (argv[2]);
      if (pi >= 0)
        st->char_loc[pi] = argv[2];   /* teleport for the description test */
      txt = a5text_view_location (st);
      printf ("%s\n", txt);
      free (txt);
    }

  rule ("Object descriptions");
  {
    /* Describe the first handful of dynamic (takeable) objects. */
    int shown = 0;
    for (i = 0; i < a->n_objects && shown < 4; i++)
      {
        if (!st->obj[i].is_static)
          {
            describe_object (st, a->objects[i].key);
            shown++;
          }
      }
  }

  rule ("Inventory (player)");
  {
    int pi = a5state_character_index (st, "Player");
    int held = 0;
    for (i = 0; i < a->n_objects; i++)
      if (st->obj[i].where == A5_OWHERE_HELD_BY
          && pi >= 0
          && st->obj[i].key != NULL
          && strcmp (st->obj[i].key, a->characters[pi].key) == 0)
        {
          char *name = a5text_object_name (st, &a->objects[i], A5_ART_INDEFINITE);
          printf ("  %s\n", name);
          free (name);
          held++;
        }
    if (held == 0)
      printf ("  (carrying nothing)\n");
  }

  a5state_free (st);
  a5model_free (a);
  return 0;
}

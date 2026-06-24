/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Regression test for gs_create()'s validation of object parent indices.
 *
 * Some published games place a dynamic object "held by" / "in" a nonexistent
 * NPC or container (e.g. The Timmy Reid Adventure, Blood Relatives).  SCARE
 * used to store the out-of-range parent and then assert-crash on the first
 * turn-update (gs_npc_location / gs_object_openness, via obj_setup_initial).
 * gs_create() now validates the parent and hides such objects instead.
 *
 * The synthetic game (make_badparent_taf.py) has one NPC and three objects:
 *   amulet -- held by NPC 99 (nonexistent)  -> must be hidden
 *   coin   -- in container -1 (nonexistent)  -> must be hidden
 *   ring   -- held by NPC 1 (the real one)   -> must NOT be hidden
 *
 * Loading the game runs the first turn-update; reaching this code at all proves
 * it no longer crashes.  We then confirm the placements above.
 *
 * Pass the .taf path as argv[1].  Exits 0 on success, 1 on any mismatch.
 */
#include <stdio.h>
#include <string.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"

static int failures = 0;
static sc_game g_game = NULL;
static int probed = 0;

static void
expect (const char *what, int got, int want)
{
  const int ok = (!got == !want);
  printf ("  [%s] %-44s got %d, want %d\n",
          ok ? "PASS" : "FAIL", what, !!got, !!want);
  if (!ok)
    failures++;
}

static sc_int
find_object (sc_gameref_t game, const char *name)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_int o;
  for (o = 0; o < gs_object_count (game); o++)
    {
      sc_vartype_t k[3], v;
      k[0].string = "Objects"; k[1].integer = o; k[2].string = "Short";
      if (prop_get (bundle, "S<-sis", &v, k) && strcmp (v.string, name) == 0)
        return o;
    }
  return -1;
}

/* --- minimal SCARE OS port; probe the object state at the first prompt --- */

void os_print_string (const sc_char *s) { (void) s; }
void os_print_tag (sc_int t, const sc_char *a) { (void) t; (void) a; }
void os_play_sound (const sc_char *p, sc_int o, sc_int l, sc_bool v)
{ (void) p; (void) o; (void) l; (void) v; }
void os_stop_sound (void) { }
void os_show_graphic (const sc_char *p, sc_int o, sc_int l)
{ (void) p; (void) o; (void) l; }
void os_display_hints (sc_game game) { (void) game; }
sc_bool os_confirm (sc_int type) { (void) type; return 1; }
void *os_open_file (sc_bool s) { (void) s; return 0; }
void os_write_file (void *o, const sc_byte *b, sc_int l) { (void)o;(void)b;(void)l; }
sc_int os_read_file (void *o, sc_byte *b, sc_int l) { (void)o;(void)b;(void)l; return 0; }
void os_close_file (void *o) { (void) o; }
void os_print_string_debug (const sc_char *s) { (void) s; }
sc_bool os_read_line_debug (sc_char *b, sc_int l) { return os_read_line (b, l); }

sc_bool
os_read_line (sc_char *buffer, sc_int length)
{
  /* The first time we are asked for input the game has loaded and run its
   * first turn-update (the old crash point); inspect object placement now. */
  if (!probed)
    {
      sc_gameref_t game = (sc_gameref_t) g_game;
      sc_int amulet = find_object (game, "amulet");
      sc_int coin   = find_object (game, "coin");
      sc_int ring   = find_object (game, "ring");

      probed = 1;
      expect ("loaded without crashing", 1, 1);
      expect ("amulet (bad NPC) is hidden",
              gs_object_position (game, amulet) == OBJ_HIDDEN, 1);
      expect ("coin (bad container) is hidden",
              gs_object_position (game, coin) == OBJ_HIDDEN, 1);
      expect ("ring (real NPC) is NOT hidden",
              gs_object_position (game, ring) == OBJ_HIDDEN, 0);
    }

  strncpy ((char *) buffer, "quit", length - 1);
  buffer[length - 1] = '\0';
  return 1;
}

int
main (int argc, char **argv)
{
  const char *path = (argc > 1) ? argv[1] : "badparent_test.taf";

  g_game = sc_game_from_filename (path);
  if (!g_game)
    {
      fprintf (stderr, "badparent_test: cannot load %s\n", path);
      return 1;
    }

  printf ("loaded %s\n", path);
  sc_interpret_game (g_game);
  sc_free_game (g_game);

  if (!probed)
    {
      fprintf (stderr, "badparent_test: never reached input prompt\n");
      return 1;
    }
  printf ("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}

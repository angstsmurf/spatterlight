/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * In-repo regression test for the SCARE Battle System port.
 *
 * Loads the synthetic, fully deterministic game test/battle_test.taf (built by
 * make_battle_taf.py) and checks the combat math through the public battle API.
 * Because every battle attribute has Lo == Hi, battle_roll() collapses to a
 * single value and outcomes are seed-independent, so the expected numbers below
 * are exact.
 *
 * Game contents (see make_battle_taf.py):
 *   player : Strength 10, Accuracy 60, Defense 5, Agility 5, Stamina 100
 *   Robot  : enemy, Strength 8, Accuracy 10, Defense 3, Agility 4, Stamina 100
 *   blaster: shoot weapon (Method 3, replaces strength), HitValue 30, Acc 20
 *   rock   : throw weapon (Method 5, adds HitValue),     HitValue 12, Acc 10
 *   vest   : worn armour, ProtectionValue 5 (not a weapon)
 *
 * Checks:
 *   1. shoot replaces strength : eff_str = 30  -> 30-3 = 27 damage/hit
 *   2. throw adds HitValue      : eff_str = 10+12 = 22 -> 22-3 = 19 damage/hit
 *   3. worn armour adds to defence: Robot's 8-str blow vs Def 5 + vest 5 = 10
 *      is fully absorbed (0 damage); removing the vest lets 8-5 = 3 through.
 *
 * Exits 0 on success, 1 on any mismatch.
 */
#include <stdio.h>
#include <string.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"

static int failures = 0;

static void
check (const char *what, long got, long want)
{
  const int ok = (got == want);
  printf ("  [%s] %-44s got %ld, want %ld\n",
          ok ? "PASS" : "FAIL", what, got, want);
  if (!ok)
    failures++;
}

/* Find an object by its Short name. */
static sc_int
find_object (sc_gameref_t game, const char *name)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_int o;

  for (o = 0; o < gs_object_count (game); o++)
    {
      sc_vartype_t vt_key[3], vt_rvalue;
      vt_key[0].string = "Objects";
      vt_key[1].integer = o;
      vt_key[2].string = "Short";
      if (prop_get (bundle, "S<-sis", &vt_rvalue, vt_key)
          && strcmp (vt_rvalue.string, name) == 0)
        return o;
    }
  return -1;
}

int
main (int argc, char **argv)
{
  const char *path = (argc > 1) ? argv[1] : "battle_test.taf";
  sc_game g0;
  sc_gameref_t game;
  sc_int blaster, rock, vest, robot;
  sc_int before, after;

  g0 = sc_game_from_filename (path);
  if (!g0)
    {
      fprintf (stderr, "battle_test: cannot load %s\n", path);
      return 1;
    }
  game = (sc_gameref_t) g0;
  game->is_running = TRUE;

  /* The checks below are seed-independent (every battle attribute has Lo == Hi),
   * but force SCARE's portable RNG with a fixed seed anyway so the test cannot
   * be perturbed by the host's rand()/time() state. */
  sc_set_portable_random (TRUE);
  sc_reseed_random_sequence (1);

  battle_start (game);

  /* Locate the actors. */
  blaster = find_object (game, "blaster");
  rock = find_object (game, "rock");
  vest = find_object (game, "vest");
  robot = 0;   /* the only NPC */

  printf ("loaded %s: blaster=%ld rock=%ld vest=%ld robot stamina=%ld\n",
          path, (long) blaster, (long) rock, (long) vest,
          (long) gs_npc_stamina (game, robot));

  /* Sanity: the objects parsed as expected. */
  check ("blaster is a weapon", battle_is_weapon (game, blaster), 1);
  check ("blaster method == shoot(3)", battle_weapon_method (game, blaster), 3);
  check ("rock is a weapon", battle_is_weapon (game, rock), 1);
  check ("rock method == throw(5)", battle_weapon_method (game, rock), 5);
  check ("vest is not a weapon", battle_is_weapon (game, vest), 0);

  /* 1. shoot weapon replaces strength: 30 - Def(3) = 27 damage. */
  gs_set_npc_stamina (game, robot, 100);
  before = gs_npc_stamina (game, robot);
  battle_player_attack (game, robot, blaster);
  after = gs_npc_stamina (game, robot);
  check ("shoot blaster damage (replace)", before - after, 27);

  /* 2. throw weapon adds HitValue: 10 + 12 - Def(3) = 19 damage. */
  gs_set_npc_stamina (game, robot, 100);
  before = gs_npc_stamina (game, robot);
  battle_player_attack (game, robot, rock);
  after = gs_npc_stamina (game, robot);
  check ("throw rock damage (add)", before - after, 19);

  /* 3a. With the vest worn, the Robot's blow (Str 8) vs Def 5 + armour 5 = 10
   *     is fully absorbed.  Robot has Speed 0 so it strikes every tick. */
  gs_set_npc_stamina (game, robot, 100);
  gs_set_npc_location (game, robot, gs_playerroom (game) + 1);
  gs_set_playerstamina (game, 100);
  battle_tick (game);
  check ("armour absorbs blow (vest worn)", 100 - gs_playerstamina (game), 0);

  /* 3b. Remove the vest (held, no longer worn); now 8 - Def 5 = 3 gets through. */
  gs_object_player_get (game, vest);
  gs_set_npc_stamina (game, robot, 100);
  gs_set_npc_location (game, robot, gs_playerroom (game) + 1);
  gs_set_playerstamina (game, 100);
  battle_tick (game);
  check ("blow lands without armour", 100 - gs_playerstamina (game), 3);

  printf ("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}

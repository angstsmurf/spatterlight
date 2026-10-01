/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjölund
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
 *
 * In-repo regression test for the Scarier Battle System port.
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
 *   rock   : throw weapon (Method 5, drops + Str-only),  HitValue 12, Acc 10
 *   vest   : worn armour, ProtectionValue 5 (not a weapon)
 *
 * Checks:
 *   1. shoot replaces strength : eff_str = 30  -> 30-3 = 27 damage/hit
 *   2. throw ignores HitValue and lands in the room (run400, settled live
 *      2026-08-01): eff_str = 10 -> 10-3 = 7 damage, rock leaves inventory
 *   3. worn armour adds to defence: Robot's 8-str blow vs Def 5 + vest 5 = 10
 *      is fully absorbed (0 damage); removing the vest lets 8-5 = 3 through.
 *   4. the wield model (run400, settled live 2026-08-01): no auto-wield at
 *      battle start, an armed attack persists the wield, a landed throw and
 *      a drop clear it, and re-taking a dropped weapon does not re-wield.
 *   5. hostile input: corrupt saves (bad wield, player room 0, containment
 *      loop) and one-line TAF edits (start room, negative WaitTurns, a
 *      self-containing object, an unterminated multiline) fail cleanly.
 *
 * Exits 0 on success, 1 on any mismatch.
 */
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "zlib.h"
#include "scarier.h"
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
static scr_int
find_object (scr_gameref_t game, const char *name)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int o;

  for (o = 0; o < gs_object_count (game); o++)
    {
      scr_vartype_t vt_key[3], vt_rvalue;
      vt_key[0].string = "Objects";
      vt_key[1].integer = o;
      vt_key[2].string = "Short";
      if (prop_get (bundle, "S<-sis", &vt_rvalue, vt_key)
          && strcmp (vt_rvalue.string, name) == 0)
        return o;
    }
  return -1;
}

/*
 * Hostile-input regressions (5).  Saves are built by corrupting a scratch
 * game's state and saving it, so the save layout never has to be spelled out
 * here; TAFs are built by editing one line of battle_test.taf's inflated body.
 */
struct byte_source
{
  const std::string *data;
  size_t offset;
};

static scr_int
string_read (void *opaque, scr_byte *buffer, scr_int length)
{
  byte_source *source = (byte_source *) opaque;
  size_t count = source->data->size () - source->offset;

  if (count > (size_t) length)
    count = length;
  memcpy (buffer, source->data->data () + source->offset, count);
  source->offset += count;
  return (scr_int) count;
}

static void
string_write (void *opaque, const scr_byte *buffer, scr_int length)
{
  ((std::string *) opaque)->append ((const char *) buffer, length);
}

/* Save scratch, then try to restore that save into target. */
static scr_bool
restore_saved (scr_game scratch, scr_game target)
{
  std::string save;
  byte_source source = { &save, 0 };

  scr_save_game_to_callback (scratch, string_write, &save);
  return scr_load_game_from_callback (target, string_read, &source);
}

/* Inflate a v4.0 TAF's zlib body (after the 22-byte header) into lines. */
static std::vector<std::string>
taf_lines (const std::string &taf)
{
  std::vector<std::string> lines;
  std::string body;
  z_stream stream;
  unsigned char out[16384];
  int status;

  memset (&stream, 0, sizeof (stream));
  inflateInit (&stream);
  stream.next_in = (Bytef *) taf.data () + 22;
  stream.avail_in = (uInt) (taf.size () - 22);
  do
    {
      stream.next_out = out;
      stream.avail_out = sizeof (out);
      status = inflate (&stream, Z_NO_FLUSH);
      body.append ((const char *) out, sizeof (out) - stream.avail_out);
    }
  while (status == Z_OK);
  inflateEnd (&stream);

  for (size_t start = 0, end; start < body.size (); start = end + 2)
    {
      end = body.find ("\r\n", start);
      if (end == std::string::npos)
        end = body.size ();
      lines.push_back (body.substr (start, end - start));
    }
  return lines;
}

/* Rebuild a v4.0 TAF from header and lines, and try to load it. */
static scr_game
load_taf_lines (const std::string &taf, const std::vector<std::string> &lines)
{
  std::string body, image (taf, 0, 22);
  std::vector<unsigned char> packed;
  uLongf packed_size;
  byte_source source = { &image, 0 };

  for (size_t index = 0; index < lines.size (); index++)
    body += lines[index] + "\r\n";
  packed_size = compressBound (body.size ());
  packed.resize (packed_size);
  compress (packed.data (), &packed_size,
            (const Bytef *) body.data (), body.size ());
  image.append ((const char *) packed.data (), packed_size);
  return scr_game_from_callback (string_read, &source);
}

/* Index of the first line equal to text, or -1. */
static long
line_index (const std::vector<std::string> &lines, const char *text)
{
  for (size_t index = 0; index < lines.size (); index++)
    {
      if (lines[index] == text)
        return (long) index;
    }
  return -1;
}

static void
hostile_input_checks (const char *path)
{
  std::string taf;
  FILE *stream;
  scr_game target, scratch;
  scr_gameref_t target_, scratch_;

  printf ("hostile saves and TAFs:\n");

  target = scr_game_from_filename (path);
  scratch = scr_game_from_filename (path);
  target_ = (scr_gameref_t) target;
  scratch_ = (scr_gameref_t) scratch;

  /* 5a. An untouched save restores. */
  check ("clean save restores", restore_saved (scratch, target), 1);

  /* 5b. A wield that is not an object restores as no wield. */
  gs_set_playerwield (scratch_, 99999999);
  check ("bad wield: save restores", restore_saved (scratch, target), 1);
  check ("bad wield: cleared to none", gs_playerwield (target_), -1);
  gs_set_playerwield (scratch_, -1);

  /* 5c. Player room 0 in the save (room -1) is refused. */
  scratch_->playerroom = -1;
  check ("player room 0 refused", restore_saved (scratch, target), 0);
  scratch_->playerroom = 0;

  /* 5d. Two objects inside each other are refused. */
  scratch_->objects[0].position = OBJ_IN_OBJECT;
  scratch_->objects[0].parent = 1;
  scratch_->objects[1].position = OBJ_IN_OBJECT;
  scratch_->objects[1].parent = 0;
  check ("containment loop refused", restore_saved (scratch, target), 0);
  check ("target untouched by refusals", gs_playerroom (target_), 0);

  scr_free_game (scratch);
  scr_free_game (target);

  stream = fopen (path, "rb");
  if (stream)
    {
      char buffer[4096];
      size_t count;

      while ((count = fread (buffer, 1, sizeof (buffer), stream)) > 0)
        taf.append (buffer, count);
      fclose (stream);
    }

  {
    const std::vector<std::string> lines = taf_lines (taf);
    std::vector<std::string> edit;
    const long name = line_index (lines, "Battle Test");
    const long vest = line_index (lines, "vest");
    scr_game game;

    /* The unedited body loads, so the edits below are what fail. */
    game = load_taf_lines (taf, lines);
    check ("rebuilt TAF loads", game != NULL, 1);
    scr_free_game (game);

    /* 5e. StartRoom (the line after the startup text's separator). */
    edit = lines;
    edit[2] = "5";
    game = load_taf_lines (taf, edit);
    check ("start room out of range refused", game != NULL, 0);
    scr_free_game (game);

    /* 5f. WaitTurns, five lines after GameName, negative -> no wait. */
    edit = lines;
    edit[name + 5] = "-7";
    game = load_taf_lines (taf, edit);
    check ("negative WaitTurns loads", game != NULL, 1);
    if (game)
      check ("negative WaitTurns -> 0",
             ((scr_gameref_t) game)->waitturns, 0);
    scr_free_game (game);

    /* 5g. The vest, the only container, "in" container 0: itself. */
    edit = lines;
    edit[vest + 4] = "2";
    game = load_taf_lines (taf, edit);
    check ("self-containing object refused", game != NULL, 0);
    scr_free_game (game);

    /* 5h. A multiline field ending on an empty line (no separator); under
       ASan this used to over-read the slab by one byte. */
    edit.assign (1, "Startup");
    edit.push_back ("");
    game = load_taf_lines (taf, edit);
    check ("unterminated multiline refused", game != NULL, 0);
    scr_free_game (game);
  }
}

int
main (int argc, char **argv)
{
  const char *path = (argc > 1) ? argv[1] : "battle_test.taf";
  scr_game g0;
  scr_gameref_t game;
  scr_int blaster, rock, vest, robot;
  scr_int before, after;

  g0 = scr_game_from_filename (path);
  if (!g0)
    {
      fprintf (stderr, "battle_test: cannot load %s\n", path);
      return 1;
    }
  game = (scr_gameref_t) g0;
  game->is_running = TRUE;

  /* The checks below are seed-independent (every battle attribute has Lo == Hi),
   * but force Scarier's portable RNG with a fixed seed anyway so the test cannot
   * be perturbed by the host's rand()/time() state. */
  scr_set_portable_random (TRUE);
  scr_reseed_random_sequence (1);

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

  /* 4a. The wield is a persistent reference, empty at battle start: with no
   *     wield set the combatant weapon is bare hands, never the best carried
   *     weapon.  Take the rock so two weapons are carried. */
  check ("no wield at battle start", gs_playerwield (game), -1);
  gs_object_player_get (game, rock);
  check ("two weapons carried", battle_player_weapon_count (game), 2);
  check ("no wield means bare hands",
         battle_combatant_weapon (game, -1), -1);

  /* 1. shoot weapon replaces strength: 30 - Def(3) = 27 damage. */
  gs_set_npc_stamina (game, robot, 100);
  before = gs_npc_stamina (game, robot);
  battle_player_attack (game, robot, blaster);
  after = gs_npc_stamina (game, robot);
  check ("shoot blaster damage (replace)", before - after, 27);

  /* 4b. The armed attack persisted the blaster as the wield. */
  check ("armed attack persists the wield", gs_playerwield (game), blaster);

  /* 2. A landed throw deals base strength only (HitValue never contributes
   *    in 4.0: 10 - Def(3) = 7) and the weapon lands in the player's room. */
  gs_set_npc_stamina (game, robot, 100);
  before = gs_npc_stamina (game, robot);
  battle_player_attack (game, robot, rock);
  after = gs_npc_stamina (game, robot);
  check ("throw rock damage (Str only)", before - after, 7);
  check ("thrown rock lands in the room",
         gs_object_position (game, rock), gs_playerroom (game) + 1);
  check ("landed throw clears the wield", gs_playerwield (game), -1);

  /* 4c. Dropping the wielded weapon clears the wield -- no fallback to
   *     another carried weapon -- and re-taking it does not re-wield. */
  battle_player_attack (game, robot, blaster);
  check ("re-armed with the blaster", gs_playerwield (game), blaster);
  gs_object_to_room (game, blaster, gs_playerroom (game));
  check ("dropping the wield clears it", gs_playerwield (game), -1);
  gs_object_player_get (game, blaster);
  check ("re-taking does not re-wield", gs_playerwield (game), -1);

  /* 3a. With the vest worn, the Robot's blow (Str 8) vs Def 5 + armour 5 = 10
   *     is fully absorbed.  Robot has Speed 0 so it strikes every tick. */
  gs_set_npc_stamina (game, robot, 100);
  gs_set_npc_location (game, robot, gs_playerroom (game) + 1);
  gs_set_playerstamina (game, 100);
  battle_tick_npc (game, robot);
  check ("armour absorbs blow (vest worn)", 100 - gs_playerstamina (game), 0);

  /* 3b. Remove the vest (held, no longer worn); now 8 - Def 5 = 3 gets through. */
  gs_object_player_get (game, vest);
  gs_set_npc_stamina (game, robot, 100);
  gs_set_npc_location (game, robot, gs_playerroom (game) + 1);
  gs_set_playerstamina (game, 100);
  battle_tick_npc (game, robot);
  check ("blow lands without armour", 100 - gs_playerstamina (game), 3);

  hostile_input_checks (path);

  printf ("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}

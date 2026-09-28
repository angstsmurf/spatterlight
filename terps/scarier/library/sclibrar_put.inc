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
 * Part of sclibrar.cpp, which #includes it; not compiled on its own.
 * Putting objects in and on other objects.
 */

/*
 * lib_check_put_in_recursion()
 *
 * Checks for infinite recursion when placing an object in an object.  Returns
 * TRUE if no recursion detected.
 */
static scr_bool
lib_check_put_in_recursion (scr_gameref_t game,
                            scr_int object, scr_int container, scr_bool report)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int check;

  /* Avoid the obvious possibility of infinite recursion. */
  if (container == object)
    {
      if (report)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                "You can't put an object inside itself!",
                                "I can't put an object inside itself!",
                                "%player% can't put an object inside itself!"));
        }
      return FALSE;
    }

  /* Avoid the subtle possibility of infinite recursion. */
  check = container;
  while (gs_object_position (game, check) == OBJ_ON_OBJECT
         || gs_object_position (game, check) == OBJ_IN_OBJECT)
    {
      check = gs_object_parent (game, check);
      if (check == object)
        {
          if (report)
            {
              pf_buffer_string (filter,
                                lib_select_response (game,
                                    "You can't put an object inside one",
                                    "I can't put an object inside one",
                                    "%player% can't put an object inside one"));
              pf_buffer_string (filter, " it's on or in!");
            }
          return FALSE;
        }
    }

  /* No infinite recursion detected. */
  return TRUE;
}


/*
 * A seen object lying elsewhere that a 4.0 put's noun names, admitted to
 * lib_put_named_filter() for the rest of that one command; see
 * lib_put_in_multiple_common().
 */
static scr_int lib_put_seen_absent = -1;

/*
 * The twin case: an object present but still unseen, resolved for the rest
 * of that one command by asking the scorer directly instead of the seen-gated
 * %text% parse; see lib_put_fragment_present_object() and
 * lib_put_in_multiple_common().  Named so lib_put_print_object_or_that() can
 * tell this one object to print as "that" rather than by name -- the object
 * genuinely remains unseen, and the Runner's name composer answers "that"
 * for any unseen object it is asked to print (see the "Getting off" comment
 * above lib_cmd_go_*()).
 */
static scr_int lib_put_present_unseen = -1;

/*
 * Both admissions last for one command only.  Each put backend's multiple
 * form holds one of these for its whole run, so the two objects are
 * forgotten however the command ends.
 */
struct lib_put_admission_guard_t
{
  ~lib_put_admission_guard_t ()
  {
    lib_put_seen_absent = -1;
    lib_put_present_unseen = -1;
  }
};

/*
 * lib_put_print_object_or_that()
 *
 * lib_print_object_np(), except for the one object this command resolved
 * despite it being unseen: that one prints as "that", matching the Runner's
 * name composer.  Measured on hub (run400, 2026-09-17): the minestrone soup,
 * revealed only by task 11 opening the tin can with no listing of its own,
 * answers `put soup in pan` with "I put that inside the saucepan." rather
 * than naming the soup.
 */
static void
lib_put_print_object_or_that (scr_gameref_t game, scr_int object)
{
  if (object == lib_put_present_unseen)
    pf_buffer_string (gs_get_filter (game), "that");
  else
    lib_print_object_np (game, object);
}

/*
 * lib_put_named_filter()
 * lib_put_all_filter()
 *
 * The two universes a "put" command draws its objects from.  They are not
 * the same set, and the Runners' put handlers make the split explicitly.
 *
 * Naming an object (run400's Proc_19_41, selection code at loc_46E01F on)
 * resolves it with the general object matcher and then applies *no* position
 * test whatsoever before running it -- the execute loop's guard reads
 * "var_AC(obj) = 1 Or (<all-matched> And ...)", so a named object arrives
 * pre-approved.  Anything the player can see in the room is therefore fair
 * game, and 4.0 quietly picks it up first (see lib_put_implicit_take).  The
 * pre-4.0 handlers are stricter but still not held-only: run390's
 * loc_461C66 accepts field22 = 0 (held) or field22 = the worn marker, so a
 * worn object may be put down without removing it first.
 *
 * "all" is narrower, and narrower again at 4.0 than before it.  run400's
 * candidate loop takes objects with field26 = 0 And field24 = 0, i.e. lying
 * in the player's hands, never worn and never inside anything; that is the
 * set "put all in X" and "put all except Y on Z" range over there, and it is
 * what the "You are not holding ..." leftover report is phrased for.  run390
 * also takes what is lying loose in the room: on p39DARK (Adrift_980:50,
 * 2026-09-12), with the torch, lamp and coin in hand and the stone and the
 * pebble on the cave floor, `put all in box` answers "You put the torch, the
 * lamp, the stone, the pebble and the coin inside the box."  It stops there
 * -- once every object is inside the box, the same command answers "Nothing
 * will fit inside the box." (Adrift_980:59), the empty-list message, so the
 * contents of a container standing in the room are NOT candidates.  The 4.0
 * control is Adrift_981:11: `put all in box` with the stone, pebble and coin
 * loose in the room moves only the two held objects.
 */
static scr_bool
lib_put_named_filter (scr_gameref_t game, scr_int object)
{
  /*
   * A static is named too, at 4.0.  There is no static test at name time --
   * the one that turns the piece away lives in `insides` (@465ED7) and is
   * silent -- so the line is claimed and the take piece runs on it first.
   * Probe PSTAT (Adrift_941/942_pstat.txt): `put anvil in box`, the anvil a
   * static lying in the room, answers "(Taking the anvil first)" / "You
   * can't take the anvil!" where scarier used to fall through to the game's
   * DontUnderstand.  Pre-4.0 handlers never see a static.
   */
  if (obj_is_static (game, object))
    return lib_is_version_400 (game)
           && obj_indirectly_in_room (game, object, gs_playerroom (game));

  if (lib_is_version_400 (game))
    return object == lib_put_seen_absent
           || obj_indirectly_in_room (game, object, gs_playerroom (game));

  /*
   * Pre-4.0 is wider than "held or worn" as well.  run390 takes an object
   * lying loose in the room -- there is no implicit take and no announcement,
   * the put simply runs -- and one sitting inside a container the player is
   * carrying.  Measured on p39DARK against run390 (2026-09-12):
   *
   *   Adrift_978:55   pebble loose on the cave floor, box in hand, `put
   *                   pebble in box` -> "You put the pebble inside the box."
   *   Adrift_976:31   box in hand with the coin inside it, `put coin in box`
   *                   -> "You put the coin inside the box." (a no-op move,
   *                   but the Runner says it all the same)
   *
   * and it is only this wide: the coin inside the box once the box is on the
   * FLOOR is refused, "You can't see that." (Adrift_978:40), so the reach is
   * one step from the player's hands, not the whole room's contents.  See
   * lib_put_not_reachable_pre400() for that refusal.
   *
   * 3.7 and 3.8 are narrower again, and the step into the player's own
   * container is exactly what they lack: a container's contents are out of
   * the put universe no matter whose hands the container is in.  Measured on
   * p38DARK under run380 and p37DARK under run370, 2026-09-12, the box held
   * and open with the coin inside it -- `put coin in box` answers "You can't
   * see that." (Adrift_982:18/24/33 and Adrift_986:18/24/33, and again with
   * the box brought into the lit room, Adrift_985:28 / Adrift_989:28), where
   * run390 moves the coin and says so.  Loose room objects stay reachable on
   * both: `put pebble in box` -> "You put the pebble inside the box."
   * (Adrift_983:57 / Adrift_987:57).  So run390's one step of reach into a
   * carried container is a 3.90 addition.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return gs_object_position (game, object) == OBJ_HELD_PLAYER
           || gs_object_position (game, object) == OBJ_WORN_PLAYER
           || obj_directly_in_room (game, object, gs_playerroom (game));

  return gs_object_position (game, object) == OBJ_HELD_PLAYER
         || gs_object_position (game, object) == OBJ_WORN_PLAYER
         || obj_indirectly_held_by_player (game, object)
         || obj_directly_in_room (game, object, gs_playerroom (game));
}


/*
 * lib_put_resolve_filter()
 *
 * The tie-break a named put uses when more than one object present answers
 * to the word the player typed.  It is lib_put_named_filter() with the
 * statics taken back out again: a static reaches the handler, and is turned
 * away there (see lib_put_implicit_take), but it never wins a name it shares
 * with something the player can actually move.
 *
 * Measured in three run400 replays, none of which raises the 4.0 "Which X."
 * prompt on the line:
 *
 *   easter      Adrift_273:135  `put egg in basket` -> "You put the creme
 *               egg inside the Easter basket.", with the game's other eggs
 *               standing in the room
 *   helsing     Adrift_181:50   `put beads on dance floor` runs the game's
 *               own task past a namesake "beads"
 *   provenance  Adrift_342:1781 `put wood on stump` -> "You place the piece
 *               of wood on the stump.", picking it over the cord of wood
 *
 * Only the resolver narrows this way.  The filter that selects the objects
 * the handler then works on stays wide, so `put anvil in box` -- one static,
 * no rival -- still reaches the take piece (probe PSTAT).
 */
static scr_bool
lib_put_resolve_filter (scr_gameref_t game, scr_int object)
{
  return !obj_is_static (game, object) && lib_put_named_filter (game, object);
}

static scr_bool
lib_put_all_filter (scr_gameref_t game, scr_int object, scr_int associate)
{
  if (obj_is_static (game, object) || object == associate)
    return FALSE;

  if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
    return TRUE;

  return !lib_is_version_400 (game)
         && obj_directly_in_room (game, object, gs_playerroom (game));
}


/*
 * The bytes of "(Taking ... first)" announcements printed by the current put
 * backend; see lib_put_outcome_t's is_announce_only.
 */
static size_t lib_put_announce_bytes = 0;

/*
 * lib_put_reject_reference()
 *
 * Move 'object' out of the object references and into the multiple
 * references, for the "You are not holding ..." report; see
 * lib_put_implicit_take, whose several exits all do this.  Always FALSE, for
 * a direct "return lib_put_reject_reference (...)".
 */
static scr_bool
lib_put_reject_reference (scr_gameref_t game, scr_int object)
{
  game->object_references[object] = FALSE;
  game->multiple_references[object] = TRUE;
  return FALSE;
}


/*
 * lib_put_implicit_take()
 *
 * Version 4.0 only.  Having accepted a named object that the player is not
 * carrying, run400 acquires it before the put proper, printing "(Taking the
 * lamp first)" and then continuing as if it had always been held (loc_46E295
 * on).  The announcement uses Proc_21_31_448710 with mode 0, the definite
 * "the lamp" form, and unlike the key-acquisition wording it carries no
 * "from the chest" clause.  The string appears in run400 and in no earlier
 * Runner, so this is squarely a 4.0 addition.  run400 guards the take with
 * a task match on the unmodified player line (Proc_19_35_453C50 on
 * MemVar_49428C); the caller has just run the equivalent test, so there is
 * nothing left to check here.
 *
 * The announcement is unconditional -- it is printed before the acquisition
 * is attempted, not after it succeeds -- and the acquisition itself is the
 * ordinary take, capacity limits and all.  Measured in run400 with a full
 * rucksack (Provenance, 2026-08-23):
 *
 *   > put raincoat in rucksack
 *   (Taking the raincoat first)
 *   The raincoat is too heavy for you to carry at the moment.  You are not
 *   holding the raincoat.
 *
 * Returns TRUE if the put may go ahead, FALSE if the object has been moved
 * to the multiple references for that "You are not holding ..." report.
 * *printed says whether anything was written, so the caller can join the
 * report to it with the usual two spaces.
 */
static scr_bool
lib_put_implicit_take (scr_gameref_t game, scr_int object, scr_int target,
                       scr_bool *printed)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (!lib_is_version_400 (game))
    return TRUE;

  if (obj_indirectly_held_by_player (game, object))
    return TRUE;

  /*
   * The acquisition is skipped when the object already sits at the
   * destination -- run400 tests obj.global_46 <> var_92, the container or
   * surface it is about to be moved to -- and the put executor's own
   * possession test then refuses it (Proc_21_46_44615C at loc_465EED).  So
   * "put ball on marker" with the ball already on the marker answers "You
   * are not holding the golf ball.", which is what the multiple-reference
   * report is phrased for.
   */
  if (gs_object_parent (game, object) == target
      && (gs_object_position (game, object) == OBJ_IN_OBJECT
          || gs_object_position (game, object) == OBJ_ON_OBJECT))
    return lib_put_reject_reference (game, object);

  /*
   * The take is also skipped, silently, when the TYPED line pre-matches a
   * task (run400 name_object Proc_19_41_46E5D8 @46E2C7: Proc_19_35_453C50
   * on MemVar_49428C, the line as the synonym table left it, with the
   * task's restrictions passing or failing on a restriction that has a
   * message -- House's task 60 "* %object%", restricted to the spinning
   * house with no message, is NOT a hit, and run400 goes on to "(Taking the
   * wood first)"; Adrift_91.txt); the object then reaches the put handler
   * unheld and draws its "not holding" refusal, or -- if the handler's own
   * canonical look-up claims -- the task.  The task the player spelled out
   * is what the refusal hands the line to afterwards (see
   * run_all_commands()).
   */
  if (lib_task_prematches_input (game, 1))
    return lib_put_reject_reference (game, object);

  {
    /*
     * Inside a put list's clause loop the announcement goes to the front of
     * the turn: the Runner prints it straight to its textbox while the
     * clause answers accumulate in a string shown at the end of the turn.
     * See pf_hoist_tail() and run_in_put_clause_loop().
     */
    const size_t hoist_from = pf_buffer_length (filter);

    /*
     * The name is Proc_21_31_448710 mode 0, which answers "that" for an
     * object the player has never seen: fairscare T1 `put doll in fire`,
     * the voodoo doll present but unseen, is "(Taking that first)"
     * (run400x seed 1234, runner_transcripts/fairscare.txt).
     */
    pf_buffer_string (filter, "(Taking ");
    if (gs_object_seen (game, object))
      lib_put_print_object_or_that (game, object);
    else
      pf_buffer_string (filter, "that");
    pf_buffer_string (filter, " first)\n");
    lib_put_announce_bytes += pf_buffer_length (filter) - hoist_from;
    if (run_in_put_clause_loop ())
      pf_hoist_tail (filter, hoist_from);
  }

  /*
   * The announcement is printed by name_object before the take piece runs
   * (@46E2EA-46E30C, vbCrLf included), and the piece then gives the tasks
   * "get the X" first (Proc_19_39_46302C @462AED); a claim ends the take
   * there, and the put handler goes on to test possession as the task
   * left it.
   */
  if (lib_try_game_command_take_definite (game, object))
    {
      if (obj_indirectly_held_by_player (game, object))
        return TRUE;
      *printed = TRUE;
      return lib_put_reject_reference (game, object);
    }

  /*
   * A static piece cannot be taken at all, and the take piece says so in its
   * own words: run400 @47329D builds " can't take " + name + "!", the same
   * exclamation the 4.0 single-take handler ends on.  It does NOT go on to
   * become a leftover -- probe PSTAT command 4 (`put anvil in box` with the
   * coin in hand) prints "You can't take the anvil!" and then the clause
   * separator and nothing at all, where a leftover would have been named --
   * so clear the reference and leave the multiple references alone.
   */
  if (obj_is_static (game, object))
    {
      lib_new_clause (game, FALSE);
      lib_print_response_object (game,
                                 "You can't take ",
                                 "I can't take ",
                                 "%player% can't take ", object, "!");
      game->object_references[object] = FALSE;
      *printed = TRUE;
      return FALSE;
    }

  if (lib_object_too_heavy (game, object))
    {
      lib_new_clause (game, FALSE);
      lib_print_too_heavy (game, object);
    }
  else if (lib_object_too_large (game, object))
    {
      lib_new_clause (game, FALSE);
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "Your hands are full.",
                                             "My hands are full.",
                                             "%player%'s hands are full."));
    }
  else
    {
      /*
       * The announcement ends its own line (run400 appends vbCrLf), so the
       * put report that follows starts fresh rather than being joined on.
       */
      gs_object_player_get (game, object);
      gs_set_object_unmoved (game, object, FALSE);
      return TRUE;
    }

  *printed = TRUE;
  return lib_put_reject_reference (game, object);
}


/*
 * lib_put_outcome_t
 * lib_output_length()
 * lib_put_drop_statics_400()
 *
 * What a put backend has to tell its caller beyond the object moves.
 * is_refusal_only is the 4.0 size/capacity refusal that leaves the line
 * unclaimed (lib_put_in_refused()); is_silent says the backend printed
 * nothing at all, which at 4.0 also leaves the line unclaimed, with no line
 * ending of its own; is_tasks_only says everything it printed came from
 * the task look-ups, whose output has already ended its line, so the
 * caller's own newline would only add a blank one (shadowpeak `put bottle
 * on altar`, pestilence `put brick on desk`, hub `put saucepan on hob`).
 * Output is measured on the filter buffer, so a task's text counts however
 * it was produced.
 */
typedef struct
{
  scr_bool is_refusal_only;
  scr_bool is_silent;
  scr_bool is_tasks_only;
  scr_bool is_announce_only;
} lib_put_outcome_t;

static scr_int
lib_output_length (scr_gameref_t game)
{
  const scr_char *buffer = pf_get_buffer (gs_get_filter (game));

  return buffer ? (scr_int) strlen (buffer) : 0;
}

/*
 * lib_put_nothing_carried_400()
 *
 * The report name_object makes when its own loop has left it with nothing to
 * put down: run400 46E5A0, the literal an empty-handed `drop all` prints too
 * (see lib_cmd_drop_all).  It closes the take phase, ahead of the task
 * look-ups `insides` makes, and it is printed only when all three of these
 * hold -- measured on probe PSTAT in run400, Adrift_941/942_pstat.txt:
 *
 *   nothing left to act on    `put coin in box` with the coin in hand moves
 *                             it and says nothing (cmd 5), and `put box in
 *                             box` takes the box first, so its action list
 *                             is not empty either, and again nothing at all
 *                             follows the announcement (cmd 13/18)
 *   nothing left to name      `put coin in box` with the coin already inside
 *                             the box is "You are not holding the coin." and
 *                             no more, empty-handed (cmd 6) or not (cmd 8);
 *                             a refused static is never named at all
 *   the player holds nothing  `put anvil in box` is "You can't take the
 *                             anvil!  You are carrying nothing!" empty-handed
 *                             (cmd 2/11/17) and just "You can't take the
 *                             anvil!" with the coin in hand (cmd 4)
 *
 * Worn does not count as carried: cmd 15 wears the cap, `i` answers "You are
 * wearing a cap, and you are carrying nothing." and the put that follows
 * still reports "You are carrying nothing!".  That is name_object's own
 * universe for "all", field26 = 0 And field24 = 0 -- held, never worn and
 * never inside anything (see lib_put_all_filter).
 *
 * Returns TRUE if it printed.
 */
static scr_bool
lib_put_nothing_carried_400 (scr_gameref_t game, scr_bool has_printed)
{
  scr_int object;

  if (!lib_is_version_400 (game))
    return FALSE;

  for (object = 0; object < gs_object_count (game); object++)
    {
      if (game->object_references[object] || game->multiple_references[object])
        return FALSE;

      if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
        return FALSE;
    }

  lib_print_clause (game, has_printed,
                    "You are carrying nothing!",
                    "I am carrying nothing!",
                    "%player% is carrying nothing!");
  return TRUE;
}


/*
 * lib_put_in_closed_400()
 *
 * 4.0's closed-container refusal, printed from insides' own place in the
 * put: after name_object's take piece, the tasks() call, the possession and
 * itself tests, and before the size and capacity tests (run400 46639C, the
 * state test at 4661BE = 7 " is locked!" / 4661C9 = 6 " is closed!", ahead
 * of the size test at 466219).  Because the take piece has already run, a
 * put into a shut container still ACQUIRES the object: probe PCLOSED
 * (Adrift_1194.txt) `put ring in box` with the box held and shut prints
 * "(Taking the ring first)" / "The box is closed!" and `i` then lists the
 * ring; `put coin in chest` with the chest shut on the floor is the same
 * shape, "(Taking the coin first)" / "The chest is closed!", and the coin
 * is held afterwards.  An object already in hand draws the refusal alone
 * (`put stone in box`, "The box is closed!").  Returns TRUE and drops every
 * remaining reference when it refused, so the size and capacity tests see
 * nothing; like those, the refusal leaves the line for the task pass, since
 * insides exits without setting its result byte.
 */
static scr_bool
lib_put_in_closed_400 (scr_gameref_t game, scr_int container,
                       scr_bool has_printed)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, object_count;
  scr_bool any_referenced;

  if (!lib_is_version_400 (game)
      || gs_object_openness (game, container) <= OBJ_OPEN)
    return FALSE;

  any_referenced = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      if (game->object_references[object])
        {
          game->object_references[object] = FALSE;
          any_referenced = TRUE;
        }
    }
  if (!any_referenced)
    return FALSE;

  lib_new_clause (game, has_printed);
  lib_print_object_np (game, container);
  pf_buffer_string (filter,
                    lib_select_plurality (game, container, " is", " are"));
  if (gs_object_openness (game, container) == OBJ_LOCKED)
    pf_buffer_string (filter, " locked!");
  else
    pf_buffer_string (filter, " closed!");
  return TRUE;
}


/*
 * Set while lib_put_that_390() hands a mid-word target to the named-put
 * pipeline: the objects come from co() alone, and the put is 3.9's and-arm
 * however many there are.
 */
static scr_bool lib_put_mid_word_390 = FALSE;

/*
 * lib_put_in_backend()
 *
 * Common backend handler for placing objects in containers.  Places all
 * objects currently referenced in the game into a container, trying game
 * commands first, and then moving other unhandled objects into the container.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */
static lib_put_outcome_t
lib_put_in_backend (scr_gameref_t game, scr_int container,
                    scr_bool is_all_form)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object_count, object, count, capacity, free_space;
  scr_int length_before, length_after_tasks;
  scr_bool closed_refusal_only;
  scr_bool has_printed, is_refusal_only, task_claimed;
  scr_bool static_refused, recursion_rejected, has_moved, nothing_fits_390;
  scr_int entry_count;
  lib_put_outcome_t outcome;
  lib_list_t list, pending;

  /* How many objects the line named, for 3.9's all/and arm below. */
  entry_count = 0;
  for (object = 0; object < gs_object_count (game); object++)
    entry_count += game->object_references[object] ? 1 : 0;

  /*
   * Try game commands for all referenced objects first.  If any succeed,
   * remove that reference from the list.  At the same time, check for and
   * weed out any moves that result in infinite recursion.
   */
  length_before = lib_output_length (game);
  lib_put_announce_bytes = 0;
  has_printed = FALSE;
  task_claimed = FALSE;
  static_refused = recursion_rejected = has_moved = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      /*
       * Version 4.0 picks up an object it has been asked to put down, and it
       * does so BEFORE the handler's task look-up.  run400's name_object
       * loop runs the take piece at loc_46E2B5 and only then hands the pair
       * to insides (Proc_19_43_46639C @loc_46E34F), which is where the
       * canonical line reaches the tasks -- so a task that goes on to claim
       * the put still carries the announcement ahead of its own text.
       * Measured on frustrated turns 53-55 (Adrift_274_frustrated.txt):
       * `put small rock on left pan` matches task 511 `put*small*left*` and
       * still opens "(Taking the small rock first)".
       *
       * It runs ahead of the self-container test as well: insides tests
       * possession (Proc_21_46_44615C @465EED) before it tests arg_10 =
       * arg_C (@465FA0), and name_object's take piece has already run by
       * then.  Probe PBOXBOX (Adrift_1192.txt): `put box in box` with the
       * box on the floor prints "(Taking the box first)" / "You can't put
       * an object inside itself!" and the box IS taken -- `i` answers "You
       * are carrying a box." -- where the same line with the box held
       * prints only the itself line, and a ring inside the box changes
       * nothing.  Probe PSTAT's silent commands 13 and 18 are not a
       * counter-measurement: there the coin, object #1, sits inside the
       * box, and run400's carried-weight cycle (447680) eats the report;
       * see the deliberate deviation in notes/WINE-TRANSCRIPTS-TODO.md.
       */
      {
        scr_bool take_printed = FALSE;
        const scr_bool is_static = obj_is_static (game, object);

        lib_put_implicit_take (game, object, container, &take_printed);
        has_printed |= take_printed;
        static_refused |= is_static;
      }

      /*
       * The tasks' turn.  At 4.0 this is the handler's own look-up, on the
       * definite canonical line, ahead of the size and capacity tests; see
       * lib_try_game_command_with_object_400().  A matching task it does
       * not reach -- the typed spelling with no canonical twin -- gets the
       * line from run_all_commands() only if the library then refuses the
       * put, joined after the refusal (PUT4, Adrift_81.txt).  It runs even
       * for an object the take above could not acquire: run400 reaches
       * insides' tasks() call at loc_465EB5 before the possession test at
       * loc_465EED, so a claim takes the object back out of the "You are
       * not holding ..." report.  The look-up itself is deferred to the
       * second pass below, so that the whole take phase precedes it.
       */
      if (lib_is_version_400 (game))
        {
          pending.push_back (object);
          continue;
        }

      /*
       * Reject and remove attempts to place objects in themselves.  Pre-4.0
       * never gets this far with a direct self-put -- the callers answer
       * "Put the box inside what?" / "You can't do that!" first (see
       * lib_put_what_pre400) -- so this is the subtle case, the container
       * already inside the object.  The 4.0 test sits after the deferred
       * task pass below.
       */
      if (!lib_check_put_in_recursion (game, object, container, !has_printed))
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
          recursion_rejected = TRUE;
          continue;
        }

      if (lib_try_game_command_with_object (game,
                                            "put", object, "in", container))
        {
          game->object_references[object] = FALSE;
          game->multiple_references[object] = FALSE;
          has_printed = TRUE;
          task_claimed = TRUE;
          continue;
        }
    }

  /*
   * name_object's own close to the take phase, and then the look-ups it
   * deferred.  run400 takes every named piece first and hands the pair to
   * `insides` only afterwards (@46E34F), so the report above comes out
   * ahead of the first task: probe PSTAT command 12, `put slab in box`
   * with the slab a static and the inventory empty, reads "(Taking the
   * slab first)" / "You can't take the slab!  You are carrying nothing!
   * SLABTASK."
   */
  if (!recursion_rejected && lib_put_nothing_carried_400 (game, has_printed))
    has_printed = TRUE;

  for (const scr_int pending_object : pending)
    {
      if (lib_try_game_command_with_object_400 (game, "put", pending_object,
                                                "in", container))
        {
          game->object_references[pending_object] = FALSE;
          game->multiple_references[pending_object] = FALSE;
          has_printed = TRUE;
          task_claimed = TRUE;
        }
    }

  /* The typed line, when the rebuild missed; see
     lib_try_typed_put_line_400(). */
  if (!task_claimed && pending.size () == 1
      && lib_try_typed_put_line_400 (game))
    {
      game->object_references[pending.front ()] = FALSE;
      game->multiple_references[pending.front ()] = FALSE;
      has_printed = TRUE;
      task_claimed = TRUE;
    }

  /*
   * 4.0's self-container test, in insides' own place: after the take phase
   * and the tasks() call at 465EB5, after the possession test at 465EED --
   * so only an object the take left in hand reaches it, a failed take
   * having already moved it to the "You are not holding ..." report -- and
   * before the "already inside", size and capacity tests (arg_10 = arg_C
   * @465FA0, the wording chosen by the target's flags at 465FDA/46600C/
   * 46602A).  Probe PBOXBOX (Adrift_1192.txt): `put box in box` with the
   * box on the floor prints "(Taking the box first)" / "You can't put an
   * object inside itself!" and the box IS taken -- `i` answers "You are
   * carrying a box." -- where the same line with the box held prints only
   * the itself line, and a ring inside the box changes nothing.  Probe
   * PSTAT's silent commands 13 and 18 are not a counter-measurement: there
   * the coin, object #1, sits inside the box, and run400's carried-weight
   * cycle (447680) eats the report; see the deliberate deviation in
   * notes/WINE-TRANSCRIPTS-TODO.md.
   */
  for (const scr_int pending_object : pending)
    {
      if (!game->object_references[pending_object])
        continue;

      if (!lib_check_put_in_recursion (game, pending_object, container,
                                       !has_printed))
        {
          game->object_references[pending_object] = FALSE;
          has_printed = TRUE;
          recursion_rejected = TRUE;
        }
    }
  length_after_tasks = lib_output_length (game);

  /*
   * 4.0's closed-container test, in insides' own place: after the take
   * phase, the tasks and the itself test, before size and capacity.  It is
   * a refusal in the size refusal's sense -- printed, and the line left
   * for the task pass -- unless something already spoke ahead of it.
   */
  closed_refusal_only = FALSE;
  if (lib_put_in_closed_400 (game, container, has_printed))
    {
      closed_refusal_only = !has_printed;
      has_printed = TRUE;
    }

  /*
   * Retrieve the container's total volume, and the volume it has left.  The
   * free space is tracked across the loop below rather than recomputed, since
   * each object put in spends some of it.
   */
  capacity = obj_get_container_capacity (game, container);
  free_space = obj_get_container_free_space (game, container);

  /*
   * run390's " is full." arm (461E59): after the count and its refusals,
   * before the move, an `inside` put whose container is already holding
   * exactly its capacity answers "<The X> is full." and moves nothing --
   * but only while something on the line still fits, which for a full
   * container means a size of nil.  Size is SizeMultiple raised to the
   * SizeWeight's tens digit, so a size of nil needs a SizeMultiple of nil,
   * which the ADRIFT editor never writes; pPUTFULL39 could not raise the
   * arm and it was read as dead.  It is not: pPUTZERO39 (SizeMultiple 0,
   * a bag of capacity 2 holding a coin and a stone, `put feather in bag`
   * with the feather at SizeWeight 10) is "The bag is full." in run390 and
   * the feather stays in hand (Adrift_pputzero39.txt).  The same line with
   * a size-1 pebble is the ordinary "The pebble can't fit inside the bag
   * at the moment.", so the per-object refusal above still comes first,
   * and `put all in bag` with two size-0 objects held is "The bag is
   * full." rather than "Nothing will fit inside the bag." -- the count
   * sees them fit, so the nil-count refusal never speaks.  A surface is
   * barred by the arm's third test (var_E0 = "inside"): the same probe
   * puts three objects onto a plate of capacity 2 without a word.
   *
   * That measurement also settles obj_scale()'s long-standing claim that a
   * multiple of nil gives a size of nil, which nothing had tested.
   */
  if (lib_is_version_390 (game) && free_space == 0
      && obj_is_container (game, container))
    {
      scr_bool any_fits = FALSE;

      for (object = 0; object < object_count; object++)
        {
          if (game->object_references[object]
              && obj_get_size (game, object) <= free_space)
            any_fits = TRUE;
        }

      if (any_fits)
        {
          for (object = 0; object < object_count; object++)
            game->object_references[object] = FALSE;

          lib_new_clause (game, has_printed);
          lib_print_object_np (game, container);
          pf_buffer_string (filter, " is full.");
          has_printed = TRUE;
        }
    }

  /*
   * Version 3.9's all/and arm counts before it moves anything: insides()
   * (461AF8-461BDB) walks the objects in index order, counting each whose
   * Size fits in what is left once the ones counted before it are charged,
   * and then (461EE8-46239B) moves the FIRST that many of them, whatever
   * their sizes, capping the list with "  You can't put any more inside
   * the bag as it is full." at the first one left over and saying nothing
   * of the rest.  A count of nil is "Nothing will fit inside the bag."
   * (461E0B), with no object named.  pPUTREF39.taf / pPUTREF39E.taf
   * (--emptybag), run390x, 2026-09-19: `put stone and pebble in bag` with
   * the bag full is "Nothing will fit inside the bag.", as is `put all in
   * bag` (Adrift_pputref394.txt:5/11); with room for two, `put stone and
   * pebble and lamp in bag` is "You put the stone and the lamp inside the
   * bag.  You can't put any more inside the bag as it is full." (Adrift_
   * pputref396.txt:5).  A single object keeps the size/capacity pair
   * below.  The " is full." arm at 461E9D never spoke on these probes,
   * the bag exactly full included, because it wants an object that fits
   * into a container with nothing left; it is modelled just above.
   */
  nothing_fits_390 = FALSE;
  if (lib_is_version_390 (game)
      && (is_all_form || entry_count > 1 || lib_put_mid_word_390))
    {
      scr_int fits, spent, left_over, remaining;

      fits = spent = remaining = 0;
      free_space = obj_get_container_free_space (game, container);
      for (object = 0; object < object_count; object++)
        {
          remaining += game->object_references[object] ? 1 : 0;
          if (game->object_references[object]
              && obj_get_size (game, object) <= free_space - spent)
            {
              spent += obj_get_size (game, object);
              fits++;
            }
        }

      /* Nothing fits: leave the references for the refusal below. */
      nothing_fits_390 = fits == 0 && remaining > 0;

      list.clear ();
      left_over = 0;
      for (object = 0; fits > 0 && object < object_count; object++)
        {
          if (!game->object_references[object])
            continue;

          game->object_references[object] = FALSE;
          if (static_cast<scr_int> (list.size ()) < fits)
            {
              list.push_back (object);
              gs_object_move_into (game, object, container);
            }
          else
            left_over++;
        }

      if (!list.empty ())
        {
          lib_print_clause (game, has_printed,
                            "You put ", "I put ", "%player% put ");
          lib_print_list (game, list, lib_put_print_object_or_that, " and ");
          lib_print_wrapped_object (game, " inside ", container, ".");
          if (left_over > 0)
            {
              pf_buffer_string (filter, "  ");
              lib_print_response_object (game,
                                         "You can't put any more inside ",
                                         "I can't put any more inside ",
                                         "%player% can't put any more inside ",
                                         container, " as it is full.");
            }
          has_printed = TRUE;
          has_moved = TRUE;
        }
    }

  /* Put in every object that remains referenced. */
  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      scr_int size;

      if (!game->object_references[object])
        continue;

      /* If too big, or no longer room for it, ignore for now. */
      size = obj_get_size (game, object);
      if (size > capacity || size > free_space)
        continue;
      free_space -= size;

      list.push_back (object);
      gs_object_move_into (game, object, container);
      game->object_references[object] = FALSE;
    }

  if (!list.empty ())
    {
      lib_print_clause (game, has_printed,
                        "You put ",
                        "I put ",
                        "%player% put ");
      /*
       * Pre-3.9's "put all in" names what it moved with the raw Prefix &
       * " " & Short, where its named twin one row up runs the same objects
       * through tense() and gets the definite form.  p38DARK/p37DARK, box
       * held, torch (Prefix "an") and lamp (Prefix "a") in hand: `put all
       * in box` -> "You put an torch and a lamp inside the box."
       * (Adrift_985:22, Adrift_989:22, 2026-09-12), against `put torch in
       * box` -> "You put an torch inside the box." and `put pebble in box`
       * -> "You put the pebble inside the box." on the same probes
       * (Adrift_983:60/57).  Only the list is raw -- the container that
       * follows it is definite in both.  3.9 and 4.0 normalise throughout
       * ("the torch, the lamp, ..." Adrift_980:50, "the torch and the lamp"
       * Adrift_981:11), so this is the pre-3.9 all-form alone.  Scarier
       * deliberately names them definitely there too (deviation policy):
       * "You put an torch and a lamp inside the box." is the raw prefix
       * leaking into a success report.
       */
      lib_print_list (game, list, lib_put_print_object_or_that, " and ");
      pf_buffer_string (filter, " inside ");
      lib_print_object_np (game, container);
      pf_buffer_character (filter, '.');
    }
  has_printed |= !list.empty ();
  has_moved = !list.empty ();

  /*
   * Version 3.8 has one container refusal and one only.  run380 answers "The
   * box is full." to everything it will not take in -- whether the container
   * has no room left, or (with a Capacity of 0) never had any, and whatever
   * the object's Size/weight class, which it does not charge against the
   * container at all (measured 2026-08-03; see obj_get_container_capacity).
   * Consume the leftovers here, so that neither 4.0 report below finds
   * anything to say about them.
   */
  if (obj_uses_burden_model (game))
    {
      count = 0;
      for (object = 0; object < object_count; object++)
        {
          if (!game->object_references[object])
            continue;

          game->object_references[object] = FALSE;
          count++;
        }

      if (count > 0)
        {
          lib_new_clause (game, has_printed);
          lib_print_object_np (game, container);
          pf_buffer_string (filter,
                            lib_select_plurality (game, container,
                                                  " is", " are"));
          pf_buffer_string (filter, " full.");
          has_printed = TRUE;
        }
    }

  /*
   * A put that moved nothing and said nothing so far is about to say only
   * the size and capacity refusals below; see the return value.  A closed
   * refusal that spoke first counts the same way.
   */
  is_refusal_only = !has_printed || closed_refusal_only;

  /* 3.9's all/and arm with nothing fitting names no object; see above. */
  if (nothing_fits_390)
    {
      for (object = 0; object < object_count; object++)
        game->object_references[object] = FALSE;
      lib_new_clause (game, has_printed);
      lib_print_wrapped_object (game, "Nothing will fit inside ",
                                container, ".");
      has_printed = TRUE;
    }

  /*
   * Report objects not put in because of their size.  These objects remain in
   * standard references, as do objects rejected because of capacity limits.
   * By removing too large objects in this loop, we're left later on with just
   * the objects rejected by capacity limits.
   */
  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      if (!(obj_get_size (game, object) > capacity))
        continue;

      list.push_back (object);
      game->object_references[object] = FALSE;
    }

  if (!list.empty ())
    {
      lib_new_clause (game, has_printed);
      lib_print_list (game, list, lib_print_object_np, " and ");
      pf_buffer_string (filter,
                        list.size () == 1
                        ? lib_select_plurality (game, list[0],
                                                " is too big", " are too big")
                        : " are too big");
      pf_buffer_string (filter, " to fit inside ");
      lib_print_object_np (game, container);
      pf_buffer_character (filter, '.');
    }
  has_printed |= !list.empty ();

  /*
   * Report objects not put in because the container is too full.  This should
   * be all remaining objects in standard references.
   */
  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      list.push_back (object);
      game->object_references[object] = FALSE;
    }

  if (!list.empty ())
    {
      lib_new_clause (game, has_printed);
      lib_print_list (game, list, lib_print_object_np, " and ");
      lib_print_wrapped_object (game, " can't fit inside ",
                                container, " at the moment.");
    }
  has_printed |= !list.empty ();

  /* Refusals only if they were printed, and nothing before them. */
  is_refusal_only = is_refusal_only && has_printed;

  /* Note any remaining multiple references left out of the operation. */
  lib_drain_multiple_references (game, object_count, list);

  lib_print_object_list (game, has_printed, list, " or ", '.',
                         "You are not holding ",
                         "I am not holding ",
                         "%player% is not holding ");

  /*
   * Refusal-only when the whole put came to nothing but size or capacity
   * refusals: nothing moved, no task or implicit take spoke, nobody was
   * "not holding" anything.  4.0 does not count that as handling the
   * command; see lib_put_in_refused().
   */
  /*
   * A static named in a 4.0 put leaves the line for the task pass as surely
   * as a size refusal does: run400's `insides` exits at loc_465ED7 on
   * obj.global_24 = 1, ahead of the result byte var_86 that a completed move
   * sets, so nothing claims.  thelasthour turn 26 is the live case --
   * `put hands into hole`, the hand a static, answers "I can't take the
   * hand!  I am carrying nothing!" and then task 15's own "Can't take the
   * mouse. Too far." -- and probe PSTAT command 12 is the same shape built
   * from nothing (`put slab in box` -> "... You are carrying nothing!
   * SLABTASK.").
   */
  outcome.is_refusal_only = (is_refusal_only && list.empty ())
                            || (static_refused && !has_moved);
  outcome.is_silent = lib_output_length (game) == length_before;
  outcome.is_tasks_only = task_claimed
                          && lib_output_length (game) == length_after_tasks;
  outcome.is_announce_only = task_claimed && lib_put_announce_bytes > 0
                             && (size_t) (lib_output_length (game)
                                          - length_before)
                                == lib_put_announce_bytes;
  return outcome;
}


/*
 * lib_put_in_refused()
 *
 * The put's line has been printed and terminated.  At 4.0, in the priority
 * pass, a refusal-only put leaves the command unclaimed: run400's insides
 * handler (Proc_19_43_46639C) exits every message path without setting its
 * return byte, so "The rock is too big to fit inside the slot." is printed
 * and a matching task then answers the same line, joined on with two spaces
 * (arena probe PUT7, Adrift_87, 2026-09-05; Zack Smackfoot `put knife in
 * slot`, Adrift_57).  Signal that to run_priority_commands() and report the
 * handler's return.  Pre-4.0 the refusal claims the line, as it always has.
 */
static scr_bool
lib_put_in_refused (scr_gameref_t game, scr_bool is_refusal_only)
{
  if (is_refusal_only && lib_is_version_400 (game) && run_in_priority_pass ())
    {
      run_priority_refuse ();
      return FALSE;
    }

  return TRUE;
}


/*
 * lib_put_in_finish()
 * lib_put_on_finish()
 *
 * End the put's line and settle its claim from the backend's outcome; see
 * lib_put_outcome_t.  A 4.0 put that printed nothing (statics only, see
 * lib_put_drop_statics_400()) is left unclaimed with no line ending, for
 * the task pass to answer.
 *
 * A 4.0 put whose only words are the implicit take's announcement, with a
 * task claiming it silently, ends on the game's DontUnderstand text.  run400
 * writes "(Taking X first)" straight to the textbox (@46E2EA-46E30C) while
 * the turn's answer collects in MemVar_4941B0, and a claim that printed
 * nothing leaves that empty for generaltasks' tail (48B573) to fill.  hcw
 * turn 162 (Adrift_1055_hcw.txt:1087): `put susan in trunk` pre-matches the
 * lower-cased rebuilds, dispatches the raw "get sleeping Susan" / "put
 * sleeping Susan in the car trunk", runs nothing (see
 * lib_run_rebuilt_line_400()) and reads "(Taking sleeping Susan first)" /
 * "I don't understand what you mean.".
 */
static scr_bool
lib_put_in_finish (scr_gameref_t game, const lib_put_outcome_t &outcome)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (outcome.is_silent && lib_is_version_400 (game))
    return FALSE;
  if (outcome.is_announce_only && lib_is_version_400 (game))
    {
      pf_buffer_string (filter, prop_get_global_string (gs_get_bundle (game),
                                                        "DontUnderstand"));
      pf_buffer_answer_break (filter);
      return TRUE;
    }
  if (!outcome.is_tasks_only)
    pf_buffer_answer_break (filter);
  return lib_put_in_refused (game, outcome.is_refusal_only);
}

static scr_bool
lib_put_on_finish (scr_gameref_t game, const lib_put_outcome_t &outcome)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (outcome.is_silent && lib_is_version_400 (game))
    return FALSE;
  if (outcome.is_announce_only && lib_is_version_400 (game))
    {
      pf_buffer_string (filter, prop_get_global_string (gs_get_bundle (game),
                                                        "DontUnderstand"));
      pf_buffer_answer_break (filter);
      return TRUE;
    }
  if (!outcome.is_tasks_only)
    pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_put_in_filter()
 * lib_put_in_not_container_filter()
 *
 * Helper functions for deciding if an object may be put in another this
 * context.  Returns TRUE if an object may be manipulated, FALSE otherwise.
 * The named form is much the wider of the two; see lib_put_named_filter().
 */
static scr_bool
lib_put_in_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return lib_put_named_filter (game, object);
}

/* The resolver twin of the above; see lib_put_resolve_filter(). */
static scr_bool
lib_put_in_resolve_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return lib_put_resolve_filter (game, object);
}

static scr_bool
lib_put_in_not_container_filter (scr_gameref_t game,
                                 scr_int object, scr_int container)
{
  return lib_put_all_filter (game, object, container);
}


/*
 * lib_put_target_pre390()
 *
 * Name the container of a "put X in Y", or the supporter of a "put X on Y",
 * the way run370 and run380 do, which is not the way anything else in the
 * library names an object: the room is no part of the test.  On p38DARK/p37DARK with the player standing in the
 * lit room and the box left behind on the cave floor, `put lamp in box` and
 * `put stone in box` both answer "You are not holding a box."
 * (Adrift_984:25/31, Adrift_988:25/31, 2026-09-12), and so does `put all in
 * box` (Adrift_984:50/988:50) -- the box is found, and then refused for not
 * being in the player's hands.  run390 never finds it and asks "Put the
 * lamp inside what?" instead (Adrift_980:24).
 *
 * A noun that names nothing anywhere is still nothing: `put coin in zzzz`
 * and `put coin in me` fall through to lib_cmd_put_in_nowhere()
 * (Adrift_983:72, Adrift_982:66).
 *
 * The room-filtered resolver runs first and keeps the ambiguity prompt for
 * the cases it already handled; only when it comes back empty-handed, and
 * unambiguously so, is the whole game searched, and then only a single
 * candidate is accepted.  Two namesakes in two rooms are left to the caller
 * as no container at all, which is where they were before this.
 *
 * The surface half of the row is the same code: with the table left in the
 * cave and the player standing in the lit room, `put lamp on table` and `put
 * stone on table` are both "You are not holding a table." on run370 and
 * run380 (p37SURF/p38SURF, Adrift_1005:5/7 and Adrift_1001:5/7, 2026-09-12),
 * where run390 never finds the table and asks "Put the lamp onto what?"
 * instead.  That is only to be expected once the two handlers turn out to be
 * one; see lib_put_target_takes_on().
 */
static scr_int
lib_put_target_pre390 (scr_gameref_t game, const scr_char *prompt,
                       scr_bool *is_ambiguous)
{
  const scr_bool requires_seen = lib_matcher_requires_seen (game);
  std::vector<scr_bool> named;
  scr_int index_, object, count, container;

  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return lib_disambiguate_object (game, prompt, is_ambiguous);

  /* lib_disambiguate_object_common() clears what it rejects; keep a copy. */
  named.assign (gs_object_count (game), FALSE);
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    named[index_] = game->object_references[index_];

  container = lib_disambiguate_object (game, prompt, is_ambiguous);
  if (container != -1 || *is_ambiguous)
    return container;

  count = 0;
  object = -1;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (!named[index_] || (requires_seen && !gs_object_seen (game, index_)))
        continue;
      count++;
      object = index_;
    }

  if (count != 1)
    return -1;

  for (index_ = 0; index_ < gs_object_count (game); index_++)
    game->object_references[index_] = (index_ == object);
  return object;
}


/*
 * lib_put_target_takes_on()
 *
 * Which of the two put handlers a line actually runs.  From 3.9 on the two
 * are separate and the answer is simply the preposition the player typed.
 *
 * Below 3.9 they are ONE handler, and the preposition in the answer comes
 * from the TARGET's kind rather than from anything the player wrote.
 * Measured on p37SURF under run370 and p38SURF under run380, 2026-09-12,
 * with the box (a container) and the table (a surface) both in hand:
 *
 *   put pebble on box   You put the pebble inside the box.
 *                                     (Adrift_1012:4, Adrift_1011:4)
 *   put stone in table  You put the stone on the table.
 *                                     (Adrift_1012:7, Adrift_1011:7)
 *   put lamp on box     with the box shut
 *                       You can't put anything inside the box as it is
 *                       closed!       (Adrift_1012:10, Adrift_1011:10)
 *   put all on box      You put an torch, a lamp and a table inside the box.
 *                                     (Adrift_1012:12, Adrift_1011:12)
 *
 * -- where run390 and run400 keep the two apart and refuse every one of
 * them, "You can't put anything onto the box." / "...inside the table."
 * (Adrift_1007:4/7, Adrift_1008:4/7).  It also explains the third of the
 * pre-3.9 answers that used to look like a surface test and is not: `put
 * coin on box` with the box standing on the cave floor is "You are not
 * holding a box." (Adrift_1003:16), the container handler's own held
 * refusal, reached because the line was routed by the box's kind.
 *
 * A target that is both, or neither, has no kind to be routed by, so it
 * keeps the preposition the player typed -- which is what the neither case
 * is measured to do: `put coin on stone` is "You can't put anything on the
 * stone." (Adrift_1003:14) against `put coin in stone`'s "...inside the
 * stone." on the same Runner.
 */
static scr_bool
lib_put_target_takes_on (scr_gameref_t game, scr_int target, scr_bool typed_on)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return typed_on;

  if (obj_is_container (game, target) && !obj_is_surface (game, target))
    return FALSE;
  if (obj_is_surface (game, target) && !obj_is_container (game, target))
    return TRUE;
  return typed_on;
}


/* The put-on half of the pipeline, defined with the rest of its family. */
static scr_bool lib_put_on_is_valid (scr_gameref_t game, scr_int supporter);
static lib_put_outcome_t lib_put_on_backend (scr_gameref_t game,
                                             scr_int supporter,
                                             scr_bool is_all_form);
static scr_bool lib_put_on_filter (scr_gameref_t game,
                                   scr_int object, scr_int unused);
static scr_bool lib_put_on_not_supporter_filter (scr_gameref_t game,
                                                 scr_int object,
                                                 scr_int supporter);


/*
 * lib_put_refusal_swept
 * lib_put_invalid_result()
 *
 * run390 insides() settles the target before its task look-up at 461A6C,
 * exactly as it does a line naming fewer than two objects (see
 * lib_put_refusal_first_390()): the wrong-kind refusals "You can't put
 * anything inside/onto <the X>." (4623F8) and the shut container's "... as it
 * is closed!" are written with no checktask of their own, and the post-put
 * sweep at 462550 then decides.  A claimant named in the line runs the tasks
 * QUIET, so a failing task leaves the refusal standing; with none, the LOUD
 * pass lets its FailMessage replace it; a passing task replaces it either
 * way.  Measured on pPUTREF39.taf (make_39_putrefprobe.py), run390x,
 * 2026-09-19 (Adrift_pputref39.txt / Adrift_pputref392.txt), each line with
 * a matching task whose restriction fails:
 *
 *   put coin in lamp    You can't put anything inside the lamp.
 *   put coin in box     You can't put anything inside the box as it is closed!
 *   put coin on box     You can't put anything onto the box.
 *   put gem in ring     T7 FAIL.   (both in the bag: no claimant, LOUD)
 *   put stone in box    T10 PASS.  (a passing task)
 *
 * where the deferred refusals had let every one of them print its task's
 * FailMessage.  The validators run the sweep themselves and raise this flag,
 * so that their callers claim the line rather than hand it on.
 */
static scr_bool lib_put_refusal_swept = FALSE;

/*
 * lib_put_reset()
 *
 * Forget every per-command put admission and flag, for a game that is
 * about to (re)start; run_player_input() calls this beside the other
 * library resets.  A sweep whose put never reached its verdict, or a
 * pre-4.0 noun still mid-word when the game ended, would otherwise leak
 * into the next game's first put.
 */
void
lib_put_reset (void)
{
  lib_put_seen_absent = -1;
  lib_put_present_unseen = -1;
  lib_put_announce_bytes = 0;
  lib_put_mid_word_390 = FALSE;
  lib_put_refusal_swept = FALSE;
}

static scr_bool
lib_put_invalid_result (void)
{
  if (lib_put_refusal_swept)
    {
      lib_put_refusal_swept = FALSE;
      return TRUE;
    }
  return !run_in_priority_pass ();
}

/* The refusal-first sweep itself, on text printed since from. */
static void lib_put_sweep_390 (scr_gameref_t game,
                               const scr_char *input, size_t from);

static void
lib_put_refusal_sweep_390 (scr_gameref_t game, size_t from)
{
  lib_put_sweep_390 (game, run_get_dispatch_input (), from);
  lib_put_refusal_swept = TRUE;
}

static scr_bool lib_put_refusal_first_390 (scr_gameref_t game);


/*
 * lib_put_static_absent_pre390()
 *
 * lib_put_target_pre390() finds a target anywhere in the game, and 3.7/3.8
 * insides() then refuses a static one that is not in the player's room with
 * "You can't see " & Prefix & " " & Short & "." -- the arm
 * lib_put_co_refusal_pre390() already gives for a line it could not parse.
 * p37PUT/p38PUT, standing in the cave with the open static cupboard in the
 * lit room: `put coin in cupboard` is "You can't see a cupboard." (run370x
 * Adrift_160_p37drop.rtf, run380x Adrift_161_p38drop.rtf, 2026-09-19);
 * Scarier had put the coin into it.  The not-a-container refusal comes
 * first: `put coin in statue`, the plain static statue also left behind, is
 * "You can't put anything inside the statue." on the same turns.  The
 * static surface is the same handler, and unmeasured.
 */
static scr_bool
lib_put_static_absent_pre390 (scr_gameref_t game, scr_int target)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390
      || !obj_is_static (game, target)
      || obj_directly_in_room (game, target, gs_playerroom (game)))
    return FALSE;
  if (run_priority_defer_if_active ())
    return TRUE;
  lib_cant_see_named_pre_390 (game, target, FALSE, ".\n");
  return TRUE;
}


/*
 * lib_put_in_is_valid()
 *
 * Validate the container requested in "put in" commands.
 */
static scr_bool
lib_put_in_is_valid (scr_gameref_t game, scr_int container)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /* Verify that the container object is a container. */
  if (!obj_is_container (game, container))
    {
      /*
       * In the tentative priority pass the refusal is deferred, so a matched
       * task's fail message can claim the input first; the STANDARD_COMMANDS
       * duplicate prints it when no task does (run400-verified, 2026-08-02).
       */
      const scr_bool is_first = lib_put_refusal_first_390 (game);
      const size_t from = pf_buffer_length (filter);

      if (!is_first && run_priority_defer_if_active ())
        return FALSE;
      /*
       * Only 4.0 shouts.  The pre-4.0 Runners all carry the literal
       * " can't put anything inside " -- run400 carries none of it, it
       * composes its own from "You can't put anything " & preposition --
       * and they end the line with a full stop: p39DARK under run390,
       * `put coin in stone` -> "You can't put anything inside the stone."
       * (Adrift_976:43), against run400's "You can't put anything inside
       * the stone!" on the same turn of p4TFROM (Adrift_977:24).  Measured
       * 2026-09-12.
       */
      lib_print_response_object (game,
                                 "You can't put anything inside ",
                                 "I can't put anything inside ",
                                 "%player% can't put anything inside ",
                                 container,
                                 lib_is_version_400 (game) ? "!\n" : ".\n");
      if (is_first)
        lib_put_refusal_sweep_390 (game, from);
      return FALSE;
    }

  /*
   * Version 3.8 will only fill a dynamic container the player is holding: an
   * open box sitting on the floor answers "You are not holding a box." (note
   * the object's own prefix, not the "the" of most refusals).  Static
   * containers are exempt -- run380 puts a coin into Wrecked's static red
   * locker where it stands, which is just as well, since nothing could ever
   * pick one up.  Both measured 2026-08-03.
   */
  if (lib_put_static_absent_pre390 (game, container))
    return FALSE;
  if (obj_uses_burden_model (game)
      && !obj_is_static (game, container)
      && gs_object_position (game, container) != OBJ_HELD_PLAYER)
    {
      if (run_priority_defer_if_active ())
        return FALSE;
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are not holding ",
                                             "I am not holding ",
                                             "%player% is not holding "));
      lib_print_object (game, container);
      pf_buffer_string (filter, ".\n");
      return FALSE;
    }

  /*
   * If the container is closed, reject now -- before 4.0 only.  4.0's closed
   * test is insides' own, and it sits late: run400 name_object has already
   * run its take piece (@46E2B5) before insides (46639C) tests the target's
   * state at 4661BE/4661C9, after the tasks() call, the possession test and
   * the itself test.  So at 4.0 it belongs to the backend, after the take
   * phase; see lib_put_in_closed_400().
   */
  if (gs_object_openness (game, container) > OBJ_OPEN
      && !lib_is_version_400 (game))
    {
      const scr_bool is_first = lib_put_refusal_first_390 (game);
      const size_t from = pf_buffer_length (filter);

      if (!is_first && run_priority_defer_if_active ())
        return FALSE;

      /*
       * 4.0 reworded this one too, and pre-4.0 has no lock to report: the
       * literal " as it is closed!" is in run370, run380 and run390 and in
       * no run400, while " as it is locked!", " is locked!" and every other
       * spelling of "locked" are in run400 alone.  So a container the game
       * calls locked draws the closed wording before 4.0.  The closed half
       * is measured on both arms with the same probe turn, `put lamp in
       * box` with the box shut: run390 "You can't put anything inside the
       * box as it is closed!" (p39DARK, Adrift_978:28) against run400 "The
       * box is closed!" (p4TFROM, Adrift_979:18), 2026-09-12.
       */
      lib_print_response_object (game,
                                 "You can't put anything inside ",
                                 "I can't put anything inside ",
                                 "%player% can't put anything inside ",
                                 container, " as it is closed!\n");
      if (is_first)
        lib_put_refusal_sweep_390 (game, from);
      return FALSE;
    }

  /* Container is a valid target for "put in". */
  return TRUE;
}


/*
 * lib_put_all_common()
 *
 * Put all objects currently held by the player into a container, or onto a
 * supporter.  One body, because below 3.9 it is one handler; which way a
 * given line runs is lib_put_target_takes_on()'s to say, and the all form
 * is routed by the target's kind exactly as the named one is -- `put all on
 * box` with the box in hand is "You put an torch, a lamp and a table inside
 * the box." on run370 and run380 (Adrift_1012:12 / Adrift_1011:12,
 * 2026-09-12).
 */
static scr_bool
lib_put_all_common (scr_gameref_t game, scr_int target, scr_bool typed_on)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_bool is_on = lib_put_target_takes_on (game, target, typed_on);
  const scr_int container = target;
  scr_int objects;
  lib_put_outcome_t outcome;

  /* Validate the target object to put onto or into (deferred -> unhandled). */
  if (is_on ? !lib_put_on_is_valid (game, container)
            : !lib_put_in_is_valid (game, container))
    return lib_put_invalid_result ();

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              is_on ? lib_put_on_not_supporter_filter
                                    : lib_put_in_not_container_filter,
                              container, FALSE, NULL);
  gs_clear_multiple_references (game);

  /*
   * 3.9's insides() tests c("and") AFTER c("all") (4618D4-46190C), so a
   * line holding both is the and-arm, var_CC = 2, whose count pass
   * (461B4B-461B87) keeps only the objects co(obj, 0) names.  p39PUT T29
   * `put all except coin and stone in cupboard`, the coin and stone lying
   * on the held table, is "Nothing will fit inside the cupboard." (run390x
   * Adrift_154_p39put.txt, 2026-09-19): neither named object is held or
   * loose in the room, and the table, unnamed, is not a candidate.  (The
   * and-arm also admits worn objects, 461B38; not modelled.)
   */
  if (lib_is_version_390 (game) && run_get_dispatch_input ()
      && lib_co_contains (run_get_dispatch_input (), "and"))
    {
      scr_int object;

      for (object = 0; object < gs_object_count (game); object++)
        {
          if (game->object_references[object]
              && !lib_co_pre400 (game, run_get_dispatch_input (), object, 0))
            {
              game->object_references[object] = FALSE;
              objects--;
            }
        }
    }

  outcome = {};
  if (objects > 0)
    outcome = is_on ? lib_put_on_backend (game, container, TRUE)
                    : lib_put_in_backend (game, container, TRUE);
  else
    {
      /*
       * Nothing to put, and all FOUR Runner generations word it differently
       * -- this is the widest generation split on the put row.  Every arm is
       * now measured on the same probe turn, `put all in box` repeated once
       * the first has emptied the player's hands:
       *
       *   run400  You are carrying nothing!          p4TFROM, Adrift_981:20
       *   run390  Nothing will fit inside the box.   p39DARK, Adrift_980:59
       *   run380  You are not carrying anything.     p38DARK, Adrift_985:25
       *   run370  You have nothing to put inside the box.
       *                                              p37DARK, Adrift_989:25
       *
       * (2026-09-12; the 3.7/3.8 halves were driven with fast.sh over
       * cmdfile_p38putin4.txt, a feed authored for this row because none of
       * the three original put feeds ever runs `put all in` with the
       * container in the player's hands.)  The census had guessed the 3.7
       * wording for both pre-3.9 Runners, on the strength of " have nothing
       * to put inside " sitting in run370 and run380 alike; the measurement
       * refutes that.  run380 does hold the string -- it is just not on
       * this path -- and answers with the flat drop-all wording instead,
       * the same literal lib_print_nothing_held() prints.
       *
       * The surface row was driven on the same day, `put all on table` with
       * the table in hand and the first call having emptied the player:
       *
       *   run400  (nothing; see below)            p4SURF,  Adrift_997:6
       *   run390  You have nothing to put onto the table.
       *                                           p39SURF, Adrift_996:6
       *   run380  You are not carrying anything.  p38SURF, Adrift_1002:6
       *   run370  You have nothing to put inside the table.
       *                                           p37SURF, Adrift_1006:6
       *
       * -- so 3.7's wording really is " have nothing to put inside " whatever
       * the target is, 3.8 shares its flat line with the container row, and
       * only 3.9 tells the two apart.  Note that 3.9 does NOT reach for
       * "Nothing will fit" on a surface, even one already over its capacity:
       * the table held six objects on Adrift_1009:6 and the answer was still
       * "You have nothing to put onto the table."
       */
      if (lib_is_version_400 (game))
        {
          /*
           * 4.0 speaks only for hands that are genuinely empty.  With the
           * table itself the only thing carried, `put all on table` says
           * nothing of its own and falls through to the generic catch-all,
           * "I don't understand what you want me to do with the table."
           * (p4SURF, Adrift_997:6), where the same command with nothing at
           * all in hand is "You are carrying nothing!" (p4SURF,
           * Adrift_995:16).  The container row is the same shape, and the
           * shut state of the container changes nothing on either arm: probe
           * PCLOSED, `put all in box` with the box the only thing carried,
           * answers "I don't understand what you want me to do with the
           * box." both shut and just opened (Adrift_1194.txt:11/13), and with
           * nothing at all in hand `put all in chest` / `put all in box`
           * against SHUT containers on the floor answer "You are carrying
           * nothing!" (Adrift_1195.txt:4/5) -- name_object counts the held
           * objects with 44615C @46E553-46E580 and speaks at 46E5BC only
           * when the count is nil, ahead of any insides call, and so ahead
           * of the closed test.  Something else in hand reaches insides and
           * its closed refusal, "The chest is closed!" (Adrift_1195.txt:7).
           */
          {
            scr_int index_;

            for (index_ = 0; index_ < gs_object_count (game); index_++)
              {
                if (gs_object_position (game, index_) == OBJ_HELD_PLAYER)
                  return FALSE;
              }
          }
          pf_buffer_string (filter,
                            lib_select_response (game,
                                               "You are carrying nothing!",
                                               "I am carrying nothing!",
                                               "%player% is carrying nothing!"));
        }
      else if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
        {
          if (is_on)
            lib_print_response_object (game,
                                       "You have nothing to put onto ",
                                       "I have nothing to put onto ",
                                       "%player% has nothing to put onto ",
                                       container, ".");
          else
            lib_print_wrapped_object (game, "Nothing will fit inside ",
                                      container, ".");
        }
      else if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380)
        pf_buffer_string (filter,
                          lib_select_response (game,
                                               "You are not carrying anything.",
                                               "I am not carrying anything.",
                                               "%player% is not carrying anything."));
      else
        lib_print_response_object (game,
                                   "You have nothing to put inside ",
                                   "I have nothing to put inside ",
                                   "%player% has nothing to put inside ",
                                   container, ".");
    }

  return is_on ? lib_put_on_finish (game, outcome)
               : lib_put_in_finish (game, outcome);
}


/*
 * lib_cmd_put_all_in()
 * lib_cmd_put_all_on()
 *
 * Facets of lib_put_all_common().
 */
scr_bool
lib_cmd_put_all_in (scr_gameref_t game)
{
  scr_int container;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  container = lib_put_target_pre390 (game, "put that into", &is_ambiguous);
  if (container == -1)
    return is_ambiguous;

  return lib_put_all_common (game, container, FALSE);
}

scr_bool
lib_cmd_put_all_on (scr_gameref_t game)
{
  scr_int supporter;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  supporter = lib_put_target_pre390 (game, "put that onto", &is_ambiguous);
  if (supporter == -1)
    return is_ambiguous;

  return lib_put_all_common (game, supporter, TRUE);
}


/*
 * lib_put_fragment_names_nothing()
 *
 * run400's name_object resolves the fragment with the noun scorer
 * Proc_21_58_463640 in mode 2 -- every object PRESENT, seen or not -- and
 * only the empty answer (&HFF) reaches the clobbering exit at 46E23B.
 * That is a narrower test than the library's own %text% parse above: the
 * parse also applies the put filter, so an object whose name the fragment
 * plainly holds can still fail it.  hub's `put soup in pan` is the case --
 * object "minestrone soup" carries the alias "soup" and scores 1 in the
 * Runner, so run400 names it and never rewrites the line, while Scarier's
 * parse rejects it for sitting inside the can.  Ask the scorer directly.
 */
static scr_bool lib_cmd_unclear_object (scr_gameref_t game);
static scr_bool lib_is_put_where_line_400 (scr_gameref_t game);

static scr_bool
lib_put_fragment_names_nothing (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  std::string fragment;
  scr_int index_;

  if (!input || !run_unnamed_put_fragment (input, fragment))
    return FALSE;

  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (!obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        continue;
      if (lib_verb_object_name_score (game, index_, fragment.c_str ()) > 0)
        return FALSE;
    }

  return TRUE;
}

/*
 * lib_put_fragment_present_object()
 *
 * The unique highest-scoring object among those merely PRESENT (seen or
 * not) that the unnamed put fragment names -- run400's mode 2 scorer,
 * asked directly rather than through the seen-gated %text% parse above.
 * -1 for none or a tie, exactly like lib_verb_object_resolve_400_string();
 * a tie is left to the ordinary flow (which will itself fail and fall to
 * the unclear-object/rewrite handling below in the caller).
 */
static scr_int
lib_put_fragment_present_object (scr_gameref_t game, const scr_char *fragment)
{
  scr_int index_, object, best, best_count;

  object = -1;
  best = 0;
  best_count = 0;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      scr_int score;

      if (!obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        continue;

      score = lib_verb_object_name_score (game, index_, fragment);
      if (score == 0)
        continue;
      if (score > best)
        {
          object = index_;
          best = score;
          best_count = 1;
        }
      else if (score == best)
        best_count++;
    }

  return best_count == 1 ? object : -1;
}

/*
 * lib_put_in_present_filter()
 *
 * The universe run390's put names its object from, before any test of where
 * that object is: everything present in the room, the contents of open
 * containers included.  It is deliberately wider than lib_put_named_filter()
 * -- the pre-4.0 handler answers differently for a noun that names nothing
 * at all and one that names something it cannot reach, so the two have to be
 * told apart; see lib_put_named_pre400().
 */
static scr_bool
lib_put_in_present_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return obj_indirectly_in_room (game, object, gs_playerroom (game));
}


/*
 * lib_put_no_object_pre400()
 * lib_put_not_reachable_pre400()
 * lib_put_what_pre400()
 *
 * The three answers a pre-4.0 "put X in Y" gives before it ever moves
 * anything, and they are decided in this order -- the object's own failure
 * outranks the container's.  Measured on p39DARK under run390, 2026-09-12:
 *
 *   Adrift_978:67  `put zzzz in box`   You can't do that!
 *   Adrift_980:27  `put stone in lamp` You can't do that!    (the stone is a
 *                  room away; the lamp is held and is not a container, and
 *                  that refusal never gets a word in)
 *   Adrift_978:40  `put coin in box`   You can't see that.   (the coin is
 *                  inside the box, the box on the floor -- present, named,
 *                  and out of reach; see lib_put_named_filter)
 *   Adrift_980:24  `put lamp in box`   Put the lamp inside what?  (the lamp
 *                  is held, the box a room away)
 *   Adrift_976:34  `put coin in coin`  Put the coin inside what?
 *   Adrift_976:37  `put box in box`    Put the box inside what?
 *   Adrift_976:64  `put coin in me`    Put the coin inside what?
 *
 * The surface handler answers the same shapes the same way, in its own
 * preposition (p39SURF under run390, 2026-09-12): `put stone on lamp` with
 * the stone a room away is "You can't do that!" (Adrift_990:6), `put coin on
 * table` with the coin on the table and the table on the floor is "You can't
 * see that." (Adrift_990:5), and `put coin on coin`, `put table on table`
 * and `put coin on me` are "Put the coin onto what?", "Put the table onto
 * what?" and "Put the coin onto what?" (Adrift_990:11/12/19).
 *
 * -- so the container fragment is resolved with the object already spoken
 * for, and a fragment that names only the object, or names nothing at all,
 * leaves the Runner with no container and it asks.  (4.0 answers the last
 * three "You can't put anything inside the coin!", "You can't put an object
 * inside itself!" and "I don't understand what you want to put things
 * inside." on the same turns of p4TFROM, Adrift_977:15/18/46.)
 *
 * " can't do that!" is in run370, run380 and run390 and in no run400, and
 * " can't see that." is in all four; the prompt is composed, so the census
 * cannot pin it and it is gated at 3.90, the version it was measured on.
 * 3.7 and 3.8 keep what they had.
 */
static scr_bool lib_put_co_refusal_pre390 (scr_gameref_t game);

/*
 * lib_put_refusal_first_390()
 *
 * TRUE if this refusal is run390's to give before any task sees the line.
 * insides() runs above tasks(0) in generaltasks (45F471 / 45F48B), and its
 * first question, before the task look-up at 461A6C, is how many objects
 * co() names in the line (461000-461641).  co() answers only for a present,
 * seen object, so a put whose object is absent or never seen counts one
 * name, the container's, and fewer than two is answered on the spot
 * (461646-461754): "Put <the X> inside what?" when the object was named and
 * the container was not, "You can't do that!" otherwise.  Both go to the
 * post-put sweep (462550), whose claimant runs the tasks QUIET and whose
 * absence leaves them LOUD, as after a put that moved something; see
 * lib_put_task_sweep_390().
 *
 * Measured on Lair of the CyberCow (runner_transcripts/cybercow.txt T62,
 * run390x, 2026-09-19): `put bones in robot` with the bones never made --
 * the fairy got away -- is "You can't do that!".  The robot, lying in the
 * cellar, claims the sweep; tasks 81-85 all match and all fail QUIET, so
 * task 85's FailMessage "You're not holding the little bones to install
 * them in the Invincible Robot." never replaces the refusal, where the
 * deferred refusal had let the loud task pass print it.
 *
 * Only the refusals the priority pass reaches are moved; the put-nowhere
 * catch-alls below STANDARD_COMMANDS still follow the task passes.
 */
static scr_bool
lib_put_refusal_first_390 (scr_gameref_t game)
{
  return lib_is_version_390 (game) && run_in_priority_pass ()
         && run_get_dispatch_input ();
}

static scr_bool
lib_put_no_object_pre400 (scr_gameref_t game)
{
  if (lib_put_refusal_first_390 (game))
    {
      const size_t from = pf_buffer_length (gs_get_filter (game));

      lib_print_response_message (game,
                                  "You can't do that!\n",
                                  "I can't do that!\n",
                                  "%player% can't do that!\n");
      lib_put_sweep_390 (game, run_get_dispatch_input (), from);
      return TRUE;
    }

  if (run_priority_defer_if_active ())
    return FALSE;

  /* 3.7/3.8 insides() names what co() chose; see lib_put_co_refusal_pre390(). */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390
      && lib_put_co_refusal_pre390 (game))
    return TRUE;

  return lib_print_response_message (game,
                                     "You can't do that!\n",
                                     "I can't do that!\n",
                                     "%player% can't do that!\n");
}

/*
 * lib_cmd_put_no_clause_pre400()
 *
 * `put on X` and `put in X`, the spellings with nothing between the verb and
 * the preposition.  From 3.80 insides() takes them like any other put (its
 * entry is c("put") with c("inside"/"into"/"in"/"on"), run380 445746), and
 * the line co() names at most once, so it is answered "You can't do that!"
 * before any target test -- 3.8 at 445A2A, 3.9 at 461646, where the target
 * pass has already taken the one name for the container because it stands
 * after InStr(line, "on").  3.70 never gets here: the wear rows above claim
 * the line, insides() being a plain bottom-of-generaltasks Call there
 * (43BA0A); see lib_wear_is_put_line_380().
 */
scr_bool
lib_cmd_put_no_clause_pre400 (scr_gameref_t game)
{
  if (lib_is_version_400 (game))
    return FALSE;
  return lib_put_no_object_pre400 (game);
}

static scr_bool
lib_put_not_reachable_pre400 (scr_gameref_t game)
{
  if (lib_put_refusal_first_390 (game))
    {
      const size_t from = pf_buffer_length (gs_get_filter (game));

      lib_print_response_message (game,
                                  "You can't see that.\n",
                                  "I can't see that.\n",
                                  "%player% can't see that.\n");
      lib_put_sweep_390 (game, run_get_dispatch_input (), from);
      return TRUE;
    }

  if (run_priority_defer_if_active ())
    return FALSE;

  return lib_print_response_message (game,
                                     "You can't see that.\n",
                                     "I can't see that.\n",
                                     "%player% can't see that.\n");
}

static scr_bool
lib_put_what_pre400 (scr_gameref_t game, scr_int object, scr_bool typed_on)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;

  if (lib_put_refusal_first_390 (game))
    {
      const size_t from = pf_buffer_length (filter);

      pf_buffer_string (filter, "Put ");
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        typed_on ? " onto what?\n" : " inside what?\n");
      lib_put_sweep_390 (game, run_get_dispatch_input (), from);
      return TRUE;
    }

  if (run_priority_defer_if_active ())
    return FALSE;

  pf_buffer_string (filter, "Put ");
  lib_print_object_np (game, object);
  pf_buffer_string (filter, typed_on ? " onto what?\n" : " inside what?\n");
  return TRUE;
}


/*
 * lib_put_task_sweep_390()
 *
 * run390's insides() moves a single named object and writes "You put X onto
 * Y." (461F8A-46210D) without ever setting its own result; that is left to
 * the sweep that follows (462553-462760), and it decides which task pass the
 * line gets afterwards.  The sweep walks every object whose o(22) is 0 (held),
 * the player's room (a dynamic lying there, or a static authored with
 * InitialPosition 4 + room, which is the only o(22) a static ever has) or
 * &H9C (worn), and the first one co() finds named in the line claims the put:
 * the buffer is emptied, tasks(1) -- checktask(line, 0), QUIET -- runs on the
 * typed line, the result becomes 1, and the put text comes back only if the
 * buffer is still empty (462760).  A task the move enabled then speaks
 * instead of the put text; one whose restrictions fail says nothing.
 * Nothing named and eligible means the result stays 0, and generaltasks goes
 * on to tasks(0) (45F48B) -- checktask(line, 1), LOUD -- with the put text
 * still in the buffer: a matched task's failing restriction overwrites it
 * with the FailMessage (452BBD), a passing one runs and replaces it.  The
 * moved object itself never qualifies (it is now &HF6/&HEC), so the usual
 * claimant is the container, and only when it is held or lying in the room.
 *
 * Measured, run390 (2026-09-19):
 *   - Lost Tomb (runner_transcripts/losttomb.txt T85): `put dung beetle on
 *     green pillar` -- pillar static, nothing else eligible named -- moves the
 *     beetle and the LOUD pass runs task 30 (bare `*`, all four animals on
 *     their pillars), whose "The four pillars slowly sink into the ground."
 *     replaces the put message.
 *   - Secret of the Lost World (runner_transcripts/secret_of_lost_world.txt
 *     T118-T119, and the probe Adrift_128_solwred): `put red gem on statue`
 *     names the held green gem (co() matches Short "gem"; the prefix is not
 *     consulted), moves it onto the static statue, and the LOUD pass matches
 *     task 5 `* red * statue`, whose first restriction wants the red gem held:
 *     "You don't have a red gem." replaces the put text, and `x statue` then
 *     shows "A green gem is on ancient statue.".  T119 `put green gem on
 *     statue` finds the gem already there and task 6's "You don't have a
 *     green gem." is all that prints.
 *   - Troll (runner_transcripts/troll.txt T116, run400x and the drop/firewater
 *     probes Adrift_128_trolldrop, Adrift_130_trollfire): `put breadcrumbs in
 *     basin` with the basin HELD -- the basin claims the sweep, the QUIET pass
 *     matches task 51 `* breadcrumbs * basin *` whose second restriction
 *     fails, and the put text "You put the lot of breadcrumbs inside the wash
 *     basin." stands.
 * The "all" and "and" forms rebuild a line per object instead, and are not
 * ported.  3.9 only; run380 sweeps take-from lines instead
 * (lib_take_from_task_sweep_380()), and run400 dispatches its insides
 * look-ups before the move.
 */
static scr_bool lib_put_co_named_term (scr_gameref_t game,
                                       const scr_char *line, scr_int object);

/* insides()' co() name count over the whole game (run380 var_A6). */
static scr_int
lib_put_co_count_pre390 (scr_gameref_t game)
{
  const scr_char *line = run_get_dispatch_input ();
  scr_int object, matches = 0;

  if (!line)
    return 2;
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (lib_put_co_named_term (game, line, object))
        matches++;
    }
  return matches;
}

static scr_bool
lib_put_sweep_claims_390 (scr_gameref_t game, const scr_char *line)
{
  const scr_int playerroom = gs_playerroom (game);
  const scr_int object_count = gs_object_count (game);
  scr_int object;

  for (object = 0; object < object_count; object++)
    {
      scr_bool is_eligible;

      if (obj_is_static (game, object))
        {
          const scr_int code = obj_initial_location_code (game, object);

          is_eligible = code == 0 || code == playerroom + 1;
        }
      else
        {
          const scr_int position = gs_object_position (game, object);

          is_eligible = position == OBJ_HELD_PLAYER
                        || position == OBJ_WORN_PLAYER
                        || position == playerroom + 1;
        }
      if (is_eligible && lib_put_co_named_term (game, line, object))
        return TRUE;
    }
  return FALSE;
}

/*
 * lib_put_sweep_390()
 *
 * The sweep itself, on whatever insides() left in the buffer from `from`:
 * the task pass it picks, and the put text put back only if that pass said
 * nothing.
 */
static void
lib_put_task_pass_390 (scr_gameref_t game, const scr_char *input,
                       size_t from, scr_bool is_loud)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *buffer;
  std::string text;

  buffer = pf_get_buffer (filter);
  if (buffer && strlen (buffer) > from)
    text = buffer + from;
  pf_truncate (filter, from);
  run_typed_line_task_commands (game, input, is_loud);
  if (pf_buffer_length (filter) == from && !text.empty ())
    pf_buffer_string (filter, text.c_str ());
}

static void
lib_put_sweep_390 (scr_gameref_t game, const scr_char *input, size_t from)
{
  lib_put_task_pass_390 (game, input, from,
                         !lib_put_sweep_claims_390 (game, input));
}

static void
lib_put_task_sweep_390 (scr_gameref_t game, scr_int container,
                        const lib_list_t &moving, size_t from)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *input = run_get_dispatch_input ();
  scr_bool has_moved = FALSE;

  if (version < TAF_VERSION_390 || version >= TAF_VERSION_400 || !input)
    return;

  for (const scr_int object : moving)
    {
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && gs_object_parent (game, object) == container)
        has_moved = TRUE;
    }
  if (!has_moved)
    return;

  lib_put_sweep_390 (game, input, from);
}


/*
 * lib_put_co_resolve_390()
 *
 * run390's insides() does not parse the object fragment at all.  Its
 * single-object loop (461EE8-462125) offers every object to co(obj, 0) --
 * the Short, or failing that an Alias, typed as a whole word, the prefix
 * consulted only when present, seen namesakes make the term ambiguous -- and
 * moves each one that is also seen (o(44)), present (obhere), not the
 * container, and either held or lying loose in the player's room (o(22) =
 * room).  So a prefix word that names an ABSENT namesake is simply not
 * heard: secret_of_lost_world T118 (run390, runner_transcripts/
 * secret_of_lost_world.txt and the probe Adrift_128_solwred, 2026-09-19)
 * `put red gem on statue` with the green gem the only gem in play moves the
 * green gem onto the statue -- `x statue` afterwards: "A green gem is on
 * ancient statue.", and `i` no longer lists it.  (What prints is task 5's
 * "You don't have a red gem."; see lib_put_task_sweep_390().)
 *
 * Only reached when the parser found nothing under the typed words; a
 * fragment the parser does resolve is left to it.  3.9 only -- run380's
 * insides is unmeasured here.
 */
static scr_bool
lib_put_co_resolve_390 (scr_gameref_t game, scr_int container,
                        scr_int *references)
{
  const scr_char *line = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  const scr_int object_count = gs_object_count (game);
  scr_int object, count = 0;

  if (!line)
    return FALSE;

  for (object = 0; object < object_count; object++)
    {
      const scr_int position = gs_object_position (game, object);

      if (object == container || obj_is_static (game, object))
        continue;
      if (position != OBJ_HELD_PLAYER && position != room + 1)
        continue;
      if (!gs_object_seen (game, object)
          || !obj_indirectly_in_room (game, object, room))
        continue;
      if (!lib_co_pre400 (game, line, object, 0))
        continue;
      game->multiple_references[object] = TRUE;
      count++;
    }
  if (count == 0)
    return FALSE;
  *references = count;
  return TRUE;
}


/*
 * lib_put_named_pre400()
 *
 * The pre-4.0 "put <named object> in <container>" pipeline, and -- the same
 * pipeline, because below 3.9 it is literally the same handler -- the "put
 * <named object> on <supporter>" one.  It differs from 4.0's in the order it
 * asks its questions as much as in the words it uses: the object is named
 * first and its failures are answered first, the target's own refusals come
 * next, and only then is the object tested for being somewhere the player
 * can put it from.  See lib_put_no_object_pre400() for the evidence.
 *
 * typed_on is the preposition the player wrote; which handler actually runs
 * is lib_put_target_takes_on()'s to say.
 */
static scr_bool
lib_put_named_pre400 (scr_gameref_t game, scr_int target, scr_bool typed_on)
{
  const scr_bool is_pre_390 =
      prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390;
  const scr_bool is_on = lib_put_target_takes_on (game, target, typed_on);
  const scr_int container = target;
  scr_int object, object_count, objects, references;
  scr_bool has_object, status;
  lib_put_outcome_t outcome;
  lib_list_t moving;
  size_t sweep_from;

  object_count = gs_object_count (game);
  references = 0;

  /* Name the object over everything present, out of reach or not. */
  has_object = !lib_put_mid_word_390
               && lib_parse_multiple_objects (game, "move",
                                              lib_put_in_present_filter,
                                              -1, &references);
  if (!has_object && !is_pre_390
      && prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400
      && lib_put_co_resolve_390 (game, container, &references))
    has_object = TRUE;
  if (!has_object)
    {
      /*
       * 3.7 and 3.8 invert 3.9's precedence and answer for the CONTAINER
       * first -- but only once insides() has counted two co() names on the
       * line (var_A6, run380 4457A1; "You can't do that!" below that).  A
       * fragment that names nothing ANYWHERE leaves the container alone on
       * the line, and the flat refusal outranks its tests: p37PUT/p38PUT
       * `put zzz in statue` (static, no container), `put zzz in coin`
       * (held, no container) and `put zzz in chest` (static container,
       * open and shut) are all "You can't do that!" (run370x / run380x
       * Adrift_154_p37put2.rtf, Adrift_155_p38put2.rtf, 2026-09-19).  An
       * object named but out of reach still counts, and goes on to the
       * container tests below.
       */
      if (!is_pre_390 || lib_put_co_count_pre390 (game) < 2)
        return lib_put_no_object_pre400 (game);
    }
  else if (references == 0)
    return TRUE;

  /*
   * What a 3.7/3.8 put does with a static is unmeasured, so a line that
   * names one there is left exactly where it was, for the task passes and
   * the generic tail below them.  3.9 goes on: insides() counts only the
   * dynamic objects it could move (461AF8), so a static that is present
   * leaves the message at "You put " and the target's own refusals first,
   * then " can't see that." (4624EF) and the sweep, as for any object it
   * could not reach.  pPUTFULL39 `put statue in cupboard`, both statics in
   * the room and the cupboard open, is "You can't see that." (run390x
   * Adrift_pputfull39.txt, 2026-09-19).
   */
  for (object = 0; has_object && is_pre_390 && object < object_count;
       object++)
    {
      if (game->multiple_references[object] && obj_is_static (game, object))
        {
          gs_clear_multiple_references (game);
          return FALSE;
        }
    }

  /*
   * The container fragment named only the object itself, so it named none.
   *
   * This one outranks the container's own refusals on 3.7 and 3.8 as well
   * as on 3.9 -- it is the one thing that does -- but the two generations
   * say different things about it.  3.9 asks; 3.7 and 3.8 answer flatly,
   * and they do so even where the named object is not a container at all
   * and the container tests below would have had something to say.
   * Measured on p38DARK/p37DARK, 2026-09-12: `put coin in coin` with the
   * coin inside the box, `put box in box` with the box held and open, and
   * `put stone in stone` with the stone loose on the floor all answer "You
   * can't do that!" (Adrift_982:36/39/42, Adrift_986:36/39/42), against the
   * "You can't put anything inside the coin." that the very same coin draws
   * as somebody else's container two feeds later (Adrift_984:47).
   */
  if (has_object && references == 1 && game->multiple_references[container])
    {
      gs_clear_multiple_references (game);
      if (is_pre_390)
        return lib_put_no_object_pre400 (game);
      return lib_put_what_pre400 (game, container, typed_on);
    }

  /*
   * Validate the container object to put into (deferred -> unhandled).
   *
   * On 3.7 and 3.8 this is where the turn is usually decided: the
   * container's not-holding, not-a-container and closed refusals all
   * outrank whatever the object fragment did or did not name.  Measured on
   * p38DARK under run380 and p37DARK under run370, 2026-09-12, all with the
   * object a room away or sealed out of reach --
   *
   *   put lamp in box   box on the floor a room away
   *                     You are not holding a box.  (Adrift_984:25/988:25)
   *   put stone in lamp lamp held, not a container
   *                     You can't put anything inside the lamp.
   *                                                 (Adrift_984:28/988:28)
   *   put lamp in box   box held and shut
   *                     You can't put anything inside the box as it is
   *                     closed!                     (Adrift_983:30/987:30)
   *
   * -- where run390 answers for the object on every one of those turns
   * (Adrift_980:24/27, Adrift_978:28).  Note the first: run370 and run380
   * resolve the container over the whole game, not just the room, which is
   * why it reaches this test at all; see lib_put_target_pre390().
   */
  if (is_on ? !lib_put_on_is_valid (game, container)
            : !lib_put_in_is_valid (game, container))
    {
      gs_clear_multiple_references (game);
      return lib_put_invalid_result ();
    }

  /* The container is fine, so now the object fragment gets its answer. */
  if (!has_object)
    return lib_put_no_object_pre400 (game);

  /* Now, and only now, the object has to be somewhere the player can reach. */
  for (object = 0; object < object_count; object++)
    {
      if (game->multiple_references[object]
          && !lib_put_named_filter (game, object))
        {
          gs_clear_multiple_references (game);
          return lib_put_not_reachable_pre400 (game);
        }
    }

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              is_on ? lib_put_on_filter : lib_put_in_filter,
                              -1, FALSE, &references);
  for (object = 0; object < object_count; object++)
    {
      if (game->object_references[object])
        moving.push_back (object);
    }
  sweep_from = pf_buffer_length (gs_get_filter (game));

  outcome = {};
  if (objects > 0 || references > 0)
    outcome = is_on ? lib_put_on_backend (game, container, FALSE)
                    : lib_put_in_backend (game, container, FALSE);
  else
    lib_print_nothing_held (game, FALSE, FALSE, ".");

  status = is_on ? lib_put_on_finish (game, outcome)
                 : lib_put_in_finish (game, outcome);
  lib_put_task_sweep_390 (game, container, moving, sweep_from);

  /*
   * A 3.9 put that came to nothing but a size or capacity refusal moved
   * nothing, so no sweep runs; insides() returns unclaimed and generaltasks'
   * LOUD tasks(0) gets the line with the refusal still in the buffer, where
   * a matching task's FailMessage overwrites it.  pPUTREF39.taf, run390x,
   * 2026-09-19: `put coin in bag` with the bag full is "T6 FAIL." (Adrift_
   * pputref392.txt:5) and the coin stays in hand, where with no task the
   * same line is "The coin can't fit inside the bag at the moment."; a
   * passing task likewise speaks alone (make_39_putprobe.py PUTBIG).
   */
  if (outcome.is_refusal_only && lib_is_version_390 (game)
      && run_in_priority_pass () && run_get_dispatch_input ())
    lib_put_task_pass_390 (game, run_get_dispatch_input (), sweep_from, TRUE);
  return status;
}


/*
 * lib_put_already_inside_400()
 *
 * Version 4.0 only.  An object already sitting in the container the player
 * has just named is refused before the container is looked at at all, and
 * the wording turns on whether the CONTAINER is in the player's hands:
 *
 *   box held, coin inside it    The coin is already inside the box!
 *   box on the floor, ditto     You are not holding the coin.
 *
 * -- p4TFROM under run400, `put coin in box` at Adrift_977:12 and again at
 * Adrift_977:34 with the box shut (still "already inside", so this outranks
 * the closed-container refusal), against Adrift_979:30 with the box dropped.
 * Measured 2026-09-12.  " is already inside " is in run400 and in no earlier
 * Runner; the floor case is the take phase's own skip, which
 * lib_put_implicit_take() already carries into the "You are not holding ..."
 * report.
 *
 * Returns TRUE if it consumed every named object, so the caller is done.
 */
static scr_bool
lib_put_already_inside_400 (scr_gameref_t game, scr_int container)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, object_count, left;
  scr_bool has_printed;

  if (!lib_is_version_400 (game)
      || !obj_indirectly_held_by_player (game, container))
    return FALSE;

  object_count = gs_object_count (game);
  has_printed = FALSE;
  left = 0;
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object])
        continue;

      if (gs_object_position (game, object) == OBJ_IN_OBJECT
          && gs_object_parent (game, object) == container)
        {
          lib_new_clause (game, has_printed);
          lib_print_object_np (game, object);
          pf_buffer_string (filter,
                            lib_select_plurality (game, object,
                                                  " is", " are"));
          pf_buffer_string (filter, " already inside ");
          lib_print_object_np (game, container);
          pf_buffer_character (filter, '!');
          game->multiple_references[object] = FALSE;
          has_printed = TRUE;
          continue;
        }
      left++;
    }

  if (has_printed && left == 0)
    {
      pf_buffer_answer_break (filter);
      return TRUE;
    }
  return FALSE;
}


/*
 * lib_put_already_on_400()
 *
 * The surface twin of the above, and it splits the same way on whether the
 * SUPPORTER is in the player's hands.  Measured on p4SURF under run400,
 * 2026-09-12, all with the coin sitting on the table:
 *
 *   table held      put coin on table   The coin is already on the table!
 *                                       (Adrift_991:7, and again at :10 once
 *                                       the coin has been taken and put back)
 *   table on floor  put coin on table   You are not holding the coin.
 *                                       (Adrift_991:18, after `drop table`)
 *
 * and the same pair on the nut and the table, Adrift_993:7/9.  " is already
 * on " is in run400 and in no earlier Runner; the floor case is the take
 * phase's own skip, which lib_put_implicit_take() already reports.
 *
 * Returns TRUE if it consumed every named object, so the caller is done.
 */
static scr_bool
lib_put_already_on_400 (scr_gameref_t game, scr_int supporter)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, object_count, left;
  scr_bool has_printed;

  if (!lib_is_version_400 (game)
      || !obj_indirectly_held_by_player (game, supporter))
    return FALSE;

  object_count = gs_object_count (game);
  has_printed = FALSE;
  left = 0;
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object])
        continue;

      if (gs_object_position (game, object) == OBJ_ON_OBJECT
          && gs_object_parent (game, object) == supporter)
        {
          lib_new_clause (game, has_printed);
          lib_print_object_np (game, object);
          pf_buffer_string (filter,
                            lib_select_plurality (game, object,
                                                  " is", " are"));
          pf_buffer_string (filter, " already on ");
          lib_print_object_np (game, supporter);
          pf_buffer_character (filter, '!');
          game->multiple_references[object] = FALSE;
          has_printed = TRUE;
          continue;
        }
      left++;
    }

  if (has_printed && left == 0)
    {
      pf_buffer_answer_break (filter);
      return TRUE;
    }
  return FALSE;
}


/*
 * lib_put_shut_in_container_400()
 *
 * The already-inside refusal has to reach an object the ordinary matcher
 * cannot: with the box SHUT and the coin inside it, `put coin in box` is
 * still "The coin is already inside the box!" on run400 (p4TFROM,
 * Adrift_977:34, 2026-09-12), where obj_indirectly_in_room() -- and so
 * lib_disambiguate_object_common()'s own gate -- has already ruled the coin
 * out of the room.  Score the %text% fragment against the container's
 * contents directly, and return the single object it names, or -1.
 */
static scr_int
lib_put_shut_in_container_400 (scr_gameref_t game, scr_int container)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *named = var_get_ref_text (vars);
  scr_int object, found;

  if (!named || !obj_indirectly_held_by_player (game, container))
    return -1;

  found = -1;
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (gs_object_position (game, object) != OBJ_IN_OBJECT
          || gs_object_parent (game, object) != container)
        continue;
      if (lib_verb_object_name_score (game, object, named) <= 0)
        continue;
      if (found >= 0)
        return -1;
      found = object;
    }
  return found;
}

/*
 * lib_put_named_400()
 *
 * The 4.0 put noun goes through name_object's own resolver, not the
 * seen-gated %text% matcher: 46E5D8 hands the object fragment
 * (MemVar_494174, the line up to " in "/" on ") to Proc_21_58_463640 in mode
 * 2 (@46E02D), the same two-pass scorer a plain `drop X` uses -- pass 0 over
 * what the player holds, directly or inside something held, pass 1 over
 * everything present, no seen gate in either; see
 * lib_name_object_resolve_400().
 * Probe PPUTTIE (Adrift_1193.txt, 2026-09-19), a box in hand:
 *
 *   put key in box   brass key held, iron key on the floor
 *                    You put the brass key inside the box.   (no prompt)
 *   put key in box   the brass key now inside the held box
 *                    The brass key is already inside the box!
 *   put gem in box   both gems on the floor: a pass-1 tie, the pending
 *                    object cleared by the restore at 46355E
 *                    It is not clear which gem you are referring to.
 *   put coin in box  both coins held: a pass-0 tie, pending object kept
 *                    Which coin.  The gold coin or the silver coin?
 *
 * Scarier's matcher used to ask "Which key." for the first two and prompt
 * for the gems.  The all/and/except forms take name_object's own branches
 * (46E053, 46E0B7) and are left to the ordinary parse.  Returns TRUE when
 * the line is answered here (a prompt or the not-clear refusal); otherwise
 * a unique winner is left alone in the multiple references with
 * *REFERENCES 1, or *REFERENCES stays -1 for the ordinary parse.
 */
static scr_bool
lib_put_named_400 (scr_gameref_t game, scr_int *references)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *fragment = var_get_ref_text (gs_get_vars (game));
  std::vector<scr_int> marked;
  scr_int object, pending, last_tied, mark_count;

  *references = -1;
  if (!lib_is_version_400 (game) || !fragment || !run_get_dispatch_input ()
      || lib_input_contains_word_400 (fragment, "all")
      || lib_input_contains_word_400 (fragment, "everything")
      || lib_input_contains_word_400 (fragment, "and")
      || lib_input_contains_word_400 (fragment, "except"))
    return FALSE;

  object = lib_name_object_resolve_400 (game, fragment, 2, &pending,
                                        &last_tied, &marked, &mark_count);
  if (object == -2)
    return FALSE;

  if (object == -1)
    {
      if (pending < 0)
        {
          pf_buffer_string (filter, "It is not clear which ");
          pf_buffer_string (filter,
                            lib_drop_named_term_400 (game, last_tied,
                                                     fragment, FALSE));
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 " you are referring to.\n",
                                                 " I am referring to.\n",
                                                 " %player% is referring to.\n"));
          return TRUE;
        }

      /* As lib_drop_named_400(): the run-together list is unmeasured. */
      if ((scr_int) marked.size () == mark_count)
        {
          lib_co_400_raise_named (game,
                                  lib_drop_named_term_400 (game, pending,
                                                           fragment, TRUE),
                                  marked);
          return TRUE;
        }
      return FALSE;
    }

  /*
   * Mode 2 has no seen gate, and the Runner's name composer answers "that"
   * for an object still unseen; see lib_put_print_object_or_that().
   */
  if (!gs_object_seen (game, object))
    lib_put_present_unseen = object;
  gs_clear_multiple_references (game);
  game->multiple_references[object] = TRUE;
  *references = 1;
  return FALSE;
}


/*
 * lib_put_in_multiple_common()
 *
 * Put the objects held by the player and listed in %text% into an object,
 * or -- for is_except -- every one of them but those listed.  The except
 * form has to keep the container itself out of the list it builds, so it
 * filters on a different predicate.
 */
static scr_bool
lib_put_in_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  scr_int container, objects, references;
  scr_bool is_ambiguous;
  lib_put_outcome_t outcome;

  /* Get the referenced object, and if none, consider complete. */
  container = is_except
              ? lib_disambiguate_object (game, "put that into", &is_ambiguous)
              : lib_put_target_pre390 (game, "put that into",
                                       &is_ambiguous);
  if (container == -1)
    return is_ambiguous;

  /* Pre-4.0 names the object first, and answers for it first. */
  if (!is_except && !lib_is_version_400 (game))
    return lib_put_named_pre400 (game, container, FALSE);

  /* Parse the multiple objects list to find the target objects. */
  lib_put_admission_guard_t admission_guard;
  references = -1;
  if (!is_except && lib_put_named_400 (game, &references))
    return TRUE;
  scr_bool parsed = references == 1
                    || lib_parse_multiple_objects (game,
                                   is_except ? "retain" : "move",
                                   is_except ? lib_put_in_not_container_filter
                                             : lib_put_in_resolve_filter,
                                   is_except ? container : -1, &references);

  /*
   * name_object's scorer (463640) counts every object the player has seen,
   * wherever it now lies, and nothing between it and the take asks where the
   * object is.  So a noun naming nothing present but one seen object elsewhere
   * is fetched from that other room: p4LOCK `put gem in jar`, the gem left in
   * Beta, answers "(Taking the gem first)" / "You put the gem inside the
   * jar." (Adrift_1162, 2026-09-14).  An unseen noun still falls to the
   * clobbering exit below.
   */
  if (!parsed && !is_except && lib_is_version_400 (game))
    {
      const scr_char *input = run_get_dispatch_input ();
      std::string fragment;

      if (input && run_unnamed_put_fragment (input, fragment))
        {
          if (lib_put_fragment_names_nothing (game))
            {
              const scr_int seen = lib_verb_object_resolve_400_string
                                     (game, fragment.c_str (), NULL, FALSE);

              if (seen >= 0 && !obj_is_static (game, seen))
                {
                  lib_put_seen_absent = seen;
                  gs_clear_multiple_references (game);
                  game->multiple_references[seen] = TRUE;
                  references = 1;
                  parsed = TRUE;
                }
            }
          else
            {
              /*
               * The fragment names something present after all -- just not
               * anything the seen-gated %text% parse above could see.  Ask
               * the scorer directly, the same way run400's own mode 2 does;
               * see lib_put_fragment_present_object().
               */
              const scr_int present =
                lib_put_fragment_present_object (game, fragment.c_str ());

              if (present >= 0 && !obj_is_static (game, present))
                {
                  /*
                   * Only the genuinely unseen case prints as "that" --
                   * lib_put_fragment_present_object() ignores seen state
                   * when it searches, so it can also recover an object the
                   * top parse missed for some other reason while it was
                   * already seen (measured on TheADRIFTProject, 2026-09-19:
                   * `put battery in remote` finds an already-seen battery
                   * this way and must still name it, not say "that").
                   */
                  if (!gs_object_seen (game, present))
                    lib_put_present_unseen = present;
                  gs_clear_multiple_references (game);
                  game->multiple_references[present] = TRUE;
                  references = 1;
                  parsed = TRUE;
                }
            }
        }
    }

  if (!parsed)
    {
      /*
       * ... unless the container is holding it.  A shut container's contents
       * are out of the matcher's reach but not out of the take phase's; see
       * lib_put_shut_in_container_400().
       */
      if (!is_except && lib_is_version_400 (game))
        {
          const scr_int shut = lib_put_shut_in_container_400 (game, container);

          if (shut >= 0)
            {
              gs_clear_multiple_references (game);
              game->multiple_references[shut] = TRUE;
              if (lib_put_already_inside_400 (game, container))
                return TRUE;
              gs_clear_multiple_references (game);
            }
        }

      /*
       * A 4.0 "put X in Y" whose X names nothing is run400's 46E142, the one
       * exit of name_object that leaves the command line rewritten to the
       * fragment "put X " -- see run_priority_unnamed_put_object().  The
       * container has to be a real container for the Runner to have got that
       * far: 46DE19 turns anything else away with "can't put anything inside
       * X!", along the exit that puts the line back.  The except form takes
       * the "all" branch at 46E04E and never reaches the clobber.
       */
      if (!is_except
          && lib_is_version_400 (game)
          && run_in_priority_pass ()
          && obj_is_container (game, container)
          && lib_put_fragment_names_nothing (game))
        {
          /*
           * With no put/drop-class task pre-matching the typed line the
           * Runner speaks here and the line is done, a turn: p4PUT `put zzz
           * in box` -> "It is not clear which object you are referring to."
           * and the probe's ticker fires (Adrift_953, 2026-09-08).  With
           * one, it stays silent and the tasks get the fragment.
           */
          if (!lib_task_prematches_input (game, 2))
            {
              /* 46E165: the whole word "drop" picks "Drop what?" (p4PUT
               * `drop zzz in box`, also a turn). */
              if (lib_input_contains_word (run_get_dispatch_input (), "drop"))
                return lib_what (game, "Drop");
              return lib_cmd_unclear_object (game);
            }
          run_priority_unnamed_put_object ();
        }
      return FALSE;
    }
  else if (references == 0)
    return TRUE;

  /* 4.0's take phase speaks before the container is examined at all. */
  if (!is_except && lib_put_already_inside_400 (game, container))
    return TRUE;

  /* Validate the container object to put into (deferred -> unhandled). */
  if (!lib_put_in_is_valid (game, container))
    return lib_put_invalid_result ();

  /* As a special case, complain about requests to retain the container. */
  if (is_except
      && lib_multiple_retains_associate (game, container, "retain"))
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              is_except ? lib_put_in_not_container_filter
                                        : lib_put_in_filter,
                              is_except ? container : -1, is_except,
                              &references);
  outcome = {};
  if (objects > 0 || references > 0)
    outcome = lib_put_in_backend (game, container, is_except);
  else
    lib_print_nothing_held (game, FALSE, is_except && objects == 0, ".");

  return lib_put_in_finish (game, outcome);
}


/*
 * lib_cmd_put_in_nowhere()
 * lib_cmd_put_on_nowhere()
 *
 * "put X in Y" -- or "put X on Y" -- where nothing present answers to Y.  The %object% rows above
 * cannot match such a line at all, so it used to fall all the way through to
 * the game's DontUnderstand; run390 answers it, and the answer depends on
 * whether X named anything:
 *
 *   `put lamp in box`  (the box a room away)   Put the lamp inside what?
 *   `put coin in me`   ("me" is not an object) Put the coin inside what?
 *   `put stone in box` (both a room away)      You can't do that!
 *   `put coin in zzzz`                         Put the coin inside what?
 *   `put zzzz in box`  (via the %object% row)  You can't do that!
 *
 * (p39DARK, Adrift_976:64, Adrift_978:67/70, Adrift_980:24/27/30,
 * 2026-09-12.)  The surface row is the same in its own preposition -- `put
 * lamp on table` with the table a room away is "Put the lamp onto what?",
 * `put stone on table` with both a room away is "You can't do that!", and so
 * is `put coin on zzzz` with the coin unseen on the table (p39SURF,
 * Adrift_998:5/7 and Adrift_992:15, 2026-09-12).  The
 * object's failure outranks the container's, which is why the noun is
 * resolved here before the prompt is printed.  4.0 has its own answers for
 * the same shapes, up in lib_cmd_put_container_400(); the prompt is composed
 * from pieces every Runner holds, so the census cannot date it and it is
 * gated at 3.90 -- see lib_put_what_pre400().
 *
 * A table of its own, run after STANDARD_COMMANDS has had its go so that a
 * line naming a real container never reaches it; see STANDARD_PUT_COMMANDS.
 */
/*
 * lib_instr()
 * lib_put_co_named_term()
 * lib_put_co_reachable()
 * lib_put_co_refusal_pre390()
 *
 * What run380's insides() (444BAC-447553, entry 44755C) answers for a put line
 * its container fragment could not resolve.  It does not parse fragments at
 * all.  It walks every object with co() -- Short or alias anywhere in the
 * line, wherever the object is -- counting matches in var_A6 and choosing one
 * in var_A8 (4457A1-4459C8):
 *
 *   - while nothing chosen so far is REACHABLE (var_108: a static in the room,
 *     or a dynamic object in the room, held, or worn), each match simply
 *     replaces the choice, so the last match in index order wins;
 *   - once one is, a later match replaces it only when its Short or alias
 *     starts further right in the line than the choice's.
 *
 * Fewer than two matches is the flat "You can't do that!" (445A2A).  Two or
 * more, and the choice is not a container or surface, is "You can't put
 * anything on|inside " & tense(Prefix) & " " & Short & "." with `on` chosen by
 * c("on") (446428); a dynamic container not held is "You are not holding
 * <Prefix> <Short>." and a static one not here "You can't see <Prefix>
 * <Short>.".
 *
 * Measured on cave.taf (3.80) up the tree, nothing referenced present
 * (Adven_1_cave.rtf): `put raft in water` (river water 18, raft 42, pool water
 * 70) is "You can't put anything inside the pool water." (turn 52), and `put
 * amulet on table` (table 48, amulet 64) is "You can't put anything on the
 * star shaped amulet." (turn 212) -- the object, not the table, because the
 * unreachable table never pinned the choice.
 *
 * Returns TRUE when it printed; FALSE leaves the line to the flat refusal,
 * which is also what it answers for the closed and the reachable-container
 * arms, both unmeasured from here.
 */
static scr_int
lib_instr (const scr_char *line, const scr_char *term)
{
  scr_int length, index_;

  if (!line || !term || term[0] == NUL)
    return 0;
  length = strlen (term);
  for (index_ = 0; line[index_] != NUL; index_++)
    {
      if (scr_strncasecmp (line + index_, term, length) == 0)
        return index_ + 1;
    }
  return 0;
}

static const scr_char *
lib_put_co_alias (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];

  return lib_first_alias (bundle, vt_key, "Objects", object);
}

static scr_bool
lib_put_co_named_term (scr_gameref_t game, const scr_char *line,
                       scr_int object)
{
  const scr_char *shortname, *alias;

  shortname = prop_get_indexed_string (gs_get_bundle (game),
                                       "Objects", object, "Short");
  if (lib_co_contains (line, shortname))
    return TRUE;
  alias = lib_put_co_alias (game, object);
  return alias && alias[0] != NUL && lib_co_contains (line, alias);
}

static scr_bool
lib_put_co_reachable (scr_gameref_t game, scr_int object)
{
  const scr_int position = gs_object_position (game, object);

  if (obj_directly_in_room (game, object, gs_playerroom (game)))
    return TRUE;
  return !obj_is_static (game, object)
         && (position == OBJ_HELD_PLAYER || position == OBJ_WORN_PLAYER);
}

/* insides()' "further right" test: either name of I past either name of
 * the choice, the choice's name found.  3.9 also wants I's name past PAST,
 * the preposition's position; 3.7/3.8 pass 0. */
static scr_bool
lib_put_co_further_right (scr_gameref_t game, const scr_char *line,
                          scr_int object, scr_int chosen, scr_int past)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *names[2], *chosen_names[2];
  scr_int name, chosen_name;

  names[0] = prop_get_indexed_string (bundle, "Objects", object, "Short");
  names[1] = lib_put_co_alias (game, object);
  chosen_names[0] = prop_get_indexed_string (bundle, "Objects", chosen,
                                             "Short");
  chosen_names[1] = lib_put_co_alias (game, chosen);

  for (chosen_name = 0; chosen_name < 2; chosen_name++)
    {
      const scr_int chosen_at =
          lib_instr (line, chosen_names[chosen_name]);

      if (chosen_at == 0)
        continue;
      for (name = 0; name < 2; name++)
        {
          const scr_int at = lib_instr (line, names[name]);

          if (at > chosen_at && at > past)
            return TRUE;
        }
    }
  return FALSE;
}

static scr_bool
lib_put_co_refusal_pre390 (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *line = run_get_dispatch_input ();
  scr_int object, matches, chosen;
  scr_bool pinned;

  if (!line || !lib_co_contains (line, "put"))
    return FALSE;

  matches = 0;
  chosen = -1;
  pinned = FALSE;
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (!lib_put_co_named_term (game, line, object))
        continue;

      matches++;
      if (chosen == -1 || !pinned)
        {
          chosen = object;
          pinned = lib_put_co_reachable (game, object);
        }
      else if (lib_put_co_further_right (game, line, object, chosen, 0))
        chosen = object;
    }
  if (matches < 2)
    return FALSE;

  if (!obj_is_container (game, chosen) && !obj_is_surface (game, chosen))
    {
      const scr_bool on = lib_co_contains (line, "on");

      lib_print_response_object (game,
                                 on ? "You can't put anything on "
                                    : "You can't put anything inside ",
                                 on ? "I can't put anything on "
                                    : "I can't put anything inside ",
                                 on ? "%player% can't put anything on "
                                    : "%player% can't put anything inside ",
                                 chosen, ".\n");
      return TRUE;
    }

  if (!obj_is_static (game, chosen)
      && gs_object_position (game, chosen) != OBJ_HELD_PLAYER)
    {
      pf_buffer_string (filter,
                        lib_select_response (game, "You are not holding ",
                                             "I am not holding ",
                                             "%player% is not holding "));
      lib_print_object_raw (game, chosen);
      pf_buffer_string (filter, ".\n");
      return TRUE;
    }
  if (obj_is_static (game, chosen)
      && !obj_directly_in_room (game, chosen, gs_playerroom (game)))
    return lib_cant_see_named_pre_390 (game, chosen, FALSE, ".\n");

  return FALSE;
}

static scr_bool
lib_put_nowhere_common (scr_gameref_t game, scr_bool typed_on)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  std::string named;
  scr_int object;
  scr_bool is_ambiguous;

  if (lib_is_version_400 (game))
    return FALSE;

  /* Take a copy; the match below rewrites the referenced text. */
  named = var_get_ref_text (vars);

  /* What "put all in/on <nothing>" answers before 4.0 is unmeasured. */
  if (uip_match ("[all/everything]", named.c_str (), game))
    return FALSE;

  /*
   * 3.7 and 3.8 have no composed prompt to reach for, and no interest in
   * what the object fragment named: a container fragment that names nothing
   * anywhere in the game ends the line flat.  `put coin in zzzz` with the
   * coin lying in the box (Adrift_983:72 / Adrift_987:72) and `put coin in
   * me` with the coin held (Adrift_982:66 / Adrift_986:66) both answer "You
   * can't do that!", 2026-09-12.  Note that a fragment naming a container
   * the player merely cannot see no longer arrives here at all -- those are
   * resolved over the whole game now; see lib_put_target_pre390().
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return lib_put_no_object_pre400 (game);

  if (!uip_match ("%object%", named.c_str (), game))
    return lib_put_no_object_pre400 (game);

  object = lib_disambiguate_object (game, "put", &is_ambiguous);
  if (object == -1)
    return is_ambiguous ? TRUE : lib_put_no_object_pre400 (game);

  return lib_put_what_pre400 (game, object, typed_on);
}

scr_bool
lib_cmd_put_in_nowhere (scr_gameref_t game)
{
  return lib_put_nowhere_common (game, FALSE);
}

scr_bool
lib_cmd_put_on_nowhere (scr_gameref_t game)
{
  return lib_put_nowhere_common (game, TRUE);
}


/*
 * lib_cmd_put_in_that_390()
 * lib_cmd_put_on_that_390()
 *
 * run390 insides() counts, over the whole object table, every object co()
 * finds named in the line (var_8A, 461000-461641), and separately notes the
 * one named after the preposition (var_8C).  With two or more named, no
 * "all", and none of them after the preposition, it writes "You can't put
 * anything inside that!" / "onto that!" (461769) before any task look-up,
 * and the post-put sweep follows (4624BF -> 462550): a claimant named in the
 * line runs the tasks QUIET, so a failing task leaves the refusal standing.
 * pPUTREF39.taf (make_39_putrefprobe.py), run390x, 2026-09-19: `put pebble
 * and stone in junk` / `on junk`, each with a matching task whose restriction
 * fails, answer "You can't put anything inside that!" / "onto that!"
 * (Adrift_pputref392.txt:8/12), where Scarier printed the FailMessage.
 *
 * The target is insides()' own choice (461000-461641), 3.8's loop (see
 * lib_put_co_refusal_pre390()) with one more test: an object's Short or
 * alias must first appear in the line after InStr(line, Left(var_E0, 2)),
 * where var_E0 is "onto" when the line holds the whole word on or onto and
 * "inside" otherwise.  That InStr is a raw substring search, so the "in"
 * inside "coin" or the "on" inside "stone" counts as the preposition:
 * `put coin and stone in junk` chooses the stone and answers "You can't put
 * anything inside the stone." (pPUTREF39 T4, Adrift_pputref39.txt:8), which
 * is the wrong-kind refusal of lib_put_in_is_valid().  A choice that is a
 * valid open container or surface would put the other object into it; that
 * arm is unmeasured and declines.  (The Runner tests the chooser's own name
 * against the preposition with an un-LCased Short, so a capitalised Short
 * may never displace a pinned choice; also unmeasured, not modelled.)
 *
 * These rows sit in the priority table so as to answer ahead of the tasks;
 * anything that is not this case declines to the rows below and to the
 * put-nowhere catch-alls, and 3.7, 3.8 and 4.0 never get here at all.  A
 * line whose whole-word preposition is followed by an object name is left to
 * the ordinary rows.
 */

static scr_int
lib_put_target_390 (scr_gameref_t game, const scr_char *line)
{
  const scr_bool on = lib_co_contains (line, "on")
                      || lib_co_contains (line, "onto");
  const scr_int past = lib_instr (line, on ? "on" : "in");
  scr_int object, chosen;
  scr_bool pinned;

  chosen = -1;
  pinned = FALSE;
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (!lib_put_co_named_term (game, line, object))
        continue;

      if (chosen == -1 || !pinned)
        {
          const scr_char *const shortname =
              prop_get_indexed_string (gs_get_bundle (game),
                                       "Objects", object, "Short");

          if (lib_instr (line, shortname) > past
              || lib_instr (line, lib_put_co_alias (game, object))
                 > past)
            {
              chosen = object;
              pinned = lib_put_co_reachable (game, object);
            }
        }
      else if (lib_put_co_further_right (game, line, object, chosen, past))
        chosen = object;
    }
  return chosen;
}

static scr_bool
lib_put_that_390 (scr_gameref_t game, scr_bool typed_on)
{
  const scr_char *line = run_get_dispatch_input ();
  const scr_char *const in_words[] = {"into", "inside", "in", NULL};
  const scr_char *const on_words[] = {"onto", "on", NULL};
  const scr_char *const *words = typed_on ? on_words : in_words;
  std::string lower, tail;
  size_t cut;
  scr_int object, named, chosen;

  if (!lib_put_refusal_first_390 (game) || !line)
    return FALSE;

  /*
   * c("all") skips the two-names test (461646) but not the target choice:
   * with no object named past the preposition var_8C stays -1 and insides()
   * refuses at 461769 like any other line.  pPUTFULL39 `put all in junk` /
   * `put all on junk` are "You can't put anything inside that!" / "... onto
   * that!" (run390x Adrift_pputfull39.txt, 2026-09-19).  A named target is
   * left to the "put all" rows.  generaltasks has already made "everything"
   * "all" (Replace at 45F225), so `put everything in zzz` / `on zzz` refuse
   * the same way (p39PUT, run390x Adrift_160_p39drop.txt, 2026-09-19).
   */
  if (lib_co_contains (line, "all") || lib_co_contains (line, "everything"))
    {
      if (lib_put_target_390 (game, line) != -1)
        return FALSE;
      goto refuse;
    }

  named = 0;
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (lib_put_co_named_term (game, line, object))
        named++;
    }
  if (named < 2)
    return FALSE;

  /* The text after the first whole-word preposition must name nothing. */
  lower = line;
  for (char &c : lower)
    c = scr_tolower (c);
  cut = std::string::npos;
  for (const scr_char *const *word = words; *word; word++)
    {
      const std::string needle = std::string (" ") + *word + " ";
      const size_t at = (" " + lower + " ").find (needle);

      if (at != std::string::npos && (cut == std::string::npos || at < cut))
        cut = at + needle.length () - 1;
    }
  if (cut == std::string::npos || cut > lower.length ())
    return FALSE;
  tail = lower.substr (cut);
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (lib_put_co_named_term (game, tail.c_str (), object))
        return FALSE;
    }

  /* A mid-word preposition can still leave the Runner a target. */
  chosen = lib_put_target_390 (game, lower.c_str ());
  if (chosen != -1)
    {
      const scr_bool on = lib_co_contains (line, "on")
                          || lib_co_contains (line, "onto");

      scr_bool status;

      if (!(on ? lib_put_on_is_valid (game, chosen)
               : lib_put_in_is_valid (game, chosen)))
        {
          lib_put_refusal_swept = FALSE;
          return TRUE;
        }

      gs_clear_multiple_references (game);
      lib_put_mid_word_390 = TRUE;
      status = lib_put_named_pre400 (game, chosen, on);
      lib_put_mid_word_390 = FALSE;
      return status;
    }

refuse:
  {
    const size_t from = pf_buffer_length (gs_get_filter (game));

    lib_print_response_message (game,
                                 typed_on
                                 ? "You can't put anything onto that!\n"
                                 : "You can't put anything inside that!\n",
                                 typed_on
                                 ? "I can't put anything onto that!\n"
                                 : "I can't put anything inside that!\n",
                                 typed_on
                                 ? "%player% can't put anything onto that!\n"
                                 : "%player% can't put anything inside that!\n");
    lib_put_sweep_390 (game, line, from);
  }
  return TRUE;
}

scr_bool
lib_cmd_put_in_that_390 (scr_gameref_t game)
{
  return lib_put_that_390 (game, FALSE);
}

scr_bool
lib_cmd_put_on_that_390 (scr_gameref_t game)
{
  return lib_put_that_390 (game, TRUE);
}


/*
 * lib_cmd_put_in_except_multiple()
 * lib_cmd_put_in_multiple()
 *
 * Facets of lib_put_in_multiple_common().
 *
 * Before 4.0 insides() has no exception list at all: "except" and "but"
 * appear only in the take and drop handlers (run380 43D8EE / 438793, run390
 * drops 4456AB; run370 has neither word), so `put all except X in Y` is
 * c("all")'s arm and puts everything.  p37PUT / p38PUT / p39PUT `put all
 * except stone in cupboard` put the stone too (run370x / run380x / run390x
 * Adrift_154_p37put2.rtf, Adrift_155_p38put2.rtf, Adrift_154_p39put.txt,
 * 2026-09-19).  Deliberate deviation: Scarier keeps the exception at every
 * version, in and on alike.
 */
scr_bool
lib_cmd_put_in_except_multiple (scr_gameref_t game)
{
  return lib_put_in_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_put_in_multiple (scr_gameref_t game)
{
  return lib_put_in_multiple_common (game, FALSE);
}


/*
 * lib_check_put_on_recursion()
 *
 * Checks for infinite recursion when placing an object on an object.  Returns
 * TRUE if no recursion detected.
 */
static scr_bool
lib_check_put_on_recursion (scr_gameref_t game,
                            scr_int object, scr_int supporter, scr_bool report)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int check;

  /* Avoid the obvious possibility of infinite recursion. */
  if (supporter == object)
    {
      if (report)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                  "You can't put an object onto itself!",
                                  "I can't put an object onto itself!",
                                  "%player% can't put an object onto itself!"));
        }
      return FALSE;
    }

  /* Avoid the subtle possibility of infinite recursion. */
  check = supporter;
  while (gs_object_position (game, check) == OBJ_ON_OBJECT
         || gs_object_position (game, check) == OBJ_IN_OBJECT)
    {
      check = gs_object_parent (game, check);
      if (check == object)
        {
          if (report)
            {
              pf_buffer_string (filter,
                                lib_select_response (game,
                                      "You can't put an object onto one",
                                      "I can't put an object onto one",
                                      "%player% can't put an object onto one"));
              pf_buffer_string (filter, " it's on or in!");
            }
          return FALSE;
        }
    }

  /* No infinite recursion detected. */
  return TRUE;
}


/*
 * lib_put_on_backend()
 *
 * Common backend handler for placing objects on supporters.  Places all
 * objects currently referenced in the game onto a supporter, trying game
 * commands first, and then moving other unhandled objects onto the supporter.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */
static lib_put_outcome_t
lib_put_on_backend (scr_gameref_t game, scr_int supporter,
                    scr_bool is_all_form)
{
  const scr_bool is_pre_390 =
      prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390;
  lib_move_verb_t verb;
  scr_int object_count, object, length_before, length_after_tasks;
  scr_bool has_printed, task_claimed, recursion_rejected;
  lib_put_outcome_t outcome;
  lib_list_t pending;

  /*
   * Try game commands for all referenced objects first.  If any succeed,
   * remove that reference from the list.  At the same time, check for and
   * weed out any moves that result in infinite recursion.
   */
  length_before = lib_output_length (game);
  lib_put_announce_bytes = 0;
  has_printed = FALSE;
  task_claimed = FALSE;
  recursion_rejected = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      /*
       * Version 4.0 picks up an object it has been asked to put down, and it
       * does so BEFORE the handler's task look-up.  run400's name_object
       * loop runs the take piece at loc_46E2B5 and only then hands the pair
       * to insides (Proc_19_43_46639C @loc_46E34F), which is where the
       * canonical line reaches the tasks -- so a task that goes on to claim
       * the put still carries the announcement ahead of its own text.
       * Measured on frustrated turns 53-55 (Adrift_274_frustrated.txt):
       * `put small rock on left pan` matches task 511 `put*small*left*` and
       * still opens "(Taking the small rock first)".
       */
      {
        scr_bool take_printed = FALSE;

        lib_put_implicit_take (game, object, supporter, &take_printed);
        has_printed |= take_printed;
      }

      /* The tasks' turn; see lib_put_in_backend(). */
      if (lib_is_version_400 (game))
        {
          pending.push_back (object);
          continue;
        }

      /* Reject and remove attempts to place objects on themselves; the
       * 4.0 test sits after the deferred task pass (see lib_put_in_backend). */
      if (!lib_check_put_on_recursion (game, object, supporter, !has_printed))
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
          recursion_rejected = TRUE;
          continue;
        }

      if (lib_try_game_command_with_object (game,
                                            "put", object, "on", supporter))
        {
          game->object_references[object] = FALSE;
          game->multiple_references[object] = FALSE;
          has_printed = TRUE;
          task_claimed = TRUE;
          continue;
        }
    }

  /*
   * name_object's own close to the take phase, and then the look-ups it
   * deferred.  run400 takes every named piece first and hands the pair to
   * `insides` only afterwards (@46E34F), so the report above comes out
   * ahead of the first task: probe PSTAT command 12, `put slab in box`
   * with the slab a static and the inventory empty, reads "(Taking the
   * slab first)" / "You can't take the slab!  You are carrying nothing!
   * SLABTASK."
   */
  if (!recursion_rejected && lib_put_nothing_carried_400 (game, has_printed))
    has_printed = TRUE;

  for (const scr_int pending_object : pending)
    {
      if (lib_try_game_command_with_object_400 (game, "put", pending_object,
                                                "on", supporter))
        {
          game->object_references[pending_object] = FALSE;
          game->multiple_references[pending_object] = FALSE;
          has_printed = TRUE;
          task_claimed = TRUE;
        }
    }

  /* The typed line, when the rebuild missed; see
     lib_try_typed_put_line_400(). */
  if (!task_claimed && pending.size () == 1
      && lib_try_typed_put_line_400 (game))
    {
      game->object_references[pending.front ()] = FALSE;
      game->multiple_references[pending.front ()] = FALSE;
      has_printed = TRUE;
      task_claimed = TRUE;
    }

  /*
   * 4.0's self-supporter test, after the take phase, the tasks() call and
   * the possession test, as insides orders it; see lib_put_in_backend,
   * which is the measured side of the same handler (probe PBOXBOX).  The
   * wording at 46600C is the surface one whenever the target is a surface
   * and not also a container.
   */
  for (const scr_int pending_object : pending)
    {
      if (!game->object_references[pending_object])
        continue;

      if (!lib_check_put_on_recursion (game, pending_object, supporter,
                                       !has_printed))
        {
          game->object_references[pending_object] = FALSE;
          has_printed = TRUE;
          recursion_rejected = TRUE;
        }
    }
  length_after_tasks = lib_output_length (game);

  /*
   * "onto" is 3.9's word.  run370 and run380 write " on ": `put stone on
   * table` with the table in hand is "You put the stone on the table."
   * (Adrift_1003:15, Adrift_999:15, 2026-09-12), against run390's "You put
   * the stone onto the table." (Adrift_990:15).  The all form also prints
   * its list with the objects' own prefixes there, the same way the pre-3.9
   * "put all in" does: `put all on table` is "You put an torch and a lamp on
   * the table." (Adrift_1006:5, Adrift_1002:5) where the named form says
   * "the".  See lib_put_in_backend() for the container twin.
   */
  verb = LIB_PUT_ON_VERB;
  if (is_pre_390)
    verb.onto = " on ";

  lib_move_backend (game, &verb, supporter, has_printed);

  outcome.is_refusal_only = FALSE;
  outcome.is_silent = lib_output_length (game) == length_before;
  outcome.is_tasks_only = task_claimed
                          && lib_output_length (game) == length_after_tasks;
  outcome.is_announce_only = task_claimed && lib_put_announce_bytes > 0
                             && (size_t) (lib_output_length (game)
                                          - length_before)
                                == lib_put_announce_bytes;
  return outcome;
}


/*
 * lib_put_on_filter()
 * lib_put_on_not_supporter_filter()
 *
 * Helper functions for deciding if an object may be put on another this
 * context.  Returns TRUE if an object may be manipulated, FALSE otherwise.
 * The named form is much the wider of the two; see lib_put_named_filter().
 */
static scr_bool
lib_put_on_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return lib_put_named_filter (game, object);
}

/* The resolver twin of the above; see lib_put_resolve_filter(). */
static scr_bool
lib_put_on_resolve_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return lib_put_resolve_filter (game, object);
}

static scr_bool
lib_put_on_not_supporter_filter (scr_gameref_t game,
                                 scr_int object, scr_int supporter)
{
  return lib_put_all_filter (game, object, supporter);
}


/*
 * lib_put_on_is_valid()
 *
 * Validate the supporter requested in "put on" commands.  The twin of
 * lib_put_in_is_valid(), and split by version in the same two places.
 */
static scr_bool
lib_put_on_is_valid (scr_gameref_t game, scr_int supporter)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_bool is_pre_390 =
      prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390;

  /* Verify that the supporter object is a supporter. */
  if (!obj_is_surface (game, supporter))
    {
      const scr_bool is_first = lib_put_refusal_first_390 (game);
      const size_t from = pf_buffer_length (filter);

      /* Deferred in the tentative priority pass; see lib_put_in_is_valid. */
      if (!is_first && run_priority_defer_if_active ())
        return FALSE;
      /*
       * Three generations, three spellings, and Scarier had none of them: it
       * said "on" with 4.0's exclamation mark.  3.7 and 3.8 carry the whole
       * literal " can't put anything on "; 3.9 composes " can't put anything "
       * with "onto" (the census finds bare "onto" in run390 and run400 only,
       * and the run370/run380 pools hold nothing but "Mapontop"); and only
       * 4.0 shouts.  `put coin on stone` on the surface probe, 2026-09-12:
       *
       *   run370  You can't put anything on the stone.    Adrift_1003:14
       *   run380  You can't put anything on the stone.    Adrift_999:14
       *   run390  You can't put anything onto the stone.  Adrift_990:14
       *   run400  You can't put anything onto the stone!  Adrift_991:14
       */
      lib_print_response_object (game,
                                 is_pre_390 ? "You can't put anything on "
                                            : "You can't put anything onto ",
                                 is_pre_390 ? "I can't put anything on "
                                            : "I can't put anything onto ",
                                 is_pre_390
                                   ? "%player% can't put anything on "
                                   : "%player% can't put anything onto ",
                                 supporter,
                                 lib_is_version_400 (game) ? "!\n" : ".\n");
      if (is_first)
        lib_put_refusal_sweep_390 (game, from);
      return FALSE;
    }

  /*
   * And below 3.9 a surface has to be in the player's hands, exactly as a
   * container does -- one handler, one test.  With the table left standing
   * in the cave, `put lamp on table`, `put stone on table` and `put all on
   * table` all answer "You are not holding a table." on run370 and run380
   * (Adrift_1005:5/7, Adrift_1001:5/7, Adrift_1014:3, Adrift_1013:3,
   * 2026-09-12) -- the object's own prefix, not the "the" of most refusals.
   * Statics are exempt, and have to be: `put stone on bench`, the bench a
   * static surface on the cave floor, is "You put the stone on the bench."
   * on both (Adrift_1004:12, Adrift_1000:12).
   *
   * 3.9 and 4.0 have no such test.  The same `put all on table` with the
   * table on the floor is "You put the torch, the lamp, the stone, the
   * pebble and the box onto the table." on run390 (Adrift_1009:3), and
   * run400 moves onto it too (Adrift_1016:6).
   */
  if (lib_put_static_absent_pre390 (game, supporter))
    return FALSE;
  if (is_pre_390
      && obj_uses_burden_model (game)
      && !obj_is_static (game, supporter)
      && gs_object_position (game, supporter) != OBJ_HELD_PLAYER)
    {
      if (run_priority_defer_if_active ())
        return FALSE;
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are not holding ",
                                             "I am not holding ",
                                             "%player% is not holding "));
      lib_print_object (game, supporter);
      pf_buffer_string (filter, ".\n");
      return FALSE;
    }

  /* Surface is a valid target for "put on". */
  return TRUE;
}


/*
 * lib_put_on_multiple_common()
 *
 * Put the objects held by the player and listed in %text% onto an object,
 * or -- for is_except -- every one of them but those listed.  As with
 * putting in, the except form filters the supporter itself out of the list.
 */
static scr_bool
lib_put_on_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  scr_int supporter, objects, references;
  scr_bool is_ambiguous;
  lib_put_outcome_t outcome;

  /* Get the referenced object, and if none, consider complete. */
  supporter = is_except
              ? lib_disambiguate_object (game, "put that onto", &is_ambiguous)
              : lib_put_target_pre390 (game, "put that onto", &is_ambiguous);
  if (supporter == -1)
    return is_ambiguous;

  /* Pre-4.0 names the object first, and answers for it first. */
  if (!is_except && !lib_is_version_400 (game))
    return lib_put_named_pre400 (game, supporter, TRUE);

  /* Parse the multiple objects list to find the target objects; the 4.0
   * noun goes through name_object's resolver first (lib_put_named_400). */
  lib_put_admission_guard_t admission_guard;
  references = -1;
  if (!is_except && lib_put_named_400 (game, &references))
    return TRUE;
  if (references == 1)
    ;
  else if (!lib_parse_multiple_objects (game, is_except ? "retain" : "move",
                                        is_except
                                        ? lib_put_on_not_supporter_filter
                                        : lib_put_on_resolve_filter,
                                        is_except ? supporter : -1,
                                        &references))
    return FALSE;
  else if (references == 0)
    return TRUE;

  /* 4.0's take phase speaks before the supporter is examined at all. */
  if (!is_except && lib_put_already_on_400 (game, supporter))
    return TRUE;

  /* Validate the supporter object to put into. */
  if (!lib_put_on_is_valid (game, supporter))
    return lib_put_invalid_result ();

  /* As a special case, complain about requests to retain the supporter. */
  if (is_except
      && lib_multiple_retains_associate (game, supporter, "retain"))
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              is_except ? lib_put_on_not_supporter_filter
                                        : lib_put_on_filter,
                              is_except ? supporter : -1, is_except,
                              &references);
  outcome = {};
  if (objects > 0 || references > 0)
    outcome = lib_put_on_backend (game, supporter, is_except);
  else
    lib_print_nothing_held (game, FALSE, is_except && objects == 0, ".");

  return lib_put_on_finish (game, outcome);
}


/*
 * lib_cmd_put_on_except_multiple()
 * lib_cmd_put_on_multiple()
 *
 * Facets of lib_put_on_multiple_common().
 */
scr_bool
lib_cmd_put_on_except_multiple (scr_gameref_t game)
{
  return lib_put_on_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_put_on_multiple (scr_gameref_t game)
{
  return lib_put_on_multiple_common (game, FALSE);
}

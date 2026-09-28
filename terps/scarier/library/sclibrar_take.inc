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
 * Carrying capacity, "count", and taking objects.
 */

/*
 * lib_carried_burden()
 *
 * Return the total burden of everything the player is currently holding, under
 * the version 3.8 pooled model (see obj_get_burden()).
 *
 * Worn objects are free.  Wearing something does not spend any of MaxCarried,
 * and neither does keeping it on -- measured with a MaxCarried 2 probe holding
 * a wearable and two others: run370 and run380 both accept all three, and
 * "count" reports the two held (pworn/qworn, 2026-08-23).  Taking anything off
 * therefore costs a slot again.
 *
 * Always computed afresh, and that is faithful: unlike 4.0's running size and
 * weight totals, the pooled burden does not drift.  A container is not charged
 * for its contents and wearing is not a charge at all, so neither of the two
 * shapes of 4.0 leak has anything to leak here -- confirmed against both
 * Runners with the drop-a-loaded-container and wear-then-drop sequences
 * (pleakc/pleakw and their 3.70 twins).  This is why "glk capacity" is a no-op
 * for a 3.7/3.8 game: there is no Runner arithmetic to escape.  It is a no-op
 * for a 3.9 game too, for the same reason by a different route -- run390 keeps
 * a running total but keeps it exactly; see obj_uses_running_load().
 */
static scr_int
lib_carried_burden (scr_gameref_t game)
{
  scr_int index_, burden = 0;

  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (gs_object_position (game, index_) == OBJ_HELD_PLAYER)
        burden += obj_get_burden (game, index_);
    }

  return burden;
}


/*
 * lib_carried_size()
 * lib_carried_weight()
 *
 * The player's current carried size and weight as the capacity checks see
 * them.  For a 4.0 game this is by default the Runner's running total
 * (gs_carried_*, with its take/drop double-count); otherwise, and in legacy
 * mode, it is recomputed afresh from what is currently held or worn.
 *
 * Only 4.0 keeps a running total worth reproducing: run390 adjusts the same
 * two globals but does so exactly, so recomputing is what a 3.9 game looks
 * like from the outside -- see obj_uses_running_load() for the probe that
 * separates the two.  Legacy mode also drops the Runner's stale parent rule
 * from obj_weigh(), so it is free of the phantom weight too; a 3.9 game gets
 * that for the same reason, having no running total to poison.
 */
static scr_int
lib_carried_size (scr_gameref_t game)
{
  scr_int index_, size;

  if (obj_uses_running_load (game) && !game->capacity_recompute)
    return gs_carried_size (game);

  size = 0;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (gs_object_position (game, index_) == OBJ_HELD_PLAYER
          || gs_object_position (game, index_) == OBJ_WORN_PLAYER)
        size += obj_get_size (game, index_);
    }

  return size;
}

static scr_int
lib_carried_weight (scr_gameref_t game)
{
  scr_int index_, weight;

  if (obj_uses_running_load (game) && !game->capacity_recompute)
    return gs_carried_weight (game);

  weight = 0;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (gs_object_position (game, index_) == OBJ_HELD_PLAYER
          || gs_object_position (game, index_) == OBJ_WORN_PLAYER)
        weight += obj_get_weight (game, index_);
    }

  return weight;
}


/*
 * lib_cmd_count()
 *
 * Display player weight and size limits and amounts currently carried.
 */
scr_bool
lib_cmd_count (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int size, weight;

  /*
   * A version 3.8 game has neither of these axes, and its "count" is a single
   * unlabelled line counting objects rather than the two tab-stopped Size and
   * Weight lines below: "You have 0 objects.  The most you can hold is 2."
   * There is no singular -- one object still reads "1 objects" -- and the
   * first person is "I have ... The most I can hold is ...".  Measured on
   * run380 with the pcount/pcount1 probes, 2026-08-23.
   */
  if (obj_uses_burden_model (game))
    {
      pf_buffer_string (filter,
                        lib_select_response (game, "You have ", "I have ",
                                             "%player% have "));
      pf_buffer_integer (filter, lib_carried_burden (game));
      pf_buffer_string (filter, " objects");
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             ".  The most you can hold is ",
                                             ".  The most I can hold is ",
                                             ".  The most %player_pronoun% can hold is "));
      pf_buffer_integer (filter, obj_get_player_burden_limit (game));
      pf_buffer_string (filter, ".\n");

      lib_set_admin (game);
      return TRUE;
    }

  /* Report the same carried totals the capacity checks use. */
  size = lib_carried_size (game);
  weight = lib_carried_weight (game);

  /*
   * Print the player limits and amounts used, in the Runner's own layout --
   * a tab stop after each label (two tabs for the shorter "Size:") and the
   * game's perspective for the pronouns: "Size:\t\tI have 0.  The most I
   * can hold is 108." (run400, goldilocks/iachini transcripts 2026-08-22).
   */
  pf_buffer_string (filter, "Size:\t\t");
  pf_buffer_string (filter,
                    lib_select_response (game, "You have ", "I have ",
                                         "%player% have "));
  pf_buffer_integer (filter, size);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         ".  The most you can hold is ",
                                         ".  The most I can hold is ",
                                         ".  The most %player_pronoun% can hold is "));
  pf_buffer_integer (filter, obj_get_player_size_limit (game));
  pf_buffer_string (filter, ".\n");

  pf_buffer_string (filter, "Weight:\t");
  pf_buffer_string (filter,
                    lib_select_response (game, "You have ", "I have ",
                                         "%player% have "));
  pf_buffer_integer (filter, weight);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         ".  The most you can hold is ",
                                         ".  The most I can hold is ",
                                         ".  The most %player_pronoun% can hold is "));
  pf_buffer_integer (filter, obj_get_player_weight_limit (game));
  pf_buffer_string (filter, ".\n");

  lib_set_admin (game);
  return TRUE;
}


/*
 * lib_object_too_heavy()
 *
 * Return TRUE if the given object is too heavy for the player to carry.
 */
static scr_bool
lib_object_too_heavy (scr_gameref_t game, scr_int object)
{
  scr_int player_limit, weight, object_weight;

  /*
   * Version 3.8 has no weight axis, and no "too heavy" refusal to go with one
   * -- its single pooled burden is checked in lib_object_too_large() below,
   * and answers "Your hands are full." to everything it refuses.
   */
  if (obj_uses_burden_model (game))
    return FALSE;

  /* Get the player limit and the given object weight. */
  player_limit = obj_get_player_weight_limit (game);
  object_weight = obj_get_weight (game, object);

  /* Establish the player's current carried weight. */
  weight = lib_carried_weight (game);

  /* Return TRUE if the new object exceeds limit. */
  return weight + object_weight > player_limit;
}


/*
 * lib_print_too_heavy()
 *
 * Print the weight refusal for one object, into a clause the caller has
 * already opened.
 *
 * The whole sentence is a 4.0 rewording.  run390 answers a fixed "That is too
 * heavy for you to carry." -- it names no object, and it never qualifies the
 * refusal.  run400 names the object and always qualifies it: "The lump is too
 * heavy for Player to carry at the moment."  Both were measured on the p39wt
 * probe (2026-08-23), one Runner each, against a brick that fits in empty
 * hands and a lump that does not: each Runner gave its single shape for both,
 * so the "at the moment" is not the portability hedge SCARE took it for.
 * Neither Runner has a plural form -- there is no "are too heavy" string in
 * either binary.
 *
 * A 3.7 or 3.8 game never reaches here: the pooled burden has no weight axis
 * and lib_object_too_heavy() stands down for it.
 */
static void
lib_print_too_heavy (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
    {
      pf_buffer_string (filter,
                        lib_select_response
                          (game,
                           "That is too heavy for you to carry.",
                           "That is too heavy for me to carry.",
                           "That is too heavy for %player% to carry."));
      return;
    }

  lib_print_object_np (game, object);
  pf_buffer_string (filter,
                    lib_select_response
                      (game,
                       " is too heavy for you to carry at the moment.",
                       " is too heavy for me to carry at the moment.",
                       " is too heavy for %player% to carry at the moment."));
}


/*
 * lib_object_too_large()
 *
 * Return TRUE if the given object is too large for the player to carry.
 */
static scr_bool
lib_object_too_large (scr_gameref_t game, scr_int object)
{
  scr_int player_limit, size, object_size;

  /*
   * Version 3.8's pooled burden against its plain MaxCarried limit, which
   * replaces both 4.0 axes outright: every class costs at least 1, so the
   * burden is never looser than the object count the size axis would have
   * enforced against normalised 3.8 objects.
   */
  if (obj_uses_burden_model (game))
    {
      scr_int object_burden = obj_get_burden (game, object);

      player_limit = obj_get_player_burden_limit (game);

      return lib_carried_burden (game) + object_burden > player_limit;
    }

  /* Get the player limit and the given object size. */
  player_limit = obj_get_player_size_limit (game);
  object_size = obj_get_size (game, object);

  /* Establish the player's current carried size. */
  size = lib_carried_size (game);

  /* Return TRUE if the new object exceeds limit. */
  return size + object_size > player_limit;
}


/*
 * lib_take_over_capacity()
 *
 * Return TRUE if taking the object would put the player over a carrying
 * limit, setting *is_size to say which refusal applies.
 *
 * 4.0's per-object take (run400 get_piece_inner 46302C) tests size first --
 * the object's own size (44) on top of the running total, "<Your> hands are
 * full." at 462ECD -- and only then weight, recursive over contents (447680),
 * "<The X> is too heavy..." at 462F62.  It tests both whatever holds the
 * object, a container the player carries included: the contents are already
 * in both running totals, so a take out of a carried bag counts them twice.
 * wilkins `take times` at 99/90 on both axes answers "My hands are full."
 * (Adrift_850 line 84), businessasusual `take all` with limits 9/9 and every
 * object 9/9 takes one and refuses the next by size, and provenance
 * `get rope` out of the carried rucksack is refused by weight.
 *
 * Earlier versions test weight first and skip objects inside or on something
 * the player already holds.
 */
static scr_bool
lib_take_over_capacity (scr_gameref_t game, scr_int object, scr_bool *is_size)
{
  if (lib_is_version_400 (game))
    {
      *is_size = lib_object_too_large (game, object);
      return *is_size || lib_object_too_heavy (game, object);
    }

  if ((gs_object_position (game, object) == OBJ_IN_OBJECT
       || gs_object_position (game, object) == OBJ_ON_OBJECT)
      && obj_indirectly_held_by_player (game, gs_object_parent (game, object)))
    return FALSE;

  *is_size = FALSE;
  if (lib_object_too_heavy (game, object))
    return TRUE;
  *is_size = lib_object_too_large (game, object);
  return *is_size;
}


/*
 * lib_take_from_over_capacity_390()
 *
 * 3.9's take-from has its own capacity test, in insides() rather than takes().
 * It tests size first (4638C8) and weight second (4638DE).  The weight test
 * is waived when the container itself is held by the player (container
 * [22] = 0); the size test never is.  ALEXIS `get all from large stone table`
 * refuses the size-81 knife by size: "You can't take any more, as your hands
 * are full."  (Adrift_145 T28.)
 */
static scr_bool
lib_take_from_over_capacity_390 (scr_gameref_t game, scr_int object,
                                 scr_int container, scr_bool *is_size)
{
  *is_size = lib_object_too_large (game, object);
  if (*is_size)
    return TRUE;
  return gs_object_position (game, container) != OBJ_HELD_PLAYER
         && lib_object_too_heavy (game, object);
}


/*
 * lib_print_take_npc_refusal()
 *
 * The take-a-character refusal, for lib_cmd_take_npc() and the 4.0 overwrite
 * in lib_take_multiple_common().
 */
static void
lib_print_take_npc_refusal (scr_gameref_t game, scr_int npc)
{
  /*
   * Reject this attempt.  The Runner names the NPC by its Prefix and first
   * Alias here, not its Name: run400 47F750-47F7BC and run390 45969B-4596C6
   * print "I don't think " & [Prefix & " "] & Alias(0) when the alias is
   * set (the prefix only when it is too), and fall back to the Name when it
   * is empty; run380 44057D and run370 4386EE always print Prefix & " " &
   * Alias(0), empty or not.  House (4.00), Cathy alias "girl", no prefix:
   * "I don't think girl would appreciate being handled." (Adrift_95,
   * 2026-09-06).
   */
  {
    const scr_filterref_t filter = gs_get_filter (game);
    const scr_prop_setref_t bundle = gs_get_bundle (game);
    const scr_bool is_390_plus =
        prop_get_taf_version (bundle) >= TAF_VERSION_390;
    const scr_char *prefix, *alias = NULL;
    scr_vartype_t vt_key[4];

    prefix = prop_get_indexed_string (bundle, "NPCs", npc, "Prefix");
    alias = lib_first_alias (bundle, vt_key, "NPCs", npc);
    if (!prefix)
      prefix = "";
    if (!alias)
      alias = "";

    pf_buffer_string (filter, "I don't think ");
    if (!is_390_plus)
      {
        pf_buffer_string (filter, prefix);
        pf_buffer_string (filter, " ");
        pf_buffer_string (filter, alias);
      }
    else if (alias[0] != NUL)
      {
        if (prefix[0] != NUL)
          {
            pf_buffer_string (filter, prefix);
            pf_buffer_string (filter, " ");
          }
        pf_buffer_string (filter, alias);
      }
    else
      lib_print_npc_np (game, npc);
    pf_buffer_string (filter, " would appreciate being handled.\n");
  }
}

/*
 * lib_take_npc_overwrite()
 *
 * The present character an object take line also names, or -1.  The same
 * characters() pass as lib_examine_npc_overwrite_400() has a take arm
 * (run400 47F70B-47F7BC, run390 459658-4596C6, run380 44054B-44057D,
 * run370 4386BC-4386EE: whole-word take/get/pick up and the NPC in the room)
 * that ASSIGNS the refusal to the message buffer without testing it, so the
 * object's take answer is thrown away while the take itself stands.  All
 * four start the sentence with `MemVar = "I don't think "` -- run400 47F753,
 * run390 45966D, and the single-expression assignments at run380 44057D and
 * run370 4386EE -- so every version clobbers.  (An earlier reading of the
 * run390 listing, where the decompiler hangs the literal off the Prefix test
 * at 45969B and leaves the alias-free arm at 4596BB looking like an append,
 * said 3.9 appended and was not ported; the pushes at 45966A say otherwise
 * and so does the Runner.)
 *
 * The "no task ran" halves differ by VB precedence and are not modelled:
 * 3.9/4.0 gate all three verbs on it, 3.7 gates none, and 3.8 gates only
 * "pick up" (`c("take") Or c("get") Or c("pick up") And notask`).  Nothing
 * reaches here with a task already run for the line.
 *
 * Measured on ONNAFA (4.00): `get key of pure harry` with Red Harry (alias
 * Harry) present answers only "I don't think Harry would appreciate being
 * handled." (runner_transcripts/onnafa.txt:1622), and the key is used later.
 * The pre-4.0 halves are p3xORD, cmdfile_p2chr.txt: `take hat bob`,
 * `take coin bob`, `get hat bob`, `pick up hat bob`, `take bob hat` and
 * `take box bob` all answer only the refusal and all still take the object
 * (Adrift_265_3c37.rtf, 266_3c38.rtf, 267_3c39.txt, 268_3c40.txt).
 */
static scr_int
lib_take_npc_overwrite (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();

  if (!input
      || !(lib_input_contains_word (input, "take")
           || lib_input_contains_word (input, "get")
           || lib_input_contains_word (input, "pick up")))
    return -1;
  return lib_examine_npc_overwrite_400 (game);
}

/*
 * lib_take_from_npc_overwrite_380()
 *
 * The 3.8 take-from arm, the LAST text arm of characters() (441093-4412AB,
 * just above the walk ticker), so it overwrites everything above it --
 * including the take refusal that lib_take_npc_overwrite() just printed and
 * including the ask topic.  It is entered on whole-word get or take with the
 * character in the player's room, and then walks the WHOLE object table:
 * every object whose Short or first Alias the line names by c() ASSIGNS a
 * sentence, so the highest-index match speaks.  There is no scope test of
 * any kind and, unlike its 3.9 twin at 45A27C, no c("from") gate on the "is
 * not carrying" halves -- that gate is exactly what 3.9 added.
 *
 * An object the character itself holds or wears is "<Name> refuses to give
 * you <the object>!" (4411AA); anything else is "<Name> is not carrying <the
 * object>!" (441227 for an object inside something else, 44128A for a loose
 * one).  3.7 has no such arm at all.
 *
 * Measured on p38ORD, cmdfile_p2chr.txt (Adrift_266_3c38.rtf): `take hat
 * bob` and `get hat bob` are "Bob is not carrying the hat!" while the hat is
 * loose on the floor and still gets taken, `take nut bob` names the nut
 * shut inside the box, `take hat coin bob` speaks for the coin (the higher
 * index), `take ask bob about hat` loses the topic reply to it, and `pick up
 * hat bob` keeps the take refusal because "pick up" is not one of this arm's
 * two verbs.  3.7's same feed answers the take refusal throughout
 * (Adrift_265_3c37.rtf).
 *
 * TRUE once it has spoken, having truncated the buffer back to mark first.
 */
static scr_bool
lib_take_from_npc_overwrite_380 (scr_gameref_t game, scr_int npc, size_t mark)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int object, found = -1;

  if (!input
      || prop_get_taf_version (bundle) != TAF_VERSION_380
      || !(lib_input_contains_word (input, "get")
           || lib_input_contains_word (input, "take")))
    return FALSE;

  for (object = 0; object < gs_object_count (game); object++)
    {
      scr_vartype_t vt_key[4];
      const scr_char *word;

      word = prop_get_indexed_string (bundle, "Objects", object, "Short");
      if (!scr_strempty (word) && lib_input_contains_word (input, word))
        {
          found = object;
          continue;
        }
      word = lib_first_alias (bundle, vt_key, "Objects", object);
      if (word && !scr_strempty (word)
          && lib_input_contains_word (input, word))
        found = object;
    }

  if (found == -1)
    return FALSE;

  pf_truncate (filter, mark);
  lib_print_npc_np (game, npc);
  if ((gs_object_position (game, found) == OBJ_HELD_NPC
       || gs_object_position (game, found) == OBJ_WORN_NPC)
      && gs_object_parent (game, found) == npc)
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           " refuses to give you ",
                                           " refuses to give me ",
                                           " refuses to give %player% "));
  else
    pf_buffer_string (filter, " is not carrying ");
  lib_print_object_np (game, found);
  pf_buffer_string (filter, "!\n");
  return TRUE;
}

/*
 * lib_cmd_take_npc()
 *
 * Reject attempts to take an npc.
 */
scr_bool
lib_cmd_take_npc (scr_gameref_t game)
{
  scr_int npc;
  scr_bool is_ambiguous;

  /*
   * 3.7/3.8: the last character the line names answers, here or not; see
   * lib_print_npc_not_here_pre390().  `take guard` with Ann and Bob here and
   * Cora next door is "Cora is not here!" (run370x Adrift_193_pnpcamb37b,
   * run380x Adrift_192).
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    {
      npc = lib_last_named_npc (game);
      if (npc == -1)
        return FALSE;
      if (!lib_print_npc_not_here_pre390 (game, npc))
        {
          const size_t mark = pf_buffer_length (gs_get_filter (game));

          var_set_ref_character (gs_get_vars (game), npc);
          lib_print_take_npc_refusal (game, npc);
          /* 3.8's take-from arm speaks below it; it names an object the
             take resolver never saw, as `take nut bob` does for a nut shut
             inside a CLOSED box (Adrift_266_3c38.rtf). */
          lib_take_from_npc_overwrite_380 (game, npc, mark);
        }
      return TRUE;
    }

  /* Get the referenced npc, and if none, consider complete.  3.9 fills an
     empty buffer, so the FIRST present character answers (Adrift_193). */
  npc = lib_disambiguate_npc_pick (game, "take", &is_ambiguous,
                                   NPC_PICK_FIRST);
  if (npc == -1)
    return is_ambiguous;

  lib_print_take_npc_refusal (game, npc);
  return TRUE;
}


/*
 * lib_take_backend_common()
 *
 * Common backend handler for taking objects.  Takes all objects currently
 * referenced in the game, trying game commands first, and then moving other
 * unhandled objects to the player inventory.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */

/* Set when the take command named exactly one object; cleared by the
   backend.  Selects the single-take "already carrying" refusal wording. */
static scr_bool lib_take_single_named = FALSE;
static scr_bool lib_take_refusal_claimed = FALSE;
/* Set when a 4.0 take line was split on "and" into pieces the take
   resolver named one by one; see lib_take_and_400().  Each piece gets the
   single-take wording ("already carrying", "can't take X!"). */
static scr_bool lib_take_and_pieces_400 = FALSE;

/*
 * lib_take_refusal_redispatch_400()
 *
 * A 4.0 take refusal does not claim its line.  The "can't take X!" path of
 * run400's per-piece take (Proc_19_23_473A34 @47329D) leaves by ExitProcI2
 * at @4733FC without setting the handler's result, so get_outer
 * (Proc_19_22_4582D8) does not GoTo loc_48B4E3 and generaltasks' task
 * dispatcher at 48A481 gets the line, its text joined onto the refusal with
 * the two-space separator.  The line it matches is the typed one with the
 * take verbs rewritten, "remove "/"pick "/"take " -> "get " (get_outer's
 * Replace chain, @458127-458176), never a rebuild from the resolved object.
 * Professor.taf with the Mailbox on-a Rope static, measured in run400 on
 * 2026-09-14 (Adrift_p4profmail2/3, Adrift_1152_p4profmail4,
 * Adrift_1153_p4profmail5):
 *
 *   `take mailbox`, `take the mailbox` -> "You can't take the Mailbox on-a
 *     Rope!  The mailbox is already down." (task 8's
 *     `[check/look/get]{in/the}[mailbox]{on}{a}{rope}`), or its CompleteText
 *     while the mailbox is still up;
 *   `pick up mailbox` ("get up mailbox"), `take rope` ("get rope") and `take
 *     mailbox on-a rope` (the hyphen misses {on}{a}) -> the refusal alone.
 *
 * It is the one place run400 runs a second task on a typed line: `take
 * mailbox` has already run task 7 `[take/get/pick up]...` silently, its
 * type-1 restriction on the referenced object failing with no message.  A
 * rewrite that leaves the line unchanged offers nothing the task passes did
 * not already see, so it is skipped.
 */
static scr_bool lib_take_refusal_redispatch = FALSE;

static void
lib_take_refusal_redispatch_400 (scr_gameref_t game)
{
  static const scr_char *const verbs[] = {"remove ", "pick ", "take "};
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *typed = run_get_dispatch_input ();
  std::string line;

  if (!typed)
    return;
  line = typed;
  for (const scr_char *verb : verbs)
    {
      const size_t length = strlen (verb);
      size_t at = 0;

      while ((at = line.find (verb, at)) != std::string::npos)
        {
          line.replace (at, length, "get ");
          at += 4;
        }
    }
  if (line == typed)
    return;

  /*
   * The dispatcher sees no referenced object: generaltasks cleared it at the
   * top of the command (48A004) and the refusal path never binds one, so
   * task 7's type-1 restriction on "the referenced object" fails silently
   * and task 8 answers, where the library's resolved mailbox would have
   * printed task 7's "The mailbox isn't down here right now."
   */
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_int ref_object = var_get_ref_object (vars);
  const scr_int ref_character = var_get_ref_character (vars);

  var_set_ref_object (vars, -1);
  var_set_ref_character (vars, -1);
  pf_note_trailing_auto_break (filter);
  pf_buffer_join_pending (filter);
  run_game_task_commands (game, line.c_str ());
  pf_clear_join_pending (filter);
  var_set_ref_object (vars, ref_object);
  var_set_ref_character (vars, ref_character);
}

/* Set when a "take from <object>" command named exactly one object; cleared
   by the backend.  Only that single-named form echoes the taken object's
   raw prefix in pre-4.0 games (see the wording comment in the backend). */
static scr_bool lib_take_from_single_named = FALSE;

/*
 * lib_take_from_verb()
 *
 * "You take " is a 3.80 rewording.  run370 spells the from-container take
 * "You get ", and the census is unambiguous about it: the literal " take "
 * is in run380, run390 and run400 and in none of run370, while " get " is
 * in all four, and run370 carries "You can't get anything from that." where
 * 380 and 390 carry that and " can't take anything from " both.
 *
 * Measured on the same turn of two probes, `get coin from box` with the box
 * held and the coin inside it: run370 answers "You get a coin from the box."
 * (p37DARK, Adrift_986:27) and run380 "You take a coin from the box."
 * (p38DARK, Adrift_982:27), 2026-09-12.  The typed verb was "get" on both,
 * so this is the handler's own wording and not an echo of the input.
 *
 * The bare-verb refusal in lib_take_from_unseen_refusal() prints the same
 * phrase from the same place in the Runner and moves with it; that half is
 * the census's, not a measurement.
 */
static const scr_char *
lib_take_from_verb (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_380)
    return lib_select_response (game,
                                "You get ", "I get ", "%player% get ");

  return lib_select_response (game,
                              "You take ", "I take ", "%player% take ");
}


/*
 * lib_take_container_unheld()
 * lib_print_not_holding()
 *
 * The pre-3.9 hold gate on the container or surface an object is taken
 * from.  run380's takes() (loc_43E47B) rewrites a plain "take X" whose X sits
 * in or on a present object into "take X from <parent>", and its insides()
 * from-branch (loc_446CBB..446CFB) then insists that a DYNAMIC parent is held
 * (position 0) or worn (&H9C) by the player, else "<You> <are> not holding
 * <raw prefix> <short>." -- before the closed test, before any game task is
 * tried for "get X from Y".  A static parent only has to be in the room.
 * run370's insides() (43B118, "not holding" at 439D33/43AB5E) is the same;
 * run390 has no "not holding" on any take path and run400 none at all, so
 * from 3.90 on a contained object is taken wherever its parent is.
 *
 * Measured on jb2000.taf (James Bond - Happy Landings, 3.80) in run380,
 * 2026-09-04: `take key card` with the card inside a jacket lying in the
 * room answers "You are not holding a brown jacket.", and `take learner`
 * from a suitcase on the floor "You are not holding a brown suitcase.";
 * `take jacket` first, and the card comes out with "You take the cockpit
 * key card from the brown jacket."  The prefix is the author's, untensed
 * (var_41C(0) & " " & var_41C(4)), hence lib_print_object.  Held means
 * held directly: a container nested inside a carried one has position
 * &HF6, not 0, and is refused like any other.
 */
static scr_bool
lib_take_container_unheld (scr_gameref_t game, scr_int container)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return FALSE;
  if (obj_is_static (game, container))
    return FALSE;

  return !(gs_object_position (game, container) == OBJ_HELD_PLAYER
           || gs_object_position (game, container) == OBJ_WORN_PLAYER);
}

static void
lib_print_not_holding (scr_gameref_t game, scr_int container,
                       const scr_char *suffix)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "You are not holding ",
                                         "I am not holding ",
                                         "%player% is not holding "));
  lib_print_object (game, container);
  pf_buffer_string (filter, suffix);
}

/*
 * lib_cannot_reach_container()
 *
 * Pre-4.0 refuses to reach into a container or onto a surface while the
 * player is sitting, standing or lying on some OTHER object.
 *
 * run390's insides() (463EEC) tests the player record's +16 byte -- the
 * object the player is on or in, &HFF when free -- against the container
 * being reached into, and prints the refusal unless it is one or the other
 * (loc_4631D0..4631E1):
 *
 *     If player.on = &HFF Or player.on = container Then ok
 *     Else <You> & " can't reach " & the(container) & " from " &
 *          the(player.on) & "!"
 *
 * Both names come out of the definite-name helper, so it is "the " & Short
 * with the author's capitalisation kept: "You can't reach the dresser from
 * the Bed!"
 *
 * Measured 2026-09-05 on A_Morning_with_a_Headache.taf (3.90) in run390: the
 * game starts the player in bed, and `take alarm clock` -- the clock sits on
 * the dresser -- is refused exactly that way, as is `take suit` off the
 * chair.  The literal " can't reach " is in run370, run380 and run390's
 * constant pools and is ABSENT from run400's, so 4.0 dropped the rule
 * outright; the gate is < 4.00 rather than the usual 3.9 split.
 *
 * Two things here are inferred rather than measured, and are noted in
 * notes/WINE-TRANSCRIPTS-TODO.md: whether a game task for the same command
 * would run first (nothing in that game claims `take alarm clock`), and how
 * this orders against 3.7/3.8's own "not holding" gate, which is why it sits
 * after lib_take_container_unheld() rather than before it.
 */
static scr_bool
lib_cannot_reach_container (scr_gameref_t game, scr_int container)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    return FALSE;

  return gs_playerparent (game) != -1 && gs_playerparent (game) != container;
}

static void
lib_print_cannot_reach (scr_gameref_t game, scr_int container)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "You can't reach ",
                                         "I can't reach ",
                                         "%player% can't reach "));
  lib_print_object_np (game, container);
  pf_buffer_string (filter, " from ");
  lib_print_object_np (game, gs_playerparent (game));
  pf_buffer_string (filter, "!");
}

/*
 * lib_drain_multiple_references_if()
 * lib_drain_multiple_references()
 *
 * Fill 'list' with every object still marked in the multiple references,
 * clearing each as it's collected; the filtered form takes only the objects
 * 'keep' accepts and leaves the rest marked.  Shared body for the take, wear,
 * move and put backends' reports of the objects left over ("You are not
 * holding ...", "You can't take ...", "You can't wear ...").
 */
template <typename Keep>
static void
lib_drain_multiple_references_if (scr_gameref_t game, scr_int object_count,
                                  lib_list_t &list, Keep keep)
{
  scr_int object;

  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object] || !keep (object))
        continue;

      list.push_back (object);
      game->multiple_references[object] = FALSE;
    }
}

static void
lib_drain_multiple_references (scr_gameref_t game, scr_int object_count,
                               lib_list_t &list)
{
  lib_drain_multiple_references_if (game, object_count, list,
                                    [] (scr_int) { return TRUE; });
}


/*
 * lib_object_printer_390()
 *
 * The printer for an object named in a refusal: the composed name from 3.9,
 * the raw Prefix & " " & Short before it (run380 43E03E).
 */
static lib_print_item_t
lib_object_printer_390 (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390
         ? lib_print_object_np : lib_print_object_raw;
}


static void
lib_take_backend_common (scr_gameref_t game, scr_int associate,
                         scr_bool is_associate_object, scr_bool is_associate_npc)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object_count, object, total, npc;
  scr_bool has_printed;
  lib_list_t list;
  /*
   * Objects that would take the player over capacity, in the order they
   * were encountered.  The Runner's multi-take loop appends a separate
   * "<Name> is too heavy for me to carry at the moment."/" hands are full."
   * sentence for every such object (run400 @0047C127/@0047C0F0), not just
   * the first.
   */
  lib_list_t over_capacity, over_is_size;
  /*
   * 3.9 take-from: the capacity test lives in insides(), with its own order
   * and, for the all/and forms, its own reporting; see below.
   */
  const scr_bool from_390 = is_associate_object
      && prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390
      && !lib_is_version_400 (game);
  const scr_bool from_390_multi = from_390 && !lib_take_from_single_named;
  scr_int fits;
  assert (!is_associate_object || !is_associate_npc);

  /*
   * Try game commands for all referenced objects first.  If any succeed,
   * remove that reference from the list.  At the same time, filter out and
   * flag any object that takes us over the player's capacity.
   */
  has_printed = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      scr_bool status;

      if (!game->object_references[object])
        continue;

      /*
       * Pre-3.9 refuses the whole take when the object's parent is a
       * dynamic container or surface the player isn't holding; see
       * lib_take_container_unheld().  With an associate the parent was
       * already vetted by lib_take_from_is_valid().
       */
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && lib_take_container_unheld (game,
                                        gs_object_parent (game, object)))
        {
          scr_int index_;

          lib_print_not_holding (game, gs_object_parent (game, object), ".");
          for (index_ = 0; index_ < object_count; index_++)
            game->object_references[index_] = FALSE;
          return;
        }

      /*
       * And pre-4.0 refuses it outright while the player is on or in some
       * other object; see lib_cannot_reach_container().
       */
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && lib_cannot_reach_container (game,
                                         gs_object_parent (game, object)))
        {
          scr_int index_;

          lib_print_cannot_reach (game, gs_object_parent (game, object));
          for (index_ = 0; index_ < object_count; index_++)
            game->object_references[index_] = FALSE;
          return;
        }

      /*
       * See if the object takes us beyond capacity.  If it does, note it and
       * continue.
       */
      {
        scr_bool is_size;

        if (!from_390 && lib_take_over_capacity (game, object, &is_size))
          {
            over_capacity.push_back (object);
            over_is_size.push_back (is_size);
            game->object_references[object] = FALSE;
            continue;
          }
      }

      /* Now try for a game command, using the associate if supplied. */
      if (is_associate_object)
        status = lib_try_game_command_with_object (game, "get",
                                                   object, "from", associate);
      else if (is_associate_npc)
        status = lib_try_game_command_with_npc (game, "get",
                                                object, "from", associate);
      else
        {
          scr_bool looked_up;

          status = lib_try_game_command_take_from_parent_400 (game, object,
                                                              &looked_up);
          /*
           * 4.0's take piece looks up "get " & name(obj, 0) only
           * (462B0D): `take pebble` never reaches a task `take a pebble`
           * and takes the pebble (p4WITHQ2.taf, Adrift_1159).
           */
          if (!looked_up && lib_is_version_400 (game))
            status = lib_try_game_command_take_definite (game, object);
          else if (!looked_up)
            status = lib_try_game_command_short (game, "get", object);
        }
      if (status)
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
        }
    }

  /*
   * We attempt acquisition of get-able objects here only for cases where
   * there is either no associate, or where the associate is an object.  If
   * the associate is an NPC, we're going to refuse all acquisitions later
   * on, by forcing object references.
   */
  /*
   * 3.9's all/and take-from first counts the objects that would fit, adding
   * each fitting one to running size and weight totals (run390 463394-4634F4,
   * with no held-container waiver).  If none fits it answers "<Your> hands are
   * full." when any failed on size, otherwise "That is too heavy.", and takes
   * nothing (4634F9-463545).
   */
  fits = 0;
  if (from_390_multi)
    {
      scr_int size, weight;
      scr_bool any_size, any_weight;

      size = lib_carried_size (game);
      weight = lib_carried_weight (game);
      fits = 0;
      any_size = any_weight = FALSE;
      for (object = 0; object < object_count; object++)
        {
          if (!game->object_references[object]
              || !(gs_object_position (game, object) == OBJ_IN_OBJECT
                   || gs_object_position (game, object) == OBJ_ON_OBJECT)
              || gs_object_parent (game, object) != associate)
            continue;

          if (size + obj_get_size (game, object)
              > obj_get_player_size_limit (game))
            any_size = TRUE;
          else if (weight + obj_get_weight (game, object)
                   > obj_get_player_weight_limit (game))
            any_weight = TRUE;
          else
            {
              size += obj_get_size (game, object);
              weight += obj_get_weight (game, object);
              fits++;
            }
        }

      if (fits == 0 && (any_size || any_weight))
        {
          if (any_size)
            lib_print_clause (game, has_printed,
                              "Your hands are full.",
                              "My hands are full.",
                              "%player%'s hands are full.");
          else
            lib_print_clause (game, has_printed,
                              "That is too heavy.",
                              "That is too heavy.",
                              "That is too heavy.");
          for (object = 0; object < object_count; object++)
            game->object_references[object] = FALSE;
          return;
        }
    }

  total = 0;
  if (!is_associate_npc)
    {
      scr_int parent, start, limit;

      /*
       * Attempt to acquire each remaining get-able object in turn, looping
       * on each possible parent object in turn, with an initial parent of
       * -1 for objects not contained or supported.
       *
       * If we're dealing with only objects from a known container or
       * supporter, eliminate all but one iteration of the parent search.
       */
      start = is_associate_object ? associate : -1;
      limit = is_associate_object ? associate : object_count - 1;

      for (parent = start; parent <= limit; parent++)
        {
          list.clear ();
          for (object = 0; object < object_count; object++)
            {
              if (!game->object_references[object])
                continue;

              /*
               * If parent is -1, ignore contained objects, otherwise ignore
               * objects not contained, or if contained, not contained by the
               * current parent.
               */
              if (parent == -1)
                {
                  if (gs_object_position (game, object) == OBJ_IN_OBJECT
                      || gs_object_position (game, object) == OBJ_ON_OBJECT)
                    continue;
                }
              else
                {
                  if (!(gs_object_position (game, object) == OBJ_IN_OBJECT
                        || gs_object_position (game, object) == OBJ_ON_OBJECT))
                    continue;
                  if (gs_object_parent (game, object) != parent)
                    continue;
                }

              /*
               * Here we have to repeat capacity checks.  As objects are
               * acquired more and more of the player's capacity gets used up.
               * This means a check directly before each acquisition.
               */
              {
                scr_bool is_size;

                if (from_390
                    ? lib_take_from_over_capacity_390 (game, object, parent,
                                                       &is_size)
                    : lib_take_over_capacity (game, object, &is_size))
                  {
                    over_capacity.push_back (object);
                    over_is_size.push_back (is_size);
                    continue;
                  }
              }

              list.push_back (object);
              gs_object_player_get (game, object);
              /* A successful library take spends OnlyWhenNotMoved mode 1
               * (run400 `takes` @0047BF66).  Nothing else does. */
              gs_set_object_unmoved (game, object, FALSE);
            }

          if (!list.empty ())
            {
              /*
               * 4.0 builds the take line in front of whatever the turn has
               * printed so far: get_piece copies the buffer aside (run400
               * @0047359A), clears it, writes "You take ...." and then, if
               * the copy was not empty, pspace()s and appends it back
               * (@004736B6).  So a task claiming one of the objects prints
               * after the library's line.  fullcircle `get all`, with a
               * `get *branch*` task, answers "You take the helm and the
               * locket.  You take the branch."  (Adrift_1053_fullcircle.txt
               * line 356.)  run390 orders it the same way: The Will's
               * `get all` at the Narrow Squeeze, where task 142 `* get *
               * clover` claims the clover, answers "You take the rare stamp,
               * ... and the gold ring.  You take the jade clover." (run390x,
               * runner_transcripts/thewill.txt T197).
               */
              scr_owned_string saved;
              if (has_printed && total == 0
                  && (lib_is_version_400 (game) || lib_is_version_390 (game)))
                saved.reset (pf_transfer_buffer (filter));
              else if (has_printed)
                pf_buffer_string (filter, total == 0 ? "\n" : "  ");
              /*
               * 4.0 reworded the loose-in-the-room take; the from-container
               * branch keeps the same verb in every version.  Measured live on
               * a bare `take rock` / `get rock` / `take all`: run370 and
               * run390 answer "You pick up the rock.", run400 "You take the
               * rock."  (run380 shows the pre-4.0 half of the same handler
               * through its "nothing to pick up here" refusal.)
               *
               * What 4.0 did change in the from-container branch is the
               * *taken* object's prefix: pre-4.0 prints it raw, 4.0 normalizes
               * it like everything else.  The container is normalized in both.
               * Measured on the one game carried across three .taf versions by
               * the generators (see lib_print_object_np above): `take gun` out
               * of a "some"-prefixed container holding an "a"-prefixed pistol
               * gives run380 "You take a small pistol from some aluminum
               * clothes.", run390 "You take a small pistol from the aluminum
               * clothes.", run400 "You take the small pistol from the aluminum
               * clothes."  Only the pistol's own prefix moves at 4.0; the
               * clothes move at 3.9, which is the "some" rule, not this one.
               *
               * The raw pre-4.0 prefix belongs to the Runner's SINGLE-take
               * handler only -- any take that named exactly one object,
               * with or without an explicit "from" clause (the pistol probe
               * above was a bare `take gun`).  The multi-take loop
               * normalizes in every version: run390 on ALEXIS ("a"-prefixed
               * diary) answers `take diary from table` with "You take a
               * diary from the old oak table." but `take diary and cloak
               * from table` with "You take the diary and the woven cloak
               * from the old oak table." and `get all from table` with "You
               * take the diary, the brass lantern, the woven cloak and the
               * nice food from the old oak table."  (Adrift_8.txt, measured
               * live 2026-08-22.)
               *
               * That multi-take exception is a 3.9 change of its own: pre-3.9
               * the multi-take loop prints raw too, so from 3.7 to 3.8 every
               * from-container take is raw whatever it named.  Measured live
               * under Wine 2026-08-24 on mikes.taf (3.80) in run380, whose
               * dresser holds four objects with EMPTY prefixes (deobfuscated
               * from the .taf, so this is not a guess): `take all from
               * dresser` answers "You take a socks, a shirt, a underwear and
               * a pair of pants from the dresser."  Empty is the case that
               * separates the two printers -- raw defaults it to "a ", the
               * normalizing one to "the " -- and the same replay pins both
               * halves at once, since the container itself comes out "the
               * dresser".  3.9 normalizes the same shape: the ALEXIS `get all
               * from table` above.
               */
              if (parent == -1)
                pf_buffer_string (filter,
                                  lib_is_version_400 (game)
                                  ? lib_select_response (game,
                                                         "You take ",
                                                         "I take ",
                                                         "%player% take ")
                                  : lib_select_response (game,
                                                         "You pick up ",
                                                         "I pick up ",
                                                         "%player% pick up "));
              else if (!from_390_multi)
                pf_buffer_string (filter, lib_take_from_verb (game));
              if (from_390_multi)
                {
                  /*
                   * 3.9's all/and take-from punctuates its list by counting
                   * down from the pre-pass count of objects that fit, not
                   * from the number actually taken (run390 463AAA-463B18):
                   * ", " while more than one remains, " and " before the
                   * last, " from <container>." on reaching zero, and nothing
                   * after that.  The verb is printed only if anything fit.
                   * Objects the take loop accepts past the count are glued
                   * on after the full stop.  Measured on Namiki's `take all
                   * from bag`, seven objects with a count of five: "I take
                   * my bandana, ... and the Kenzo jacket from the tennis
                   * bag.the Dior skirtmy jewelry".
                   */
                  scr_int remaining = fits;
                  size_t index;

                  if (fits > 0)
                    pf_buffer_string (filter, lib_take_from_verb (game));
                  for (index = 0; index < list.size (); index++)
                    {
                      lib_print_object_np (game, list[index]);
                      remaining--;
                      if (remaining > 1)
                        pf_buffer_string (filter, ", ");
                      else if (remaining == 1)
                        pf_buffer_string (filter, " and ");
                      else if (remaining == 0)
                        {
                          pf_buffer_string (filter, " from ");
                          lib_print_object_np (game, parent);
                          pf_buffer_character (filter, '.');
                        }
                    }
                }
              else
                {
                  lib_print_list (game, list,
                                  parent == -1 || lib_is_version_400 (game)
                                  || (prop_get_taf_version (gs_get_bundle (game))
                                      >= TAF_VERSION_390
                                      && !(lib_take_single_named
                                           || lib_take_from_single_named))
                                  ? lib_print_object_np : lib_print_object,
                                  " and ");
                  if (parent != -1)
                    {
                      pf_buffer_string (filter, " from ");
                      lib_print_object_np (game, parent);
                    }
                  pf_buffer_character (filter, '.');
                }
              if (saved)
                {
                  const scr_char *text = saved.get ();

                  while (*text == '\n' || *text == ' ')
                    text++;
                  pf_buffer_pspace (filter);
                  pf_buffer_string (filter, text);
                }
            }
          total += (scr_int) list.size ();
          has_printed |= !list.empty ();
        }
    }

  /*
   * If we ran out of capacity, either in weight or in size, print the
   * details.  Each over-weight object gets its own sentence, in the order
   * encountered; a "hands are full" size refusal is printed at most once.
   */
  /*
   * 3.9's all/and take-from refuses nothing object by object.  After the loop
   * it appends one summary: the weight wording if any object failed on weight,
   * otherwise the size one (run390 463BB5-463C2B).
   */
  if (from_390_multi && !over_capacity.empty ())
    {
      scr_bool any_weight = FALSE;
      size_t over;

      for (over = 0; over < over_capacity.size (); over++)
        any_weight |= !over_is_size[over];
      if (any_weight)
        lib_print_clause (game, has_printed,
                          "You can't take any more, as it is too heavy.",
                          "I can't take any more, as it is too heavy.",
                          "%player% can't take any more, as it is too heavy.");
      else
        lib_print_clause (game, has_printed,
                          "You can't take any more, as your hands are full.",
                          "I can't take any more, as my hands are full.",
                          "%player% can't take any more, as %player%'s hands"
                          " are full.");
      has_printed = TRUE;
      over_capacity.clear ();
    }

  {
    scr_bool size_reported;
    size_t over;

    size_reported = FALSE;
    for (over = 0; over < over_capacity.size (); over++)
      {
        if (over_is_size[over])
          {
            if (size_reported)
              continue;
            size_reported = TRUE;

            lib_print_clause (game, has_printed,
                              "Your hands are full.",
                              "My hands are full.",
                              "%player%'s hands are full.");
          }
        else
          {
            lib_new_clause (game, has_printed);
            lib_print_too_heavy (game, over_capacity[over]);
          }
        has_printed |= TRUE;
      }
  }

  /*
   * Note any remaining multiple references left out of the take operation.
   * This is some workload...
   *
   * First, deal with the case where we have an associated object.
   */
  if (is_associate_object)
    {
      lib_drain_multiple_references (game, object_count, list);

      /*
       * 4.0 drops a named object that is not in the container without a word:
       * run400 answers `get coin and stone from box` with just "You take the
       * coin from the box." (p4TFROM, Adrift_974, 2026-09-10), and there is
       * no such literal anywhere in run400.
       *
       * Pre-4.0 does report it, and composes the sentence a word at a time
       * (run390 loc_4636D0-46378A: tense & Short & isare() & "not " & var_E0
       * & " " & definite(container) & "!", where var_E0 is "inside" or "on").
       * No " is not in " literal exists in any of the four Runners, so the
       * wording here is "inside", not "in", and the sentence ends with an
       * exclamation mark: run390 answers `get stone from box` with "The stone
       * is not inside the box!" (p39DARK, Adrift_970/973, 2026-09-10).  It
       * reports an object the player is already holding the same way -- the
       * measured turn is `get coin from box` with the coin in hand and the
       * box open and empty, "The coin is not inside the box!" (Adrift_973) --
       * so there is no held-or-worn exemption above.
       *
       * 3.7/3.8 have no such sentence (no "not inside"/"not on" in run370 or
       * run380): their take loop (run380 446FC5) skips a named object that
       * is not in the container without a word.  A line that takes nothing
       * at all is lib_take_from_nothing_taken_pre390()'s.
       */
      if (!list.empty () && !lib_is_version_400 (game)
          && prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
        {
          lib_new_clause (game, has_printed);
          lib_print_list (game, list, lib_print_object_np, " and ");
          pf_buffer_string (filter,
                            list.size () == 1
                            ? lib_select_plurality (game, list[0],
                                                    " is not ", " are not ")
                            : " are not ");
          pf_buffer_string (filter,
                            obj_is_container (game, associate)
                            ? "inside " : "on ");
          lib_print_object_np (game, associate);
          pf_buffer_character (filter, '!');
          has_printed = TRUE;
        }
    }

  /*
   * Now, deal with the case where we have an associated NPC.  Once this
   * case is handled, we can force the object references so that the code
   * that follows on from here will report errors taking all objects.
   *
   * Note that this means that we can never successfully take an object
   * from an NPC; that'll have to happen via a game's own commands.
   */
  if (is_associate_npc)
    {
      lib_drain_multiple_references_if
        (game, object_count, list, [game] (scr_int item)
         { return gs_object_position (game, item) != OBJ_PART_NPC; });

      if (!list.empty ())
        {
          lib_new_clause (game, has_printed);
          lib_print_npc_np (game, associate);
          pf_buffer_string (filter, " is not carrying ");
          lib_print_list (game, list, lib_print_object_np, " or ");
          pf_buffer_character (filter, '!');
        }
      has_printed |= !list.empty ();

      /*
       * Merge any remaining object references into multiple references,
       * so that succeeding code complains about the inability to acquire
       * these objects.
       */
      for (object = 0; object < object_count; object++)
        {
          game->multiple_references[object] |= game->object_references[object];
          game->object_references[object] = FALSE;
        }
    }

  /*
   * The remainder of this routine is common error reporting for both object
   * and NPC associates (and also for no associates).
   */
  /*
   * 4.0's single-take piece tests possession with 44615C, which counts a
   * WORN object as carried (position 0 or &H9C, run400 General 4460F4), so
   * a single-named take of worn clothing gets the held object's task look-up
   * and "You are already carrying <object>." -- not "already wearing".
   * 3monkeys T65 `get sheet` with the sheet worn (run400x, 2026-09-19).
   */
  /*
   * A 4.0 line split on "and" resolves each piece through the same
   * single-take handler (run400 4731A2 loops the pieces through
   * Proc_19_23_473A34), so every piece gets the single wording: p4TKA
   * `take hat and stone` with the hat worn is "You take the stone.  You are
   * already carrying the hat." (Adrift_208_ptka_4 cells 14/40/44, 2026-09-20).
   */
  const scr_bool worn_is_held = lib_is_version_400 (game)
                                && (lib_take_single_named
                                    || lib_take_and_pieces_400);
  lib_drain_multiple_references_if
    (game, object_count, list, [game, worn_is_held] (scr_int item)
     {
       return gs_object_position (game, item) == OBJ_HELD_PLAYER
              || (worn_is_held
                  && gs_object_position (game, item) == OBJ_WORN_PLAYER);
     });

  /*
   * 4.0 gives the tasks their look at a held object too, and earlier than
   * the one ahead of " can't take " below: run400's take piece
   * Proc_19_39_46302C pre-matches "get <the object>" in the take-family
   * mode (@462AED / @462B84) and dispatches it with the restriction-failure
   * pass on (@462C5C, Proc_19_24_44CCE0(1, 1)) BEFORE either the
   * " can't take " test at 462CA0 or the " already carrying " one at
   * 462D01 -- and a claim exits the piece outright.  So a task restricted on
   * already holding the object answers with its own fail message where the
   * library would have said "You are already carrying the cone.".
   *
   * Measured on IceCream.taf in run400 (Adrift_900_icecream2.txt,
   * 2026-09-07): `take cone`, with the cone already in hand at the start of
   * the game, is task 14's "  You already have an empty cone." -- the
   * FailMessage of its one restriction -- and not the library refusal.
   *
   * The look-up is the one 4.0's implicit take uses, definite name and
   * " from <holder>" clause included; see
   * lib_try_game_command_take_definite().
   */
  if (lib_is_version_400 (game) && !list.empty ())
    {
      lib_list_t held;
      scr_bool is_claimed = FALSE;

      for (const scr_int item : list)
        {
          if (lib_try_game_command_take_definite (game, item))
            is_claimed = TRUE;
          else
            held.push_back (item);
        }
      list.swap (held);
      if (is_claimed)
        {
          /* The task's text is complete in itself, terminator and all. */
          lib_take_refusal_claimed = TRUE;
          has_printed = TRUE;
        }
    }

  /*
   * A take that named a single object uses the Runner's single-take
   * handler, which words the held-object refusal "I am already carrying
   * <object>." (run400 @00462D25); "'ve already got <object>!" is the
   * multi-take processor's wording (run400 @0047BE1B).
   *
   * 4.0 only: the pre-4.0 Runners have no "already carrying" wording at
   * all (no such literal in run370/run380/run390.bas; the only held-take
   * refusal is "'ve already got <object>!" -- run370 436561, run380
   * 43E03E, run390_3 454EC5).  Measured live 2026-08-31 on cave.taf
   * run380: single-named `take parchment` while holding it answers
   * "You've already got half of a parchment!".
   */
  if (lib_is_version_400 (game)
      && ((lib_take_single_named && list.size () == 1)
          || (lib_take_and_pieces_400 && !list.empty ())))
    {
      /* One clause per piece on an "and" line (p4TKA cells 14/40/44). */
      for (const scr_int item : list)
        {
          lib_new_clause (game, has_printed);
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You are already carrying ",
                                                 "I am already carrying ",
                                                 "%player% is already carrying "));
          lib_print_object_np (game, item);
          pf_buffer_character (filter, '.');
          has_printed |= TRUE;
        }
    }
  else
    /* Pre-3.9 spells it Prefix & " " & Short (run380 43E03E): p38EXAM `take
     * stone` held is "You've already got a stone!" (Adrift_1165). */
    has_printed |= lib_print_object_list (game, has_printed, list, " and ", '!',
                                          "You've already got ",
                                          "I've already got ",
                                          "%player%'ve already got ",
                                          lib_object_printer_390 (game));

  lib_drain_multiple_references_if
    (game, object_count, list, [game] (scr_int item)
     { return gs_object_position (game, item) == OBJ_WORN_PLAYER; });

  /*
   * No Runner contracts this one.  Every take path builds it as
   * <pronoun> & " " & <are> & " already wearing " & <name> & "!" -- run400
   * 47BE82, run390 454F31, run380 43E0A2 -- so "You're" was never right; the
   * name is the plain Prefix & " " & Short concatenation below 3.9 and the
   * definite printer from 3.9 on, as everywhere else on this path.
   *
   * 3.7 has no worn arm at all: its takes() tests position 0 (held,
   * "'ve already got", 436561) and the room constant (the pick-up branch,
   * 436585) and nothing else, so a worn object falls through to the same
   * " can't see <raw> from here!" arm an absent one gets (436909/43696A).
   *
   * p37TWO/p38TWO/p39TWO `take hat` with the hat worn (cmdfile_p2verb3.txt
   * cell 7, Adrift_255_2x37.rtf / 256_2x38.rtf / 257_2x39.txt, 2026-09-21):
   * "You can't see a hat from here!" / "You are already wearing a hat!" /
   * "You are already wearing the hat!".
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_380
      && !list.empty ())
    {
      /* The arm assigns the whole message per object, so the last one
         speaks; the raw name is this handler's own concatenation. */
      lib_new_clause (game, has_printed);
      lib_cant_see_named_pre_390 (game, list.back (), FALSE, " from here!");
      has_printed = TRUE;
    }
  else
    has_printed |= lib_print_object_list (game, has_printed, list, " and ", '!',
                                          "You are already wearing ",
                                          "I am already wearing ",
                                          "%player% is already wearing ",
                                          lib_object_printer_390 (game));

  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      lib_drain_multiple_references_if
        (game, object_count, list, [game, npc] (scr_int item)
         {
           return (gs_object_position (game, item) == OBJ_HELD_NPC
                   || gs_object_position (game, item) == OBJ_WORN_NPC)
                  && gs_object_parent (game, item) == npc;
         });

      if (!list.empty ())
        {
          lib_new_clause (game, has_printed);
          lib_print_npc_np (game, npc);
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 " refuses to give you ",
                                                 " refuses to give me ",
                                                 " refuses to give %player% "));
          lib_print_list (game, list, lib_print_object_np, " and ");
          pf_buffer_character (filter, '!');
        }
      has_printed |= !list.empty ();
    }

  lib_drain_multiple_references (game, object_count, list);

  /*
   * Only the 4.0 single-take handler ends this with "!" (run400 @47329D
   * builds " can't take " + name + "!"); its multi-take processor
   * (run400 @47C172) and both of 3.9's (run390 @4551A6, @455533) end it
   * with ".".  Measured 2026-08-29 in run390 on CAH.taf (cruel), `take it`
   * after `x jacket` -> "(a jacket)" / "You can't take the jacket.".
   */
  /*
   * 4.0 gives the tasks one more look at each object it is about to refuse:
   * run400's per-piece take handler (Proc_19_23_473A34) calls the task
   * pre-matcher Proc_19_35_453C50 at @473241, directly ahead of the loop
   * that builds " can't take " (@47329D), and a hit re-dispatches to the
   * task dispatcher Proc_19_24_44CCE0, restriction-failure pass included.
   * The match is by the resolved object's name, not by the typed words:
   * `take kelly` -- an alias, with no "poster" anywhere in the line -- still
   * runs "Get * poster", as do `take poster`, `pick up poster`, `take the
   * poster` and `take bed` ("Get * bed"); `grab poster` is no take verb at
   * all and gets "I don't understand what you want me to do with the kelly
   * brook poster."  (man overboard.taf, run400, Adrift_1_moprobe.txt and
   * Adrift_1_man_overboard.txt line 39-40, measured 2026-08-29.)  The loop
   * at the top of this function does the same for take-able objects; the
   * static ones never reach it, so they get their turn here instead.
   */
  if (!has_printed && !list.empty () && lib_is_version_400 (game))
    {
      lib_list_t refused;

      /*
       * The pre-match is lower-cased and the dispatch case-kept, as in the
       * take piece: a hit that then runs nothing ends on DontUnderstand with
       * no refusal line.  Professor in the Laboratory, mailbox down: `take
       * mailbox` and `get x rope` pre-match task 9
       * `[check/get/pull]{the}[mailbox]{on-a/on a}{rope}` on "get the mailbox
       * on-a rope", miss it on "get the Mailbox on-a Rope", and answer "What
       * was that?..." (Adrift_p4profmail2 T24/T25; `get mailbox`, typed,
       * runs task 9 in Adrift_1154_p4profmail6).  A hit on a failing
       * restriction dispatches the typed line instead; see
       * lib_rebuilt_fallback_typed.
       */
      lib_rebuilt_raw_dispatch = TRUE;
      lib_rebuilt_silent_continues = TRUE;
      lib_rebuilt_fallback_typed = TRUE;
      for (const scr_int item : list)
        {
          if (!lib_try_game_command_short_canonical (game, "get", item))
            refused.push_back (item);
        }
      lib_rebuilt_fallback_typed = FALSE;
      lib_rebuilt_silent_continues = FALSE;
      lib_rebuilt_raw_dispatch = FALSE;
      list.swap (refused);
      /* The tasks' text is complete in itself; no refusal line follows. */
      lib_take_refusal_claimed = list.empty ();
      /* A refusal that stands leaves the line to the task dispatcher. */
      lib_take_refusal_redispatch = !list.empty ();
    }
  if (lib_is_version_400 (game) && lib_take_and_pieces_400)
    {
      /* Each "and" piece is its own single-take refusal: p4TKA `take stone
         and statue` -> "You take the stone.  You can't take the statue!"
         (Adrift_208_ptka_4 cells 17/20, 2026-09-20). */
      for (const scr_int item : list)
        {
          lib_list_t one;
          one.push_back (item);
          has_printed |= lib_print_object_list (game, has_printed, one,
                                                " and ", '!',
                                                "You can't take ",
                                                "I can't take ",
                                                "%player% can't take ",
                                                lib_print_object_np);
        }
    }
  else
    lib_print_object_list (game, has_printed, list, " and ",
                           lib_is_version_400 (game)
                           && lib_take_single_named && list.size () == 1
                           ? '!' : '.',
                           "You can't take ",
                           "I can't take ",
                           "%player% can't take ",
                           /* 3.7/3.8 name it by the raw Prefix: "You can't
                              take a chair." (run380 43E697, run370 4369FF;
                              run370x/run380x p3738sit3, 2026-09-19). */
                           lib_object_printer_390 (game));

  lib_take_single_named = FALSE;
  lib_take_from_single_named = FALSE;
  lib_take_and_pieces_400 = FALSE;
}


/*
 * lib_take_backend()
 * lib_take_from_object_backend()
 * lib_take_from_npc_backend()
 *
 * Facets of lib_take_backend_common().  Provide backend handling for either
 * the plain "take" handlers, or the "take from <something>" handlers.
 */
static void
lib_take_backend (scr_gameref_t game)
{
  lib_take_backend_common (game, -1, FALSE, FALSE);
}

static void
lib_take_from_object_backend (scr_gameref_t game, scr_int associate)
{
  lib_take_backend_common (game, associate, TRUE, FALSE);
}

static void
lib_take_from_npc_backend (scr_gameref_t game, scr_int associate)
{
  lib_take_backend_common (game, associate, FALSE, TRUE);
}


/*
 * run370's insides() gate -- a deliberate deviation.
 *
 * run370's insides() is entered only by a line holding c("get") and
 * c("from"), or c("remove") and c("from") (43A745) -- run380's gate
 * (4468B3) admits take and pick as well.  So at 3.7 `take gem from box` is
 * no take-from at all but the catch-all's "I don't understand what you
 * want me to do with the box.", and a bare take's rewrite into "<line> from
 * <parent>" (4368C4) only lands in insides() when the line says get: with
 * a gem in an open box on the floor, `get gem` is "You are not holding a
 * box." while `take gem` and `pick up gem` are "Take what?", and a nut in a
 * held bag is "You get a nut from the bag." for `get nut` and "Take what?"
 * for `take nut` (p37TFSW, run370x Adrift_189_ptfsw3_37.rtf, 2026-09-19).
 *
 * Scarier gives 3.7 run380's gate instead, so `take gem from box`, `take
 * nut` from a held bag and `empty bag` work there as they do at 3.8, and
 * nothing in the take, take-from or take-from-character paths tests the
 * line's verb.  This note is the record of what run370 did.
 */


/*
 * lib_take_filter()
 *
 * Helper function for deciding if an object may be acquired in this context.
 * Returns TRUE if an object may be acquired, FALSE otherwise.
 */
static scr_bool
lib_take_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  /*
   * To be take-able by name, an object must be visible in the room, not
   * static, and not already held or worn by the player or an NPC.  Note
   * that obj_indirectly_in_room() recurses only through open containers
   * and surfaces, so a named take (or "take X and Y") reaches objects
   * inside an open container or on a surface present in the room -- the
   * Runners' "and"-list take mode does the same -- while excluding the
   * contents of closed containers.
   *
   * Not on 3.7, though, unless the line says get.  Reaching inside is
   * run380's takes() rewrite of a plain "take X" into "take X from
   * <parent>" (loc_43E47B; see lib_take_container_unheld).  run370 has the
   * same rewrite (4368C4), but its insides() only takes a get or remove
   * line (see the run370 insides() note above), so for take or pick up a bare take
   * naming something that sits in or on another object names nothing at all
   * and falls to the catch-all.  Measured on the same turn of two probes,
   * `take coin` with the coin inside an open box standing on the cave
   * floor: run370 answers "Take what?" (p37DARK, Adrift_987:48 and again
   * Adrift_988:41) where run380 answers "You are not holding a box."
   * (p38DARK, Adrift_983:48 / Adrift_984:41), i.e. run380 rewrote the line
   * and then refused it on the hold gate.  2026-09-12.  The explicit form
   * is untouched -- run370 plays `get coin from box` perfectly well
   * (Adrift_986:27) and applies the same hold gate to it (Adrift_987:18).
   */
  return obj_indirectly_in_room (game, object, gs_playerroom (game))
         && !obj_is_static (game, object)
         && !(gs_object_position (game, object) == OBJ_HELD_PLAYER
              || gs_object_position (game, object) == OBJ_WORN_PLAYER)
         && !(gs_object_position (game, object) == OBJ_HELD_NPC
              || gs_object_position (game, object) == OBJ_WORN_NPC);
}


/*
 * lib_take_all_filter()
 *
 * The universe that "all" ranges over, which is much narrower than the set
 * a named take can reach: every Runner builds the "all" candidate list from
 * dynamic objects lying directly on the floor of the player's room (run380
 * takes() requires location == current room and static == 0; run400's take
 * handler, Proc_19_6, does the same plus a seen test).  Objects inside or
 * on top of other objects are never included, even when the container is
 * open and its contents revealed -- only naming them, or "take all from X",
 * reaches inside.  A direct consequence is that "all" also leaves alone
 * anything already in the player's possession, including the contents of a
 * carried open container: in Ticket to No Where, holding the open bag of
 * shopping and typing "get all" answers "You take the pamphlet." and leaves
 * the tights, pet food, deodorant and gloves in the bag, while "get paper"
 * still lifts the scrap of paper out of the carried wallet (verified live
 * against run400.exe, 2026-08-02).
 *
 * The seen test does need porting: since obj_mark_room_objects_seen() moved
 * the room's marking into the room lister, a loose object in a room whose
 * description never printed is unseen, and 4.0 leaves it where it lies.
 */
static scr_bool
lib_take_all_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return !obj_is_static (game, object)
         && gs_object_position (game, object) == gs_playerroom (game) + 1
         && (!lib_matcher_requires_seen (game)
             || gs_object_seen (game, object));
}


static scr_bool lib_take_from_filter (scr_gameref_t game, scr_int object,
                                      scr_int associate);

/*
 * lib_take_all_sweep_390()
 *
 * 3.9's `take all` also empties every open container and every surface in
 * the room, after the floor.  run390 takes() (4558CE-455B28) runs, when the
 * line has "all", over every object that lies on the room's floor or is a
 * static present in the room, and is an open container or a surface; for each
 * it sets the line to "get all" and calls insides() with it, which is the
 * take-from handler (its gate is `c("from") Or (c("all") And container > 0)`).
 * The floor's own text is kept only if the floor pass took something (var_86,
 * set at 454FC5); every piece that took something is joined to the previous
 * one by two spaces.  A container with nothing the player has seen in it
 * says nothing, and a closed one is not visited.  No 3.7/3.8/4.0 Runner has
 * the loop ("get all" is a run390-only literal).
 *
 * Measured on p39PUT (run390x Adrift_154_p39put.txt T23; feeds
 * cmdfile_p39takeall.txt / cmdfile_p39takeall2.txt, Adrift_p39takeall.txt /
 * Adrift_p39takeall2b.txt, 2026-09-19): "You take the coin, ... and the
 * table from the cupboard." with nothing on the floor; "You pick up the
 * table.  You take the gem from the cupboard." with the open chest empty
 * after it; "You take the gem from the cupboard.  You take the coin from the
 * chest."; an unseen gem in the open chest is left behind, and a closed
 * chest is skipped.  The capacity arms (insides() returning 2, and the
 * "can't take any more/anything, as ... hands are full." rewrite at 455A68)
 * are not modelled.
 */
static scr_bool
lib_take_all_sweep_390 (scr_gameref_t game, scr_bool has_printed)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int associate, objects;

  for (associate = 0; associate < gs_object_count (game); associate++)
    {
      if (!obj_directly_in_room (game, associate, gs_playerroom (game)))
        continue;
      if (!((obj_is_container (game, associate)
             && gs_object_openness (game, associate) <= OBJ_OPEN)
            || obj_is_surface (game, associate)))
        continue;

      gs_set_multiple_references (game);
      objects = lib_apply_filter (game,
                                  lib_take_from_filter, associate, FALSE, NULL);
      gs_clear_multiple_references (game);
      if (objects == 0)
        continue;

      if (has_printed)
        pf_buffer_string (filter, "  ");
      lib_take_from_single_named = FALSE;
      lib_take_from_object_backend (game, associate);
      has_printed = TRUE;
    }
  return has_printed;
}


/*
 * lib_cmd_take_all()
 *
 * Attempt to take all objects currently visible to the player.
 */
scr_bool
lib_cmd_take_all (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects;
  scr_bool has_printed;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_take_all_filter, -1, FALSE, NULL);
  gs_clear_multiple_references (game);
  if (objects > 0)
    lib_take_backend (game);
  has_printed = objects > 0;
  if (prop_get_taf_version (gs_get_bundle (game)) == TAF_VERSION_390)
    has_printed = lib_take_all_sweep_390 (game, has_printed);
  if (!has_printed)
    pf_buffer_string (filter,
                      lib_is_version_400 (game)
                      ? "There is nothing worth taking here."
                      : "There is nothing to pick up here.");
  lib_take_refusal_redispatch = FALSE;

  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_take_absent_score()
 *
 * The Runner's co() score for one object against the typed line: 1 if the
 * line contains the whole Short, 1 more for the first Alias it contains,
 * and 1 more for each word of the Prefix that appears in the line (run400
 * General.Sub_22_66 at 000632BE .. 00063387).  Zero means the line names
 * nothing of this object -- a Prefix word alone never does, since the
 * prefix loop is only reached once a name has matched.  *term is left at
 * the name the object answered to, Short before Alias, which is what
 * mdlSpreadTheLoad.Sub_20_43 at 00046BFC hands the ambiguity message.
 */
static scr_int
lib_take_absent_score (scr_gameref_t game, scr_int object,
                       const scr_char *input, const scr_char **term)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *shortname, *prefix;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias, score;

  score = 0;
  *term = NULL;

  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!scr_strempty (shortname) && lib_co_contains (input, shortname))
    {
      score++;
      *term = shortname;
    }

  alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < alias_count; alias++)
    {
      const scr_char *name;

      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (scr_strempty (name) || !lib_co_contains (input, name))
        continue;

      score++;
      if (!*term)
        *term = name;
      break;
    }

  if (score == 0)
    return 0;

  /* Each Prefix word the line also carries is worth another point. */
  prefix = prop_get_indexed_string (bundle, "Objects", object, "Prefix");
  if (!scr_strempty (prefix))
    {
      std::string word;
      const scr_char *cursor;

      for (cursor = prefix; ; cursor++)
        {
          if (*cursor != NUL && !scr_isspace (*cursor))
            {
              word.append (1, *cursor);
              continue;
            }

          if (!word.empty () && lib_co_contains (input, word.c_str ()))
            score++;
          word.clear ();

          if (*cursor == NUL)
            break;
        }
    }

  return score;
}


/*
 * lib_cmd_take_absent()
 *
 * 4.0's named take once nothing here can answer to the noun.  The Runner's
 * take handler (run400 Proc_19_6) resolves the direct object with the co()
 * style whole-name resolver General.Sub_22_66 at 00073011, whose result is
 * 255 for "the line named nothing", a positive index+256 for one object, and
 * -(index + 2) for an ambiguity; the three answers are printed at the tail
 * of the same procedure -- "Take what?" at 0007332B and again as the very
 * last default at 00073A25, "It is not clear which " & <term> & " you are
 * referring to." at 0007335C, and "There is nothing worth taking here." at
 * 00073A13, which is where a line that resolved an object but took nothing
 * lands (the `If var_92 > 255` at 00073798 is the "take X from Y" test, so a
 * plain named take falls straight through to it).
 *
 * The resolver runs its scan TWICE.  Its first pass (mode 1, at 00063161)
 * only considers what Sub_22_61 calls present -- held, worn, in the room, or
 * inside an open container there -- and if that pass ends with no candidate
 * at all it resets the mode to 0 and jumps back to the top of the procedure
 * (00063465: `If var_9C = 1 And var_A0 = 0 And param_10 > 0`, then Branch
 * 000630BC), where the test at 0006310D is the object's seen byte alone.  So
 * the noun that names nothing here is offered every object the player has
 * ever SEEN, and that second pass is what this function reproduces.
 *
 * What it ranges over is measured rather than argued:
 * harness/make_400_takeprobe.py builds p4TAKE (a coin loose in Alpha, a
 * statue static in Alpha, a widget and a gizmo loose in Bravo), with --tie
 * p4TAKE2 (a red widget and a blue widget, both Short "widget" with Prefix
 * "a red"/"a blue", plus a lamp aliased "light", all in Bravo), and with
 * --hidden p4TAKE3 (the same pair, with tasks that move either or both of
 * them to hidden).  Six feeds under Wine, run400, transcripts
 * Adrift_p4take1 .. Adrift_p4take6 in the harness prefix:
 *
 *     take widget   (Bravo never entered)        ->  Take what?
 *     take widget   (from Alpha, Bravo seen)     ->  There is nothing worth
 *                                                    taking here.
 *     take statue   (from Bravo, Alpha seen)     ->  There is nothing worth
 *                                                    taking here.
 *     take light    (from Alpha, an ALIAS)       ->  There is nothing worth
 *                                                    taking here.
 *     take widget   (two seen absent namesakes)  ->  It is not clear which
 *                                                    widget you are referring
 *                                                    to.
 *     take widget   (one, then both, hidden)     ->  It is not clear which
 *                                                    widget you are referring
 *                                                    to.
 *     take red      (a Prefix word only)         ->  Take what?
 *     take zzz                                   ->  Take what?
 *     take widget   (both present, in Bravo)     ->  Which widget.  The red
 *                                                    widget or the blue
 *                                                    widget?
 *
 * So the candidate set is the objects the player has SEEN -- the same seen
 * byte the examine path reads, and the same one that makes `x widget` answer
 * "You can't see the widget from here!" in the very next command of
 * Adrift_p4take2 -- matched by WHOLE name, Short or Alias, with a Prefix word
 * naming nothing on its own.  Statics count (the statue) and so do hidden
 * objects (p4TAKE3's pair still ties with both of them nowhere).  A candidate
 * that is present hands the line back to the ordinary path, which is where
 * the present tie's co() prompt is raised.
 *
 * The scores then decide between the two refusals, and only a genuine tie
 * for the best score is ambiguous.  That is what zelda's `get key` turns on:
 * its super-hot key is Short "key" with an Alias "key" as well, so it scores
 * 2 against the iron key's 1 and wins outright -- run400 answers with the
 * flat refusal (Adrift_319_zelda.txt:573) even though both keys are seen and
 * both are called "key".
 */
scr_bool
lib_cmd_take_absent (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *best_term = NULL;
  scr_int object, best_score, best_count, best_object;

  /* Pre-3.9 takes() names the first match out of reach, seen or not; see
   * lib_absent_named_object_pre_390(). */
  object = lib_absent_named_object_pre_390 (game, TRUE);
  if (object != -1)
    return lib_cant_see_named_pre_390 (game, object, FALSE, " from here!\n");

  if (!lib_is_version_400 (game) || !input)
    return FALSE;

  /*
   * An "and" list (473121-473221) resolves each piece on its own in mode
   * 1, which scores only what is here, so absent objects never tie: with
   * nothing present marked, the handler runs to its empty-buffer tail,
   * "There is nothing worth taking here." (473A13) -- never "Take what?".
   * Measured on p4AND from Bravo, `get hat and coin` with both seen in
   * Alpha (run400 Adrift_956).
   */
  if (lib_input_contains_word_400 (input, "and"))
    {
      for (object = 0; object < gs_object_count (game); object++)
        {
          const scr_char *term;

          if (lib_take_absent_score (game, object, input, &term) > 0
              && obj_indirectly_in_room (game, object, gs_playerroom (game)))
            return FALSE;
        }
      pf_buffer_string (filter, "There is nothing worth taking here.\n");
      return TRUE;
    }

  best_score = 0;
  best_count = 0;
  best_object = -1;

  for (object = 0; object < gs_object_count (game); object++)
    {
      const scr_char *term;
      scr_int score;

      score = lib_take_absent_score (game, object, input, &term);
      if (score == 0)
        continue;

      /* Something the noun names is here; the ordinary path handles it. */
      if (obj_indirectly_in_room (game, object, gs_playerroom (game)))
        return FALSE;

      if (!gs_object_seen (game, object))
        continue;

      if (score > best_score)
        {
          best_score = score;
          best_count = 1;
          best_term = term;
          best_object = object;
        }
      else if (score == best_score)
        {
          best_count++;
          best_term = term;
        }
    }

  /* Nothing the player has seen answers to it; "Take what?" says so. */
  if (best_count == 0)
    return FALSE;

  if (best_count > 1)
    {
      pf_buffer_string (filter, "It is not clear which ");
      pf_buffer_string (filter, best_term);
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             " you are referring to.\n",
                                             " I am referring to.\n",
                                             " %player% is referring"
                                             " to.\n"));
      return TRUE;
    }

  /*
   * A unique seen-absent winner sitting inside a closed container answers
   * for the container instead: the plain take is rewritten into an
   * implicit "take from" (run400 mdlSpreadTheLoad auto-from rewrite
   * 47302F) and lands in the SAME closed-container arm as an explicit one
   * ("<The X> is closed." 47395C).  Measured escape_to_new_york turn 152
   * `get parcel`, the parcel seen but absent, inside a closed parent
   * (Ticket run400 xoshiro trace 2026-09-12).
   *
   * An OPEN parent gets past that arm, and the take-from then finds
   * nothing it can reach and ends at "Take what?" (47332B).  British Fox
   * T147 `get silver key`: the key was listed by `open bag` (the lister
   * 46A950 stamps it seen), then an event put the bag back on the dungeon
   * table and the player in the corridor.  run400 answers "Take what?"
   * where the plain refusal was "There is nothing worth taking here."
   * (runner_transcripts britishfox, 2026-09-24).
   */
  if (gs_object_position (game, best_object) == OBJ_IN_OBJECT)
    {
      scr_int parent = gs_object_parent (game, best_object);

      if (obj_is_container (game, parent)
          && gs_object_openness (game, parent) > OBJ_OPEN)
        {
          pf_new_sentence (filter);
          lib_print_object_np (game, parent);
          pf_buffer_string (filter, " is closed.\n");
          return TRUE;
        }
      if (obj_is_container (game, parent))
        return FALSE;
    }

  pf_buffer_string (filter, "There is nothing worth taking here.\n");
  return TRUE;
}


/* Set by lib_cmd_get_what() only; see the scored fallback below. */
static scr_bool lib_take_scored_fallback = FALSE;

static const scr_char *lib_drop_named_term_400 (scr_gameref_t game,
                                                scr_int object,
                                                const scr_char *input,
                                                scr_bool last_alias);
static scr_int lib_take_resolve_400_string (scr_gameref_t game,
                                            const scr_char *input,
                                            std::vector<scr_int> *tied);

/*
 * lib_take_tie_400()
 *
 * get_piece's two answers to a tie from 463640 (see
 * lib_take_whole_line_400()): the flat "It is not clear which ..." with no
 * pending object, else the Which question.  Returns FALSE, having printed
 * nothing, only for a list the Runner would run together.
 */
static scr_bool
lib_take_tie_400 (scr_gameref_t game, const scr_char *line, scr_int pending,
                  scr_int last_tied, const std::vector<scr_int> &marked,
                  scr_int mark_count)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (pending < 0)
    {
      pf_buffer_string (filter, "It is not clear which ");
      pf_buffer_string (filter,
                        lib_drop_named_term_400 (game, last_tied, line,
                                                 FALSE));
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             " you are referring to.",
                                             " I am referring to.",
                                             " %player% is referring to."));
      pf_buffer_answer_break (filter);
      gs_clear_multiple_references (game);
      return TRUE;
    }

  /* Me(428) is joined by a countdown from the pass-0 count; with more marks
   * than that the Runner runs names together, unmeasured. */
  if ((scr_int) marked.size () != mark_count)
    return FALSE;

  lib_co_400_raise_named (game,
                          prop_get_indexed_string (gs_get_bundle (game),
                                                   "Objects", last_tied,
                                                   "Short"),
                          marked);
  gs_clear_multiple_references (game);
  return TRUE;
}

/*
 * lib_take_whole_line_400()
 *
 * get_piece (Proc_19_23_473A34) names a take's object from the WHOLE
 * fragment before it parses anything: 473011 hands the line to the noun
 * scorer 463640 in mode 1 (lib_name_object_resolve_400()), and what comes
 * back decides the piece whatever else the line said.  A unique winner is
 * the only object marked (473022): p4WTIE (run400, Adrift_wtie12/16/17,
 * 2026-09-20) has `take rope with ruby` take the ruby and `take rope with
 * knife` and `take zzz with knife` answer "You are already carrying the
 * knife." -- the static rope is never a candidate, the held knife only on
 * the second pass.  A tie with a pending object asks (4733BD): "Which " &
 * the LAST tied object's raw Short & ".  " & the pass-0 marks & "?", so
 * `take ruby with stone` is "Which ruby.  The red stone, the blue stone or
 * the ruby?" and `take knife with stone`, the knife skipped on pass 0, is
 * "Which stone.  The red stone or the blue stone?" (Adrift_wtie16/17).  A
 * tie with none (Me(424) < 0, 47333C) is the flat "It is not clear which "
 * & <the last tied object's typed name, 446C74> & " you are referring to."
 * (47335F-4733A5): p4AND `get coin, hat`, both held, "It is not clear
 * which hat you are referring to." (Adrift_955), the coin scoring because
 * c() ends a word at the comma.
 *
 * Returns TRUE when it has answered the line.  *BOUND is set when a unique
 * winner is now the one referenced object, and the caller skips its own
 * parse; a line the scorer names nothing on keeps the parser's flow.
 */
static scr_bool
lib_take_whole_line_400 (scr_gameref_t game, scr_int *references,
                         scr_bool *bound)
{
  const scr_char *line = run_get_dispatch_input ();
  std::vector<scr_int> marked;
  scr_int object, pending, last_tied, mark_count;

  *bound = FALSE;
  if (!lib_is_version_400 (game) || !line
      || lib_input_contains_word_400 (line, "all")
      || lib_input_contains_word_400 (line, "and"))
    return FALSE;

  /* A "from" line names its container first (472E03-472F35), so a line
     whose container resolves nothing is "I don't understand where you want
     to get things from." however its piece ties: `take stone from zzz`
     (p4WTIE, Adrift_wtfrom).  lib_take_from_piece_400() scores the piece. */
  if (lib_input_contains_word_400 (line, "from"))
    return FALSE;

  object = lib_name_object_resolve_400 (game, line, 1, &pending, &last_tied,
                                        &marked, &mark_count);
  if (object >= 0)
    {
      gs_clear_multiple_references (game);
      game->multiple_references[object] = TRUE;
      *references = 1;
      *bound = TRUE;
      return FALSE;
    }
  if (object != -1)
    return FALSE;

  return lib_take_tie_400 (game, line, pending, last_tied, marked,
                           mark_count);
}

/*
 * lib_take_from_task_sweep_380()
 *
 * run380's insides() ends every take-from that found its source object with
 * a sweep over the whole object table (loc_447405): for each object with a
 * parent it stores "get " & Short & " from " & parent's Short into the line
 * (ImpAdStStr @00047446, hidden by the decompiler) and, if checktask passes
 * on it, runs tasks(1) there.  The object just taken has its parent cleared
 * (4470EF) and so is skipped.  tra.taf, run380, 2026-09-14: task 11 `get
 * *knives*` (knives in the silverware drawer) runs after `get meat from
 * refrigerator`, and after the bare `get moxie` / `get pop-tarts` that
 * takes() rewrites into a take-from (43E47B) -- "You take old meat from the
 * big white refrigerator.  You take all of the knives from the silverware
 * drawer." (Adrift_1181/1183/1185_kn*.rtf; Adven_9_timmy_reid.rtf turn 8).
 * `get garbage container`, off the floor, runs nothing (Adrift_1186).
 * run370 has no sweep, and run390 none either.
 */
static void
lib_take_from_task_sweep_380 (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = prop_get_taf_version (bundle);
  scr_int object;

  if (version < TAF_VERSION_380 || version >= TAF_VERSION_390)
    return;

  /* The task's text joins the take's own line after two spaces. */
  pf_buffer_join_pending (gs_get_filter (game));
  for (object = 0; object < gs_object_count (game); object++)
    {
      std::string line;

      if (gs_object_position (game, object) != OBJ_IN_OBJECT
          && gs_object_position (game, object) != OBJ_ON_OBJECT)
        continue;

      line = "get ";
      line += prop_get_indexed_string (bundle, "Objects", object, "Short");
      line += " from ";
      line += prop_get_indexed_string (bundle, "Objects",
                                       gs_object_parent (game, object),
                                       "Short");
      run_game_task_commands (game, line.c_str ());
    }
  pf_clear_join_pending (gs_get_filter (game));
}

/*
 * lib_take_held_namesake_preempt_pre400()
 *
 * The pre-4.0 takes() walks EVERY object in index order and lets each one
 * whose Short or Alias sits in the typed line write the answer, whether or
 * not it is the object the line meant.  A held one writes "<You>'ve already
 * got <object>!" (run390 @454EC5, run380 @43E03E, run370 @436561) and a
 * worn one "<You> are already wearing <object>!" (run390 @454F31, run380
 * @43E0A2), both raising the outcome rank to 4.  The branch that takes an
 * object out of a container, off a surface or from a character -- the one
 * that rewrites the line into "take X from <parent>" -- is gated on the
 * buffer being still empty (or " pick up "/" from here!") AND rank < 3
 * (run390 @4551E4, run380 @43E3F6/@43E41F), so a held namesake indexed
 * BELOW the target has already spoken and the target is never taken.  An
 * object lying loose in the room is different: "<You> pick up X." overwrites
 * the buffer (run390 @455014), so only the not-in-the-room targets lose.
 *
 * Measured in run390 (runner_transcripts/stardust T38, 2026-09-19): `take
 * needle box` with the sharp needle (object 3) in hand and the needle box
 * (object 12) on the desk answers "You've already got the sharp needle!"
 * and leaves the box where it was -- T99 `put needle in box` is then "You
 * don't have the box.".  secret_of_lost_world T53 `take blue gem` with the
 * green gem (11) held and the blue gem (12) on the fresh skeleton: "You
 * already have the green gem!" (the game's ALR of "You've already got"),
 * the blue gem still listed on the skeleton at T55.  Scarier resolved the
 * line to the box / the blue gem alone and took it.
 *
 * Only the single-named plain take: the "and" list and "all" walk other
 * modes of the loop.  Below 3.9 the object prints its raw prefix, as the
 * already-got list below does (p38EXAM, Adrift_1165).  3.7 has no
 * from-parent branch at all -- a bare take of something in or on an object
 * writes nothing (lib_take_filter) -- so there the namesake speaks from
 * either side of the target; the last one in index order, as every write
 * overwrites.  A 3.8/3.9 namesake ABOVE the target is left alone: the
 * rewrite has run by the time it speaks and what the caller does with the
 * rewritten line is unmeasured.
 */
/*
 * The test the takes() loop puts each object through before letting it
 * write: run390 @454E1D co(obj, 1), run380 @43DFC3 co(obj), run370 @4364E6
 * a bare c(Short).  co() (run390 @43B6BC, run380 @42DE60) is false unless
 * the object's Short or Alias sits in the line; the term found is then
 * counted over every present object answering to it (obhere, and seen from
 * 3.9 -- lib_co_candidate).  With more than one, 3.9's mode 1 recounts
 * only the ones lying loose in the player's room (@43B37B, field 22 =
 * room) and uses that count when it is not zero; if the term is still
 * ambiguous the object passes only when the last word of its own Prefix is
 * in the line (@43B572 / @42DD47), otherwise 3.9 asks that the object
 * itself be present and seen (@43B661) where 3.8 passes it outright
 * (@42DDEA).  So `take blue plate` with the red plate in hand and the blue
 * one seen in the dishwasher does NOT let the red plate speak: two "plate"s
 * present, none loose, and "red" is not in the line -- deardiary T52
 * (runner_transcripts/deardiary, run390): "I take a blue plate from the
 * dishwasher.".  Nor does life_of_mike T28 `take truck keys` (run380) with
 * the mustang keys, aliased "keys" too, in hand.
 */
/* run390 isheld() @42A34C: field 22 is 0 (held), &H9C (worn), or &HF6/&HEC
 * (in/on) with a parent that isheld; nothing else counts. */
static scr_bool
lib_isheld_390 (scr_gameref_t game, scr_int object)
{
  scr_int depth;

  for (depth = 0; depth < 64; depth++)
    {
      if (obj_is_static (game, object)
          && gs_object_static_unmoved (game, object))
        return FALSE;
      switch (gs_object_position (game, object))
        {
        case OBJ_HELD_PLAYER:
        case OBJ_WORN_PLAYER:
          return TRUE;
        case OBJ_IN_OBJECT:
        case OBJ_ON_OBJECT:
          object = gs_object_parent (game, object);
          continue;
        default:
          return FALSE;
        }
    }
  return FALSE;
}

scr_bool
lib_co_pre400 (scr_gameref_t game, const scr_char *line, scr_int object,
               scr_int mode)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int taf_version = prop_get_taf_version (bundle);
  const scr_char *shortname, *term;
  scr_int room, other, count;

  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  term = lib_co_contains (line, shortname) ? shortname : NULL;
  if (!term && taf_version >= TAF_VERSION_380)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; alias < alias_count && !term; alias++)
        {
          const scr_char *alias_name;

          vt_key[3].integer = alias;
          alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (lib_co_contains (line, alias_name))
            term = alias_name;
        }
    }
  if (!term || lib_co_term_shadowed (game, line, object, term))
    return FALSE;
  if (taf_version < TAF_VERSION_380)
    return TRUE;

  room = gs_playerroom (game);
  count = 0;
  for (other = 0; other < gs_object_count (game); other++)
    {
      if (lib_co_candidate (game, other, room)
          && lib_co_object_answers_to (game, other, term))
        count++;
    }
  /*
   * Mode 1 is takes()' (run390): the recount is over the loose namesakes.
   * Mode 2 is drops()' (run390 4458CF): the recount is over isheld()
   * (42A34C) -- held, worn, or in or on a parent that isheld, recursively,
   * no openness test.  So `drop hat` with the red hat in hand and the blue
   * one on the floor recounts to one and drops the red hat, while red worn
   * and blue held recounts to two and drops nothing (p39TASK,
   * Adrift_185_pname_39.txt / Adrift_185_pname2_39.txt, 2026-09-19).
   *
   * Deliberate deviation: run380 has no modes, and no Runner narrows wear
   * or remove, so `wear hat` with the red hat held and the blue one on the
   * floor wears nothing under "Which hat."  Scarier recounts at 3.8 too,
   * and mode 3 (wear: held, not worn) and mode 4 (remove: worn) do the
   * same for those verbs.  See lib_co_mode_fits().
   */
  if (count > 1 && mode != 0)
    {
      scr_int fitting = 0;

      for (other = 0; other < gs_object_count (game); other++)
        {
          if (lib_co_object_answers_to (game, other, term)
              && lib_co_mode_fits (game, other, mode))
            fitting++;
        }
      if (fitting > 0)
        count = fitting;
    }
  if (count > 1)
    return lib_co_names_prefix (game, line, object);
  if (taf_version >= TAF_VERSION_390)
    return lib_co_candidate (game, object, room);
  return TRUE;
}

static scr_bool
lib_take_co_pre400 (scr_gameref_t game, const scr_char *line, scr_int object)
{
  return lib_co_pre400 (game, line, object, 1);
}


/*
 * lib_take_crowd_pre400()
 *
 * Below 4.0 a plain take COUNTS before it acts, and two objects named on
 * one line stop it dead.  takes() pre-passes the whole object table (run390
 * 454B95-454C9D, run380 43DCFB-43DE04, run370 4361ED-436301) and, for every
 * dynamic object lying directly in the player's room that co(obj, 1) -- a
 * bare c(Short) at 3.70 -- finds in the line, raises two counters: one for
 * the match (run390 454BF5, run380 43DD53, run370 436250) and, when the last
 * word of that object's own Prefix is NOT typed, a second (454C25 / 43DD8C /
 * 436289) that also stamps a per-object flag (454C85 / 43DDEC / 4362E9).
 * Only when BOTH counters come out at exactly 1 are the flags wiped again
 * (454CF2 / 43DE66 / 436363), and the loop that takes objects passes over
 * every object whose flag still stands (454E47-454E62 `... And var_118(obj)
 * = 0`, run380 43DFC3).  So a line naming two present, takeable objects
 * takes NEITHER, writes nothing, and comes out of takes() at the buffer-
 * empty catch-all as "Take what?" (run390 45588D).  The "Which <X> would you
 * like to take." prompt two lines above that clear (454CAD / 43DE41 /
 * 43633E) is dead code in all three: its guard is `var_10C > var_11A` and
 * the second counter can never outrun the first.
 *
 * A HELD object is not a candidate -- the pre-pass tests field 22 against
 * the room constant -- so it neither blocks nor is blocked, and the main
 * loop still lets it write "<You>'ve already got <X>!" before a loose object
 * indexed above it overwrites that with the take.  Hence `take hat coin`
 * with the hat in hand is "You pick up the coin." and carries both, while
 * `take coin hat` with both on the floor is "Take what?" and carries
 * neither.  A word that names nothing costs nothing: `take zzz coin` takes
 * the coin.  4.00 dropped the pre-pass; there the line is the noun scorer's
 * and the answer is "It is not clear which <object> you are referring to."
 * for the highest-indexed object it names, which Scarier already gives.
 *
 * Measured 2026-09-21 on p37ORD/p38ORD/p39ORD/p4ORD with `cmdfile_p2mult
 * .txt` (Adrift_267_3m37.rtf, 268_3m38.rtf, 269_3m39.txt, 270_3m40.txt).
 *
 * Returns the object the loop is left free to take, or -1; sets *crowded
 * when every loose candidate was flagged and the take is off.
 */
static scr_int
lib_take_crowd_pre400 (scr_gameref_t game, scr_bool *crowded)
{
  const scr_char *const line = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  scr_int object, counted, flagged, acting, last_flagged, last_acting;

  *crowded = FALSE;
  if (!line
      || prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400
      || lib_input_contains_word (line, "all")
      || lib_input_contains_word (line, "and"))
    return -1;

  counted = flagged = acting = 0;
  last_flagged = last_acting = -1;
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (obj_is_static (game, object)
          || gs_object_position (game, object) != room + 1
          || !lib_co_pre400 (game, line, object, 1))
        continue;
      counted++;
      if (lib_co_names_prefix (game, line, object))
        {
          acting++;
          last_acting = object;
        }
      else
        {
          flagged++;
          last_flagged = object;
        }
    }

  if (counted == 1 && flagged == 1)
    return last_flagged;
  if (acting > 0)
    return last_acting;
  if (counted > 0)
    *crowded = TRUE;
  return -1;
}

static scr_bool
lib_take_held_namesake_preempt_pre400 (scr_gameref_t game,
                                       scr_bool unresolved)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int taf_version = prop_get_taf_version (bundle);
  const scr_char *line = run_get_dispatch_input ();
  scr_int object, target, limit, speaker;

  if (!line || taf_version >= TAF_VERSION_400)
    return FALSE;

  /*
   * With no object resolved at all (unresolved: the parser found nothing
   * for the noun, as when the only namesake of the typed words is unseen),
   * takes() still walks every object, and a held or worn one whose co()
   * passes still writes the buffer: secret_of_lost_world T53 `take blue gem`
   * with the blue gem unseen on the fresh skeleton and the green gem in hand
   * answers "You already have the green gem!" (run390, ALR-rewritten
   * "'ve already got "; runner_transcripts/secret_of_lost_world, 2026-09-19).
   */
  /*
   * takes() is entered on c("get") Or c("take") Or c("pick"), And Not
   * c("from"), c("get on"), c("get down") (454476-4544DE), so `take off
   * bathrobe` reaches its loop too and the worn bathrobe writes " are already
   * wearing "; but the loop does not set the function's result (var_86 stays
   * 0 there; 455B28 is the taken exit), so generaltasks goes on to removes()
   * 45F49E, whose own line REPLACES the buffer: the_hangover T50 `take off
   * bathrobe` -> "You remove your bathrobe.", deardiary T5 `take off dress`
   * -> "I remove the blue dress." (runner_transcripts/deardiary).  Scarier's
   * remove rows sit below the take row, so decline here and let them answer.
   * Whether removes() also overwrites a refused `take off <held>` is unread.
   */
  if (unresolved && lib_co_contains (line, "off"))
    return FALSE;

  target = -1;
  if (!unresolved)
    {
      for (object = 0; object < gs_object_count (game); object++)
        {
          if (!game->multiple_references[object])
            continue;
          if (target != -1)
            return FALSE;
          target = object;
        }
      if (target == -1)
        return FALSE;
    }

  switch (unresolved ? OBJ_IN_OBJECT : gs_object_position (game, target))
    {
    case OBJ_IN_OBJECT:
    case OBJ_ON_OBJECT:
      break;
    case OBJ_HELD_NPC:
    case OBJ_WORN_NPC:
      if (taf_version < TAF_VERSION_380)
        return FALSE;
      break;
    default:
      return FALSE;
    }

  limit = taf_version < TAF_VERSION_380 || unresolved
          ? gs_object_count (game) : target;
  speaker = -1;
  for (object = 0; object < limit; object++)
    {
      if (object == target)
        continue;
      if (gs_object_position (game, object) != OBJ_HELD_PLAYER
          && gs_object_position (game, object) != OBJ_WORN_PLAYER)
        continue;
      /* Deliberate deviation: a held namesake whose Prefix the line rules
         out (`take old pin`, the new red pin in hand) does not speak; see
         lib_co_names_prefix(). */
      if (lib_take_co_pre400 (game, line, object)
          && !lib_co_prefix_excluded (game, line, object))
        speaker = object;
    }
  if (speaker == -1)
    return FALSE;

  if (gs_object_position (game, speaker) == OBJ_HELD_PLAYER)
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           "You've already got ",
                                           "I've already got ",
                                           "%player%'ve already got "));
  else
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           "You are already wearing ",
                                           "I am already wearing ",
                                           "%player% is already wearing "));
  if (taf_version >= TAF_VERSION_390)
    lib_print_object_np (game, speaker);
  else
    lib_print_object_raw (game, speaker);
  pf_buffer_string (filter, "!\n");
  gs_clear_multiple_references (game);
  return TRUE;
}

/*
 * lib_take_and_none_pre400()
 *
 * takes()'s "and" arm (var_A8/var_B4 = 2, set when the line has c("and")
 * and no c("all"): run390 45455D, run380 43D81A, run370 435EBA) pre-passes
 * every object before taking anything.  A dynamic object the line names is
 * a candidate when it lies loose in the player's room, or -- 3.8 and 3.9 --
 * in or on something loose here or a static present here; it either fits
 * or sets the hands-full flag.  With no candidate at all the summary
 * answers and nothing else in takes() speaks: 3.9 (454B08-454B5B) says
 * "<You> can't get either of them." when exactly two non-static objects
 * were named (var_122), else "... any of them."; run380 43DCC1 and run370
 * 4361B3 have the "any" form only.
 *
 * The versions name and filter differently.  run390 names with co(obj, 1)
 * (45483A) and wants the object seen (o(44)); run380 names with co(obj)
 * (43DA69) and has no seen test, so p38PUT's gem in the open static chest,
 * never listed, is a candidate and `take gem and coin` takes it; run370
 * names by c(Short) (43607F; c(Alias) only rewrites the line first) and
 * counts nothing in or on anything, so there the same line is "You can't
 * get any of them.".  Held objects and characters are named but never
 * candidates: p39ABSNPC `take erin and stone` with the stone held (run390x
 * Adrift_1206_p39absnpc.txt T36), and p3xPUT `take coin and stone`, `take
 * nut and coin` (the nut in the held bag), `take statue and cupboard`,
 * `take zzz and qqq` (run370x/run380x/run390x Adrift_170/171/172_ptakeand,
 * cmdfile_p3738takeand.txt).  Each Runner's pre-pass also skips an object
 * whose "get <Short>" pre-matches a task; not modelled.
 */
static scr_bool
lib_take_and_none_pre400 (scr_gameref_t game)
{
  const scr_int taf_version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *line = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  scr_int object, named = 0;

  if (taf_version >= TAF_VERSION_400
      || !line || lib_input_contains_word (line, "all")
      || !lib_input_contains_word (line, "and"))
    return FALSE;

  for (object = 0; object < gs_object_count (game); object++)
    {
      scr_int position;

      if (obj_is_static (game, object) || !lib_take_co_pre400 (game, line, object))
        continue;
      named++;
      if (taf_version >= TAF_VERSION_390 && !gs_object_seen (game, object))
        continue;
      position = gs_object_position (game, object);
      if (obj_directly_in_room (game, object, room))
        return FALSE;
      if (taf_version >= TAF_VERSION_380
          && (position == OBJ_IN_OBJECT || position == OBJ_ON_OBJECT)
          && obj_directly_in_room (game, gs_object_parent (game, object), room))
        return FALSE;
    }

  pf_buffer_string (gs_get_filter (game),
                    lib_select_response (game, "You", "I", "%player%"));
  pf_buffer_string (gs_get_filter (game),
                    named == 2 && taf_version >= TAF_VERSION_390
                    ? " can't get either of them.\n"
                    : " can't get any of them.\n");
  return TRUE;
}


/*
 * lib_instr_nocase()
 *
 * VB's InStr(1, line, name, vbTextCompare): the 1-based position of the
 * first case-insensitive occurrence of name in line, 0 when absent or when
 * name is empty.
 */
static scr_int
lib_instr_nocase (const scr_char *line, const scr_char *name)
{
  size_t length, offset;

  if (!line || !name || !*name)
    return 0;
  length = strlen (name);
  for (offset = 0; line[offset]; offset++)
    {
      if (scr_strncasecmp (line + offset, name, length) == 0)
        return (scr_int) offset + 1;
    }
  return 0;
}


/*
 * lib_name_instr_range()
 *
 * The lowest and highest InStr positions of an object's names on the line:
 * its Short and, 3.8 up, each Alias (run370 has rewritten aliases into the
 * Short by now).  Names absent from the line count for nothing.  FALSE when
 * none of the names is on the line.
 */
static scr_bool
lib_name_instr_range (scr_gameref_t game, const scr_char *line,
                      scr_int object, scr_int *lowest, scr_int *highest)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int position;

  *lowest = 0;
  *highest = 0;
  position = lib_instr_nocase (line,
                               prop_get_indexed_string (bundle, "Objects",
                                                        object, "Short"));
  if (position > 0)
    *lowest = *highest = position;

  if (prop_get_taf_version (bundle) >= TAF_VERSION_380)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; alias < alias_count; alias++)
        {
          vt_key[3].integer = alias;
          position = lib_instr_nocase (line,
                                       prop_get_string (bundle, "S<-sisi",
                                                        vt_key));
          if (position <= 0)
            continue;
          if (*lowest == 0 || position < *lowest)
            *lowest = position;
          if (position > *highest)
            *highest = position;
        }
    }
  return *highest > 0;
}


/*
 * lib_obhere_380()
 *
 * run380's obhere() (4272E8, Form1.frm:4950; run370 has the same).  A
 * dynamic object is here when it is held, worn, loose in the player's room,
 * held or worn by a character in the room, or IN a parent that is here and
 * not closed; ON is not tested at all (only &HF6 recurses), so an object on
 * a surface is never "here" to it.  A static object is here when its room
 * list holds the player's room.
 */
static scr_bool
lib_obhere_380 (scr_gameref_t game, scr_int object)
{
  const scr_int room = gs_playerroom (game);
  scr_int parent;

  if (obj_is_static (game, object))
    return obj_indirectly_in_room (game, object, room);

  switch (gs_object_position (game, object))
    {
    case OBJ_HELD_PLAYER:
    case OBJ_WORN_PLAYER:
      return TRUE;
    case OBJ_HELD_NPC:
    case OBJ_WORN_NPC:
      parent = gs_object_parent (game, object);
      return parent >= 0 && parent < gs_npc_count (game)
             && npc_in_room (game, parent, room);
    case OBJ_IN_OBJECT:
      parent = gs_object_parent (game, object);
      return parent >= 0 && parent < gs_object_count (game)
             && lib_obhere_380 (game, parent)
             && !(obj_is_container (game, parent)
                  && gs_object_openness (game, parent) > OBJ_OPEN);
    case OBJ_ON_OBJECT:
      return FALSE;
    default:
      return gs_object_position (game, object) == room + 1;
    }
}


/*
 * lib_present_370()
 *
 * What run370's catch-all counts as present when it names the object of a
 * take-from line it did not answer (lib_take_from_answer_370): a static
 * object in the room, or a dynamic one loose here, held, worn, or in or on
 * something the player carries.  Something in a box on the floor is not
 * present to it -- `get nut from statue` names the statue, not the nut
 * (p37TKA, run370x Adrift_205_ptka_37.rtf cell 122, 2026-09-20).
 */
static scr_bool
lib_present_370 (scr_gameref_t game, scr_int object)
{
  const scr_int room = gs_playerroom (game);

  if (obj_is_static (game, object))
    return obj_indirectly_in_room (game, object, room);

  switch (gs_object_position (game, object))
    {
    case OBJ_HELD_PLAYER:
    case OBJ_WORN_PLAYER:
      return TRUE;
    case OBJ_IN_OBJECT:
    case OBJ_ON_OBJECT:
      return obj_indirectly_held_by_player (game,
                                            gs_object_parent (game, object));
    default:
      return obj_directly_in_room (game, object, room);
    }
}


/*
 * lib_take_from_head()
 *
 * The words of a take-from line before its "from" (at offset at), minus the
 * verb word and the "up" of "pick up": the names the line gives, trimmed.
 */
static std::string
lib_take_from_head (const scr_char *line, scr_int at)
{
  std::string head (line, (size_t) at);
  size_t start, end;

  start = head.find_first_not_of (' ');
  if (start == std::string::npos)
    return "";
  end = head.find (' ', start);
  if (end == std::string::npos)
    return "";
  if (scr_strncasecmp (head.c_str () + start, "pick", 4) == 0 && end - start == 4)
    {
      size_t up = head.find_first_not_of (' ', end);

      if (up != std::string::npos
          && scr_strncasecmp (head.c_str () + up, "up", 2) == 0
          && (head.size () == up + 2 || head[up + 2] == ' '))
        end = up + 2;
    }
  head = head.substr (end);
  start = head.find_first_not_of (' ');
  if (start == std::string::npos)
    return "";
  end = head.find_last_not_of (' ');
  return head.substr (start, end - start + 1);
}


/*
 * lib_take_from_slot_valid_pre390()
 *
 * The container tests run380's insides() applies to its slot, silently:
 * a container or surface (field 29), held or worn when dynamic, not closed.
 */
static scr_bool
lib_take_from_slot_valid_pre390 (scr_gameref_t game, scr_int slot)
{
  if (slot < 0)
    return FALSE;
  if (!(obj_is_container (game, slot) || obj_is_surface (game, slot)))
    return FALSE;
  if (obj_is_container (game, slot)
      && gs_object_openness (game, slot) > OBJ_OPEN)
    return FALSE;
  return !lib_take_container_unheld (game, slot);
}


static scr_bool lib_take_from_and_line (scr_gameref_t game);
static scr_bool lib_take_from_and (scr_gameref_t game);
static scr_bool lib_take_from_trailing (scr_gameref_t game);
static void lib_take_from_empty (scr_gameref_t game, scr_int associate,
                                 scr_bool is_except);

/*
 * lib_take_and_pre400()
 *
 * Pre-4.0 takes()'s "and" arm, once lib_take_and_none_pre400() has found a
 * candidate (run390 454B08-455B28, run380 43D788-43E996, run370's twin).
 * The message is seeded with "<You> pick up " and the loop then runs over
 * every non-static object the line names, in INDEX order, never in line
 * order:
 *
 *   held      "<You>'ve already got X!" REPLACES the message, but only once
 *             it no longer starts with the seed (run380 43E00E); while it
 *             does, a held object is passed over in silence.
 *   worn      3.8 up: "<You> are already wearing X!" replaces the message
 *             (43E0A2), with no such gate; 3.7 has no worn arm.
 *   loose     taken; the name is appended with ", ", " and " or "." by the
 *             count of candidates still to come (43E61F-43E6C2), or with
 *             the hands-full line when it will not fit (43E7C1).
 *   in a      3.8 up, while the message is still empty or the seed, or ends
 *   parent    " from here!": obhere() -> the line gets " from <parent>"
 *             appended for insides() to answer (43E47B); else the message
 *             becomes "<You> can't see X from here!".  ON is not tested.
 *
 * so a worn object named first and a loose one after it print as one string,
 * "You are already wearing a hat!the stone." (run380x) / "You are already
 * wearing the hat!the stone." (run390x); a held one is silent: `take hat and
 * stone` with the hat in hand is "You pick up the stone.".  A tail that is
 * still the bare seed is "Please take objects from one place at a time."
 * (43E996; 3.9 4558B9; run370 has no such line), and the rewritten line then
 * reaches insides(), whose answer -- the take-from, or its nothing-inside
 * refusal -- replaces that one: `take nut and cup` with the nut in a held
 * open box is "You take a nut from the box.", while `take nut and key` with
 * the box on the floor stays at "Please take objects from one place at a
 * time." because the box is not held (3.8's slot test).
 *
 * Measured on p37TKA/p38TKA/p39TKA (Adrift_205/206/207_ptka, cells 3, 6, 9,
 * 14, 17, 20, 23, 32, 40, 44, 168) and p37TKB/p38TKB/p39TKB
 * (Adrift_207/208/209_ptkb, cells 3, 8, 31, 38, 44), 2026-09-20.  3.9's own
 * loop (454B08) prints the same strings with definite names.
 */
static scr_bool
lib_take_and_pre400 (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int taf_version = prop_get_taf_version (bundle);
  const scr_char *line = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  const scr_char *you;
  scr_int object, candidates, taken, rewrite_parent;
  scr_bool overwritten, appended, cant_see, hands_full, picking;
  size_t mark;

  if (taf_version >= TAF_VERSION_400 || !line
      || lib_input_contains_word (line, "all")
      || !lib_input_contains_word (line, "and"))
    return FALSE;

  /* The pre-pass of lib_take_and_none_pre400(), counting this time. */
  candidates = 0;
  for (object = 0; object < gs_object_count (game); object++)
    {
      scr_int position;

      if (obj_is_static (game, object)
          || !lib_take_co_pre400 (game, line, object))
        continue;
      if (taf_version >= TAF_VERSION_390 && !gs_object_seen (game, object))
        continue;
      position = gs_object_position (game, object);
      if (obj_directly_in_room (game, object, room))
        candidates++;
      else if (taf_version >= TAF_VERSION_380
               && (position == OBJ_IN_OBJECT || position == OBJ_ON_OBJECT)
               && obj_directly_in_room (game, gs_object_parent (game, object),
                                        room))
        candidates++;
    }
  if (candidates == 0)
    return FALSE;

  you = lib_select_response (game, "You", "I", "%player%");
  mark = pf_buffer_length (filter);
  pf_buffer_string (filter, you);
  pf_buffer_string (filter, " pick up ");
  picking = TRUE;

  taken = 0;
  rewrite_parent = -1;
  overwritten = appended = cant_see = hands_full = FALSE;
  for (object = 0; object < gs_object_count (game); object++)
    {
      scr_int position;

      if (obj_is_static (game, object)
          || !lib_take_co_pre400 (game, line, object))
        continue;
      if (taf_version >= TAF_VERSION_390 && !gs_object_seen (game, object))
        continue;

      position = gs_object_position (game, object);
      if (position == OBJ_HELD_PLAYER)
        {
          if (overwritten)
            {
              pf_truncate (filter, mark);
              picking = FALSE;
              pf_buffer_string (filter,
                                lib_select_response (game,
                                                     "You've already got ",
                                                     "I've already got ",
                                                     "%player%'ve already got "));
              if (taf_version >= TAF_VERSION_390)
                lib_print_object_np (game, object);
              else
                lib_print_object_raw (game, object);
              pf_buffer_character (filter, '!');
              cant_see = FALSE;
            }
        }
      else if (position == OBJ_WORN_PLAYER)
        {
          if (taf_version >= TAF_VERSION_380)
            {
              pf_truncate (filter, mark);
              picking = FALSE;
              pf_buffer_string (filter,
                                lib_select_response (game,
                                                     "You are already wearing ",
                                                     "I am already wearing ",
                                                     "%player% is already wearing "));
              if (taf_version >= TAF_VERSION_390)
                lib_print_object_np (game, object);
              else
                lib_print_object_raw (game, object);
              pf_buffer_character (filter, '!');
              overwritten = TRUE;
              cant_see = FALSE;
            }
        }
      else if (obj_directly_in_room (game, object, room))
        {
          scr_bool is_size;

          if (lib_take_over_capacity (game, object, &is_size))
            {
              if (!hands_full)
                {
                  pf_buffer_string (filter, "  ");
                  pf_buffer_string (filter, you);
                  pf_buffer_string (filter,
                                    lib_select_response (game,
                                        " can't take any more, as your hands are full.",
                                        " can't take any more, as my hands are full.",
                                        " can't take any more, as %player%'s hands are full."));
                  hands_full = TRUE;
                }
              appended = TRUE;
            }
          else
            {
              gs_object_player_get (game, object);
              gs_set_object_unmoved (game, object, FALSE);
              taken++;
              candidates--;
              /* The Runner appends the name straight after an overwritten
               * answer ("You are already wearing a hat!the stone.");
               * Scarier deliberately starts a fresh "  You pick up "
               * sentence there (deviation policy). */
              if (!picking)
                {
                  pf_buffer_string (filter, "  ");
                  pf_buffer_string (filter, you);
                  pf_buffer_string (filter, " pick up ");
                  picking = TRUE;
                }
              lib_print_object_np (game, object);
              pf_buffer_string (filter,
                                candidates > 1 ? ", "
                                : candidates == 1 ? " and " : ".");
              appended = TRUE;
            }
        }
      else if (taf_version >= TAF_VERSION_380 && position == OBJ_IN_OBJECT
               && ((!overwritten && !appended) || cant_see))
        {
          if (lib_obhere_380 (game, object))
            rewrite_parent = gs_object_parent (game, object);
          else
            {
              pf_truncate (filter, mark);
              picking = FALSE;
              pf_buffer_string (filter, you);
              pf_buffer_string (filter, " can't see ");
              if (taf_version >= TAF_VERSION_390)
                lib_print_object_np (game, object);
              else
                lib_print_object_raw (game, object);
              pf_buffer_string (filter, " from here!");
              overwritten = TRUE;
              cant_see = TRUE;
            }
        }
    }

  if (!overwritten && !appended)
    {
      pf_truncate (filter, mark);
      if (taf_version < TAF_VERSION_380)
        return FALSE;
      pf_buffer_string (filter, "Please take objects from one place at a time.");
    }

  /* The rewritten line reaches insides(); its answer replaces the tail. */
  if (taken == 0 && rewrite_parent != -1
      && lib_take_from_slot_valid_pre390 (game, rewrite_parent))
    {
      scr_int any = 0;

      gs_clear_object_references (game);
      gs_clear_multiple_references (game);
      for (object = 0; object < gs_object_count (game); object++)
        {
          if ((gs_object_position (game, object) == OBJ_IN_OBJECT
               || gs_object_position (game, object) == OBJ_ON_OBJECT)
              && !obj_is_static (game, object)
              && gs_object_parent (game, object) == rewrite_parent
              && lib_co_pre400 (game, line, object, 0)
              && (taf_version < TAF_VERSION_390
                  || gs_object_seen (game, object)))
            {
              game->object_references[object] = TRUE;
              any++;
            }
        }
      pf_truncate (filter, mark);
      if (any > 0)
        {
          lib_take_from_single_named = FALSE;
          lib_take_from_object_backend (game, rewrite_parent);
        }
      else
        lib_take_from_empty (game, rewrite_parent, FALSE);
      lib_take_from_task_sweep_380 (game);
    }

  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_take_and_400()
 *
 * 4.0's takes() cuts an "and" line into pieces (Proc_19_23_473A34, entered
 * from 4731A2 per piece) and resolves each with the noun scorer on its own,
 * then treats the pieces as one take:
 *
 *   - a piece that names an object in or on something makes the whole line
 *     a take-from of the FIRST such piece's parent, and only the pieces in
 *     or on that parent move: `take stone and nut` with the nut in an open
 *     box is "You take the nut from the box." and the stone stays put (p4TKB
 *     cells 3, 8, 13, 19, 25, 31, 38, 44; p4TKA 23, 168);
 *   - otherwise every piece is taken in index order, a held or worn one
 *     answering "You are already carrying X." after the takes and a static
 *     one "You can't take X!" (p4TKA cells 3, 14, 17, 20, 32, 36, 40, 44);
 *   - a piece that names an object elsewhere (the gem in the Cave) costs
 *     nothing; a piece that names nothing at all keeps the parser's own
 *     answers, as does a tie.
 *
 * run400x Adrift_208_ptka_4.txt / Adrift_210_ptkb_4.txt, 2026-09-20.  0 when
 * this does not apply and the ordinary parse should run; 1 when the pieces
 * are in multiple_references and *references counts them.
 */
static scr_int
lib_take_and_400 (scr_gameref_t game, scr_int *references)
{
  const scr_char *text = var_get_ref_text (gs_get_vars (game));
  std::vector<scr_int> pieces, refused;
  std::string copy;
  size_t start;
  scr_int object, first_parent = -1;

  if (!lib_is_version_400 (game) || !text
      || lib_input_contains_word (text, "all")
      || !lib_input_contains_word (text, "and"))
    return 0;

  copy = text;
  start = 0;
  while (start <= copy.size ())
    {
      size_t end = copy.find (" and ", start);
      std::string piece;
      std::vector<scr_int> tied;
      scr_int other;
      scr_bool known;

      piece = copy.substr (start, end == std::string::npos
                                  ? std::string::npos : end - start);
      start = end == std::string::npos ? copy.size () + 1 : end + 5;

      {
        size_t from = piece.find_first_not_of (' ');
        size_t to = piece.find_last_not_of (' ');

        if (from == std::string::npos)
          return 0;
        piece = piece.substr (from, to - from + 1);
      }

      object = lib_take_resolve_400_string (game, piece.c_str (), &tied);
      if (object == -1)
        return 0;
      if (object >= 0)
        {
          pieces.push_back (object);
          continue;
        }

      object = lib_verb_object_resolve_400_string (game, piece.c_str (),
                                                   NULL, TRUE);
      if (object >= 0 && obj_is_static (game, object))
        {
          refused.push_back (object);
          continue;
        }

      known = FALSE;
      for (other = 0; other < gs_object_count (game) && !known; other++)
        known = lib_verb_object_name_score (game, other, piece.c_str ()) > 0;
      if (!known)
        return 0;
    }
  if (pieces.empty ())
    return 0;

  for (size_t index_ = 0; index_ < pieces.size (); index_++)
    {
      const scr_int position = gs_object_position (game, pieces[index_]);

      if (position == OBJ_IN_OBJECT || position == OBJ_ON_OBJECT)
        {
          first_parent = gs_object_parent (game, pieces[index_]);
          break;
        }
    }

  gs_clear_multiple_references (game);
  *references = 0;
  for (size_t index_ = 0; index_ < pieces.size (); index_++)
    {
      const scr_int piece = pieces[index_];
      const scr_int position = gs_object_position (game, piece);

      if (first_parent != -1
          && !((position == OBJ_IN_OBJECT || position == OBJ_ON_OBJECT)
               && gs_object_parent (game, piece) == first_parent))
        continue;
      if (!game->multiple_references[piece])
        {
          game->multiple_references[piece] = TRUE;
          (*references)++;
        }
    }
  if (first_parent == -1)
    {
      for (size_t index_ = 0; index_ < refused.size (); index_++)
        {
          if (!game->multiple_references[refused[index_]])
            {
              game->multiple_references[refused[index_]] = TRUE;
              (*references)++;
            }
        }
    }
  if (*references == 0)
    return 0;

  lib_take_and_pieces_400 = TRUE;
  return 1;
}


/*
 * lib_move_named_whole_line_pre400()
 *
 * The noun half of "a library verb is matched anywhere in the line".  Below
 * 4.0 neither takes() nor drops() parses the words after its verb: each one
 * walks the object table in index order and asks co() whether the object's
 * Short or an Alias stands ANYWHERE in the typed line -- takes() with mode 1
 * (run390 454E1D, run380 43DFC3, run370 4364E6) and drops() with mode 2
 * (run390 4458CF, and the same walk in run380 438889 / run370 430689).  So a
 * word the line holds that names nothing costs nothing: `blorp take coin` is
 * "You pick up the coin." and `blorp drop coin` "You drop the coin." in all
 * three older Runners (p37REW/p38REW/p39REW with cmdfile_pcasc.txt --
 * Adrift_250_casc37b.rtf, Adrift_249_casc38.rtf, Adrift_250_casc39.txt,
 * 2026-09-20), and so are `take zzz hat` and `drop zzz hat` at 3.90 (p39WHAT,
 * Adrift_p39what2.txt, cmdfile_p39what2.txt, same day).  Scarier binds
 * %text% positionally, so with the verb hoisted to the front by
 * run_hoist_verb_line() the rest of the line went to the noun and the take
 * fell to "Take what?" while the drop fell to the absent row's "You don't
 * have a coin!".
 *
 * Only from the parse's failure branch, so every line whose words the parser
 * does account for keeps the answer it had; and only when ONE object in the
 * whole table answers the line, the caller's own filter accepting it.  Both
 * halves matter.  A crowd still reaches the disambiguation the Runner's own
 * namesake count asks for; and a second co() match the filter would reject
 * is not passed over, because the Runner walks those objects in the same
 * loop and they write their own refusals from it -- drops()' " don't have
 * <X>!" (run390 445CE3, run380 438E13) for a namesake that is not held --
 * and which of the two speaks is decided by index order and a message-empty
 * gate that nothing here measures.  wrecked (run380) `drop room key` is that
 * case: the small key is in hand where the room key is not, and the answer
 * has to stay the refusal it was.  The "all" and "and" arms walk co()
 * themselves (lib_take_and_pre400, lib_drop_and_arm_collect_pre400) and are
 * left alone here.
 */
static scr_bool
lib_move_named_whole_line_pre400 (scr_gameref_t game,
                                  scr_bool (*resolver) (scr_gameref_t,
                                                        scr_int, scr_int),
                                  scr_int mode, scr_int *references)
{
  const scr_char *const line = run_get_dispatch_input ();
  scr_int object, found;

  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400
      || !line
      || lib_input_contains_word (line, "all")
      || lib_input_contains_word (line, "and"))
    return FALSE;

  found = -1;
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (!lib_co_pre400 (game, line, object, mode))
        continue;
      if (found != -1)
        return FALSE;
      found = object;
    }
  if (found == -1 || !resolver (game, found, -1))
    return FALSE;

  gs_clear_multiple_references (game);
  game->multiple_references[found] = TRUE;
  *references = 1;
  return TRUE;
}


/*
 * lib_take_npc_overwrite_tail()
 *
 * The tail every take refusal shares: a present character the line names
 * answers over whatever takes() just wrote, and at 3.80 its take-from arm
 * answers again over that.  See lib_take_npc_overwrite() for the arms and
 * their gates.  Ask it wherever a take writes and returns, not only from
 * the main loop -- the held-namesake pre-empt writes its "You've already
 * got the hat!" and returns, and the Runner overwrites that line too:
 * `take give hat bob` with the hat in hand is "I don't think Bob would
 * appreciate being handled." at 3.7/3.9/4.0 and "Bob is not carrying the
 * hat!" at 3.8 (cmdfile_p2give.txt cell 5, Adrift_269_3g37.rtf,
 * 270_3g38.rtf, 271_3g39.txt, 272_3g40.txt, 2026-09-21).
 */

/*
 * lib_take_ask_overwrite_400()
 *
 * characters()' ask arm, the last text arm of its cascade, answering over the
 * take arm at 4.0.  The block is entered on whole-word `ask` or `talk to`
 * (47F8E5-47F8FF) and, with the character in the player's room, its gate is
 * `(no task And MemVar_4942E0 = 0) Or buffer = "<player> can't talk to
 * that."` (47F900-47F93C) -- no buffer-empty test, unlike its 3.9 twin at
 * 4597FE, which is why 4.0 alone does this.  Inside, c("about") and the
 * character named (45E99C mode 1) and InStr(line, "about") > 0 (47F93F-
 * 47F96F) take the subject as Mid(line, InStr(line, "about") + 6) (47F9A5),
 * and the topic loop ASSIGNS the reply it finds (47FA7C, 47FAA7, 47FAD2).
 * The no-topic "does not respond" (47FB83) writes only over an empty, "can't
 * talk to that." or " can't see " buffer, so an unanswered ask leaves the
 * take refusal standing; the "Use the format" hint for a line with no
 * `about` (47FBB0) is not measured and not modelled.
 *
 * Measured on p4ORD, cmdfile_p2chr.txt (run400x Adrift_268_3c40.txt,
 * 2026-09-21): `take ask bob about hat` is "Bob says, 'That is a fine hat.'"
 * and the hat is still taken; 3.7/3.8/3.9 keep their take-arm answers.
 *
 * TRUE once it has spoken, having truncated the buffer back to mark first.
 */
static scr_bool
lib_take_ask_overwrite_400 (scr_gameref_t game, scr_int npc, size_t mark)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *about;
  scr_int topic;

  if (!input || !lib_is_version_400 (game)
      || !(lib_input_contains_word (input, "ask")
           || lib_input_contains_word (input, "talk to"))
      || !lib_input_contains_word (input, "about")
      || !npc_in_room (game, npc, gs_playerroom (game)))
    return FALSE;
  about = strstr (input, "about");
  if (!about)
    return FALSE;
  about += strlen (about) >= 6 ? 6 : strlen (about);

  /* The topic loop reads the subject, not whatever the take referenced. */
  const std::string saved (var_get_ref_text (vars));
  var_set_ref_text (vars, about);
  topic = lib_npc_find_topic (game, npc);
  var_set_ref_text (vars, saved.c_str ());
  if (topic == -1)
    return FALSE;

  pf_truncate (gs_get_filter (game), mark);
  return lib_npc_reply_to (game, npc, topic);
}

static scr_bool
lib_take_npc_overwrite_tail (scr_gameref_t game, size_t mark)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int npc = lib_take_npc_overwrite (game);

  if (npc == -1 || lib_take_refusal_redispatch)
    return FALSE;

  lib_take_single_named = FALSE;
  lib_take_refusal_claimed = FALSE;
  pf_truncate (filter, mark);
  lib_print_take_npc_refusal (game, npc);
  lib_take_from_npc_overwrite_380 (game, npc, mark);
  lib_take_ask_overwrite_400 (game, npc, mark);
  return TRUE;
}



/*
 * lib_take_multiple_common()
 *
 * Take the objects available to the player and listed in %text%, or -- for
 * is_except -- every one of them but those listed.
 */
static scr_bool
lib_take_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_bool (*resolver) (scr_gameref_t, scr_int, scr_int);
  scr_int objects, references;
  scr_bool parsed = FALSE;

  /*
   * `get off X` reaches the take rows first, and below 4.0 takes() names X
   * over the whole line, so a player standing on the stool picked it up
   * (run370x/run380x/run390x p3xSIT).  Deliberate deviation: while the
   * player is on something, leave the line to lib_cmd_get_off_object().
   */
  if (!is_except && lib_player_parent_here (game) != -1)
    {
      const scr_char *line = run_get_dispatch_input ();

      if (line && (scr_strncasecmp (line, "get off ", 8) == 0
                   || scr_strncasecmp (line, "get down off ", 13) == 0
                   || scr_strncasecmp (line, "get up off ", 11) == 0))
        return FALSE;
    }

  /*
   * A take line holding "from" that the take-from rows above declined --
   * the container clause named nothing, or two things, or nothing at all --
   * is still a take-from to every Runner (see lib_take_from_and() and
   * lib_take_from_trailing()); pre-4.0 it is never a bare take, so the
   * catch-alls of the take-from-nowhere rows answer it, not the held
   * namesake or "Take what?" (`get coin from zzz` with the coin in hand:
   * run370x/run380x "You can't do that!", run390x "The coin isn't in or on
   * anything!"; p3xTKB Adrift_207/208/209_ptkb cell 144, 2026-09-20).
   */
  if (!is_except)
    {
      const scr_int version = prop_get_taf_version (gs_get_bundle (game));
      const scr_char *line = run_get_dispatch_input ();
      const scr_int at = line ? run_c_word_pre400 (version, line, "from") : -1;

      if (at >= 0)
        {
          if (line[at + 4 + strspn (line + at + 4, " ")] == NUL)
            return lib_take_from_trailing (game);
          if (lib_take_from_and_line (game))
            return lib_take_from_and (game);
          if (version < TAF_VERSION_400)
            return FALSE;
        }
    }

  if (!is_except && lib_take_and_none_pre400 (game))
    return TRUE;
  if (!is_except && lib_take_and_pre400 (game))
    return TRUE;

  /*
   * "take all except ..." works over the "all" universe, which excludes
   * whatever the player already carries; a named take reaches further, into
   * a carried open container (see lib_take_all_filter).
   */
  resolver = is_except ? lib_take_all_filter : lib_take_filter;

  /* 4.0: an "and" line resolves piece by piece; see lib_take_and_400(). */
  if (!is_except)
    parsed = lib_take_and_400 (game, &references) == 1;

  /* 4.0: the whole fragment scored first; see lib_take_whole_line_400(). */
  if (!is_except && !parsed)
    {
      scr_bool bound;

      if (lib_take_whole_line_400 (game, &references, &bound))
        return TRUE;
      parsed = bound;
    }

  /* Parse the multiple objects list to find the target objects. */
  if (parsed)
    ;
  else if (!lib_parse_multiple_objects (game, is_except ? "leave" : "take",
                                        resolver, -1,
                                        &references))
    {
      /*
       * 4.0's get_piece (Proc_19_23_473A34) names the piece's object with the
       * noun scorer 463640 (473011), which counts whole words and skips the
       * rest, so a word it does not know costs nothing.  Professor.taf, run400:
       * `get x rope` in the square refuses "You can't take the Mailbox on-a
       * Rope!" (Adrift_p4profmail2 T22), the alias "rope" scoring; in the
       * Laboratory it ends on DontUnderstand after the refusal's pre-match
       * (T25, Adrift_1154_p4profmail6 T26).  Scarier's object parser wanted
       * every word, and the line fell to "Take what?".  The X-Files, run400
       * Adrift_424/522_xfiles.txt:248: `take phone book` -> "You take Your
       * Cell Phone from Your Backpack.".  One piece only; a list keeps the
       * parser's own answers.  Only from the "Take what?" catch-all, so every
       * other take row has had the line first (Pilfers `get off bed`).
       */
      const scr_char *text = var_get_ref_text (gs_get_vars (game));
      scr_int object;

      /* Pre-4.0: a held or worn namesake still answers a noun that resolved
         to nothing; see lib_take_held_namesake_preempt_pre400().  A present
         character the line names answers over it; see
         lib_take_npc_overwrite_tail(). */
      if (!is_except)
        {
          const size_t mark = pf_buffer_length (filter);

          if (lib_take_held_namesake_preempt_pre400 (game, TRUE))
            {
              lib_take_npc_overwrite_tail (game, mark);
              return TRUE;
            }
        }

      /* Pre-4.0: takes() names its object over the WHOLE line, co(obj, 1);
         see lib_move_named_whole_line_pre400(). */
      if (!is_except
          && lib_move_named_whole_line_pre400 (game, resolver, 1, &references))
        ;
      else if (!lib_take_scored_fallback || is_except || !text
               || strchr (text, ',') || lib_input_contains_word (text, "and"))
        return FALSE;
      else
        {
          object = lib_verb_object_resolve_400_string (game, text, NULL, TRUE);
          if (object < 0)
            return FALSE;
          gs_clear_multiple_references (game);
          game->multiple_references[object] = TRUE;
          references = 1;
        }
    }
  else if (references == 0)
    return TRUE;

  /* Pre-4.0: a held or worn namesake indexed below the one object the line
     named answers for it when that object is not loose in the room; see
     lib_take_held_namesake_preempt_pre400().  A present character the line
     names answers over that too; see lib_take_npc_overwrite_tail(). */
  if (!is_except && references == 1)
    {
      const size_t mark = pf_buffer_length (filter);

      if (lib_take_held_namesake_preempt_pre400 (game, FALSE))
        {
          lib_take_npc_overwrite_tail (game, mark);
          return TRUE;
        }
    }

  /*
   * Pre-4.0, the pre-pass counts the loose candidates first and flags them;
   * see lib_take_crowd_pre400().  Two of them and the take never happens, so
   * decline the row and let the catch-all answer "Take what?"; one, and it
   * is that object the loop takes, whatever the parser bound -- `take hat
   * coin` with the hat in hand picks up the coin.
   */
  if (!is_except && references == 1)
    {
      scr_bool crowded;
      const scr_int winner = lib_take_crowd_pre400 (game, &crowded);

      if (crowded)
        {
          gs_clear_multiple_references (game);
          return FALSE;
        }
      if (winner != -1 && !game->multiple_references[winner])
        {
          gs_clear_multiple_references (game);
          game->multiple_references[winner] = TRUE;
        }
    }

  /*
   * Pre-4.0, takes() reaches an object only through co(idx, 1) (run390
   * 454E1D, run380 43DFC3, run370 4364E6), so the one object the parser
   * resolved is still not taken when that test fails for it: two seen
   * namesakes present with no loose one and no Prefix word typed is the
   * pending "Which <term>" ambiguity, and NOTHING moves.  secret_of_lost_world
   * T59 `take scroll` with the ancient scroll in hand and the decayed scroll
   * on the fresh skeleton prints "Which scroll.  Ancient scroll or the
   * decayed scroll?" and leaves the decayed scroll where it was, so T74
   * `give princess decayed scroll` fails its held restriction ("You can't do
   * that right now.") and the lily, the red gem and the win never come
   * (run390, runner_transcripts/secret_of_lost_world, 2026-09-19).  Scarier
   * printed the same prompt over a take that had quietly happened.  Decline
   * the row: the ambiguity prompt replaces the turn's output anyway, and a
   * plain miss falls to the catch-all "Take what?" the Runner's empty buffer
   * reaches.
   *
   * 3.70 is not in this: its takes() calls no co() at all, and settles a
   * crowd of its own namesakes in lib_disambiguate_object_common().  A rock
   * ALIASED "gem" beside a gem is no crowd to it -- `take gem` takes both
   * and the rock speaks (p37OPENA, Adrift_232_oy370, 2026-09-20) -- and
   * this gate used to turn that into "Take what?".
   */
  if (!is_except && references == 1
      && prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380
      && prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
    {
      const scr_char *line = run_get_dispatch_input ();
      scr_int target;

      for (target = 0; target < gs_object_count (game); target++)
        if (game->multiple_references[target])
          break;
      if (line && target < gs_object_count (game)
          && !lib_take_co_pre400 (game, line, target))
        {
          gs_clear_multiple_references (game);
          return FALSE;
        }
    }

  /*
   * run390 takes() rewrites a bare take of something in or on an object to
   * "<line> from <parent's Prefix Short>" (4552EE) and hands it to
   * insides(), whose container slot walk may pick another object the
   * rewritten line names (8811c2b76).  Deliberate deviation: Scarier takes
   * the object the line named from where it is.
   */

  /* Note single-object takes; the backend words some refusals differently. */
  lib_take_single_named = !is_except && references == 1;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              resolver, -1, is_except,
                              &references);

  /* A 3.8 bare take of something in or on an object is a take-from, and
     gets its task sweep; see lib_take_from_task_sweep_380(). */
  scr_bool is_take_from_380 = FALSE;
  if (!is_except)
    {
      scr_int index_;

      for (index_ = 0; index_ < gs_object_count (game); index_++)
        if (game->object_references[index_]
            && (gs_object_position (game, index_) == OBJ_IN_OBJECT
                || gs_object_position (game, index_) == OBJ_ON_OBJECT))
          is_take_from_380 = TRUE;
    }

  const size_t take_mark = pf_buffer_length (filter);

  if (objects > 0 || references > 0)
    {
      lib_take_backend (game);

      /* A present character the line names overwrites the answer. */
      if (lib_take_npc_overwrite_tail (game, take_mark))
        return TRUE;
    }
  else if (lib_is_version_400 (game))
    {
      /*
       * 4.0 has one flat refusal and no "else" form: run400 answers
       * "There is nothing worth taking here." to `take all except rock`
       * whether or not the rock is the only thing left in the room.
       */
      pf_buffer_string (filter, "There is nothing worth taking here.");
    }
  else
    {
      pf_buffer_string (filter, "There is nothing");
      if (is_except && objects == 0)
        pf_buffer_string (filter, " else");
      pf_buffer_string (filter, " to pick up here.");
    }
  lib_take_single_named = FALSE;
  lib_take_and_pieces_400 = FALSE;

  if (is_take_from_380 && !lib_take_refusal_claimed)
    lib_take_from_task_sweep_380 (game);
  if (!lib_take_refusal_claimed)
    pf_buffer_answer_break (filter);
  lib_take_refusal_claimed = FALSE;
  if (lib_take_refusal_redispatch)
    {
      lib_take_refusal_redispatch = FALSE;
      lib_take_refusal_redispatch_400 (game);
    }
  return TRUE;
}


/*
 * lib_cmd_take_except_multiple()
 * lib_cmd_take_multiple()
 *
 * Facets of lib_take_multiple_common().
 */
scr_bool
lib_cmd_take_except_multiple (scr_gameref_t game)
{
  return lib_take_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_take_multiple (scr_gameref_t game)
{
  return lib_take_multiple_common (game, FALSE);
}

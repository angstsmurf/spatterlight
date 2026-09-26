/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Part of sclibrar.cpp, which #includes it; not compiled on its own.
 * Asking and talking to NPCs.
 */

/*
 * lib_ask_format_subject()
 * lib_ask_format_character()
 *
 * The ask-format hint spells its placeholders with angle brackets in 3.7 and
 * with square ones from 3.8 on -- the only thing that changed about it in
 * four Runner releases:
 *
 *   run370 loc_4387A0, loc_438BDC   "ask " & name & " about <subject>" & "."
 *   run370 loc_43D782               "ask <character> about <subject>"
 *   run380 loc_44062F, loc_440A6B   "ask " & name & " about [subject]" & "."
 *   run380 loc_444219               "ask [character] about [subject]"
 *   run390 loc_45976B / loc_45DA37, run400 loc_47F879 / loc_488D55: as 3.8.
 *
 * The generic hint has no full stop after its closing quote in any release
 * (run400 488D6E-488D8C appends only Chr(34)); measured on run370, run380 and
 * run390, p3xNPCAMB `ask` (Adrift_194/195/196).  The per-character hint does
 * append "." (run400 47F8C4).  Scarier deliberately ends both with the full
 * stop (deviation policy: a missing stop is a display accident), except in a
 * game with an ALR on the hint -- two dozen games replace the Runner's
 * unstopped 'Use the format "ask [character] about [subject]"' with a
 * sentence of their own, which the stop would double ("no response..") --
 * where the Runner's form is kept.
 */
static const scr_char *
lib_ask_format_subject (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380
         ? " about [subject]\".\n" : " about &lt;subject&gt;\".\n";
}

static const scr_char *
lib_ask_format_character (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (prop_get_taf_version (bundle) < TAF_VERSION_380)
    return "Use the format \"ask &lt;character&gt; about &lt;subject&gt;\".\n";
  return pf_alr_mentions (bundle, "[subject]\"")
         ? "Use the format \"ask [character] about [subject]\"\n"
         : "Use the format \"ask [character] about [subject]\".\n";
}


/*
 * lib_cmd_ask_npc()
 * lib_cmd_ask_object()
 * lib_cmd_ask_other()
 * lib_cmd_talk_to_npc()
 * lib_cmd_talk_to_npc_pre_390()
 *
 * Malformed and rhetorical question responses.
 *
 * `talk to X` and `speak to X` reach the same hint.  It is produced by the
 * per-character pass, inside the block that a command only enters when it
 * names the character (run380 loc_4401AA, `c(name) Or c(descriptor)`), by a
 * branch whose guard is the one thing here that moved between releases:
 *
 *   run370 loc_438748, run380 loc_4405D7   c("talk") Or c("speak")
 *   run390 loc_45973D, run400 loc_47F84A   c("talk to") Or c("speak to")
 *
 * So 3.7 and 3.8 answer a bare `talk bob` or `speak bob` with the hint and
 * 3.9 and 4.0 do not -- there the bare word falls through to the
 * generaltasks `c("talk")` rabblings line, which every Runner has
 * (run400 loc_488DA2), and `speak` alone reaches nothing at all.
 *
 * `talk to X about Y` is different again: `c("ask") Or c("talk to")` guards
 * the real conversation branch in all four (run370 loc_4387F4, run380
 * loc_440683, run390 loc_4597F2, run400 loc_47F8F7), and it runs after the
 * hint branch and overwrites it.  `speak to` is not in that list, so it only
 * ever gets the hint.  See the grammar rows in scrunner.cpp.
 *
 * Not ported: the pre-parse rewrites of a command *beginning* `ask about `
 * or `talk about ` into `ask <last named character> about ...` (run370
 * loc_4380CF/loc_438185, run380 loc_43FF4F/loc_440005, run390 loc_459010/
 * loc_4590E1, run400 loc_47F14C/loc_47F20F).  They need the Runner's
 * "character most recently named by a command" register, which SCARE has no
 * equivalent of.
 */
/*
 * lib_pre_400_ask_column()
 *
 * run390 459882 and its 3.7/3.8 twins open the characters() conversation
 * block with a POSITION test, the one place in the whole handler that cares
 * where in the line a character is named:
 *
 *   var_1EC = c("ask") And InStr(line, LCase(Name)) <> 5
 *                      And InStr(line, LCase(Alias(0))) <> 5
 *   If var_1EC Then GoTo <past the block>          ' 459895
 *   var_1EC = c("talk to") And InStr(line, LCase(Name)) <> 9
 *                          And InStr(line, LCase(Alias(0))) <> 9
 *   If var_1EC Then GoTo <past the block>          ' 45992F
 *
 * so the name has to start the moment "ask " or "talk to " ends -- column 5
 * and column 9, 1-based.  Everything else in characters() is containment:
 * the talk-to hint arm just above (45975C-4597D7) has no position test at
 * all, which is why `talk to blue guard` still answers 'Use the format "ask
 * Cid about [subject]".' while `ask blue guard about key` falls past the
 * block to therest's seed, "You can't talk to that.".  run400 47F8F7 has no
 * such test: 4.0 binds the character by containment and settles a crowd
 * with the Prefix contest, so `ask blue guard about key` is "BOB KEY.".
 *
 * MEASURED p39PFX (run390x, Adrift_1211) against p4PFX (run400x,
 * Adrift_1210), cmdfile_ppfx.txt turns 7 and 8.
 */
static scr_bool
lib_pre_400_ask_column (scr_gameref_t game, scr_int npc, scr_int column)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *line = run_get_dispatch_input ();
  scr_vartype_t vt_key[4];
  scr_int form;

  if (prop_get_taf_version (bundle) >= TAF_VERSION_400)
    return TRUE;
  if (!line || (scr_int) strlen (line) <= column)
    return FALSE;

  for (form = 0; form < 2; form++)
    {
      const scr_char *name;
      scr_int posn, length;

      name = form == 0
             ? prop_get_indexed_string (bundle, "NPCs", npc, "Name")
             : lib_first_alias (bundle, vt_key, "NPCs", npc);
      if (!name || name[0] == '\0')
        continue;

      /* InStr: the FIRST occurrence is the one that has to sit at column. */
      length = strlen (name);
      for (posn = 0; line[posn] != '\0'; posn++)
        if (scr_strncasecmp (line + posn, name, length) == 0)
          break;
      if (line[posn] != '\0' && posn == column)
        return TRUE;
    }

  return FALSE;
}

scr_bool
lib_cmd_ask_npc (scr_gameref_t game)
{
  scr_int npc;
  scr_bool is_ambiguous;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc_pick (game, "ask", &is_ambiguous, NPC_PICK_LAST);
  if (npc == -1)
    return is_ambiguous;

  /* Pre-4.0 wants the name at column 5, right after "ask ". */
  if (!lib_pre_400_ask_column (game, npc, 4))
    return FALSE;

  /* Incomplete ask command, so offer help and return. */
  lib_print_wrapped_npc (game, "Use the format \"ask ",
                         npc, lib_ask_format_subject (game));
  return TRUE;
}

/*
 * lib_any_named_npc()
 *
 * The NPC the line named, with no gate on whether they're seen or in the
 * room -- run400's characters() (47F845-47F8CE) tests only c(name) Or
 * c(descriptor), unlike lib_disambiguate_npc()'s present-and-seen filter.
 * Returns the unique such NPC, or -1 for none or a tie (a tie is
 * unmeasured, so it is left to fall through as before).
 */
static scr_int
lib_any_named_npc (scr_gameref_t game)
{
  scr_int index_, npc, count;

  count = 0;
  npc = -1;
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (game->npc_references[index_])
        {
          count++;
          npc = index_;
        }
    }
  return count == 1 ? npc : -1;
}

scr_bool
lib_cmd_talk_to_npc (scr_gameref_t game)
{
  scr_int npc, named;
  scr_bool is_ambiguous;

  /*
   * A named NPC who isn't currently present still gets the hint in 4.0:
   * characters() names by whole-word match alone, with no room gate, and
   * that answer OVERWRITES the generic "talk"/"speak" rabblings default
   * (run400 488DA2) rather than falling through to it.  Capture the named
   * NPC before lib_disambiguate_npc()'s own room/seen filter clears the
   * reference.  Measured escape_to_new_york turns 199-200 `talk to
   * goodson` (Ticket run400 xoshiro trace 2026-09-12).  run390's hint
   * (45975C-4597C0) has no room gate either, and nothing after it in
   * characters() overwrites it for an absent NPC: the "isn't here!" arm
   * (459C2A) wants an empty or "can't talk to that." buffer.  Measured
   * p39ABSNPC (Adrift_1206_p39absnpc.txt): `talk to fred` (never seen),
   * `speak to erin`, `talk to erin about key` and `talk to girl` (Alias)
   * from the next room all answer the hint.
   */
  named = prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390
          ? lib_any_named_npc (game) : -1;

  /*
   * Pre-4.0 assigns the hint outright for every character named, here or
   * not, so the LAST one named answers -- 3.7 and 3.8 included, whose arm
   * (run380 4405D7) has no room test either.  `talk to guard` with Ann and
   * Bob here and Cora next door, never met, is "Use the format "ask Cora
   * about [subject]"." (run370x/run380x/run390x, Adrift_193_pnpcamb37b/
   * 192/193).
   */
  if (!lib_is_version_400 (game))
    {
      npc = lib_last_named_npc (game);
      if (npc == -1)
        return FALSE;
      var_set_ref_character (gs_get_vars (game), npc);
      lib_print_wrapped_npc (game, "Use the format \"ask ",
                             npc, lib_ask_format_subject (game));
      return TRUE;
    }

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc (game, "talk to", &is_ambiguous);
  if (npc == -1)
    {
      if (is_ambiguous)
        return TRUE;
      if (named == -1)
        return FALSE;
      npc = named;
    }

  lib_print_wrapped_npc (game, "Use the format \"ask ",
                         npc, lib_ask_format_subject (game));
  return TRUE;
}

scr_bool
lib_cmd_talk_to_npc_pre_390 (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return FALSE;

  return lib_cmd_talk_to_npc (game);
}

/*
 * lib_line_names_object_before_about()
 *
 * The Runner's ask/talk-to block, when the line contains the whole word
 * "about", truncates the line to Left(input, InStr("about") - 1) before it
 * asks co(refobj) whether the up-front object is named -- run400 therest
 * 488B8A-488BEE (co() = Proc_21_39_46486C), run380 444039-444113, run390
 * 45D848-45D941 alike.  So `ask young girl about flowers` with the flowers
 * in hand does NOT get "no reply from the flowers": the flowers stand
 * after "about".  Scarier's `ask %object% *` row binds a trailing
 * %object% by containment anywhere in the line, hence this test.
 *
 * Measured on Ticket (4.00, Adrift_1127_ticket.txt turn 154): the Runner
 * prints "Young Girl isn't here!" where Scarier used to print "You get no
 * reply from the flowers.".
 */
static scr_bool
lib_line_names_object_before_about (scr_gameref_t game, scr_int object)
{
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *scan;
  scr_char *head;
  scr_bool is_named;

  if (!input)
    return TRUE;

  for (scan = input; *scan != NUL; scan++)
    {
      if ((scan == input || scan[-1] == ' ')
          && scr_strncasecmp (scan, "about", 5) == 0
          && (scan[5] == NUL || scan[5] == ' '))
        break;
    }
  if (*scan == NUL)
    return TRUE;

  head = (scr_char *) scr_malloc (scan - input + 1);
  memcpy (head, input, scan - input);
  head[scan - input] = NUL;
  is_named = lib_verb_object_name_score (game, object, head) > 0;
  scr_free (head);
  return is_named;
}

scr_bool
lib_cmd_ask_object (scr_gameref_t game)
{
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "ask", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* An object named only after "about" is the subject, not the asked. */
  if (!lib_line_names_object_before_about (game, object))
    return FALSE;

  /* No reply. */
  lib_print_response_object (game,
                             "You get no reply from ",
                             "I get no reply from ",
                             "%player% get no reply from ",
                             object, ".\n");
  return TRUE;
}

scr_bool
lib_cmd_ask_other (scr_gameref_t game)
{
  /* Incomplete ask command, so offer help and return. */
  return lib_print_message (game, lib_ask_format_character (game));
}


/*
 * lib_cmd_ask_about_nothing()
 *
 * `ask ... about ...` that named neither a character nor an object.
 *
 * Every Runner splits its `c("ask") Or c("talk to")` block on `c("about")`
 * (run370 loc_43E9B7, run380 loc_444039, run390 loc_45D8E5, run400
 * loc_488B87).  The branch WITHOUT "about" is the per-character format hint
 * -- what lib_cmd_ask_other() prints -- and the branch WITH it looks for a
 * named, present object ("<You> get no reply from <it>.", lib_cmd_ask_object)
 * and, failing that, seeds the response buffer with
 *
 *   MemVar_4941D0(0) & " can't talk to that."      run400 loc_488C65
 *   MemVar_44F108(0) & " can't talk to that."      run380 loc_44410C
 *
 * so a line the character handler then answers -- a topic, "<npc> does not
 * respond to your question.", "<npc> isn't here!" -- overwrites it, and a
 * line nothing answers keeps it.  Scarier's grammar puts the character and
 * object rows ahead of this one, which is the same order.
 *
 * Measured on thelasthour (4.00, Adrift_297_thelasthour.txt turn 80):
 * `ask sly about him` -- "sly" is a task word, not a character, and the
 * game has no male to fill "him" in -- answers "(No male)" and then "I
 * can't talk to that." where Scarier used to print the format hint.
 *
 * Two answers outrank the seed.  4.0's up-front resolver (Proc_21_58_463640,
 * see lib_absent_seen_object()) has already put "<You> can't see <the
 * object>." in the buffer for a seen-but-absent object named ANYWHERE in the
 * line, and therest leaves that alone: 488C51 seeds "can't talk to that."
 * only into an EMPTY buffer.  Then characters(), called from the tail at
 * 48B56E after therest, walks the NPCs the line names (Proc_21_40_45E99C)
 * and for one not in the player's room (47FC22, no seen test) overwrites an
 * empty or "can't talk to that." buffer with "<Name> isn't here!" (47FC6D);
 * the first such NPC wins, since the buffer then ends otherwise.  run380
 * 440AC0/440ACC, run390 459C0B/459C32 and run370 438C0A do the same, minus
 * the 4.0-only absent-object pass.
 *
 * Measured on Ticket (4.00, Adrift_1127_ticket.txt turns 154 and 156): `ask
 * young girl about flowers` with the girl elsewhere answers "Young Girl
 * isn't here!" while the flowers are held, and "You can't see the flowers."
 * once they have been given away.
 */
scr_bool
lib_cmd_ask_about_nothing (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int npc;

  if (lib_cmd_verb_absent_400 (game))
    return TRUE;

  if (input)
    {
      for (npc = 0; npc < gs_npc_count (game); npc++)
        {
          const scr_char *name;

          if (npc_in_room (game, npc, gs_playerroom (game))
              || !lib_npc_referenced (game, npc, input))
            continue;

          /* 4.0 capitalises the Name (Proc_21_3_446BB4); 3.9 prints it raw. */
          name = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
          if (lib_is_version_400 (game))
            pf_new_sentence (filter);
          pf_buffer_string (filter, name);
          pf_buffer_string (filter, " isn't here!\n");
          return TRUE;
        }
    }

  return lib_print_response_message (game,
      "You can't talk to that.\n",
      "I can't talk to that.\n",
      "%player% can't talk to that.\n");
}

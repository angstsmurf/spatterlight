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
 */

/*
 * NPC and object disambiguation, and the 4.0 "which do you mean"
 * question machinery.
 *
 * Split out of sclibrar.cpp; see sclibrar.h for what the library files
 * share and sclibrar_internal.h for what the core files share.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"
#include "sclibrar.h"
#include "sclibrar_internal.h"

/*
 * lib_disambiguate_npc()
 *
 * Filter, then search the set of NPC matches.  If only one matched, note
 * and return it.  If multiple matched, print a disambiguation message and
 * the list, and return -1 with *is_ambiguous TRUE.  If none matched, return
 * -1 with *is_ambiguous FALSE if requested, otherwise print a message then
 * return -1.
 */

/*
 * lib_which_runner_form()
 * lib_which_head()
 *
 * The Runners word every ambiguity question as a statement followed by a
 * question -- "Which <term>.  <list>?", 3.7/3.8 examines() "Which <Short>
 * would you like to examine.  <list>?" -- and at 4.0 an empty character
 * Prefix leaves its space in the list ("Which woman.   woman or  woman?").
 * Scarier deliberately asks "Which <term>?  <list>?" instead, with no
 * stray spaces (deviation policy: the full stop is a display accident, not
 * text worth matching).  The exception is a game whose authors wrote ALRs
 * against the Runner's form -- cursed's 58 "Which <term>.  <list>?"
 * rewrites, asteroid_after's satellite prompts, Vendetta's "Which girl",
 * DragonShrine's "Which painting." -- where the Runner's form is kept so
 * that the author's replacement fires.  Any ALR Original starting "Which "
 * opts the game in.
 */
static scr_prop_setref_t lib_which_cached_bundle = NULL;
static scr_bool lib_which_cached_result = FALSE;

static scr_bool
lib_which_runner_form (scr_gameref_t game)
{
  scr_prop_setref_t &cached_bundle = lib_which_cached_bundle;
  scr_bool &cached_result = lib_which_cached_result;
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key;
  scr_int alr_count, index_;

  if (bundle == cached_bundle)
    return cached_result;

  cached_bundle = bundle;
  cached_result = FALSE;
  vt_key.string = "ALRs";
  alr_count = prop_get_child_count (bundle, "I<-s", &vt_key);
  for (index_ = 0; index_ < alr_count; index_++)
    {
      const scr_char *original;

      original = prop_get_indexed_string (bundle, "ALRs", index_, "Original");
      if (original && strncmp (original, "Which ", 6) == 0)
        {
          cached_result = TRUE;
          break;
        }
    }
  return cached_result;
}

/* "Which <head>?  " (Scarier) or "Which <head>.  " (the Runner's form). */
static void
lib_which_head (scr_gameref_t game, const scr_char *head,
                const scr_char *tail)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, "Which ");
  pf_buffer_string (filter, head);
  if (tail)
    pf_buffer_string (filter, tail);
  pf_buffer_string (filter, lib_which_runner_form (game) ? ".  " : "?  ");
}

/*
 * How a pre-4.0 Runner settles a line naming two present characters.  It
 * never asks: characters() is one loop over every NPC in index order, and
 * each verb's arm either assigns the message outright, so the LAST named
 * character wins, or only when the message is still empty, so the FIRST
 * does.  Measured on p37/p38/p39NPCAMB (make_3738_npcambprobe.py; Ann and
 * Bob both "a guard" in the room, Cora a third guard next door), run370x
 * runner_probes/npcamb.run370.b.rtf, run380x
 * runner_probes/npcamb.run380.rtf, run390x
 * runner_probes/npcamb.run390.txt:
 *
 *   LAST    x guard (description), ask guard (hint), ask guard about key
 *           (topic), 3.9 take stone from guard ("Bob is not carrying...")
 *   FIRST   give (to) guard, 3.9 take guard, hit/kick guard, the 3.9
 *           character catch-all (hug/eat/bare `guard`)
 *
 * talk to, where is, 3.9 kiss and 3.7/3.8 take and take-from test no room
 * at all and are settled by their callers.  NPC_PICK_ASK keeps SCARE's own
 * question, which is also what 4.0 does in its own way.  Scarier asks at
 * every version (deliberate deviation), so the pick is a record only.
 */


scr_int
lib_disambiguate_npc (scr_gameref_t game,
                      const scr_char *verb, scr_bool *is_ambiguous)
{
  return lib_disambiguate_npc_pick (game, verb, is_ambiguous, NPC_PICK_ASK);
}

/*
 * lib_last_named_npc()
 *
 * The last NPC, in index order, that the line named, present or not and
 * seen or not -- what a plainly assigning characters() arm with no room test
 * leaves behind.  Call before lib_disambiguate_npc(), which clears the
 * references it filters out.  -1 for none.
 */
scr_int
lib_last_named_npc (scr_gameref_t game)
{
  scr_int index_, npc;

  npc = -1;
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (game->npc_references[index_])
        npc = index_;
    }
  return npc;
}

/*
 * lib_print_npc_not_here_pre390()
 *
 * run380 characters()' take arm (44054B-44059A) and run370's alike: for
 * every character the line names, "I don't think <prefix> <alias> would
 * appreciate being handled." if here, else "<Name> is not here!", assigned
 * outright -- so the last named character settles it, with no seen test.
 * `take cora` and `take stone from cora` with Cora next door, never met:
 * "Cora is not here!" (run370x runner_probes/npcamb.run370.one.rtf,
 * run380x runner_probes/npcamb.run380.one.rtf), where 3.9 says "Take
 * what?".  TRUE once it has answered for an absent character.
 */
scr_bool
lib_print_npc_not_here_pre390 (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (npc == -1 || npc_in_room (game, npc, gs_playerroom (game)))
    return FALSE;

  pf_buffer_string (filter, prop_get_indexed_string (gs_get_bundle (game),
                                                     "NPCs", npc, "Name"));
  pf_buffer_string (filter, " is not here!\n");
  return TRUE;
}

scr_int
lib_disambiguate_npc_pick (scr_gameref_t game, const scr_char *verb,
                           scr_bool *is_ambiguous, scr_int pick)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int count, index_, npc, listed;

  /*
   * Filter out all referenced NPCs not actually visible or seen.  Count the
   * number of NPCs remaining as referenced by the last command, and note the
   * last referenced NPC, for where count is 1.
   */
  count = 0;
  npc = -1;
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (game->npc_references[index_]
          && gs_npc_seen (game, index_)
          && npc_in_room (game, index_, gs_playerroom (game)))
        {
          count++;
          npc = index_;
        }
      else
        game->npc_references[index_] = FALSE;
    }

  /*
   * 4.0's Prefix contest thins the crowd before any handler sees it: the
   * character whose own Prefix words the line holds most of is THE
   * character, and only a tie at the top is still a crowd.  p4PFX2's
   * `x the red guard` examines Cid ("the red") over Ann ("a big red") and
   * Bob ("a red"), where all three answer to "guard"; see
   * lib_npc_400_prefix_score().  Pre-4.0 has no such contest at all -- see
   * NPC_PICK_FIRST/NPC_PICK_LAST below -- but Scarier holds it at every
   * version (deliberate deviation): `x blue guard` is Bob, not the last
   * guard by index.
   */
  if (count > 1 && run_get_dispatch_input ())
    {
      const scr_char *line = run_get_dispatch_input ();
      scr_int best, kept;

      best = -1;
      kept = 0;
      for (index_ = 0; index_ < gs_npc_count (game); index_++)
        {
          scr_int score;

          if (!game->npc_references[index_])
            continue;
          score = lib_npc_400_prefix_score (game, index_, line);
          if (score > best)
            {
              best = score;
              kept = 1;
            }
          else if (score == best)
            kept++;
        }

      if (kept > 0 && kept < count)
        {
          count = 0;
          npc = -1;
          for (index_ = 0; index_ < gs_npc_count (game); index_++)
            {
              if (game->npc_references[index_]
                  && lib_npc_400_prefix_score (game, index_, line) == best)
                {
                  count++;
                  npc = index_;
                }
              else
                game->npc_references[index_] = FALSE;
            }
        }
    }

  /* If the reference is unambiguous, set in variables and return it. */
  if (count == 1)
    {
      /* Set this NPC as the referenced character. */
      var_set_ref_character (vars, npc);

      /* Return, setting no ambiguity. */
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      return npc;
    }

  /* If nothing referenced, return no NPC. */
  if (count == 0)
    {
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      else
        {
          pf_buffer_string (filter,
                            "Please be more clear, who do you want to ");
          pf_buffer_string (filter, verb);
          pf_buffer_string (filter, "?\n");
        }
      return -1;
    }

  /*
   * Deliberate deviation: pre-4.0 asks too.  The Runners never do -- each
   * characters() arm takes the first or the last character named, per
   * NPC_PICK_FIRST/NPC_PICK_LAST above -- so `hit guard` with two guards
   * here hit whichever the index order gave.  The callers still name the
   * Runner's pick, as a record of it.
   */
  (void) pick;

  /* 4.0 asks its own question instead; see lib_npc_400_raise_for_line(). */
  if (lib_is_version_400 (game) && lib_npc_400_raise_for_line (game))
    {
      if (is_ambiguous)
        *is_ambiguous = TRUE;
      return -1;
    }

  /* The NPC reference is ambiguous, so list the choices. */
  pf_buffer_string (filter, "Please be more clear, who do you want to ");
  pf_buffer_string (filter, verb);
  pf_buffer_string (filter, "?  ");

  pf_new_sentence (filter);
  listed = 0;
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (game->npc_references[index_])
        {
          lib_print_npc_np (game, index_);
          listed++;
          if (listed < count)
            pf_buffer_string (filter, (listed < count - 1) ? ", " : " or ");
        }
    }
  pf_buffer_string (filter, "?\n");

  /* Return no NPC for an ambiguous reference. */
  if (is_ambiguous)
    *is_ambiguous = TRUE;
  return -1;
}


/*
 * lib_co_contains()
 * lib_co_lastword()
 * lib_runner_co_scan()
 * lib_co_ambiguity_prompt()
 * lib_trace_runner_co()
 *
 * The pre-4.0 Runners' object-ambiguity test, and the end-of-turn prompt it
 * raises.  Scarier's own `%object%` matcher is positional, so `take truck
 * keys` binds only the truck keys; the Runner has no positional matcher and
 * asks instead.
 *
 * The Runner matches an object by scanning the *whole* typed command for its
 * Short name or, failing that, its Alias (run380 `c()` @429048: a
 * case-insensitive InStr whose hit must start at the string start or after a
 * space, and end at the string end, a space or a comma).  Its `co()`
 * @42DE60 (run370 @4261B4, run390 @43B6BC) then takes the term that
 * matched, counts every object *present* (obhere: in the room, carried,
 * worn, or in/on something here) whose Short or Alias is exactly that term,
 * and if more than one is present flags the command ambiguous by stamping
 * the object's number into MemVar_44F124 (@42DDC7).  One escape hatch: if
 * the player also typed the last word of the object's own Prefix ("take
 * *silver* key"), @42DD4C stamps the resolved marker &HFE instead, and that
 * marker outranks any ambiguity flagged by any other object in the same
 * scan (@42DDC1 only writes an object number when the marker is not already
 * set).  The list is built once, by the first ambiguous object (@42DC1E:
 * every present object answering to that term, tense(Prefix) & " " & Short,
 * joined with ", " and " or "); the prompt's term comes from the LAST
 * flagged object.
 *
 * generaltasks() runs that scan over every object at the start of EVERY
 * command (run380 loc_441D5D, straight after the built-in input rewrites),
 * so the flag is raised whatever handler goes on to answer the line.  It is
 * read at the very end of the turn, after events have ticked (@4431B0,
 * `If (MemVar_44F124 < 0) Or (MemVar_44F12C = 1)`): unless a game task ran
 * (MemVar_44F12C, set at 44D0BA inside the task executor) the turn's whole
 * output is thrown away and replaced by
 *
 *     Which <term>.  <The X, the Y or the Z>?
 *
 * (@4432AA with the Short when the player typed it, @443303 with the Alias
 * otherwise).  Everything the turn DID still stands: mikes.taf cmd 27 `take
 * truck keys`, with the carried mustang keys and the truck keys both
 * aliased "keys", answers "Which keys.  The mustang keys or the truck
 * keys?" -- and the truck keys are taken, because `drive truck bob` works
 * 30 commands later (run380 under Wine, runner_probes/life_of_mike.run380.rtf
 * and the 2026-09-04 re-drive; an earlier note that the keys were NOT taken
 * was wrong).  The next command is not eaten as an answer -- `east` after the
 * prompt simply moves east.  4.0 narrows differently (the up-front word score
 * of Proc_21_58_463640, see lib_absent_seen_object()) and never raises this
 * prompt from the dispatcher, so the port stops at 3.9.
 */
scr_bool
lib_co_contains (const scr_char *command, const scr_char *term)
{
  scr_int term_length, index_;

  if (!command || !term || term[0] == NUL)
    return FALSE;

  /*
   * Deliberate deviations: the Runner's c() takes InStr's FIRST hit and
   * fails if that one is not word-bounded, so "key" is never found in
   * `x keyring key`; and it matches the raw stored name, so a Short
   * authored with a trailing space ("necko wafers ", superliam) is never
   * found at all.  Scarier looks on past a bad hit and ignores trailing
   * blanks.
   */
  term_length = strlen (term);
  while (term_length > 0 && scr_isspace (term[term_length - 1]))
    term_length--;
  if (term_length == 0)
    return FALSE;
  for (index_ = 0; command[index_] != NUL; index_++)
    {
      scr_char after;

      if (scr_strncasecmp (command + index_, term, term_length) != 0)
        continue;
      if (index_ > 0 && command[index_ - 1] != ' ')
        continue;

      after = command[index_ + term_length];
      if (after == NUL || after == ' ' || after == ',')
        return TRUE;
    }
  return FALSE;
}

/*
 * lib_co_term_shadowed()
 *
 * Deliberate deviation: TRUE when the line holds the object's term only
 * inside a longer Short or Alias of another object -- `take key ring` with a
 * "key" and a "key ring" does not name the key, nor `take truck keys` the
 * "mustang keys" aliased "keys".  The Runners' co() counts both, and gives
 * up on the pair or asks which.  Each longer name is blanked out of a copy
 * of the line, so `put key on key ring` still names the key.
 */
scr_bool
lib_co_term_shadowed (scr_gameref_t game, const scr_char *line,
                      scr_int object, const scr_char *term)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const size_t term_length = term ? strlen (term) : 0;
  std::string rest;
  scr_bool shadowed = FALSE;
  scr_int other;

  if (!line || term_length == 0 || !lib_co_contains (line, term))
    return FALSE;

  rest = line;
  for (other = 0; other < gs_object_count (game); other++)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      if (other == object)
        continue;
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", other);
      for (alias = -1; alias < alias_count; alias++)
        {
          const scr_char *name;
          size_t length, posn;

          if (alias < 0)
            name = prop_get_indexed_string (bundle, "Objects", other,
                                            "Short");
          else
            {
              vt_key[3].integer = alias;
              name = prop_get_string (bundle, "S<-sisi", vt_key);
            }
          if (!name || strlen (name) <= term_length
              || !lib_co_contains (name, term))
            continue;

          length = strlen (name);
          for (posn = 0; posn + length <= rest.size (); posn++)
            {
              if (scr_strncasecmp (rest.c_str () + posn, name, length) != 0
                  || (posn > 0 && rest[posn - 1] != ' ')
                  || (posn + length < rest.size ()
                      && rest[posn + length] != ' '
                      && rest[posn + length] != ','))
                continue;
              rest.replace (posn, length, length, '#');
              shadowed = TRUE;
            }
        }
    }
  return shadowed && !lib_co_contains (rest.c_str (), term);
}

const scr_char *
lib_co_lastword (const scr_char *string)
{
  const scr_char *space;

  if (!string)
    return NULL;
  space = strrchr (string, ' ');
  return space ? space + 1 : string;
}

/*
 * lib_co_names_prefix()
 *
 * Whether the typed line names the object's Prefix, when a term has several
 * present namesakes.  The Runners' co() matches one Prefix word: 3.90 and
 * 4.00 take the last word of the whole Prefix; 3.70 and 3.80 drop its
 * FIRST word first, so a Prefix of one word
 * -- "a", "the", or a bare adjective like "big" -- distinguishes nothing at
 * all, and a "big red" tells nothing apart from a "small red".
 *
 * Measured on p*TAKEP and p*TAKEQ (runner_probes/takep.run*.* and
 * runner_probes/takeq.run*.*, 2026-09-20), everything loose in one lit
 * room:
 *
 *                               3.70 / 3.80        3.90            4.00
 *   "big" gem, "small" gem
 *     take gem                  no             no              no
 *     take big gem              no             the big gem     the big gem
 *   "a" orb, "a" orb / "the" cog, "the" cog
 *     take orb / take cog       no             no              no
 *   "old red" pin, "new red" pin
 *     take pin                  no             no              no
 *     take red pin              BOTH           the old pin     no
 *   "a very red" gem, "a very blue" gem
 *     take very gem             no             no              no
 *     take red gem              the red gem    the red gem     the red gem
 *   "big red" pin, "small red" pin
 *     take big pin              no             no              the big pin
 *
 * ("no" is the version's own refusal: "Take what?" at 3.70, "Which pin.
 * Old red pin or new red pin?" at 3.80/3.90, and at 4.00 either that
 * question or "It is not clear which pin you are referring to." from
 * drops.)  `take red gem` answering while `take very gem` does not is what
 * makes it the LAST word of what is left and not the second; `take big
 * pin` failing at 3.90 as well is what makes 3.90's word the last of the
 * WHOLE Prefix and not the first-dropped one.  4.00 is the word score
 * instead (lib_verb_object_name_score()), which counts every Prefix word.
 *
 * Deliberate deviation: Scarier asks instead whether the line names some
 * word of the Prefix other than an article, and no word that only another
 * object of the same Short has in its Prefix, and no other object of that
 * Short passes the same test.  So `take big gem` picks the big gem at
 * 3.70/3.80, and `take old pin` and `take old red pin` the old red pin
 * everywhere, where the Runners refuse, ask "Which pin?" or take both; and
 * `take red pin` asks "Which pin?" everywhere, where 3.70/3.80 take both
 * and 3.90 the old pin.
 */
static scr_bool
lib_co_is_article (const std::string &word)
{
  return scr_strcasecmp (word.c_str (), "a") == 0
         || scr_strcasecmp (word.c_str (), "an") == 0
         || scr_strcasecmp (word.c_str (), "the") == 0
         || scr_strcasecmp (word.c_str (), "some") == 0;
}

static std::vector<std::string>
lib_co_prefix_words (scr_gameref_t game, scr_int object)
{
  const scr_char *prefix;
  std::vector<std::string> words;
  size_t start, end;

  prefix = prop_get_indexed_string (gs_get_bundle (game), "Objects", object,
                                    "Prefix");
  if (!prefix)
    return words;

  const std::string text (prefix);
  for (start = 0; start < text.size (); start = end + 1)
    {
      end = text.find (' ', start);
      if (end == std::string::npos)
        end = text.size ();
      if (end > start && !lib_co_is_article (text.substr (start, end - start)))
        words.push_back (text.substr (start, end - start));
    }
  return words;
}

/* TRUE if the line names a Prefix word that only another object of the same
   Short has: `take old pin` rules out the new red pin. */
scr_bool
lib_co_prefix_excluded (scr_gameref_t game, const scr_char *line,
                        scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const std::vector<std::string> own = lib_co_prefix_words (game, object);
  const scr_char *shortname;
  scr_int other;

  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  for (other = 0; other < gs_object_count (game); other++)
    {
      const scr_char *name
        = prop_get_indexed_string (bundle, "Objects", other, "Short");

      if (other == object || !name || !shortname
          || scr_strcasecmp (name, shortname) != 0)
        continue;
      for (const std::string &word : lib_co_prefix_words (game, other))
        {
          if (std::find (own.begin (), own.end (), word) == own.end ()
              && lib_co_contains (line, word.c_str ()))
            return TRUE;
        }
    }
  return FALSE;
}

static scr_bool
lib_co_prefix_named (scr_gameref_t game, const scr_char *line, scr_int object)
{
  scr_bool named = FALSE;

  for (const std::string &word : lib_co_prefix_words (game, object))
    named |= lib_co_contains (line, word.c_str ());
  return named && !lib_co_prefix_excluded (game, line, object);
}

scr_bool
lib_co_names_prefix (scr_gameref_t game, const scr_char *line, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *shortname;
  scr_int other;

  if (!lib_co_prefix_named (game, line, object))
    return FALSE;

  /* A namesake the line names just as well is a tie: `take red pin`. */
  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  for (other = 0; other < gs_object_count (game); other++)
    {
      const scr_char *name
        = prop_get_indexed_string (bundle, "Objects", other, "Short");

      if (other != object && name && shortname
          && scr_strcasecmp (name, shortname) == 0
          && lib_co_prefix_named (game, line, other))
        return FALSE;
    }
  return TRUE;
}

/*
 * lib_alias_prepare()
 *
 * Point vt_key[0..2] at the given object's/NPC's ("Objects"/"NPCs") Alias
 * list, and return its count -- ready for a "vt_key[3].integer = alias"
 * loop fetching each one with "S<-sisi".
 */
scr_int
lib_alias_prepare (const scr_prop_setref_t bundle, scr_vartype_t *vt_key,
                   const scr_char *category, scr_int index)
{
  vt_key[0].string = category;
  vt_key[1].integer = index;
  vt_key[2].string = "Alias";
  return prop_get_child_count (bundle, "I<-sis", vt_key);
}


/*
 * lib_first_alias()
 *
 * The object's/NPC's first Alias string, or NULL if it has none.
 */
const scr_char *
lib_first_alias (const scr_prop_setref_t bundle, scr_vartype_t *vt_key,
                 const scr_char *category, scr_int index)
{
  if (lib_alias_prepare (bundle, vt_key, category, index) < 1)
    return NULL;

  vt_key[3].integer = 0;
  return prop_get_string (bundle, "S<-sisi", vt_key);
}


/* Set once a 3.7 library handler has settled the line's object; see
 * lib_co_note_line_top(). */
scr_bool lib_co_prompt_370_blocked = FALSE;

/* TRUE if the object's Short or Alias is exactly the term. */
static scr_bool
lib_named_answers_to (scr_gameref_t game, const scr_char *class_,
                      scr_int index_, const scr_char *name_field,
                      const scr_char *term)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  const scr_char *name;
  scr_int alias_count, alias;

  name = prop_get_indexed_string (bundle, class_, index_, name_field);
  if (!scr_strempty (name) && scr_strcasecmp (name, term) == 0)
    return TRUE;

  alias_count = lib_alias_prepare (bundle, vt_key, class_, index_);
  for (alias = 0; alias < alias_count; alias++)
    {
      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (!scr_strempty (name) && scr_strcasecmp (name, term) == 0)
        return TRUE;
    }
  return FALSE;
}

scr_bool
lib_co_object_answers_to (scr_gameref_t game, scr_int object,
                          const scr_char *term)
{
  return lib_named_answers_to (game, "Objects", object, "Short", term);
}

/*
 * TRUE if the object counts as a namesake candidate: present (obhere) and,
 * from 3.90, also seen -- run390 co() @43B2FB and the list loop @43B4AE both
 * test the object's seen byte (field 44) beside obhere; run380 co() has no
 * such test.
 */
scr_bool
lib_co_candidate (scr_gameref_t game, scr_int object, scr_int room)
{
  if (!obj_indirectly_in_room (game, object, room))
    return FALSE;
  return prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390
         || gs_object_seen (game, object);
}

static scr_bool
lib_object_held_pre380 (scr_gameref_t game, scr_int object)
{
  return gs_object_position (game, object) == OBJ_HELD_PLAYER
         || gs_object_position (game, object) == OBJ_WORN_PLAYER;
}

/*
 * lib_namesake_crowded_pre380()
 *
 * Whether this object is one of the namesakes 3.7's loops find themselves
 * crowded on: its Short is in the typed line, and two or more present
 * objects on the verb's own side -- loose for takes(), held for drops() --
 * have their Shorts in it.  The count (takes' var_116, 436250) is of every
 * such Short, not of one shared name: `take coin hat` with both loose is
 * "Take what?" (p37ORD, run370 runner_probes/ord.run370.mult.rtf), so
 * same-Short namesakes are only the commonest crowd.  3.7's handlers never
 * call co(), so what makes a 3.7 crowd is the SHORT alone: two objects
 * answering to one word through an Alias are not namesakes to it, and each
 * simply acts.  p37OPENA (runner_probes/opena.run370.rtf, 2026-09-20) is that
 * side: a gem and a rock aliased "gem", both Prefixed "a", and `take gem` is
 * "You pick up the rock." with BOTH in the inventory afterwards -- while two
 * objects both Short "orb" and both Prefixed "a" are "Take what?" (p37TAKEP,
 * runner_probes/takep.run370.rtf).
 */
static scr_bool
lib_namesake_crowded_pre380 (scr_gameref_t game, const scr_char *line,
                             scr_int object, scr_bool want_held)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int room = gs_playerroom (game);
  const scr_char *shortname;
  scr_int other, present;

  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (scr_strempty (shortname) || !line || !lib_co_contains (line, shortname)
      || lib_co_term_shadowed (game, line, object, shortname))
    return FALSE;
  /* takes() acts on nothing on a "from" line (43648C), so its take-from
     lines are no crowd. */
  if (!want_held && lib_co_contains (line, "from"))
    return FALSE;

  present = 0;
  for (other = 0; other < gs_object_count (game); other++)
    {
      const scr_char *name
        = prop_get_indexed_string (bundle, "Objects", other, "Short");

      if (lib_co_candidate (game, other, room)
          && lib_object_held_pre380 (game, other) == want_held
          && !scr_strempty (name) && lib_co_contains (line, name)
          && !lib_co_term_shadowed (game, line, other, name))
        present++;
    }
  return present > 1;
}

/* TRUE if the object's Short or any Alias is a whole word run of the line,
   which is how 3.7's takes()/drops() loop names an object. */
static scr_bool
lib_names_object_370 (scr_gameref_t game, const scr_char *line,
                      scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *name;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias;

  name = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!scr_strempty (name) && lib_co_contains (line, name))
    return TRUE;
  alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < alias_count; alias++)
    {
      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (!scr_strempty (name) && lib_co_contains (line, name))
        return TRUE;
    }
  return FALSE;
}

/* TRUE if every Short and Alias of the object that the line names sits
   inside a longer name another object answers to -- "key" in `take key
   ring`.  Deliberate deviation: the Runner counts the key into the crowd
   and answers "Take what?"; Scarier leaves it out.  See
   lib_co_term_shadowed(). */
static scr_bool
lib_names_only_shadowed_370 (scr_gameref_t game, const scr_char *line,
                             scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *name;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias;

  name = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!scr_strempty (name) && lib_co_contains (line, name)
      && !lib_co_term_shadowed (game, line, object, name))
    return FALSE;
  alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < alias_count; alias++)
    {
      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (!scr_strempty (name) && lib_co_contains (line, name)
          && !lib_co_term_shadowed (game, line, object, name))
        return FALSE;
    }
  return TRUE;
}

/*
 * lib_takes_offers_tasks_370()
 *
 * TRUE when run370's takes() hands the typed line to the task matcher
 * itself.  takes() is entered on c("get") Or c("take") Or c("pick") Or
 * c(<the game's own take word>) And Not c("from") -- the And binds to the
 * slot word alone (435E28).  With no "all" and no "and" on the line, its last
 * loop (436B8D-436CC6) finds the first object, in index order and wherever it
 * is, whose Short or Alias the line names, calls tasks(1) with the line as
 * typed (436CAD) and leaves.  takes() then returns Empty -- it has no store
 * to its result slot; the `takes = MemVar_4460E4` VB Decompiler prints at
 * 436D17 is its ExitProc -- so generaltasks goes on to its own tasks(0) at
 * 43B972 and the matcher runs again.  See run_takes_second_pass_370().
 */
scr_bool
lib_takes_offers_tasks_370 (scr_gameref_t game, const scr_char *line)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object;

  if (prop_get_taf_version (bundle) != TAF_VERSION_370 || !line)
    return FALSE;

  const std::string slot = lib_command_slot_370 (bundle, 11);
  if (!(lib_co_contains (line, "get") || lib_co_contains (line, "take")
        || lib_co_contains (line, "pick")
        || (lib_co_contains (line, slot.c_str ())
            && !lib_co_contains (line, "from"))))
    return FALSE;
  if (lib_co_contains (line, "all") || lib_co_contains (line, "and"))
    return FALSE;

  for (object = 0; object < gs_object_count (game); object++)
    {
      if (lib_names_object_370 (game, line, object))
        return TRUE;
    }
  return FALSE;
}

/*
 * lib_co_mode_fits()
 *
 * Whether an object is on the side a co() mode recounts: 1 loose (take),
 * 2 isheld (drop), 3 held and not worn (wear), 4 worn (remove).
 */
scr_bool
lib_co_mode_fits (scr_gameref_t game, scr_int object, scr_int mode)
{
  switch (mode)
    {
    case 1:
      return !obj_is_static (game, object)
             && gs_object_position (game, object) == gs_playerroom (game) + 1;
    case 2:
      return lib_isheld_390 (game, object);
    case 3:
      return gs_object_position (game, object) == OBJ_HELD_PLAYER;
    case 4:
      return gs_object_position (game, object) == OBJ_WORN_PLAYER;
    default:
      return TRUE;
    }
}

/*
 * Which objects fitted the line's co() mode when the line began.  The
 * end-of-turn scan runs after the handler has acted -- `drop hat` has put
 * the held hat on the floor by then -- so it counts by this snapshot.
 */
static scr_int lib_co_top_mode = 0;
static std::vector<scr_bool> lib_co_top_fits;

static scr_bool
lib_co_mode_fitted (scr_gameref_t game, scr_int object, scr_int mode)
{
  if (mode == lib_co_top_mode && mode != 0
      && object < (scr_int) lib_co_top_fits.size ())
    return lib_co_top_fits[object];
  return lib_co_mode_fits (game, object, mode);
}

/* The co() mode for a line's leading verb, as lib_co_mode_fits() reads it. */
static scr_int
lib_co_verb_mode (const scr_char *line)
{
  while (*line && scr_isspace (*line))
    line++;
  const scr_char *end = line;
  while (*end && !scr_isspace (*end))
    end++;
  const std::string word (line, end - line);
  const scr_char *rest = end;
  while (*rest && scr_isspace (*rest))
    rest++;
  if (scr_strcasecmp (word.c_str (), "drop") == 0)
    return 2;
  if (scr_strcasecmp (word.c_str (), "wear") == 0
      || scr_strcasecmp (word.c_str (), "don") == 0)
    return 3;
  if (scr_strcasecmp (word.c_str (), "remove") == 0
      || scr_strcasecmp (word.c_str (), "doff") == 0)
    return 4;
  if (scr_strcasecmp (word.c_str (), "take") == 0
      || scr_strcasecmp (word.c_str (), "get") == 0
      || scr_strcasecmp (word.c_str (), "pick") == 0)
    return scr_strncasecmp (rest, "off", 3) == 0
           && (rest[3] == NUL || scr_isspace (rest[3])) ? 4 : 1;
  return 0;
}

/*
 * The co() mode the present namesakes answering to term are counted by:
 * the verb's own, or 0 (every one) when none fits it -- lib_co_pre400()'s
 * recount.
 */
static scr_int
lib_co_effective_mode (scr_gameref_t game, const scr_char *term,
                       scr_int mode)
{
  const scr_int room = gs_playerroom (game);
  scr_int object;

  for (object = 0; mode != 0 && object < gs_object_count (game); object++)
    {
      if (lib_co_candidate (game, object, room)
          && lib_co_object_answers_to (game, object, term)
          && lib_co_mode_fitted (game, object, mode))
        return mode;
    }
  return 0;
}

/*
 * Reproduce the scan.  Returns TRUE when the Runner would prompt, with
 * *prompt_term the last flagged object's term, *list_term the term the list
 * was built from (the first ambiguous object's), and *present that first
 * object's count of present namesakes.
 */
static scr_bool
lib_runner_co_scan (scr_gameref_t game, const scr_char *command,
                    const scr_char **prompt_term, const scr_char **list_term,
                    scr_int *present_count)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object, room;
  const scr_char *flagged_term = NULL, *first_term = NULL;
  scr_int first_present = 0;
  scr_bool resolved = FALSE;

  if (!command)
    return FALSE;
  room = gs_playerroom (game);

  /*
   * Deliberate deviation: the Runner counts every present namesake, so with
   * a red hat held and a blue one on the floor `drop hat` is "Which hat?"
   * (run380 before dropping, run390 after dropping the red hat).  Scarier
   * counts only the ones the verb can act on, as lib_co_pre400() does, and
   * a single one needs no asking.
   */
  const scr_int mode = lib_co_verb_mode (command);

  for (object = 0; object < gs_object_count (game); object++)
    {
      const scr_char *shortname, *term;
      scr_int alias_count, alias, other, present;

      shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");

      /* Pick the term the Runner would have matched on: Short, then Alias. */
      term = NULL;
      if (lib_co_contains (command, shortname))
        term = shortname;
      else
        {
          scr_vartype_t vt_key[4];

          alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
          for (alias = 0; alias < alias_count; alias++)
            {
              const scr_char *alias_name;

              vt_key[3].integer = alias;
              alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
              if (lib_co_contains (command, alias_name))
                {
                  term = alias_name;
                  break;
                }
            }
        }
      if (!term || lib_co_term_shadowed (game, command, object, term))
        continue;

      /* Count present objects whose Short or Alias is exactly that term. */
      const scr_int term_mode = lib_co_effective_mode (game, term, mode);
      present = 0;
      for (other = 0; other < gs_object_count (game); other++)
        {
          if (lib_co_candidate (game, other, room)
              && lib_co_object_answers_to (game, other, term)
              && lib_co_mode_fitted (game, other, term_mode))
            present++;
        }

      if (present > 1)
        {
          if (!first_term)
            {
              first_term = term;
              first_present = present;
            }
          if (lib_co_names_prefix (game, command, object))
            resolved = TRUE;
          else if (!resolved)
            flagged_term = term;
        }
    }

  if (!flagged_term || resolved)
    return FALSE;
  if (prompt_term)
    *prompt_term = flagged_term;
  if (list_term)
    *list_term = first_term;
  if (present_count)
    *present_count = first_present;
  return TRUE;
}

scr_bool
lib_co_ambiguity_prompt (scr_gameref_t game, const scr_char *command)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *prompt_term, *list_term;
  scr_int present, room, object, listed;

  /*
   * 3.7 to 3.9.  run390 runs the same scan from generaltasks() -- co(obj, 0)
   * for every object at 45F346, just before takes()/drops() -- and reads its
   * flag MemVar_468190 at the end of the turn (@4606BD, `(468190 < 0) Or
   * (468198 = 1)`, 468198 being set by the task executor @43F032) to print
   * "Which <term>.  <list>?" @4607BC/460832.  The difference is the seen
   * gate in lib_co_candidate(): troll.taf T64 `drop cup`, with Sid's seen
   * small cup on the bar and the carried empty cup, is "Which cup. The small
   * cup or the empty cup?" in run390 (runner_transcripts/troll).  Scarier
   * counts only the cups the drop can act on and prints the drop
   * (deliberate deviation, see lib_runner_co_scan()).  4.0 has its own
   * handler-scoped prompt.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    return FALSE;
  /*
   * run370 has no scan in generaltasks: co() is called only from therest()
   * (43CC86) and insides(), so a line one of the other handlers settled --
   * `wear hat`, `drop hat`, `x hat` with two hats -- never asks; each of
   * them answers its own way (lib_disambiguate_object_common).  p37TASK,
   * runner_probes/task.run370.pname.rtf /
   * runner_probes/task.run370.pname2.rtf, 2026-09-19.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_380
      && lib_co_prompt_370_blocked)
    return FALSE;
  if (!lib_runner_co_scan (game, command, &prompt_term, &list_term, &present))
    return FALSE;

  /*
   * The whole turn's output goes; only the prompt is shown.  The bare-give
   * completion's "(to Nobody)" is not part of it: the Runner prints that
   * echo as it rewrites the line (run390 45FAB9 -> 47B568), so `give hat`
   * with two hats is "(to Nobody)" and then "Which hat.  The red hat or
   * the blue hat?" (p39TASK, run390x runner_probes/task.run390.pclear.txt,
   * 2026-09-19).
   */
  {
    const std::string echo (pf_leading_reference (filter));

    pf_empty (filter);
    if (!echo.empty ())
      pf_rebuffer_reference (filter,
                             echo.substr (1, echo.size () - 3).c_str ());
  }
  lib_which_head (game, prompt_term, NULL);

  room = gs_playerroom (game);
  listed = 0;
  const scr_int list_mode
    = lib_co_effective_mode (game, list_term, lib_co_verb_mode (command));
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (!lib_co_candidate (game, object, room)
          || !lib_co_object_answers_to (game, object, list_term)
          || !lib_co_mode_fitted (game, object, list_mode))
        continue;

      if (listed > 0)
        pf_buffer_string (filter, listed == present - 1 ? " or " : ", ");
      else
        pf_new_sentence (filter);
      lib_print_object_np (game, object);
      listed++;
    }
  pf_buffer_string (filter, "?");
  pf_buffer_answer_break (filter);

  /*
   * 3.9 takes an answer.  The prompt leaves Short & "|" & line in
   * MemVar_4681D0 (460810; the Alias form 460886) and generaltasks, on a next
   * line nothing answered (460022), splices the answer into the line where
   * the term was and runs that: `wear hat` / `red` is "You put on the red
   * hat.", `open box` / `red` is "The red box is already open!", `close
   * box` / `red box` closes it.  An answer that leaves it ambiguous just
   * asks again: `remove hat` / `hat` and `wear hat` / `zzz` (the line
   * becomes `wear zzz hat`); `open box` / `hat` is "Which box.  The red hat
   * or the blue hat?" from the rerun `open hat box` (the list is built once
   * per turn, the term is the last flagged).  A line something answers
   * drops the question (`close box` / `look`).  3.7/3.8 store nothing
   * (run380 4432AA, run370 43C997).  See lib_battle_who_continuation().
   * p39TASK run390x runner_probes/task.run390.pfx.txt, 2026-09-19.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    lib_battle_who_store (std::string (prompt_term) + "|" + command);
  return TRUE;
}

/*
 * lib_prepass_seen_3738()
 *
 * 3.7/3.8 generaltasks opens every line element with a pass over the object
 * table that counts the objects the line names -- by Short or Alias,
 * anywhere in the game (run370 43B502, run380 co() at 441D5D).  Exactly one
 * makes it the antecedent and stops there.  Any other count -- none, or two
 * and more -- runs a second loop (run370 43B6C6, run380 441F21) whose tail
 * sits outside its name test and stamps the seen byte on EVERY present
 * object: dynamic ones held, worn or loose on the player's floor, statics
 * whose room array covers the player's room (run370 43B8C3-43B918, run380
 * 442124-442179).  Nothing inside a container or on a surface is touched.
 *
 * So `n` marks everything in the room the player is walking out of, while
 * `frob stone`, naming one object, leaves the stone unknown.  Measured on
 * p38EXAM/p37EXAM (runner_probes/exam.run380.seena.rtf,
 * runner_probes/exam.run380.seenb.rtf,
 * runner_probes/exam.run370.seena.rtf,
 * runner_probes/exam.run370.seenb.rtf): `frob stone` twice in the start
 * room is "What stone?" both times; after `n` the stone is known ("You
 * must be in the same room as the stone ..." in 3.8), and `frob coin`, the
 * coin inside the crate, is still "What coin?".
 */
void
lib_prepass_seen_3738 (scr_gameref_t game, const scr_char *command)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object, room, named;

  if (!command || prop_get_taf_version (bundle) >= TAF_VERSION_390)
    return;

  named = 0;
  for (object = 0; object < gs_object_count (game); object++)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;
      scr_bool names;

      names = lib_co_contains (command, prop_get_indexed_string
                                          (bundle, "Objects", object, "Short"));
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; !names && alias < alias_count; alias++)
        {
          vt_key[3].integer = alias;
          names = lib_co_contains (command,
                                   prop_get_string (bundle, "S<-sisi", vt_key));
        }
      if (names)
        named++;
    }
  if (named == 1)
    return;

  room = gs_playerroom (game);
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (gs_object_seen (game, object))
        continue;
      if (obj_is_static (game, object)
          ? obj_directly_in_room (game, object, room)
            || (!gs_object_static_unmoved (game, object)
                && gs_object_position (game, object) == OBJ_HELD_PLAYER)
          : gs_object_position (game, object) == OBJ_HELD_PLAYER
            || gs_object_position (game, object) == OBJ_WORN_PLAYER
            || gs_object_position (game, object) == room + 1)
        gs_set_object_seen (game, object, TRUE);
    }
}

/*
 * lib_co_400_*()
 *
 * The 4.0 object-ambiguity prompt, its pending question, and the answer
 * slot the question opens.  Measured 2026-09-07 with harness/make_400_coprobe.py
 * -> p4CO.taf, two rooms and eight static objects sharing one description so
 * that only the CHOICE shows: two trees whose Short is "tree", a rock (the
 * unique control), a mustang key and a truck key both aliased "keys", a hut
 * aliased "shed" beside an object whose Short IS "shed", and a third "tree"
 * in the far room as the presence control.  One task `poke %object%`, the
 * game's DontUnderstand "NO IDEA.", and a one-shot event printing "TICK." so
 * that a swallowed turn shows.  Transcripts runner_probes/co.run400.t9*.txt,
 * run400 under Wine (harness/make_400_coprobe.py).
 *
 * The gate comment on lib_co_ambiguity_prompt() above used to say 4.0 "never
 * raises this prompt from the dispatcher, so the port stops at 3.9".  It does
 * raise it -- from the two handlers rather than from the turn driver, under
 * two different tests (runner_probes/co.run400.t926.txt, every cell isolated
 * by a neutral `look`):
 *
 *     chop tree   ->  Which tree.  The red tree or the blue tree?
 *     x    tree   ->  Which tree.  The red tree or the blue tree?
 *     chop shed   ->  NO IDEA.
 *     x    shed   ->  Which shed.  The hut or the shed?
 *     chop keys   ->  NO IDEA.
 *     x    keys   ->  Which keys.  The mustang key or the truck key?
 *     chop rock   ->  I don't understand what you want me to do with the rock.
 *     x    rock   ->  A plain thing.
 *
 * So the library EXAMINE path prompts whenever two or more PRESENT objects
 * answer to the typed term by Short or by Alias ("shed" is one Short plus one
 * alias, "keys" two aliases; both prompt), while the unhandled-verb path
 * prompts only where the term is the SHORT of every tied candidate -- a
 * Short+alias or alias+alias tie is not ambiguous enough for it and the
 * game's DontUnderstand comes out instead.  Presence really is filtered: with
 * only the far room's tree present, `chop tree` gives the plain "I don't
 * understand what you want me to do with the tree."
 * (runner_probes/co.run400.t924.txt).
 *
 * The wording is the 3.7/3.8 one already ported above -- `Which <term>.
 * <NP> or <NP>?`, a full stop, two spaces, the noun phrases in index order
 * joined ", " / " or " -- which is why lca T91's two identically named trees
 * read "Which tree.  The tree or the tree?".
 *
 * What counts as a name, what gets listed, and where the term comes from
 * (runner_probes/co.run400.t928.txt, runner_probes/co.run400.t929.txt,
 * runner_probes/co.run400.t930.txt):
 *
 *     chop key        ->  NO IDEA.
 *     x    key        ->  You see no such thing.
 *     chop mustang    ->  NO IDEA.
 *     tree            ->  Which tree.  The red tree or the blue tree?
 *     rock            ->  I don't understand what you want me to do with the
 *                         rock.
 *     x    tree rock  ->  Which tree.  The red tree, the blue tree or the rock?
 *     x    rock tree  ->  Which tree.  The red tree, the blue tree or the rock?
 *     chop tree rock  ->  Which tree.  The red tree, the blue tree or the rock?
 *
 * A name matches whole or not at all -- "key" is a word inside two Shorts and
 * names nothing -- so an ambiguity 4.0 prompts about is always a shared whole
 * name.  The list is every object the LINE referenced, in index order, and
 * not just the term's namesakes (the rock is listed under "Which tree"), and
 * the term comes from the lowest-indexed ambiguous object rather than from
 * the order the nouns were typed (`x rock tree` still says "Which tree").  No
 * verb is needed for either path: a bare `tree` prompts, a bare `rock` gets
 * the unhandled-verb catch-all naming it.
 *
 * Neither the prompt nor any of its answers is a turn.  The probe's ticker
 * has StarterType 1 and Time1 = Time2 = 1, so its "TICK." lands on the first
 * real turn of the session: runner_probes/co.run400.t925.txt prints it after
 * the opening `look`, runner_probes/co.run400.t926.txt after the `look` that
 * FOLLOWS `chop tree`, and runner_probes/co.run400.t927.txt after the `look`
 * that follows both `x keys` and its answer `mustang`.  Every prompt and
 * every answer is therefore administrative.
 *
 * The prompt leaves a question pending and the NEXT line is read against it
 * (runner_probes/co.run400.t925.txt and runner_probes/co.run400.t927.txt,
 * and lca):
 *
 *     x keys / mustang     ->  That is still ambiguous!
 *     chop tree / red      ->  I don't understand what you want me to do with
 *                              the red tree.
 *     x tree / zzz         ->  That is still ambiguous!
 *     chop tree / x tree   ->  That is still ambiguous!
 *     x shed / chop keys   ->  That is still ambiguous!
 *     x tree rock / rock   ->  That is still ambiguous!
 *     x tree rock / blue   ->  A plain thing.
 *     x tree rock / x rock ->  A plain thing.
 *     x keys / x rock      ->  A plain thing.
 *     chop tree / look     ->  (the room description)
 *     chop tree / n        ->  (the player moves;
 *                                runner_transcripts/lca.txt:738)
 *
 * The rule that fits all of them is not "the next line is an answer": it is
 * that the pending question changes only the places a line can end up with
 * nothing to say.
 *
 *   - A line that DID something runs normally and drops the question
 *     (`look`, `n`, `x rock`).
 *   - A line that raises a NEW ambiguity prints "That is still ambiguous!"
 *     instead of a second full prompt (`x tree`, and `chop keys`, whose
 *     alias tie the unhandled-verb path would otherwise pass over to
 *     DontUnderstand).
 *   - A line that did nothing goes to the answer slot, and its own output
 *     goes with it: that is either the game's DontUnderstand text or the
 *     unhandled-verb catch-all.  `rock` / `x rock` is the pair that settles
 *     the second half -- bare `rock` gets the catch-all in isolation
 *     (runner_probes/co.run400.t930.txt), so with a question open it is
 *     claimed and answers "That is still ambiguous!", while `x rock`
 *     examines the rock.
 *
 * The slot does not look at the candidates at all: it splices the typed
 * words into the stored command where the term stood and re-runs the whole
 * rebuilt line (48B097-48B15B, lib_co_400_object_answer_line()).  `chop
 * tree` / `red` runs `chop red tree`, whose catch-all names the red tree;
 * `x tree rock` / `blue` runs `x blue tree rock` and describes the blue
 * tree.  "That is still ambiguous!" is not an answer this slot gives, it is
 * the re-run tying AGAIN on what the last prompt offered: `x keys` /
 * `mustang` runs `x mustang keys`, where neither Short is whole and both
 * aliases still match, and `x tree rock` / `rock` runs `x rock tree rock`.
 * So naming a listed object outright does not pick it -- but a word that
 * narrows the rebuilt line does, wherever it lands in it.
 *
 * Either answer clears the question: runner_probes/co.run400.t925.txt's
 * `x keys` gets the full prompt again immediately after `chop keys` had
 * answered "That is still ambiguous!".
 *
 * The sibling string "That wasn't one of the options!" belongs to the OTHER
 * half of the state.  generaltasks keeps two things, not one: the question
 * itself (MemVar_494234, "term|command") and the LIST the last prompt
 * offered (MemVar_4941F4).  They are spent differently.
 *
 *   - The question is taken at the top of a typed LINE and spent by its
 *     first element: 489FD4 copies it into var_98 -- and 489FEB, where the
 *     queue drain and the answer re-runs jump back in, skips that copy -- and
 *     48B5FC clears it again when it is still the same string.  A question
 *     raised by an EARLIER ELEMENT of the line being run is therefore never
 *     spent, and 48B6AE finds it still standing: the candidate list is not
 *     consulted at all, the answer is "That wasn't one of the options!", and
 *     the question is dropped (48BB5D/48BB8E).
 *   - The offered list outlives that.  co() accumulates the candidates'
 *     names into MemVar_4941F0 as it walks them (the Which arm 464560) and
 *     48BB53 copies that string into 4941F4 for every full prompt; 48B6F6
 *     (and 48B80F for a character) compares the two and prints "That is
 *     still ambiguous!" only when they are equal, clearing both.  An element
 *     that flagged no ambiguity at all forgets it (48B61F), and 48BB92
 *     clears the flagged list at the end of every element.
 *
 * Measured on p4CO with run400 (runner_probes/co.run400.co7.txt,
 * runner_probes/co.run400.co8.txt, runner_probes/co.run400.co9.txt,
 * runner_probes/co.run400.co10.txt, runner_probes/co.run400.co11.txt,
 * 2026-09-20), all administrative -- `turns` never moves:
 *
 *     x tree and x tree        ->  prompt / wasn't one of the options
 *     x tree then x tree       ->  the same, and so is `x tree. x tree`
 *     x keys and x tree        ->  prompt / wasn't one of the options
 *     x tree and x tree and x tree
 *                              ->  prompt / wasn't / still ambiguous
 *     x tree and x tree and x keys
 *                              ->  prompt / wasn't / Which keys...?
 *     x keys and x tree and x tree
 *                              ->  Which keys...? / wasn't / Which tree...?
 *     x tree and x rock and x tree
 *                              ->  prompt / A plain thing. / wasn't
 *     x tree / x tree / x tree ->  prompt / still ambiguous / prompt
 *     x tree / x keys          ->  prompt / Which keys...?
 *     x rock and x tree / x keys
 *                              ->  prompt / Which keys...?
 *
 * The last two are what proves the "still ambiguous" arm is a comparison and
 * not a flag: a DIFFERENT ambiguity right after a pending question gets a
 * full prompt.  `x tree and x rock and x tree` is what proves the question
 * survives an element that did something, and `x tree and x tree and x tree`
 * that "wasn't one of the options" leaves the offered list behind for the
 * element after it.  That it is a list and not the term is
 * runner_probes/co.run400.co14.txt: `x tree` / `x tree rock` -- one term,
 * two objects then three -- is prompt / prompt, where `x tree` twice is
 * prompt / still ambiguous.
 *
 * An element that says NOTHING never reaches any of this: the answer slot
 * claims it first, and takes the rest of the typed line with it.  That is
 * the whole difference between `chop tree and chop tree` (prompt, then the
 * slot's own "That is still ambiguous!") and `x tree and x tree`, and
 * `chop tree and chop tree and chop tree` prints nothing for its third
 * element at all -- see lib_co_400_raise_for_short_tie().
 */

/* The open question, and the object an answer resolved it to. */
static scr_bool lib_co_400_pending = FALSE;
static scr_bool lib_co_400_refused = FALSE;
static scr_bool lib_co_400_was_pending = FALSE;
static std::string lib_co_400_term;
static std::string lib_co_400_command;
static std::vector<scr_int> lib_co_400_candidates;

/* var_98: the question this typed line began with, still to be spent. */
static scr_bool lib_co_400_spend = FALSE;

/* A handler's own prompt (name_object's, not the generaltasks scan's) was
   raised by this line element; see lib_openclose_with_half_400(). */
scr_bool lib_co_400_named_raised = FALSE;

/* therest's with-split scored this element and so owns Me(424); see
   lib_openclose_with_half_raise_400(). */
scr_bool lib_co_400_therest_split = FALSE;

/*
 * MemVar_4941F4, and whether this element flagged an ambiguity at all.
 *
 * 4941F4 holds the LIST the last prompt offered, not its term: co()
 * accumulates the candidates' names into MemVar_4941F0 as it walks them
 * (the Which arm at 464560), 48BB53 copies that into 4941F4, and 48B6FF
 * compares the two strings.  `x tree` then `x tree rock` is the cell --
 * same term, two objects then three, and run400 asks the whole question
 * again (runner_probes/co.run400.co14.txt, 2026-09-20); the term model
 * said "That is still ambiguous!".  We compare the candidate objects
 * rather than the rendered string, which differs only where two different
 * sets render alike.
 */
static std::vector<scr_int> lib_co_400_prompt_list;
static scr_bool lib_co_400_prompt_seen = FALSE;
scr_bool lib_co_400_flagged = FALSE;

void
lib_co_400_reset (void)
{
  /*
   * The "Which" form is cached per bundle; a game reloaded in the same
   * process can land its bundle at the old address, so forget it here.
   */
  lib_which_cached_bundle = NULL;
  lib_which_cached_result = FALSE;
  lib_co_400_pending = FALSE;
  lib_co_400_was_pending = FALSE;
  lib_co_400_term.clear ();
  lib_co_400_command.clear ();
  lib_co_400_candidates.clear ();
  lib_co_400_refused = FALSE;
  lib_co_400_spend = FALSE;
  lib_co_400_prompt_list.clear ();
  lib_co_400_prompt_seen = FALSE;
  lib_co_400_flagged = FALSE;
  lib_co_400_named_raised = FALSE;
  lib_co_400_therest_split = FALSE;
}

/*
 * The question as it stands between two lines, for a Spatterlight autosave;
 * see run_session_state().  The rest lives only while a line is dispatched.
 *
 * Two things cross the line boundary, matching generaltasks' two memory
 * cells: the question itself (494234), and the list the last prompt OFFERED
 * (4941F4), which is what "That is still ambiguous!" compares the next
 * line's tie against.  The offered list is reported only when the next
 * lib_co_400_begin_line() will keep it -- the last element flagged an
 * ambiguity -- since otherwise that call drops it before anything reads it.
 * Without it a relaunch answered `chop tree` / `zzz` with the whole prompt
 * again where a live session says "That is still ambiguous!".
 */
void
lib_co_400_get_question (scr_bool *pending, std::string *term,
                         std::string *command,
                         std::vector<scr_int> *candidates,
                         scr_bool *offered,
                         std::vector<scr_int> *offered_list)
{
  *pending = lib_co_400_pending;
  *term = lib_co_400_term;
  *command = lib_co_400_command;
  *candidates = lib_co_400_candidates;
  *offered = lib_co_400_flagged && lib_co_400_prompt_seen;
  if (*offered)
    *offered_list = lib_co_400_prompt_list;
  else
    offered_list->clear ();
}

void
lib_co_400_set_question (scr_bool pending, const std::string &term,
                         const std::string &command,
                         const std::vector<scr_int> &candidates,
                         scr_bool offered,
                         const std::vector<scr_int> &offered_list)
{
  lib_co_400_reset ();
  lib_co_400_pending = pending;
  lib_co_400_term = term;
  lib_co_400_command = command;
  lib_co_400_candidates = candidates;
  if (offered)
    {
      /* As the prompt left them (48BB53), with the flag that carries the
         list past the next begin_line. */
      lib_co_400_prompt_list = offered_list;
      lib_co_400_prompt_seen = TRUE;
      lib_co_400_flagged = TRUE;
    }
}

/*
 * Called once per typed line element, before it is dispatched.  IS_NEW_LINE
 * marks the first element of a typed line, the only one that can spend the
 * question that was standing when the line was read (489FD4 / 48B5FC); a
 * question raised by an earlier element of the SAME line survives into this
 * one and becomes "That wasn't one of the options!".
 *
 * The prompted term (48BB53) is forgotten by the first element that flags no
 * ambiguity at all (48B61F), which is why `x tree` / `look` / `x tree` gets
 * two full prompts.
 */
void
lib_co_400_begin_line (scr_bool is_new_line)
{
  if (!lib_co_400_flagged)
    {
      lib_co_400_prompt_list.clear ();
      lib_co_400_prompt_seen = FALSE;
    }
  lib_co_400_flagged = FALSE;
  lib_co_400_named_raised = FALSE;
  lib_co_400_therest_split = FALSE;

  if (is_new_line)
    lib_co_400_spend = lib_co_400_pending;

  /* The answer slot reads the question as it stood BEFORE the spend. */
  lib_co_400_was_pending = lib_co_400_pending;
  if (lib_co_400_spend)
    {
      lib_co_400_pending = FALSE;
      lib_co_400_spend = FALSE;
    }
  lib_co_400_refused = FALSE;
}

/*
 * lib_antecedent_begin_line_400()
 *
 * generaltasks' first write of the line (48A3F5-48A42E): the whole line
 * scored by 463640 in mode 0, and a lone winner named in its definite form
 * -- `zzz red stone` "(the red stone)", `cut rope with blue stone ruby`
 * "(the blue stone)" (runner_probes/wtie.run400.it1.txt,
 * runner_probes/wtie3.run400.it2.txt, 2026-09-21).  This is also the "I
 * don't understand what you want me to do with" reply's antecedent.
 */

void
lib_antecedent_begin_line_400 (scr_gameref_t game, const scr_char *line)
{
  std::vector<scr_int> marked;
  scr_int object, pending, last_tied, mark_count;

  uip_begin_antecedent_400 ();
  if (!lib_is_version_400 (game) || !line || uip_pronoun_was_used ())
    return;

  object = lib_name_object_resolve_400 (game, line, 0, &pending, &last_tied,
                                        &marked, &mark_count);
  if (object >= 0)
    uip_note_antecedent_400 (object, UIP_IT_DEFINITE, UIP_STAGE_SCORER);
}

scr_bool
lib_co_400_question_pending (void)
{
  return lib_co_400_was_pending;
}

/*
 * Either answer takes the question with it before the rebuilt line is
 * re-run (48B152 and 48B193, both just before the jump back to 489FEB).
 * The term, the command and the candidates stay where they are: the slot is
 * still reading them, and the next prompt writes them all again.
 *
 * 489FEB is the top of the element loop, so the re-run starts with a clean
 * reply as well: it must be able to raise its own question rather than be
 * claimed by the one it is answering.  The prompted term of 48BB53 is NOT
 * cleared here -- it is what turns the re-run's raise into "That is still
 * ambiguous!".
 */
void
lib_co_400_take_question (void)
{
  lib_co_400_pending = FALSE;
  lib_co_400_was_pending = FALSE;
  lib_co_400_refused = FALSE;
}

/*
 * Did a handler's own "Which" prompt go up for this line element?  The
 * generaltasks scan's prompt (48B6AE) is raised after openclose has run and
 * is not this; see lib_openclose_with_half_400().
 */
scr_bool
lib_co_400_named_question_raised (void)
{
  return lib_co_400_named_raised;
}

/*
 * The prompt was printed but generaltasks never registered the question:
 * 48B60C saw Me(424) = -1 and printed the buffer as it stood.  The text
 * stays; the question, its term, its command and its candidates go, and
 * the next line is no answer to anything.
 */
void
lib_co_400_drop_question (void)
{
  lib_co_400_pending = FALSE;
  lib_co_400_refused = FALSE;
  lib_co_400_term.clear ();
  lib_co_400_command.clear ();
  lib_co_400_candidates.clear ();
}

/*
 * The unhandled-verb catch-all leaves the turn as empty-handed as the
 * DontUnderstand path does, so a pending question claims that line too; see
 * the answer slot in run_process_input_line().
 */
void
lib_co_400_note_refusal (void)
{
  lib_co_400_refused = TRUE;
}

scr_bool
lib_co_400_line_refused (void)
{
  return lib_co_400_refused;
}

void
lib_co_400_print_still_ambiguous (scr_gameref_t game)
{
  pf_buffer_string (gs_get_filter (game), "That is still ambiguous!\n");
  game->is_admin = TRUE;
}

/*
 * The character question (lib_npc_400_raise_for_line()) records no
 * candidates, and its answer is not scored: run400 re-runs the original line
 * with the typed words in front of the term.  Measured on p4BATTLEMULTI
 * (runner_probes/battlemulti.run400.whichf.txt, 2026-09-13): a fresh `attack
 * guard and droid` prints "Which Guard.  A guard or a guard?" for its first
 * half, and its second half `droid`, which alone gets only the catch-all,
 * answers it -- "That is still ambiguous!" and twelve more draws, the three
 * blows of `attack droid guard`.
 */
scr_bool
lib_co_400_pending_is_npc (void)
{
  return lib_co_400_candidates.empty () && !lib_co_400_term.empty ();
}

std::string
lib_co_400_npc_answer_line (const scr_char *line)
{
  const std::string &command = lib_co_400_command;
  const std::string &term = lib_co_400_term;
  std::string::size_type at;

  for (at = 0; at + term.size () <= command.size (); at++)
    {
      if (scr_strncasecmp (command.c_str () + at, term.c_str (),
                           term.size ()) == 0
          && (at == 0 || command[at - 1] == ' ')
          && (at + term.size () == command.size ()
              || command[at + term.size ()] == ' '))
        return command.substr (0, at) + line + " " + command.substr (at);
    }
  return command + " " + line;
}

/* How many of the candidates answer to exactly this name. */
static scr_int
lib_co_400_namesake_count (scr_gameref_t game,
                           const std::vector<scr_int> &objects,
                           const scr_char *term)
{
  scr_int index_, count;

  count = 0;
  for (index_ = 0; index_ < (scr_int) objects.size (); index_++)
    {
      if (lib_co_object_answers_to (game, objects[index_], term))
        count++;
    }
  return count;
}

/*
 * Raise the question.  With one already open the generaltasks scan does not
 * print a second prompt, only the short refusal; either way the line is
 * administrative and the question that was open is now spent.
 *
 * FROM_SCAN says the question is that scan's.  name_object's own prompt --
 * the one a crowded take, drop or put reaches through 463640 -- is raised by
 * a handler that RAN, and it prints in full whatever was open before: run400
 * answers p4TAKER's `drop cog` with "Which cog.  The cog or the cog?" and
 * the `drop pad` right after it with "Which pad.  The pad or the pad?"
 * (runner_probes/taker.run400.txt, 2026-09-20).
 */
static void
lib_co_400_raise_common (scr_gameref_t game, const scr_char *term,
                         const std::vector<scr_int> &objects,
                         scr_bool from_scan)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *command;
  scr_int index_;

  game->is_admin = TRUE;

  if (from_scan)
    {
      /* 48B6AE, on a question an earlier element of this line raised. */
      if (lib_co_400_pending)
        {
          pf_buffer_string (filter, "That wasn't one of the options!\n");
          lib_co_400_pending = FALSE;
          lib_co_400_term.clear ();
          lib_co_400_command.clear ();
          lib_co_400_candidates.clear ();
          lib_co_400_flagged = TRUE;
          return;
        }

      /* 48B6F6/48B80F: the same list offered twice running. */
      lib_co_400_flagged = TRUE;
      if (lib_co_400_prompt_seen && lib_co_400_prompt_list == objects)
        {
          pf_buffer_string (filter, "That is still ambiguous!\n");
          lib_co_400_prompt_list.clear ();
          lib_co_400_prompt_seen = FALSE;
          return;
        }
    }

  lib_which_head (game, term, NULL);

  pf_new_sentence (filter);
  for (index_ = 0; index_ < (scr_int) objects.size (); index_++)
    {
      if (index_ > 0)
        pf_buffer_string (filter,
                          index_ == (scr_int) objects.size () - 1
                          ? " or " : ", ");
      lib_print_object_np (game, objects[index_]);
    }
  pf_buffer_string (filter, "?");
  pf_buffer_answer_break (filter);

  command = run_get_dispatch_input ();
  lib_co_400_pending = TRUE;
  lib_co_400_term = term;
  lib_co_400_command = command ? command : "";
  lib_co_400_candidates = objects;
  if (from_scan)
    {
      lib_co_400_prompt_list = objects;         /* 48BB53 */
      lib_co_400_prompt_seen = TRUE;
    }
  else
    lib_co_400_named_raised = TRUE;
}

void
lib_co_400_raise (scr_gameref_t game, const scr_char *term,
                  const std::vector<scr_int> &objects)
{
  lib_co_400_raise_common (game, term, objects, TRUE);
}

/* name_object's prompt; see lib_co_400_raise_common(). */
void
lib_co_400_raise_named (scr_gameref_t game, const scr_char *term,
                        const std::vector<scr_int> &objects)
{
  lib_co_400_raise_common (game, term, objects, FALSE);
}

/*
 * lib_with_split_crowd_400()
 *
 * The question a line holding " with " raises comes out of ONE half, not out
 * of the whole line.  therest splits before any verb test (4883C5) and
 * scores each half with 463640, so the marked candidates the prompt reads
 * back are the ones the last half scored.  p4WTIE (run400,
 * runner_probes/wtie.run400.1.txt and runner_probes/wtie.run400.w[6-9].txt,
 * 2026-09-20), with "stone" the Short of two objects and a knife, a box and
 * a rope beside them:
 *
 *   head ties       `cut stone with knife`, `cut stone with zzz`,
 *                   `chop stone with knife` -- "Which stone.  The red stone
 *                   or the blue stone?", the knife left out of the list
 *                   although the line names it.  therest leaves at 488430
 *                   and the catch-all asks.
 *   head resolves   `cut knife with stone`, `cut rope with stone`, `open box
 *                   with stone`, `close box with stone`, `x box with stone`,
 *                   `x knife with stone`, `x rope with stone` -- the tail is
 *                   scored next (4884DB) and ITS tie is the question, again
 *                   the two stones alone.
 *   head names
 *   nothing         `chop zzz with stone` is the game's DontUnderstand text:
 *                   therest left before the tail was ever scored, and an
 *                   empty candidate list raises nothing.
 *
 * Examine is the exception.  It sits above therest and answers first: where
 * the head ties, with the whole line's reference set -- `x stone with knife`
 * is "Which stone.  The knife, the red stone or the blue stone?" and `x
 * stone with box` names the box the same way.  Where the head does not tie
 * it describes the head's object and asks nothing of its own; `x knife with
 * stone` and `x box with stone` list the stones alone because openclose's
 * with-half asked first and examines never undid it (see
 * lib_openclose_with_half_raise_400()).  The unhandled verb's head naming
 * nothing asks nothing either (therest's restart leaves Me(424) at -1).
 *
 * Fills *crowd with the objects to ask about (empty = ask nothing), and
 * *pending with the tail tie's pending object when the tail asks, *head_object
 * with the head's object when it named one; returns TRUE when the split
 * decides, FALSE leaves the caller its own whole-line list.
 */
static scr_int
lib_with_half_tied_400 (scr_gameref_t game, const scr_char *half,
                        std::vector<scr_int> *tied)
{
  scr_int object;

  object = lib_verb_object_resolve_400_string (game, half, tied, TRUE);
  if (object < 0)
    object = lib_verb_object_resolve_400_string (game, half, tied, FALSE);

  /* Only a tie leaves a crowd behind; a winner marked just itself. */
  if (object != -1)
    tied->clear ();
  return object;
}

scr_bool
lib_with_split_crowd_400 (scr_gameref_t game, scr_bool examine,
                          std::vector<scr_int> *crowd, scr_int *pending,
                          scr_int *head_object)
{
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *tail = input ? strstr (input, " with ") : NULL;
  std::vector<scr_int> marked;
  std::string head;
  scr_int object, tied_pending, last_tied, mark_count;

  crowd->clear ();
  *pending = -1;
  *head_object = -1;
  if (!lib_is_version_400 (game) || !tail)
    return FALSE;

  head.assign (input, tail - input);
  object = lib_with_half_tied_400 (game, head.c_str (), crowd);
  if (object == -1)
    return !examine;

  /*
   * Past a head that does not tie, examine asks nothing of its own: the
   * question is openclose's, which ran before it and which examines never
   * undoes (lib_openclose_with_half_raise_400()).  `x ruby with stone`
   * describes, where `x rope with stone` asks.
   */
  crowd->clear ();
  *head_object = object;
  if (examine || object == -2)
    return TRUE;

  /*
   * The tail is 463640's, pending object and all: the question's term is
   * that object's Short, replaced by the last of its own aliases the whole
   * line holds -- `cut pebble with stone` is "Which pebble.", `cut flint
   * with stone` "Which stone." (the blue stone parks, "flint" is the red
   * one's), and `cut rope with stone flint pebble` ties 2-2 and asks
   * "Which pebble." too (runner_probes/wtie2.run400.19.txt, 2026-09-21).
   * It asks whether or not the whole line named one object.
   */
  if (lib_name_object_resolve_400 (game, tail + 6, 0, &tied_pending,
                                   &last_tied, &marked, &mark_count) == -1
      && tied_pending >= 0 && (scr_int) marked.size () == mark_count
      && marked.size () >= 2)
    {
      *crowd = marked;
      *pending = tied_pending;
    }
  return TRUE;
}

/* The with-split's tail question; see lib_with_split_crowd_400(). */
scr_bool
lib_co_400_raise_for_with_tail (scr_gameref_t game, scr_int pending,
                                const std::vector<scr_int> &crowd)
{
  const scr_char *input = run_get_dispatch_input ();

  if (!input || pending < 0 || crowd.size () < 2)
    return FALSE;

  /* An open question takes the element first; see
     lib_co_400_raise_for_short_tie(). */
  if (lib_co_400_question_pending ())
    {
      lib_co_400_note_refusal ();
      return FALSE;
    }

  lib_co_400_raise (game, lib_drop_named_term_400 (game, pending, input, TRUE),
                    crowd);
  return TRUE;
}

/*
 * The examine path's test, read off run400's walk rather than off the
 * reference set.  examines() hands a line 463640 tied (MemVar_4942F8 < -1)
 * to referencedob (457034), and it is co() -- not a scan of its own -- that
 * leaves the question behind:
 *
 *   463640   Me(424) = the tie arm's pending object (4633F0, the index+2
 *            quirk; see lib_name_object_resolve_400()) and Me(428) = the
 *            pass-0 marks, "the X, the Y or the Z?" (46348B).
 *   pass A   co(i, 3) marks every object whose word has a present, seen
 *            namesake; fewer than two marked ends the walk.
 *   pass B   co(i, 0) over the marked, in index order.  A word with one
 *            namesake resets Me(424) = -1 (46485E).  A crowded word runs
 *            the Prefix contest 454454: a unique winner that is i sets
 *            Me(424) = -2; otherwise Me(428) is REBUILT from the word's
 *            namesakes only when it is empty or does not hold the word as a
 *            binary substring (46462A-464733), and a contest with no winner
 *            parks Me(424) = i when Me(424) < 0 or i is present (464767).
 *
 * generaltasks then asks whenever Me(424) is an object (48B6B1), whatever
 * examines found: the term is Short(Me(424)) replaced by the last of its
 * aliases the line holds, the list is Me(428).  Measured on p4CO with
 * run400 (runner_probes/co.run400.co15.txt, 2026-09-21) -- red tree 0,
 * blue tree 1, rock 2, mustang key 3 and truck key 4 (both aliased
 * "keys"), hut 5 (aliased "shed"), shed 6, and a tree 7 in the other
 * room:
 *
 *   x keys shed, x shed keys   Which shed.  The hut or the shed?
 *                              (the keys rebuild, then "shed" rebuilds)
 *   x tree keys, x keys tree,  Which keys.  The red tree or the blue tree?
 *   x rock tree keys           (the absent tree 7 rebuilds last and parks
 *                              nothing; the truck key parked last)
 *   x rock keys                Which keys.  The mustang key or the truck
 *                              key?  (the rock resets, the keys park)
 *   x tree rock                Which tree.  The red tree, the blue tree or
 *                              the rock?  (463640's list stands; the rock
 *                              resets and the absent tree 7 parks)
 *   x shed tree                Which shed.  The red tree, the blue tree,
 *                              the hut or the shed?
 *   x tree hut                 Which tree.  The red tree, the blue tree or
 *                              the hut?
 *
 * And runner_probes/co.run400.t925.txt's `chop keys` answering `x shed`:
 * the rebuilt `x chop keys shed` ends on "The hut or the shed?" again, the
 * list 48B6FF compares, so "That is still ambiguous!".
 */

/* Me(428) as co() builds it, for its substring test. */
static std::string
lib_co_400_list_string (scr_gameref_t game, const std::vector<scr_int> &list)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  std::string text;
  scr_int index_;

  for (index_ = 0; index_ < (scr_int) list.size (); index_++)
    {
      if (index_ > 0)
        text += index_ == (scr_int) list.size () - 1 ? " or " : ", ";
      text += "the ";
      text += prop_get_indexed_string (bundle, "Objects", list[index_],
                                       "Short");
    }
  return text + "?";
}

/* How many words of the object's Prefix ("a" when empty) the line holds. */
static scr_int
lib_co_400_prefix_hits (scr_gameref_t game, scr_int object,
                        const scr_char *input)
{
  const scr_char *prefix;
  std::string copy;
  std::string::size_type at, next;
  scr_int found;

  prefix = prop_get_indexed_string (gs_get_bundle (game), "Objects", object,
                                    "Prefix");
  copy = scr_strempty (prefix) ? "a" : prefix;

  found = 0;
  for (at = 0; at <= copy.size (); at = next + 1)
    {
      std::string word;

      next = copy.find (' ', at);
      if (next == std::string::npos)
        next = copy.size ();
      word = copy.substr (at, next - at);
      if (!word.empty () && lib_input_contains_word_400 (input, word.c_str ()))
        found++;
    }
  return found;
}

/*
 * 454454(word, room): over the objects in the room, every name field equal
 * to the word scores its object's Prefix words in the line; the strict
 * maximum above zero wins, anything else is -1.
 */
scr_int
lib_co_400_prefix_contest (scr_gameref_t game, const scr_char *word,
                           const scr_char *input, scr_int room)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object, result, best;

  result = -1;
  best = 0;
  for (object = 0; object < gs_object_count (game); object++)
    {
      const scr_char *name;
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      if (!obj_directly_in_room (game, object, room))
        continue;

      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = -1; alias < alias_count; alias++)
        {
          scr_int hits;

          if (alias < 0)
            name = prop_get_indexed_string (bundle, "Objects", object,
                                            "Short");
          else
            {
              vt_key[3].integer = alias;
              name = prop_get_string (bundle, "S<-sisi", vt_key);
            }
          if (scr_strempty (name) || scr_strcasecmp (name, word) != 0)
            continue;

          hits = lib_co_400_prefix_hits (game, object, input);
          if (hits == best)
            result = -1;
          else if (hits > best)
            {
              result = object;
              best = hits;
            }
        }
    }
  return result;
}

/*
 * One co(object, 0) call, as pass B below and openclose's with-half loop
 * make it: *ME is Me(424), *LIST Me(428), *LIST_OK whether that list is one
 * we can render (see lib_co_400_raise_for_references()).
 */
void
lib_co_400_walk_step (scr_gameref_t game, scr_int object,
                      const scr_char *input, scr_int *me,
                      std::vector<scr_int> *list, scr_bool *list_ok)
{
  const scr_int room = gs_playerroom (game);
  const scr_char *word;
  scr_int count;

  word = lib_co_400_name_word (game, object, input);
  if (!word)
    return;
  count = lib_co_400_present_namesakes (game, word);
  if (count == 1)
    {
      *me = -1;
      return;
    }
  if (count < 2)
    return;

  /*
   * Both arms hand the antecedent setter a name as they go (46460F, 464788;
   * see uip_note_antecedent_400()): the -2 arm Prefix & " " & Short, the
   * park arm the bare Short.
   */
  if (lib_co_400_prefix_contest (game, word, input, room) == object)
    {
      *me = -2;
      uip_note_antecedent_400 (object, UIP_IT_INDEFINITE, UIP_STAGE_CO);
      return;
    }

  if (list->empty ()
      || !strstr (lib_co_400_list_string (game, *list).c_str (), word))
    {
      scr_int other;

      list->clear ();
      for (other = 0; other < gs_object_count (game); other++)
        {
          if (gs_object_seen (game, other)
              && obj_indirectly_in_room (game, other, room)
              && lib_co_object_answers_to (game, other, word))
            list->push_back (other);
        }
      *list_ok = TRUE;
    }

  if (lib_co_400_prefix_contest (game, word, input, room) == -1
      && (*me < 0 || obj_indirectly_in_room (game, object, room)))
    {
      *me = object;
      uip_note_antecedent_400 (object, UIP_IT_BARE, UIP_STAGE_CO);
    }
}

static scr_bool
lib_co_400_raise_for_references (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  std::vector<scr_int> marked, list, walk;
  scr_int object, me, last_tied, mark_count, index_;
  scr_bool list_ok;

  if (!input)
    return FALSE;

  object = lib_name_object_resolve_400 (game, input, 0, &me, &last_tied,
                                        &marked, &mark_count);
  if (object != -1)
    return FALSE;
  list = marked;
  list_ok = (scr_int) marked.size () == mark_count;

  /* Pass A. */
  for (object = 0; object < gs_object_count (game); object++)
    {
      const scr_char *word = lib_co_400_name_word (game, object, input);

      if (word && lib_co_400_present_namesakes (game, word) > 0)
        walk.push_back (object);
    }

  /* Pass B. */
  if (walk.size () >= 2)
    {
      for (index_ = 0; index_ < (scr_int) walk.size (); index_++)
        lib_co_400_walk_step (game, walk[index_], input, &me, &list,
                              &list_ok);
    }

  if (me < 0 || !list_ok || list.size () < 2)
    return FALSE;

  /*
   * examines has already described referencedob's pick, and named it to
   * the antecedent setter (471749-471789), before generaltasks replaces the
   * description with the question: `x stone` asks, and `x it` then echoes
   * "(a blue stone)", pass A's last mark
   * (runner_probes/wtie.run400.it1.txt, 2026-09-21).
   */
  object = lib_examine_referencedob_ex_400 (game, input, TRUE);
  if (object >= 0 && obj_indirectly_in_room (game, object, room))
    uip_note_antecedent_400 (object, UIP_IT_INDEFINITE, UIP_STAGE_HANDLER);

  lib_co_400_raise (game, lib_drop_named_term_400 (game, me, input, TRUE),
                    list);
  return TRUE;
}

/*
 * The examine path's test for a line SCARE's parser bound to one object: the
 * Runner's pass 1 settles on any present object whose Short the line holds;
 * failing that, every present object with a Short or alias in the line is a
 * candidate, and the first contained name (candidates in index order, Short
 * then aliases) that two or more of them answer to is the question's term.
 */
static scr_bool
lib_co_400_raise_for_contained_aliases (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  std::vector<scr_int> candidates;
  scr_int object, index_;

  if (!input)
    return FALSE;

  for (object = 0; object < gs_object_count (game); object++)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;
      scr_bool hit;

      if (!lib_co_candidate (game, object, room))
        continue;
      if (lib_co_contains (input, prop_get_indexed_string (bundle, "Objects",
                                                           object, "Short")))
        return FALSE;

      /* Deliberate deviation: an alias the line holds only inside another
         object's longer name does not make a candidate; see
         lib_co_term_shadowed(). */
      hit = FALSE;
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; alias < alias_count && !hit; alias++)
        {
          const scr_char *name;

          vt_key[3].integer = alias;
          name = prop_get_string (bundle, "S<-sisi", vt_key);
          hit = lib_co_contains (input, name)
                && !lib_co_term_shadowed (game, input, object, name);
        }
      if (hit)
        candidates.push_back (object);
    }
  if (candidates.size () < 2)
    return FALSE;

  for (index_ = 0; index_ < (scr_int) candidates.size (); index_++)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      object = candidates[index_];
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; alias < alias_count; alias++)
        {
          const scr_char *name;

          vt_key[3].integer = alias;
          name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (scr_strempty (name) || !lib_co_contains (input, name)
              || lib_co_400_namesake_count (game, candidates, name) < 2)
            continue;

          lib_co_400_raise (game, name, candidates);
          return TRUE;
        }
    }
  return FALSE;
}

/*
 * The unhandled-verb path's test.  The candidates are the objects the 4.0
 * noun score tied on, and the term is the Short of the lowest-indexed
 * candidate that the line contains and that two or more of them share --
 * aliases do not count here, which is why run400 answers `chop shed` (the
 * hut answers to "shed" only by alias) with the game's DontUnderstand while
 * `x shed` raises the question.  A candidate that shares no name still gets
 * listed: `chop tree rock` prompts with all three
 * (runner_probes/co.run400.t929.txt).
 */
/*
 * The term the scan prompts with.  The Short tie above is what RAISES the
 * question, but the word it asks about is not always that Short: an ALIAS
 * two or more of the candidates share, typed as a whole word of the line,
 * takes its place.  p4CO, run400, 2026-09-20
 * (runner_probes/co.run400.co12.txt, runner_probes/co.run400.co13.txt):
 *
 *     chop keys tree        Which keys.  ... the mustang key or the truck key?
 *     chop tree keys        Which keys.  (same list; the typed order is
 *                           not what picks the term)
 *     chop shed keys tree   Which keys.  ... the hut or the shed?
 *     chop shed tree        Which tree.  The red tree, the blue tree, the
 *                           hut or the shed?
 *     chop tree hut         Which tree.  The red tree, the blue tree or
 *                           the hut?
 *     chop keys             NO IDEA.     (an alias tie alone raises nothing)
 *
 * "keys" is an Alias of BOTH keys; "shed" is the hut's Alias and the shed's
 * Short, and it never becomes the term.  So the replacement wants two
 * candidates answering by the same field, which is the shape of the count
 * above -- and "shed" alone, like "keys" alone, raises nothing at all.
 *
 * In generaltasks this is not a second pass but one object: 48B73C-48B78C
 * reads the record of MemVar_4941EC and takes its Short, replaced by each
 * of ITS aliases that is a whole word of the line, so the object 4941EC
 * ended on decides the word.  co() parks it as it walks (index/find.py -v
 * run400 46486C: the Which arm at 464560 stores arg_C when the prefix
 * contest 454454 returns -1), and the last park wins -- which is the keys,
 * index 3 and 4, over the trees at 0 and 1.  Why the hut and the shed,
 * walked last of all, park nothing is 463640's index+2 quirk, and the
 * unhandled-verb line now reads the pending object itself (see
 * lib_co_400_raise_for_pending_tie()) and the examine line co()'s walk
 * (lib_co_400_raise_for_references()); only the " with " split's HEAD tie
 * still takes this alias test (its tail reads the pending object too, see
 * lib_with_split_crowd_400()).
 */
static const scr_char *
lib_co_400_scan_term_400 (scr_gameref_t game, const std::vector<scr_int> &tied,
                          const scr_char *term)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *replacement = term;
  scr_int index_;

  for (index_ = 0; index_ < (scr_int) tied.size (); index_++)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      alias_count = lib_alias_prepare (bundle, vt_key, "Objects",
                                       tied[index_]);
      for (alias = 0; alias < alias_count; alias++)
        {
          const scr_char *name;
          scr_int other, count;

          vt_key[3].integer = alias;
          name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (scr_strempty (name) || !lib_input_contains_word (input, name))
            continue;

          count = 0;
          for (other = 0; other < (scr_int) tied.size (); other++)
            {
              scr_vartype_t vt_other[4];
              scr_int other_count, index2;

              other_count = lib_alias_prepare (bundle, vt_other, "Objects",
                                               tied[other]);
              for (index2 = 0; index2 < other_count; index2++)
                {
                  const scr_char *other_name;

                  vt_other[3].integer = index2;
                  other_name = prop_get_string (bundle, "S<-sisi", vt_other);
                  if (!scr_strempty (other_name)
                      && scr_strcasecmp (other_name, name) == 0)
                    {
                      count++;
                      break;
                    }
                }
            }
          if (count > 1)
            replacement = name;
        }
    }

  return replacement;
}

scr_bool
lib_co_400_raise_for_short_tie (scr_gameref_t game,
                                const std::vector<scr_int> &tied)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *term;
  scr_int index_, count;

  if (!input || tied.size () < 2)
    return FALSE;

  /*
   * With a question already open this element never reaches the prompt at
   * all.  generaltasks tries the ANSWER first (48AFF3), and its gate is
   * "this element has said nothing yet" -- MemVar_4941B0, cleared at the top
   * of every element (489FEE) and still empty here because the ambiguity
   * flag holds the unhandled-verb catch-all back.  So `chop tree` twice over
   * is the answer slot's "That is still ambiguous!" and not the scan's
   * "That wasn't one of the options!", which only an element that DID answer
   * -- an examine, whose reply is in the buffer -- can reach
   * (runner_probes/co.run400.co11.txt, 2026-09-20).
   */
  if (lib_co_400_question_pending ())
    {
      lib_co_400_note_refusal ();
      return FALSE;
    }

  /*
   * The crowd's FIRST object decides: the walk keeps one best, and only a
   * tie whose Short matches the best's parks the pending object the
   * question is asked from.  So a namesake pair the line names after some
   * other object of the same score never asks -- p4WTIE's `chop stone
   * knife` is the game's DontUnderstand text, where `chop stone` and
   * p4CO's `chop tree rock` (the pair first, the odd one after) both ask
   * (runner_probes/wtie.run400.w6.txt, runner_probes/wtie.run400.w7.txt,
   * 2026-09-20).
   */
  term = prop_get_indexed_string (bundle, "Objects", tied[0], "Short");
  if (scr_strempty (term) || !lib_input_contains_word (input, term))
    return FALSE;

  count = 0;
  for (index_ = 0; index_ < (scr_int) tied.size (); index_++)
    {
      const scr_char *name;

      name = prop_get_indexed_string (bundle, "Objects", tied[index_],
                                      "Short");
      if (!scr_strempty (name) && scr_strcasecmp (name, term) == 0)
        count++;
    }
  if (count < 2)
    return FALSE;

  lib_co_400_raise (game, lib_co_400_scan_term_400 (game, tied, term), tied);
  return TRUE;
}

/*
 * lib_npc_400_raise_for_line()
 *
 * The character half of the same question.  generaltasks raises it at
 * 48B815-48B928 (48BA87-48BB53 on its second pass): the term var_A4 is the
 * flagged NPC's Name (field 0), replaced by every one of its aliases (field
 * 8, count field 12) that is a whole word of the line, and the prompt is
 * "Which " & term & ".  " & list & "?", with "That is still ambiguous!" in
 * its place while a question is already open.  Measured 2026-09-13 on
 * harness/make_400_battlemultiprobe.py
 * (runner_probes/battlemulti.run400.txt): two NPCs both Named "Guard",
 * Prefix "a", in the room --
 *
 *     attack guard                     ->  Which Guard.  A guard or a guard?
 *     attack droid guard with blaster  ->  Which Guard.  A guard or a guard?
 *     attack guard and droid           ->  That is still ambiguous!  (the
 *                                          question from the line before)
 *
 * and on light_up (runner_probes/light_up.run400.txt T294), where "Red
 * Riven" and "Blue Riven" (Prefixes "Red"/"Blue") share the alias "riven":
 * `attack riven` -> "Which riven.  Red riven or Blue riven?".  So only the
 * term's namesakes are listed -- the droid the line also names is not --
 * each as its Prefix and the lower-cased term, the first capitalised; and
 * nothing is struck, not even the droid.  Neither the question nor its
 * answer is a turn.
 */
static scr_bool
lib_npc_answers_to (scr_gameref_t game, scr_int npc, const scr_char *term)
{
  return lib_named_answers_to (game, "NPCs", npc, "Name", term);
}

/*
 * lib_npc_400_prefix_score()
 *
 * The character half of the 4.0 Prefix contest: run400's namesake check
 * (Proc_21_40_45E99C) asks Proc_21_49_450610 which of the term's namesakes
 * the line describes best, and the answer is simply how many of a
 * character's OWN Prefix words the line holds as whole words.  A strict
 * maximum wins outright and nothing is asked; anything else -- a tie at
 * any height, zero included -- leaves the ambiguity standing.
 *
 * Measured 2026-09-20 on p4PFX2.taf (make_prefixprobe.py ... 2): Ann "a big
 * red", Bob "a red" and Cid "the red", all three aliased "guard" and all in
 * the room, run400x runner_probes/pfx2.run400.txt.
 *
 *     x guard          ->  Which guard.  A big red guard, a red guard or
 *                          the red guard?      (0-0-0)
 *     x red guard      ->  the same question                    (1-1-1)
 *     x a red guard    ->  the same question                    (2-2-1)
 *     x the red guard  ->  CID DESC.                            (1-1-2)
 *     x big guard      ->  ANN DESC.                            (1-0-0)
 *     x a guard        ->  the same question                    (1-1-0)
 *
 * `x the red guard` is the cell that settles the articles: "the" has to
 * score like any other Prefix word for Cid to win it, and `x a red guard`
 * would be Cid's too if only "red" counted.  So the whole Prefix is split
 * on spaces and every word scores -- the same rule the object scorer
 * 463640 follows (lib_verb_object_name_score()).
 *
 * The question's LIST is untouched by the contest: `x a red guard` still
 * offers all three guards though Cid scores under the others.  Only the
 * object question narrows itself to its winners; see
 * lib_disambiguate_object_common().
 *
 * An empty NPC Prefix scores nothing, and that one is from the decompile
 * rather than from a probe: the 4.0 loader substitutes "a" for an empty
 * OBJECT prefix (4900EC) -- which is what makes a prefix-less object score
 * a typed "a" -- and mdlSpreadTheLoad has no such default for characters.
 *
 * 450610's own body (4504BC-45060D) differs from ours in three ways that
 * no probe has reached yet, all of them noted rather than modelled:
 *
 *   - it has NO admission test whatever, so an ABSENT character answering
 *     to the term joins the contest and can win it away from the two in
 *     the room.  We score only the namesakes, which are room-gated.
 *   - its counter is reset once per character (4504DF), not once per name
 *     as the object contest's is (45433D), so a character whose Name AND
 *     one alias both equal the term counts its Prefix words twice.
 *   - Split() is called with the delimiter argument Missing, so the Prefix
 *     is cut on single spaces with no trimming; a double space yields an
 *     empty word, which scores nothing because 454CB0("") returns 0.  Ours
 *     does the same by walking to the next ' '.
 *
 * Below 4.0 there is no character contest at all -- run390 holds no Split
 * call anywhere, and its one Prefix test, lastword() (42DA40), is reached
 * only from takes, drops, referencedob, examines and co, all objects.  Its
 * characters() picks by index order instead; see NPC_PICK_FIRST/LAST.
 */
scr_int
lib_npc_400_prefix_score (scr_gameref_t game, scr_int npc,
                          const scr_char *input)
{
  const scr_char *prefix;
  scr_char *copy, *word, *next;
  scr_int score;

  prefix = prop_get_indexed_string (gs_get_bundle (game), "NPCs",
                                    npc, "Prefix");
  if (scr_strempty (prefix))
    return 0;

  score = 0;
  copy = (scr_char *) scr_malloc (strlen (prefix) + 1);
  strcpy (copy, prefix);
  for (word = copy; word; word = next)
    {
      next = strchr (word, ' ');
      if (next)
        *next++ = NUL;
      if (word[0] != NUL && lib_input_contains_word_400 (input, word))
        score++;
    }
  scr_free (copy);
  return score;
}

/* TRUE when the contest picks one of these namesakes outright. */
static scr_bool
lib_npc_400_prefix_settles (scr_gameref_t game,
                            const std::vector<scr_int> &namesakes,
                            const scr_char *input)
{
  scr_int index_, best, best_count;

  best = -1;
  best_count = 0;
  for (index_ = 0; index_ < (scr_int) namesakes.size (); index_++)
    {
      const scr_int score = lib_npc_400_prefix_score (game,
                                                      namesakes[index_],
                                                      input);

      if (score > best)
        {
          best = score;
          best_count = 1;
        }
      else if (score == best)
        best_count++;
    }

  return best > 0 && best_count == 1;
}

static scr_bool
lib_npc_400_find_namesakes_in (scr_gameref_t game, const scr_char *input,
                               std::string *term_out,
                               std::vector<scr_int> *namesakes_out,
                               scr_int *flagged_out,
                               std::string *list_term_out = NULL)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int room = gs_playerroom (game);
  scr_int npc, found;

  if (!input)
    return FALSE;

  found = FALSE;
  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      std::vector<scr_int> namesakes;
      scr_vartype_t vt_key[4];
      const scr_char *name, *term, *list_term;
      scr_int alias_count, alias, other;

      if (!npc_in_room (game, npc, room))
        continue;

      name = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
      term = (!scr_strempty (name) && lib_input_contains_word (input, name))
             ? name : NULL;
      list_term = term;
      alias_count = lib_alias_prepare (bundle, vt_key, "NPCs", npc);
      for (alias = 0; alias < alias_count; alias++)
        {
          const scr_char *alias_name;

          vt_key[3].integer = alias;
          alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (!scr_strempty (alias_name)
              && lib_input_contains_word (input, alias_name))
            term = alias_name;
        }
      if (!term)
        continue;
      /* npc_in_command's list term (var_98) is the Name when it is a whole
       * word of the line, the alias loop skipped (GoTo 45E682); only the
       * header term (48B815) takes the alias.  noximion T57 `kill venemous
       * buzzard with sword`: "Which buzzard.   venemous buzzard or  venemous
       * buzzard?" (runner_transcripts/noximion.txt). */
      if (!list_term)
        list_term = term;

      for (other = 0; other < gs_npc_count (game); other++)
        {
          if (npc_in_room (game, other, room)
              && lib_npc_answers_to (game, other, term))
            namesakes.push_back (other);
        }
      if (namesakes.size () < 2)
        continue;

      /* The Prefix contest can settle the term outright; see above. */
      if (lib_npc_400_prefix_settles (game, namesakes, input))
        continue;

      /*
       * The Runner's loop (48B547) has no break: the term and the list are
       * the FIRST hit's -- a later namesake writes neither, its term being
       * in the list already (45E7B5) -- but MemVar_4941EC, the flagged
       * index, is overwritten by every hit that is in the player's room
       * (45E8CA), so it ends up the LAST one's.  All of ours are in the
       * room, so: first hit for the text, last for the index.
       */
      if (!found)
        {
          if (term_out)
            *term_out = term;
          if (list_term_out)
            *list_term_out = list_term;
          if (namesakes_out)
            *namesakes_out = namesakes;
        }
      found = TRUE;
      if (flagged_out)
        *flagged_out = npc;
      if (!flagged_out)
        break;
    }

  return found;
}

scr_bool
lib_npc_400_find_namesakes (scr_gameref_t game, std::string *term_out,
                            std::vector<scr_int> *namesakes_out)
{
  return lib_npc_400_find_namesakes_in (game, run_get_dispatch_input (),
                                        term_out, namesakes_out, NULL);
}

/*
 * lib_npc_400_line_names_namesakes()
 *
 * TRUE if a 4.0 line names a term that two or more present characters answer
 * to -- the test generaltasks makes before its "Which" question.  Exposed for
 * run_player_input(), which needs it after the dispatch input is cleared.
 */
scr_bool
lib_npc_400_line_names_namesakes (scr_gameref_t game, const scr_char *line)
{
  return lib_is_version_400 (game)
         && lib_npc_400_find_namesakes_in (game, line, NULL, NULL, NULL);
}

static scr_bool
lib_npc_400_raise_for_line_in (scr_gameref_t game, const scr_char *input)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  std::vector<scr_int> namesakes;
  std::string term_string, npc_term, lower;
  const scr_char *term;
  scr_int index_, flagged;

  flagged = -1;
  if (!lib_npc_400_find_namesakes_in (game, input, &term_string, &namesakes,
                                      &flagged, &npc_term))
    return FALSE;
  /* The list is built by the character scan (45E7D3) and printed whole, so
   * it keeps the character's term even when the object half prints. */

  /*
   * The two halves of the question read ONE untyped index, and the object
   * half (48B6B1) gets first refusal: `MemVar_4941EC < MemVar_494050 And
   * co(3, MemVar_4941EC)`, i.e. the flagged CHARACTER index doubles as an
   * object index, and if the line names that object the term printed over
   * the character list is the object's -- its Short, replaced by each alias
   * of it that is a whole word of the line, the last winning.
   *
   * It is a plain collision, not a term choice.  p4BATT (run400x
   * runner_probes/batt.run400.txt and runner_probes/batt.run400.b.txt,
   * 2026-09-20) has Dave 0, Ann 1, Bob 2, Cora 3 with Ann and Bob both "a
   * guard", and objects sword 0, club 1, stone 2; the flagged index is
   * Bob's 2, so `attack guard with stone`, `give stone to guard`, `x guard
   * stone` and `x stone guard` all print "Which stone.  A guard or a
   * guard?" while `x guard sword`, `attack guard with club`, `give club to
   * guard` and `x guard dave` print "Which guard.".
   */
  if (flagged >= 0 && flagged < gs_object_count (game)
      && gs_object_seen (game, flagged)
      && obj_indirectly_in_room (game, flagged, gs_playerroom (game))
      && lib_co_400_name_word (game, flagged, input))
    {
      const scr_char *name;
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      name = prop_get_indexed_string (bundle, "Objects", flagged, "Short");
      term_string = name ? name : "";
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", flagged);
      for (alias = 0; alias < alias_count; alias++)
        {
          vt_key[3].integer = alias;
          name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (!scr_strempty (name) && lib_input_contains_word_400 (input, name))
            term_string = name;
        }
    }
  term = term_string.c_str ();

  /* One pass of the original loop; the braces keep its indentation. */
    {
      game->is_admin = TRUE;
      if (lib_co_400_was_pending)
        {
          pf_buffer_string (filter, "That is still ambiguous!\n");
          return TRUE;
        }

      for (index_ = 0; npc_term[index_] != NUL; index_++)
        lower += (scr_char) tolower ((unsigned char) npc_term[index_]);

      lib_which_head (game, term, NULL);
      pf_new_sentence (filter);
      for (index_ = 0; index_ < (scr_int) namesakes.size (); index_++)
        {
          const scr_char *prefix;

          if (index_ > 0)
            pf_buffer_string (filter,
                              index_ == (scr_int) namesakes.size () - 1
                              ? " or " : ", ");
          prefix = prop_get_indexed_string (bundle, "NPCs", namesakes[index_],
                                            "Prefix");
          /*
           * Prefix & " " & term with no empty-Prefix test (45E811-45E82A):
           * an empty Prefix leaves its space, and 446BB4's capital then
           * lands on it -- "Which woman.   woman or  woman?".  The loader
           * defaults only OBJECT prefixes to "a"; asteroid_after's and
           * Vendetta's ALRs are written against the spaced form.
           */
          /* Scarier drops the empty Prefix's space unless the game's ALRs
           * were written against the spaced form (lib_which_runner_form). */
          if (!scr_strempty (prefix) || lib_which_runner_form (game))
            {
              pf_buffer_string (filter, prefix);
              pf_buffer_character (filter, ' ');
            }
          pf_buffer_string (filter, lower.c_str ());
        }
      pf_buffer_string (filter, "?\n");

      lib_co_400_pending = TRUE;
      lib_co_400_term = term;
      lib_co_400_command = input ? input : "";
      lib_co_400_candidates.clear ();
      return TRUE;
    }
}

scr_bool
lib_npc_400_raise_for_line (scr_gameref_t game)
{
  return lib_npc_400_raise_for_line_in (game, run_get_dispatch_input ());
}

/* The same, for run_player_input()'s tail, where the dispatch input is gone. */
scr_bool
lib_npc_400_raise_for_line_string (scr_gameref_t game, const scr_char *line)
{
  return lib_npc_400_raise_for_line_in (game, line);
}

/*
 * The answer slot's line.  generaltasks does not score the answer against
 * the prompt's candidates at all: it REBUILDS the stored command with the
 * answer words spliced in front of the term and re-runs the whole thing
 * (48B097-48B15B), exactly as 3.90's prompt does -- `chop tree` / `red`
 * re-runs `chop red tree`, and the ordinary noun score settles it there.
 * The term is appended when the answer does not already hold it as a word
 * (the c() test at 48B0D8), and the part of the command past the term goes
 * on the end.  With the command not holding the term at all the answer is
 * simply put in front of it (48B178).
 *
 * This is what makes `chop tree and chop keys` a second FULL prompt, "Which
 * keys.  The red tree, the blue tree, the mustang key or the truck key?":
 * the rebuilt `chop chop keys tree` names the keys as well as the trees, and
 * a scorer confined to the prompt's own candidates can never see them
 * (runner_probes/co.run400.co11.txt turn 15, 2026-09-20).  "That is still
 * ambiguous!" is then not this slot's answer but the re-run's own raise,
 * meeting the term the last prompt left behind.
 */
std::string
lib_co_400_object_answer_line (const scr_char *line)
{
  const std::string &command = lib_co_400_command;
  const std::string &term = lib_co_400_term;
  const std::string answer (line ? line : "");
  std::string::size_type at;
  std::string built;

  if (term.empty ())
    return command + " " + answer;

  for (at = 0; at + term.size () <= command.size (); at++)
    {
      if (scr_strncasecmp (command.c_str () + at, term.c_str (),
                           term.size ()) == 0)
        break;
    }
  if (at + term.size () > command.size ())
    return command + " " + answer;      /* 48B178 */

  built = command.substr (0, at) + answer;
  if (!lib_co_contains (built.c_str (), term.c_str ()))
    built += " " + term;
  built += " " + command.substr (at + term.size ());
  return built;
}

#ifdef SCARIER_DUMP_TOOLS
/*
 * SCR_TRACE_CO: report where the Runner's test disagrees with ours at each
 * lib_disambiguate_object_common() call.  `ours=` is our own post-filter
 * reference count, so ours=1 is a real divergence and ours>1 means only the
 * prompt's wording differed before lib_co_ambiguity_prompt() existed.  A
 * measurement harness only: it changes nothing.
 */
static void
lib_trace_runner_co (scr_gameref_t game, const scr_char *verb, scr_int count)
{
  static const scr_bool trace_co = getenv ("SCR_TRACE_CO") != NULL;
  const scr_char *command, *ambig_term;
  scr_int ambig_present;

  if (!trace_co)
    return;
  command = run_get_dispatch_input ();
  if (lib_runner_co_scan (game, command, &ambig_term, NULL, &ambig_present))
    fprintf (stderr, "CO-AMBIG verb=[%s] input=[%s] term=[%s]"
             " present=%ld ours=%ld\n",
             verb ? verb : "", command, ambig_term, ambig_present, count);
}
#endif

/*
 * lib_disambiguate_object_common()
 * lib_disambiguate_object()
 *
 * Filter, then search the set of object matches.  If only one matched, note
 * and return it.  If multiple matched, print a disambiguation message and
 * the list, and return -1 with *is_ambiguous TRUE.  If none matched, return
 * -1 with *is_ambiguous FALSE if requested, otherwise print a message then
 * return -1.
 *
 * If normal disambiguation returns more than one object, the resolver
 * function, if supplied, is used to see if the multiple objects can be
 * resolved into just one object.  The resolver function can normally be the
 * same as the function used to filter objects for multiple references.
 */
/* The container mode 4 admits from; see lib_resolve_admit_parent(). */
scr_int lib_resolve_parent_400 = -1;

/*
 * lib_co_note_line_top()
 *
 * TRUE once a 3.7 library handler has settled this line's object; see
 * lib_disambiguate_object_common() and lib_co_ambiguity_prompt().  Cleared
 * at the top of every line by run_all_commands().
 */
void
lib_co_note_line_top (scr_gameref_t game)
{
  const scr_char *line = run_get_dispatch_input ();
  scr_int object;

  lib_co_prompt_370_blocked = FALSE;
  lib_co_top_mode = line ? lib_co_verb_mode (line) : 0;
  lib_co_top_fits.assign (gs_object_count (game), FALSE);
  for (object = 0; object < gs_object_count (game); object++)
    lib_co_top_fits[object] = lib_co_mode_fits (game, object, lib_co_top_mode);
}

/*
 * lib_name_offset_pre400()
 * lib_first_named_pre400()
 *
 * Where the typed line first names an object -- the lowest offset at which
 * its Short or any of its Aliases occurs -- and the referenced object with
 * the lowest such offset.
 *
 * Pre-4.0 a line naming several objects that no handler acts on is answered
 * by the generic can't-do tail in therest(), and that names the FIRST of
 * them by WORD POSITION, not by object index.  Measured on p*OPENW.taf (one
 * lit room holding a gem, a rock, a static slab and a closed chest, the gem
 * held), 2026-09-20:
 *
 *     command             run370               run380/run390
 *     open rock gem       can't open the rock. can't open the gem!
 *     open gem rock       can't open the gem.  can't open the gem!
 *     open slab rock      can't open the slab. can't open the rock!
 *     close rock gem      can't close the rock.  (all three versions)
 *     close gem rock      can't close the gem.   (all three versions)
 *
 * (runner_probes/openw.run370.ow.rtf, runner_probes/openw.run380.ow.rtf,
 * runner_probes/openw.run390.ow.txt, runner_probes/openw.run370.ox.rtf,
 * runner_probes/openw.run380.ox.rtf, runner_probes/openw.run390.ox.txt.)
 * 3.80 gave `open` a refusal of its own inside openclose(), above therest,
 * and that one names the lowest object INDEX instead and ends in a bang;
 * `close` got none until 4.0, so it keeps falling through to therest at
 * every pre-4.0 version.
 */
static scr_int
lib_name_offset_pre400 (scr_gameref_t game, scr_int object,
                        const scr_char *line)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  const scr_char *name, *found;
  scr_int aliases, alias, best = -1;

  name = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!scr_strempty (name) && (found = strstr (line, name)) != NULL)
    best = found - line;

  aliases = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < aliases; alias++)
    {
      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (scr_strempty (name) || !(found = strstr (line, name)))
        continue;
      if (best < 0 || found - line < best)
        best = found - line;
    }
  return best;
}

static scr_int
lib_first_named_pre400 (scr_gameref_t game, scr_int fallback)
{
  const scr_char *line = run_get_dispatch_input ();
  scr_int index_, best = -1, offset, best_offset = 0;

  if (!line)
    return fallback;

  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (!game->object_references[index_])
        continue;
      offset = lib_name_offset_pre400 (game, index_, line);
      if (offset < 0)
        continue;
      if (best < 0 || offset < best_offset)
        {
          best = index_;
          best_offset = offset;
        }
    }
  return best >= 0 ? best : fallback;
}

scr_int
lib_disambiguate_object_common (scr_gameref_t game, const scr_char *verb,
                               scr_bool (*resolver)
                                   (scr_gameref_t, scr_int, scr_int),
                               scr_int resolver_arg,
                               scr_bool *is_ambiguous)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_bool requires_seen = lib_matcher_requires_seen (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int taf_version = prop_get_taf_version (bundle);
  scr_int count, index_, object, listed;

  /*
   * Filter out all referenced objects not actually visible or seen.  Count
   * the number of objects remaining as referenced by the last command, and
   * note the last referenced object, for where count is 1.  Version 3.8
   * games skip the seen test -- see lib_matcher_requires_seen().
   */
  count = 0;
  object = -1;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (game->object_references[index_]
          && (!requires_seen || gs_object_seen (game, index_))
          && obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        {
          count++;
          object = index_;
        }
      else
        game->object_references[index_] = FALSE;
    }

#ifdef SCARIER_DUMP_TOOLS
  lib_trace_runner_co (game, verb, count);
#endif

  /*
   * 3.7's handlers never call co(): only therest() (run370 43CC86) and
   * insides() do, so the end-of-turn "Which <term>." is raised only by a
   * line the catch-all or put answered.  Note that a handler of this line
   * has its object(s); lib_co_ambiguity_prompt() then stays quiet.
   */
  if (taf_version < TAF_VERSION_380 && count > 0
      && strncmp (verb, "put", 3) != 0 && strcmp (verb, "move") != 0)
    lib_co_prompt_370_blocked = TRUE;

  /*
   * 3.8 and 3.9 put each candidate through co() -- run380 co(obj) from
   * drops, wears, removes, openclose, examines, takes and the rest, run390
   * co(obj, mode) (43B6BC) -- BEFORE any handler filter: the term's present
   * namesakes are counted, and with two or more the object passes only if
   * the last word of its Prefix is typed (lib_co_pre400).  So `wear hat`
   * with the red hat in hand and the blue one on the floor wears NOTHING at
   * 3.8/3.9 -- held-ness never narrowed the count -- and the turn is the
   * "Which hat.  The red hat or the blue hat?" the generaltasks scan raises;
   * likewise remove, open, close, take and drop (3.8).  3.9's takes() uses
   * mode 1 (loose in the room) and drops() mode 2 (isheld), which do
   * narrow.  Measured on p38TASK/p39TASK
   * (runner_probes/task.run380.pname.rtf,
   * runner_probes/task.run380.pname2.rtf,
   * runner_probes/task.run390.pname.txt,
   * runner_probes/task.run390.pname2.txt, 2026-09-19).  Scarier narrows take
   * and drop at 3.8 too, and wear and remove at both (deliberate deviation),
   * so that `wear hat` puts on the red hat.
   *
   * 3.9's examine narrows the same way -- referencedob() (42DF43) calls the
   * same co(obj, 0), so the last Prefix word settles a crowd for it too:
   * `x tree red` with trees Prefixed "a red" and "a blue" is "A red tree."
   * (p39PFX, run390x runner_probes/pfx.run390.txt turn 17, 2026-09-20).
   * What it does NOT do is answer when the word narrows nothing: there co()
   * is false for every candidate, referencedob() counts none, and
   * examines() keeps its own "Nothing special." below, under the
   * end-of-turn "Which tree."
   */
  const scr_bool examine_390 = taf_version >= TAF_VERSION_390
                               && strcmp (verb, "examine") == 0;
  if (count > 1 && taf_version >= TAF_VERSION_380
      && taf_version < TAF_VERSION_400 && run_get_dispatch_input ())
    {
      const scr_char *line = run_get_dispatch_input ();
      scr_int mode = 0, kept = 0;

      /* The wear and remove modes, and all four at 3.8, are Scarier's
         (deliberate deviation); see lib_co_pre400(). */
      if (strcmp (verb, "drop") == 0)
        mode = 2;
      else if (strcmp (verb, "take") == 0)
        mode = 1;
      else if (strcmp (verb, "wear") == 0)
        mode = 3;
      else if (strcmp (verb, "remove") == 0)
        mode = 4;
      for (index_ = 0; index_ < gs_object_count (game); index_++)
        {
          if (game->object_references[index_]
              && lib_co_pre400 (game, line, index_, mode))
            kept++;
        }
      if (kept == 0 && !examine_390
          && lib_runner_co_scan (game, line, NULL, NULL, NULL))
        {
          /*
           * Every candidate refused, so the handler's loop acted on
           * nothing and it says so: takes() "Take what?" (3.8 and 3.9),
           * 3.8 examines() "Nothing special.".  The end-of-turn prompt
           * wipes that unless a task ran this turn -- an event's does
           * (p38EVQ2/p39EVQ2, runner_probes/evq2.run380.rtf /
           * runner_probes/evq2.run390.txt, 2026-09-20).
           */
          if (strcmp (verb, "take") == 0)
            lib_what (game, "Take");
          else if (strcmp (verb, "examine") == 0)
            pf_buffer_string (gs_get_filter (game), "Nothing special.\n");
          if (is_ambiguous)
            *is_ambiguous = TRUE;
          return -1;
        }
      if (kept > 0 && kept < count)
        {
          count = 0;
          object = -1;
          for (index_ = 0; index_ < gs_object_count (game); index_++)
            {
              if (game->object_references[index_]
                  && lib_co_pre400 (game, line, index_, mode))
                {
                  count++;
                  object = index_;
                }
              else
                game->object_references[index_] = FALSE;
            }
        }
    }

  /*
   * 4.0 runs the Prefix contest over a crowded reference instead: co()'s
   * crowded arm (run400 454454, called from Proc_21_39_46486C) scores every
   * candidate the way 463640 does -- the Short as a whole word, any alias,
   * and one more for each word of the Prefix the line holds -- and keeps
   * the strict maximum.  A single winner is the reference, and when several
   * tie at the top it is THOSE the question offers, not the whole reference
   * set.
   *
   * Measured 2026-09-20 on p4PFX2.taf, three trees Prefixed "a big red",
   * "a red" and "the red" beside a "a" rock (run400x
   * runner_probes/pfx2.run400.txt):
   *
   *     x tree          ->  Which tree.  The big red tree, the red tree or
   *                         the red tree?                       (1-1-1)
   *     x red tree      ->  the same three                      (2-2-2)
   *     x a red tree    ->  Which tree.  The big red tree or the red tree?
   *                                                             (3-3-2)
   *     x the red tree  ->  The red tree.                       (2-2-3)
   *     x big tree      ->  A big red tree.                     (2-1-1)
   *     x a tree        ->  Which tree.  The big red tree or the red tree?
   *                                                             (2-2-1)
   *
   * The older p4CO cells still hold because their candidates all tie:
   * `x tree rock` offers "the red tree, the blue tree or the rock" because
   * each scores its own Short and no Prefix word was typed.
   *
   * Below 4.0 the same crowd is settled by the last Prefix word alone; see
   * lib_co_pre400() above.
   */
  if (count > 1 && taf_version >= TAF_VERSION_400 && run_get_dispatch_input ())
    {
      const scr_char *line = run_get_dispatch_input ();
      scr_int best, kept;

      best = -1;
      kept = 0;
      for (index_ = 0; index_ < gs_object_count (game); index_++)
        {
          scr_int score;

          if (!game->object_references[index_])
            continue;
          score = lib_verb_object_name_score (game, index_, line);
          if (score > best)
            {
              best = score;
              kept = 1;
            }
          else if (score == best)
            kept++;
        }

      if (kept > 0 && kept < count)
        {
          count = 0;
          object = -1;
          for (index_ = 0; index_ < gs_object_count (game); index_++)
            {
              if (game->object_references[index_]
                  && lib_verb_object_name_score (game, index_, line) == best)
                {
                  count++;
                  object = index_;
                }
              else
                game->object_references[index_] = FALSE;
            }
        }
    }

  /*
   * If this reference is ambiguous and a resolver was supplied, try to
   * resolve it unambiguously by calling the resolver filter on the remaining
   * set references.
   */
  if (resolver && count > 1)
    {
      scr_int retry_count;

      /*
       * Search for objects accepted by the resolver filter, but don't filter
       * references just yet.  Again, note the last referenced.
       */
      retry_count = 0;
      object = -1;
      for (index_ = 0; index_ < gs_object_count (game); index_++)
        {
          if (game->object_references[index_]
              && resolver (game, index_, resolver_arg))
            {
              retry_count++;
              object = index_;
            }
        }

      /* See if we narrowed the field without eliminating every object. */
      if (retry_count > 0 && retry_count < count)
        {
          /*
           * If we got down to a single object, the ambiguity is resolved.
           * In this case, set count to 1 so that 'object' is returned.
           */
          if (retry_count == 1)
            count = retry_count;
          else
            {
              /*
               * We got down to fewer objects; reduce references so that the
               * disambiguation message is clearer.  Note that here we still
               * leave with count greater than 1.
               */
              count = 0;
              for (index_ = 0; index_ < gs_object_count (game); index_++)
                {
                  if (game->object_references[index_]
                      && resolver (game, index_, resolver_arg))
                    count++;
                  else
                    game->object_references[index_] = FALSE;
                }
            }
        }
    }

  /*
   * 3.7 has no co() in its handlers, and each settles a namesake its own
   * way (p37TASK, runner_probes/task.run370.pname.rtf /
   * runner_probes/task.run370.pname2.rtf, 2026-09-19; two hats, red held
   * and blue loose, two open boxes loose):
   *
   *   drops (430DDC) / takes (436280): every held (loose) namesake lacking
   *     the last word of its Prefix in the line is marked and skipped,
   *     unless it is the only one there is -- so two in hand is "Drop
   *     what?", two on the floor "Take what?"; the "Which ... drop/take"
   *     strings at 430866 are dead code.
   *   wears (42C9FC) / removes: the loop puts on EVERY held wearable
   *     namesake (takes off every worn one), each overwriting the message,
   *     and "not holding"/"can't wear" goes only into an empty message: red
   *     held and blue loose is "You put on a red hat.", red worn and blue
   *     held "You put on a blue hat.".
   *   openclose (426770): every namesake changes state, the last speaks:
   *     `close box` closes both, "You close the blue box.".
   *   examines (4359D5): "Which hat would you like to examine.  The red hat
   *     or the blue hat?" over the present namesakes, no state change.
   *
   * The wear/remove loops are folded here as "act on all but the last
   * directly, hand the last to the handler", so the handler's own wording
   * answers for the last one exactly as the Runner's overwriting loop
   * leaves it.  openclose's loop is the same fold, but 3.80 and 3.90 run it
   * too, so it lives in the pre-4.0 block below.
   */
  /*
   * examines() asks its own question, and it is not openclose's or takes':
   * "Which <Short of the LAST match by index> would you like to examine.
   * <the matches, in index order>?"  3.80 keeps it -- `read rock gem`, `read
   * gem rock` and `examine rock gem` are all "Which rock would you like to
   * examine.  The gem or the rock?" under run370 AND run380, and `read rock
   * with slab` is "Which slab would you like to examine.  The rock or the
   * slab?" (p*OPENW, runner_probes/openw.run370.ow.rtf,
   * runner_probes/openw.run380.ow.rtf, runner_probes/openw.run370.ox.rtf,
   * runner_probes/openw.run380.ox.rtf, 2026-09-20) -- so the word order on
   * the line never picks here, only the index does.  3.90 replaced the
   * question with referencedob()'s last-word pass; see
   * lib_examine_crowded_390().
   *
   * Pre-4.0 `read` is one of examines()' entry words, so it asks the same
   * question about the same verb; see lib_cmd_read_other().
   */
  if (count > 1 && taf_version < TAF_VERSION_390
      && (strcmp (verb, "examine") == 0 || strcmp (verb, "read") == 0))
    {
      lib_which_head (game,
                      prop_get_indexed_string (gs_get_bundle (game),
                                               "Objects", object, "Short"),
                      " would you like to examine");
      pf_new_sentence (filter);
      listed = 0;
      for (index_ = 0; index_ < gs_object_count (game); index_++)
        {
          if (!game->object_references[index_])
            continue;
          if (listed > 0)
            pf_buffer_string (filter, listed == count - 1 ? " or " : ", ");
          lib_print_object_np (game, index_);
          listed++;
        }
      pf_buffer_string (filter, "?\n");
      if (is_ambiguous)
        *is_ambiguous = TRUE;
      return -1;
    }

  /*
   * run370's wears() and removes() rewrite the line's Alias with
   * replacealias(0) -- a literal 0 (LitI2_Byte at 42C48A / 42958A), not the
   * argument-less every-object form takes() uses -- so only object #0's
   * Alias ever becomes a Short, and every other object must be named by
   * its Short: `don ball` holding a red and a blue ball, both aliased
   * "ball", is "Wear what?" (p37SLOT2, harness/make_37_slotprobe.py,
   * runner_probes/slot2.run370.b.rtf, 2026-09-21).  Deliberate deviation:
   * Scarier lets every Alias name the object, as 3.8 does.
   */
  if (count > 1 && taf_version < TAF_VERSION_380)
    {
      const scr_bool is_wear = strcmp (verb, "wear") == 0;
      const scr_bool is_remove = strcmp (verb, "remove") == 0;

      if (is_wear || is_remove)
        {
          scr_int first = -1, last = -1;

          for (index_ = 0; index_ < gs_object_count (game); index_++)
            {
              if (!game->object_references[index_])
                continue;
              if (first == -1)
                first = index_;
              if (!(resolver && resolver (game, index_, resolver_arg)))
                continue;
              if (last != -1)
                {
                  if (is_wear)
                    gs_object_player_wear (game, last);
                  else
                    gs_object_player_get (game, last);
                }
              last = index_;
            }
          object = last != -1 ? last : first;
          for (index_ = 0; index_ < gs_object_count (game); index_++)
            game->object_references[index_] = (index_ == object);
          count = 1;
        }
    }

  /*
   * takes() (run370 436280, run380, run390) and drops() (430DDC) walk a
   * crowded line the same way openclose() does: every survivor moves, and
   * the last by index overwrites the message.  Below 3.90 that is the whole
   * rule -- `take red pin` with pins Prefixed "old red" and "new red" is
   * "You pick up new red pin." at 3.70 AND 3.80, and `i` afterwards lists
   * both; `drop red pin` then drops both (p*TAKEP,
   * runner_probes/takep.run370.rtf / runner_probes/takep.run380.rtf,
   * 2026-09-20).  3.90 keeps only the FIRST by index: the same line is "You
   * pick up old red pin." and `i` lists it alone.
   *
   * What survives is the Prefix contest, and which crowd runs it is the
   * version split:
   *
   *   3.80/3.90 have co() under every handler, so the crowd is the TERM's
   *     present namesakes and the block above has already narrowed it.
   *   3.70 has no co() at all, so a crowd is objects sharing a SHORT the
   *     line names (lib_namesake_crowded_pre380), and the filter runs here:
   *     each crowded namesake keeps its reference only if the last word of
   *     its Prefix is typed (lib_co_names_prefix).  With none left the turn
   *     is takes'/drops' own "Take what?" / "Drop what?" -- the "Which ...
   *     would you like to take" strings at 430866 are dead code -- and the
   *     line is answered, so no end-of-turn co() question follows it.
   *
   * Note that a one-word Prefix filters NOTHING below 3.90, because the
   * pre-3.9 co() drops the Prefix's first word before taking its last:
   * `take big gem` over gems Prefixed "big" and "small" is "Take what?" at
   * 3.70 and the co() question at 3.80, exactly as bare `take gem` is,
   * while `take red gem` over "a very red" / "a very blue" answers (p*TAKEP
   * / p*TAKEQ, runner_probes/takep.run*.* and runner_probes/takeq.run*.*).
   */
  if (count > 0 && taf_version < TAF_VERSION_400
      && (strcmp (verb, "take") == 0 || strcmp (verb, "drop") == 0)
      && (count > 1 || taf_version < TAF_VERSION_380))
    {
      const scr_bool is_take = strcmp (verb, "take") == 0;
      const scr_char *line = run_get_dispatch_input ();
      scr_int first = -1, last = -1;

      if (taf_version < TAF_VERSION_380 && line)
        {
          scr_int eligible = 0, kept = 0;

          /*
           * The loop visits every object, not the parser's references: one
           * whose Alias is on the line has it rewritten to its Short first
           * (replacealias, run370 4363F7) and then answers c(Short) like
           * any other.  So an Alias counts as fully as a Short, and `take
           * red ball` over a red and a blue ball both aliased "ball" picks
           * up BOTH, "You pick up the blue ball." -- `drop red ball` drops
           * both the same way (p37SLOT2, harness/make_37_slotprobe.py,
           * run370x runner_probes/slot2.run370.c.rtf, 2026-09-21).  The
           * loop's action is under `If Not c("from")` (43648C), so a
           * take-from line keeps the parser's references.
           */
          for (index_ = 0;
               index_ < gs_object_count (game) && !lib_co_contains (line, "from");
               index_++)
            {
              if (game->object_references[index_]
                  || obj_is_static (game, index_)
                  || !lib_co_candidate (game, index_, gs_playerroom (game))
                  || !lib_names_object_370 (game, line, index_)
                  || lib_names_only_shadowed_370 (game, line, index_))
                continue;
              game->object_references[index_] = TRUE;
              count++;
            }

          /*
           * takes() walks what is loose and drops() what is held, so a
           * namesake in the wrong place is not in the crowd at all: two
           * orbs on the floor are "Drop what?" to nobody, they are drops'
           * ordinary "You don't have a orb!" (p37TAKEP,
           * runner_probes/takep.run370.rtf turn 14).  With nothing
           * eligible the loop never ran, nothing is ambiguous, and the
           * handler's own refusal answers about the first name on the
           * line -- drops' refusal only fills an empty message.  takes'
           * "You've already got" (436561) overwrites, so with every
           * namesake in hand the LAST by index answers: `take ball`
           * holding both balls is "You've already got a blue ball!"
           * (runner_probes/slot2.run370.c.rtf).
           */
          for (index_ = 0; index_ < gs_object_count (game); index_++)
            {
              if (game->object_references[index_]
                  && lib_object_held_pre380 (game, index_) != is_take)
                eligible++;
            }
          if (eligible == 0)
            {
              object = -1;
              if (is_take)
                for (index_ = 0; index_ < gs_object_count (game); index_++)
                  if (game->object_references[index_]
                      && lib_object_held_pre380 (game, index_))
                    object = index_;
              if (object == -1)
                object = lib_first_named_pre400 (game, -1);
              if (object == -1)
                for (index_ = 0; index_ < gs_object_count (game); index_++)
                  if (game->object_references[index_])
                    {
                      object = index_;
                      break;
                    }
              for (index_ = 0; index_ < gs_object_count (game); index_++)
                game->object_references[index_] = (index_ == object);
              count = 1;
              goto pre400_take_done;
            }

          for (index_ = 0; index_ < gs_object_count (game); index_++)
            {
              if (!game->object_references[index_])
                continue;
              if (lib_object_held_pre380 (game, index_) == is_take
                  || (lib_namesake_crowded_pre380 (game, line, index_,
                                                   !is_take)
                      && !lib_co_names_prefix (game, line, index_)))
                game->object_references[index_] = FALSE;
              else
                kept++;
            }
          if (kept == 0)
            {
              lib_what (game, is_take ? "Take" : "Drop");
              lib_co_prompt_370_blocked = TRUE;
              if (is_ambiguous)
                *is_ambiguous = TRUE;
              return -1;
            }
          count = kept;
        }

      for (index_ = 0; index_ < gs_object_count (game); index_++)
        {
          if (!game->object_references[index_])
            continue;
          if (first == -1)
            first = index_;
          if (taf_version >= TAF_VERSION_390)
            continue;
          if (last != -1)
            {
              if (is_take)
                gs_object_player_get (game, last);
              else
                gs_object_to_room (game, last, gs_playerroom (game));
            }
          last = index_;
        }

      object = taf_version >= TAF_VERSION_390 ? first : last;
      for (index_ = 0; index_ < gs_object_count (game); index_++)
        game->object_references[index_] = (index_ == object);
      count = 1;
    }
pre400_take_done:

  /*
   * No pre-4.0 Runner asks about a crowded open/close line either.
   * openclose() walks the referenced objects and acts on each OPENABLE one,
   * the last by index overwriting the message -- run370, run380 and run390
   * all answer `open rock gem chest` with "You open the chest." (p*OPENW,
   * runner_probes/openw.run370.ox.rtf, runner_probes/openw.run380.ox.rtf,
   * runner_probes/openw.run390.ox.txt, 2026-09-20), and with two closed
   * containers on the line all three answer `open box gem chest` with "You
   * open the chest." and leave the box open as well (p*OPENT, 2026-09-20).
   * So 3.7's every-namesake loop is not 3.7's alone: 3.80 and 3.90 run the
   * same one.
   *
   * The test is "openable at all", not "in the state the verb wants": with
   * the box already open, `open box gem`, `open gem box`, `open gem chest`
   * and `open chest box` are all "The <box|chest> is already open!" at
   * 3.70, 3.80 and 3.90 -- the openable object still wins the line, and the
   * handler's own already-open wording answers for it.
   *
   * With NONE openable the versions part company.  3.80 gave `open` a
   * refusal of its own inside openclose(), which names the LOWEST object
   * index on the line and ends in a bang; `close` never got one, and 3.70
   * has neither, so those fall through to therest()'s can-do tail and its
   * first name by WORD POSITION (lib_first_named_pre400()).
   */
  if (count > 1 && taf_version < TAF_VERSION_400
      && (strcmp (verb, "open") == 0 || strcmp (verb, "close") == 0))
    {
      const scr_bool is_open = strcmp (verb, "open") == 0;
      const scr_int wanted = is_open ? OBJ_CLOSED : OBJ_OPEN;
      scr_int lowest = -1, last = -1;

      for (index_ = 0; index_ < gs_object_count (game); index_++)
        {
          if (!game->object_references[index_])
            continue;
          if (lowest == -1)
            lowest = index_;
          if (gs_object_openness (game, index_) == OBJ_WONTCLOSE)
            continue;
          if (last != -1 && gs_object_openness (game, last) == wanted)
            gs_set_object_openness (game, last,
                                    is_open ? OBJ_OPEN : OBJ_CLOSED);
          last = index_;
        }

      if (last != -1)
        object = last;
      else if (is_open && taf_version >= TAF_VERSION_380)
        object = lowest;
      else
        object = lib_first_named_pre400 (game, lowest);
      for (index_ = 0; index_ < gs_object_count (game); index_++)
        game->object_references[index_] = (index_ == object);
      count = 1;
    }

  /*
   * run400's examine narrows by names the LINE contains, not by the longest
   * one: with no present object's Short in the line, every present object
   * whose alias the line holds is a candidate, and a name two of them share
   * raises the question even though a longer alias picks one out.  hub T70
   * `x lower right cupboard` (aliases "lower right cupboard" and "right
   * cupboard" on one, "right cupboard" on the other) answers "Which right
   * cupboard.  The right lower cupboard or the right upper cupboard?"
   * (runner_probes/hub.run400.adj.txt, runner_transcripts/hub.txt).
   * Scarier lets the longer alias pick (deliberate deviation), so that line
   * examines the lower cupboard.
   */
  if (count == 1 && lib_is_version_400 (game)
      && strcmp (verb, "examine") == 0
      && lib_co_400_raise_for_contained_aliases (game))
    {
      if (is_ambiguous)
        *is_ambiguous = TRUE;
      return -1;
    }

  /* If the reference is unambiguous, set in variables and return it. */
  if (count == 1)
    {
      /* Set this object as referenced. */
      var_set_ref_object (vars, object);

      /* Return, setting no ambiguity. */
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      return object;
    }

  /* If nothing referenced, return no object. */
  if (count == 0)
    {
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      else
        {
          pf_buffer_string (filter,
                            "Please be more clear, what do you want to ");
          pf_buffer_string (filter, verb);
          pf_buffer_string (filter, "?\n");
        }
      return -1;
    }

  /*
   * A 4.0 take does not ask about every crowd: takes() names its object with
   * 463640 mode 1, whose pending object decides between the question and the
   * flat "It is not clear which <term> you are referring to."  See
   * lib_name_object_resolve_400(), where p4TAKER's ten cells are.
   */
  if (count > 1 && lib_is_version_400 (game)
      && strcmp (verb, "take") == 0 && run_get_dispatch_input ())
    {
      const scr_char *line = run_get_dispatch_input ();
      std::vector<scr_int> marked;
      scr_int pending, last_tied, mark_count;

      object = lib_name_object_resolve_400 (game, line, 1, &pending,
                                            &last_tied, &marked,
                                            &mark_count);
      if (object >= 0)
        {
          for (index_ = 0; index_ < gs_object_count (game); index_++)
            game->object_references[index_] = (index_ == object);
          var_set_ref_object (vars, object);
          if (is_ambiguous)
            *is_ambiguous = FALSE;
          return object;
        }
      if (object == -1 && pending < 0)
        {
          pf_buffer_string (filter, "It is not clear which ");
          pf_buffer_string (filter,
                            lib_drop_named_term_400 (game, last_tied,
                                                     line, FALSE));
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 " you are referring to.\n",
                                                 " I am referring to.\n",
                                                 " %player% is referring to.\n"));
          if (is_ambiguous)
            *is_ambiguous = TRUE;
          return -1;
        }
      /* 4733BD: the term is the LAST tied object's raw Short. */
      if (object == -1 && (scr_int) marked.size () == mark_count)
        {
          lib_co_400_raise_named (game,
                                  prop_get_indexed_string (bundle, "Objects",
                                                           last_tied, "Short"),
                                  marked);
          if (is_ambiguous)
            *is_ambiguous = TRUE;
          return -1;
        }
    }

  /*
   * 4.0 asks the Runner's own question instead.  "Please be more clear, what
   * do you want to <verb>?" is a SCARE invention -- the string is in none of
   * the four Runner binaries -- and what run400 really prints where two
   * present objects answer to the typed noun is the same "Which <term>.
   * <list>?" the 3.7/3.8 scan above raises (see lib_co_400_raise()).  Away
   * from examine no 4.0 command reaches the listing at all: a crowd that
   * asks nothing hands the handler no object, and the command goes on to
   * its own %text% row.
   */
  if (lib_is_version_400 (game))
    {
      std::vector<scr_int> crowd;

      /*
       * A line with " with " in it asks about one half; see
       * lib_with_split_crowd_400().  `close box with stone` lists the two
       * stones and not the box, where an examine whose own half ties keeps
       * the whole line's list below.
       */
      const scr_bool examine = strcmp (verb, "examine") == 0;
      scr_bool raised;

      scr_int with_pending, with_head;

      if (lib_with_split_crowd_400 (game, examine, &crowd, &with_pending,
                                    &with_head))
        {
          /* An examine whose head names one object describes it; see
             lib_with_split_crowd_400(). */
          if (examine && with_head >= 0)
            {
              var_set_ref_object (vars, with_head);
              if (is_ambiguous)
                *is_ambiguous = FALSE;
              return with_head;
            }
          raised = with_pending >= 0
                   ? lib_co_400_raise_for_with_tail (game, with_pending, crowd)
                   : lib_co_400_raise_for_short_tie (game, crowd);
        }
      else if (examine)
        raised = lib_co_400_raise_for_references (game);
      else
        {
          /*
           * Away from examine the crowd is 463640's, over the whole line,
           * and not the reference set our own `%object% *` row bound: a
           * second noun of the same score joins it, and a crowd it leads
           * asks nothing.  p4WTIE `cut stone knife` is therest's "You
           * can't cut that." where `cut stone` asks
           * (runner_probes/wtie.run400.w6.txt, 2026-09-20) -- the knife
           * is the crowd's first object, so the stones never park a
           * pending object.
           */
          raised = lib_co_400_raise_for_pending_tie (game);
        }
      if (raised)
        {
          if (is_ambiguous)
            *is_ambiguous = TRUE;
          return -1;
        }
      if (!examine)
        {
          /*
           * And a 4.0 crowd that asks nothing leaves the handler with no
           * object at all rather than a listing: the command goes on to
           * its own %text% row, which is where "You can't cut that." and
           * "You can't open that." come from
           * (runner_probes/wtie.run400.w6.txt `cut stone knife`, `open
           * box knife`, 2026-09-20).  The invented listing below is
           * 3.9-and-below's alone.
           */
          if (is_ambiguous)
            *is_ambiguous = FALSE;
          return -1;
        }
    }

  /*
   * Two or more DIFFERENT present objects on a 4.0 examine line -- `x coin
   * and the hat`, which the splitter keeps whole because "the" names no
   * object -- never raise a question.  examines() takes referencedob's
   * pick (Proc_19_88_457034, lib_examine_referencedob_400()): pass C
   * counts each candidate's Prefix words in the line, and an equal count
   * is &HFE, "Sorry, I'm not sure which object you're referring to."
   * (4719EA, still a turn); no Prefix word typed leaves the last marked.
   * Measured on p4AND (run400 runner_probes/and.run400.txt,
   * runner_probes/and.run400.b.txt): `x coin and a hat` -> the Sorry line,
   * "a" being both objects' Prefix; `x coin and the hat`, `x coin and hat
   * and box` -> one description.
   */
  if (lib_is_version_400 (game) && strcmp (verb, "examine") == 0
      && run_get_dispatch_input ())
    {
      const scr_int pick
        = lib_examine_referencedob_400 (game, run_get_dispatch_input ());

      if (pick == -2)
        {
          pf_buffer_string (filter, "Sorry, I'm not sure which object"
                                    " you're referring to.\n");
          if (is_ambiguous)
            *is_ambiguous = TRUE;
          return -1;
        }
      if (pick >= 0)
        {
          var_set_ref_object (vars, pick);
          if (is_ambiguous)
            *is_ambiguous = FALSE;
          return pick;
        }
    }

  /*
   * 3.9 examine asks nothing either.  co() is false for each of two seen,
   * present namesakes (it only raises the end-of-turn flag), so
   * referencedob() counts none and returns -1, and examines() answers
   * "Nothing special." (run390 42DF43 -> 44BF94).  The "Which <term>." that
   * lib_co_ambiguity_prompt() prints replaces it when no task ran;
   * cybercow_win T118 `x berry` is the case where the "#Rain" event's task
   * did run.
   *
   * When co() is true for SEVERAL, referencedob() runs a second pass and
   * examines() answers "Please examine one object at a time."; the crowd
   * never reaches here, lib_examine_crowded_390() takes it first.
   */
  if (lib_is_version_390 (game) && strcmp (verb, "examine") == 0)
    {
      pf_buffer_string (filter, "Nothing special.\n");
      if (is_ambiguous)
        *is_ambiguous = TRUE;
      return -1;
    }

  /* The object reference is ambiguous, so list the choices. */
  pf_buffer_string (filter, "Please be more clear, what do you want to ");
  pf_buffer_string (filter, verb);
  pf_buffer_string (filter, "?  ");

  pf_new_sentence (filter);
  listed = 0;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (game->object_references[index_])
        {
          lib_print_object_np (game, index_);
          listed++;
          if (listed < count)
            pf_buffer_string (filter, (listed < count - 1) ? ", " : " or ");
        }
    }
  pf_buffer_string (filter, "?\n");

  /* Return no object for an ambiguous reference. */
  if (is_ambiguous)
    *is_ambiguous = TRUE;
  return -1;
}

scr_int
lib_disambiguate_object (scr_gameref_t game,
                         const scr_char *verb, scr_bool *is_ambiguous)
{
  return lib_disambiguate_object_common (game, verb, NULL, -1, is_ambiguous);
}


/*
 * lib_absent_seen_object()
 * lib_cant_see_absent_object()
 *
 * 4.0's matcher runs a second pass.  When nothing the noun names is in the
 * room, it looks again over everything the player has *seen*, and the
 * handlers above it then answer "<player> can't see <it>" instead of their
 * own generic refusal.  Pre-4.0 has no such pass: run390's co() simply fails
 * to match an object that is elsewhere, and the command falls through to the
 * flat can't-do tail.
 *
 * Measured on p4EXAM.taf, one statue seen in the North Room and examined
 * from the Test Room (runner_probes/exam.run400.txt, all 32 commands
 * echoed) against p39EXAM.taf under run390 (runner_probes/exam.run390.txt,
 * runner_probes/exam.run390.held.txt):
 *
 *     command        run400                              run390
 *     x statue       You can't see the statue from here! Nothing special.
 *     open statue    You can't see the statue.           You can't open that.
 *     close statue   You can't see a statue.             You can't close that.
 *     buy statue     You can't see the statue.           I don't think that
 *                                                          is for sale.
 *
 * Note the article: `close` alone is indefinite.  That is not a stylistic
 * choice, it is a different piece of code -- run400's openclose() composes
 * the open half from the definite-name helper Proc_21_31_448710 (475966) and
 * the close half by hand, Prefix & " " & Short (475C10-475C2D).  `examine`
 * uses the definite helper and appends " from here!" (471958-471975), and
 * `buy` never reaches its own branch at all: therest()'s very first clause
 * (4887A0-4887F5) prints the definite form and exits before the whole verb
 * chain below it.
 *
 * Only those four verbs are measured, so only those four call this.  The
 * therest() clause is verb-wide in the Runner, and the tail of
 * lib_cmd_verb_object() already models the same sentence for anything that
 * reaches it; what is unmeasured is which of the other therest() verbs 4.0
 * intercepts on the way in.
 *
 * WHERE THE CALLS GO MATTERS.  Every one of the Runner's four sites is
 * guarded by "the output buffer is still empty" (471933, 475952, 475BFC,
 * 4887A0 all test MemVar_4941B0 = vbNullString), i.e. the clause speaks only
 * when nothing else in the turn has.  So the callers are four thin handlers
 * sitting immediately above the catch-all `*` rows in scrunner.cpp, not the
 * `%object%` handlers at the top of the table: humbug names an NPC and an
 * absent object both "robot", and unraveling_god an NPC and an absent object
 * both "people", and in each case the Runner and the golden print the NPC's
 * description.  Checking inside lib_cmd_examine_object() would have stolen
 * the turn from lib_cmd_examine_npc() one row below it.  Each `_absent` row
 * re-matches %object%, which repopulates the references that
 * lib_disambiguate_object_common() cleared on the way past.
 *
 * One deliberate deviation: the close half is spelled Prefix & " " & Short
 * with no fallback, so an object with an empty Prefix would give the Runner
 * "You can't see  statue."  lib_print_object() substitutes the usual "a "
 * there instead.  Nothing has measured an empty-Prefix object in this
 * position, and the double space is almost certainly not what 4.0 meant.
 *
 * The gate is the object's seen byte, +48 in the Runner's object record,
 * written the moment an object is listed or described (run400 471749,
 * 46A142) and read by every one of the four sites above.  It is Scarier's
 * gs_object_seen(), so this needs no new state.
 */
/*
 * Whole-word containment of a single name word in the typed line, run400
 * Proc_21_38_454CB0: case-insensitive, and a hit only where the word is
 * bounded by the line's ends or spaces.  Public for run_all_commands()'s
 * recovery gate, the `c("status")` test at 47DCA1.
 *
 * The Runner's c() also ends a word at ",", "." or "?" (454C07-454C39),
 * which is what lets "coin," score on `drop coin, hat`; the 4.0-only
 * callers below take that through lib_input_contains_word_400().  The
 * 3.9 helper's terminators are unmeasured, so the paths shared with 3.9
 * keep the space-only test.
 */
static scr_bool
lib_input_contains_word_ended (const scr_char *input, const scr_char *word,
                               const scr_char *terminators)
{
  const scr_int length = strlen (word);
  const scr_char *scan;

  if (length == 0)
    return FALSE;

  for (scan = input; *scan != NUL; scan++)
    {
      if ((scan == input || scan[-1] == ' ')
          && scr_strncasecmp (scan, word, length) == 0
          && (scan[length] == NUL
              || strchr (terminators, scan[length]) != NULL))
        return TRUE;
    }

  return FALSE;
}

scr_bool
lib_input_contains_word (const scr_char *input, const scr_char *word)
{
  return lib_input_contains_word_ended (input, word, " ");
}

scr_bool
lib_input_contains_word_400 (const scr_char *input, const scr_char *word)
{
  return lib_input_contains_word_ended (input, word, " ,.?");
}

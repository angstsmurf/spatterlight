/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Task command matching, restriction gating, task dispatch and refusal.
 *
 * Split out of scrunner.cpp; see scrunner.h for what the five files share.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"
#include "scrunner.h"



/*
 * run_is_task_function()
 *
 * Check for the presence of a command function in a task command, and action
 * it if found.  This is a 4.0 feature -- at present, only getdynfromroom()
 * exists.  Returns TRUE if function found and handled.
 *
 * The syntax and the selection are measured against the live 4.0 Runner (see
 * RUNNER_TESTS_TODO.md section 9, probes GDA..GDR of
 * test/adrift4/harness/make_arena_probe.py).  run400 squeezes every space out
 * of the command, requires what is left to open "#%object%=getdynfromroom("
 * and the *raw* command to end in ")" -- so "getdynfromroom(larder)x" is not
 * a function at all -- then compares the squeezed argument to room names
 * case-insensitively and takes the first non-static object standing directly
 * in the room it finds.  Object order decides between candidates (a room
 * holding "ring" then "gem" yields the ring), a room holding only statics
 * yields nothing, and the reference set here survives the rest of the turn.
 *
 * Two run400 bugs are deliberately not reproduced, both of which can only
 * lose a match the author meant to make:
 *
 *   deliberate: run400 scans rooms with "For r = 0 To roomCount - 1" over a
 *     1-based array, so the game's *last* room can never be found.  Proved
 *     live: "larder" as room 10 of 10 matched nothing, and started returning
 *     its pie the moment a spare 11th room was appended.  (Its object loop
 *     has no such fencepost -- an object added last is still found.)
 *   deliberate: run400 squeezes the spaces out of the argument but compares
 *     it against the unsqueezed room name, so no room whose name contains a
 *     space is reachable -- not even by the manual's own worked example,
 *     "getdynfromroom(The Park)".  We squeeze both sides, which keeps every
 *     match run400 can make and adds the ones it drops.
 *
 * The 3.9 Runner has no getdynfromroom at all (not one occurrence in its
 * P-code), hence the version gate.
 */
static scr_bool
run_is_task_function (const scr_char *pattern, scr_gameref_t game)
{
  static const scr_char *const FUNCTION = "#%object%=getdynfromroom(";

  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int length, argument_length, room, object;
  scr_char *argument;

  if (run_get_version (bundle) < TAF_VERSION_400)
    return FALSE;

  /* The Runner tests the raw command's final character for the ")". */
  length = strlen (pattern);
  if (length == 0 || pattern[length - 1] != ')')
    return FALSE;

  std::vector<scr_char> squeezed (length + 1);
  run_squeeze_spaces (pattern, squeezed.data ());
  if (scr_strncasecmp (squeezed.data (), FUNCTION, strlen (FUNCTION)) != 0)
    return FALSE;

  /* Take the argument, dropping the ")" that the raw test guarantees. */
  argument = squeezed.data () + strlen (FUNCTION);
  argument_length = strlen (argument);
  assert (argument_length > 0 && argument[argument_length - 1] == ')');
  argument[argument_length - 1] = NUL;

  /* Compare the argument read in against known room names. */
  for (room = 0; room < gs_room_count (game); room++)
    {
      const scr_char *name;

      name = prop_get_indexed_string (bundle, "Rooms", room, "Short");
      std::vector<scr_char> compressed (strlen (name) + 1);
      run_squeeze_spaces (name, compressed.data ());
      if (scr_strcasecmp (compressed.data (), argument) == 0)
        break;
    }
  if (room == gs_room_count (game))
    return FALSE;

  /* Select the first dynamic object standing on the room's floor. */
  for (object = 0; object < gs_object_count (game); object++)
    {
      scr_bool bstatic;

      bstatic = prop_get_indexed_boolean (bundle, "Objects", object, "Static");
      if (!bstatic && obj_directly_in_room (game, object, room))
        break;
    }
  if (object == gs_object_count (game))
    return FALSE;

  /* Set this object reference, unambiguously, as if %object% match. */
  gs_clear_object_references (game);
  game->object_references[object] = TRUE;
  var_set_ref_object (vars, object);

  return TRUE;
}


/*
 * Cached per-task command patterns.
 *
 * run_match_task_commands() is called for every task on every player command,
 * and before caching it re-read the task's (Reverse)Command pattern strings
 * from the bundle on each attempt.  The patterns are immutable once a game
 * is loaded, and prop_get_string() returns stable pointers into the bundle,
 * so remember each task's pattern list (per direction) the first time it is
 * walked; the walk itself goes through the same fatal-checking prop_get_*
 * wrappers the uncached code used.  The cache tracks a single game;
 * gs_destroy() calls run_forget_game().
 */
typedef struct
{
  scr_bool known[2];                          /* indexed [forwards] */
  std::vector<const scr_char *> patterns[2];
  /* A command whose markers were lower-cased lives here, and patterns[]
     points into it.  See run_lower_command_markers(). */
  std::vector<std::string> rewritten[2];
} scr_task_commands_t;

static scr_bool run_task_passes_class_filter (scr_gameref_t game,
                                              scr_int task);
static const void *run_cache_game = NULL;

static std::vector<scr_task_commands_t> run_cache;

/*
 * run_lower_command_markers()
 *
 * Lower-case every `%...%` marker of a task command, in place; TRUE when
 * anything changed.
 *
 * A task command is already lower-case by the time any Runner's matcher sees
 * it -- the folding happens at load, not in checktask, which holds no LCase
 * above 44B0BA and reads the command straight out of the task record
 * (44AAA5, 44AF12).  p*VARREF said so sideways first: the variable arm builds
 * its marker as `"%" & Name & "%"` and looks for it with a binary-compare
 * InStr (44AF43), yet `wibb %NUM%` reached a variable named `num` while
 * `bork %Big%` AND `snib %big%` over a variable named `Big` reached nothing
 * -- one rule explains both, and it is that the command was folded and the
 * stored Name was not (var_get_command_number()).
 *
 * p*CASEREF then asked the four known markers directly
 * (runner_probes/caseref.run370.rtf, runner_probes/caseref.run380.rtf,
 * runner_probes/caseref.run390.txt, runner_probes/caseref.run400.txt,
 * 2026-09-20): tasks `frob %Object%`, `nurb %CHARACTER%`, `blip %Number%` and
 * `murg %TEXT%` beside their lower-case twins, fed `frob rock`, `nurb fay`,
 * `blip 7`, `murg quux`.  Every capitalised marker runs its task, in every
 * Runner that knows the marker at all (%character% and %number% from 3.90,
 * %text% at 4.00; below that the command is a literal either way and the
 * cells answer DontUnderstand on both sides).  So X-Files task 30, `Molest
 * *%Character%`, is a live %character% command and not the unknown-marker
 * literal this took it for.
 *
 * Only the markers are folded here, not the whole command.  Everything else
 * a command holds is already compared case-insensitively -- the equality
 * LCase()s both sides (44B0BA/44B0DA), checkwild and 4.0's wildcard matcher
 * fold too (p*CASEREF's `* Zag * GEM *` runs on `xxx zag xxx gem xxx`
 * everywhere, and did here before this) -- so the two models differ only
 * where a literal's case can still show, which is 4.0's NewParse group
 * compare (45D7FA/45D835, binary on both sides; see uip_set_binary_input()),
 * and no measurement separates them there.
 */
static scr_bool
run_lower_command_markers (std::string &command)
{
  scr_bool changed = FALSE;
  size_t at = 0;

  while ((at = command.find ('%', at)) != std::string::npos)
    {
      const size_t end = command.find ('%', at + 1);
      size_t index_;

      if (end == std::string::npos)
        break;
      for (index_ = at + 1; index_ < end; index_++)
        {
          const scr_char lowered = scr_tolower (command[index_]);

          if (lowered != command[index_])
            {
              command[index_] = lowered;
              changed = TRUE;
            }
        }
      at = end + 1;
    }
  return changed;
}

const std::vector<const scr_char *> &
run_task_command_patterns (scr_gameref_t game, scr_int task,
                           scr_bool forwards)
{
  const int direction = forwards ? 1 : 0;
  scr_task_commands_t *cached;

  if (run_cache_game != game)
    {
      run_cache.assign (gs_task_count (game), scr_task_commands_t ());
      run_cache_game = game;
    }

  cached = &run_cache[task];
  if (!cached->known[direction])
    {
      const scr_prop_setref_t bundle = gs_get_bundle (game);
      scr_vartype_t vt_key[4];
      scr_int command_count, command;

      vt_key[0].string = "Tasks";
      vt_key[1].integer = task;
      vt_key[2].string = forwards ? "Command" : "ReverseCommand";
      command_count = prop_get_child_count (bundle, "I<-sis", vt_key);
      cached->patterns[direction].reserve (command_count);
      for (command = 0; command < command_count; command++)
        {
          vt_key[3].integer = command;
          cached->patterns[direction]
              .push_back (prop_get_string (bundle, "S<-sisi", vt_key));
        }
      cached->rewritten[direction].resize (command_count);

      /* A command's markers reach the matcher lower-cased; see above. */
      for (command = 0; command < command_count; command++)
        {
          std::string lowered (cached->patterns[direction][command]);

          if (run_lower_command_markers (lowered))
            {
              cached->rewritten[direction][command] = lowered;
              cached->patterns[direction][command] =
                  cached->rewritten[direction][command].c_str ();
            }
        }
      cached->known[direction] = TRUE;
    }
  return cached->patterns[direction];
}


/*
 * 3.7's permanent task-command rewrite -- not ported.
 *
 * 3.7 substitutes a task command's %object% INTO THE TASK, and the rewrite
 * outlives the turn.  run370's checktask (4332CA) copies the command aside
 * into var_D0, replaces %object% with the Short of each object its c() finds
 * in the line -- writing the result back over the command record at 433377,
 * so only the first match has a %object% left to replace -- and restores the
 * saved copy at 43342B when the command did not match.  On a MATCH the
 * original never goes back, and the task is left holding a command naming
 * one object for the rest of the session, with no %object% for a later turn
 * to bind.
 *
 * p37OBJREF, whose task 1 is `nurb %object%` printing "NURBED %object%.",
 * with three rocks sharing the Short "rock":
 *
 *   `nurb rock`, then `nurb rock` again -> "NURBED a red rock." and then
 *   "NURBED %object%.": the second line still matches the command, now
 *   literally "nurb rock", but binds nothing
 *   (runner_probes/objref.run370.rtf).
 *   `nurb gem` first instead -> "NURBED a gem.", and `nurb rock` after it
 *   is the library's "Which rock.  The big rock or the red rock?", the task
 *   being spelled "nurb gem" now (runner_probes/objref.run370.b.rtf).
 *   A line that does NOT match leaves the command alone: `frob rock` (the
 *   object catch-all) and `x coin` both keep `nurb coin` working as a task
 *   afterwards (runner_probes/objref.run370.e.rtf,
 *   runner_probes/objref.run370.c.rtf, 2026-09-20).
 *
 * run380 works on a copy throughout -- p38OBJREF answers every cell of both
 * feeds normally (runner_probes/objref.run380.rtf,
 * runner_probes/objref.run380.b.rtf) -- so this is 3.7's alone.  The rewrite
 * belongs to the loaded game rather than to game state: like the Runner's own
 * task record it is not undone by UNDO and not restored from a save.
 *
 * Deliberate deviation (2026-09-27): not ported.  After one `eat apple`, a
 * task `eat %object%` would answer nothing but the apple for the rest of the
 * session, which only ever takes an author's task away from the player.
 */

/*
 * run_forget_game()
 *
 * Drop any command pattern cache built for the given game.  Called from
 * gs_destroy() so a stale cache can never outlive its game.
 */
void
run_forget_game (const void *game)
{
  if (run_cache_game == game)
    {
      run_cache_game = NULL;
      run_cache.clear ();
    }
}

/*
 * run_pattern_names_verb()
 *
 * Helper for run_match_task_commands().  Return TRUE if the pattern
 * contains, as a standalone whitespace-delimited token, the first word of
 * the string passed in (case insensitive).
 */
static scr_bool
run_pattern_names_verb (const scr_char *pattern, const scr_char *string)
{
  static const scr_char *const PATTERN_WORD_BREAK = "\t\n\v\f\r *";
  const scr_char *verb;
  scr_int verb_length;

  /* Isolate the first word of the string; no word, no possible match. */
  verb = string + strspn (string, WHITESPACE);
  verb_length = strcspn (verb, WHITESPACE);
  if (verb_length == 0)
    return FALSE;

  /*
   * Scan pattern tokens for a case-insensitive whole-word match.  A '*' ends
   * a token as surely as a space does: authors write wildcards glued to their
   * words, and frustrated.taf's `*drop*tree*` names the verb "drop" just as
   * plainly as "* drop * tree *" would.  Treating the glued form as one
   * eleven-character token hid the verb, and the pattern -- being
   * wildcard-leading -- was then skipped by every library call, so the
   * 4.0 drop handler's look-up could not find it (run400
   * runner_transcripts/frustrated.txt gives it the line).
   */
  for (pattern += strspn (pattern, PATTERN_WORD_BREAK); *pattern != NUL;)
    {
      const scr_int token_length = strcspn (pattern, PATTERN_WORD_BREAK);

      if (token_length == verb_length
          && scr_strncasecmp (pattern, verb, verb_length) == 0)
        return TRUE;

      pattern += token_length;
      pattern += strspn (pattern, PATTERN_WORD_BREAK);
    }

  return FALSE;
}

/* Set while a 4.0 question continuation with a double space runs; see
 * run_match_task_commands(). */
scr_bool run_rerun_skips_tasks = FALSE;

/* 3.9 runs a prefix rerun as joined and compares task commands against it
   space for space; see run_match_task_commands(). */
scr_bool run_rerun_exact_spaces = FALSE;

/* Set for a line no task matches the Runner's way; see
   run_line_matches_task_strictly(). */
scr_bool run_lenient_tasks = FALSE;


/* How many times run_note_task_ran() has noted a task; a caller compares two
   readings to learn whether a pass ran anything, a task that had already run
   for the line included. */
static scr_int run_task_runs_noted = 0;

static void
run_note_task_ran (scr_gameref_t game, scr_int task)
{
  run_task_runs_noted++;
  run_co_task_claimed = TRUE;
  if (run_tasks_ran_this_command.size () != (size_t) gs_task_count (game))
    run_tasks_ran_this_command.assign (gs_task_count (game), FALSE);
  run_tasks_ran_this_command[task] = TRUE;
}

static scr_bool
run_task_ran_this_command (scr_int task)
{
  return (size_t) task < run_tasks_ran_this_command.size ()
         && run_tasks_ran_this_command[task];
}

/*
 * run_npc_library_blocked()
 *
 * TRUE while the library's NPC rows are to be skipped: a 3.9+ game in which
 * some task has already run for the current line.  See the note in
 * run_try_command_table().
 */
scr_bool
run_any_task_ran_this_command (void)
{
  for (const scr_bool ran : run_tasks_ran_this_command)
    {
      if (ran)
        return TRUE;
    }
  return FALSE;
}


/*
 * Measured 2026-08-23 (make_39_doneprobe.py, run390): below 4.0 a game task
 * that matches the command element claims it even when it says nothing, so the
 * standard library verb that would otherwise answer never gets a turn.  `x
 * book` on a spent `* x * book *` task answers "You have already done that."
 * instead of the book's description, where `look at book` -- matching no task
 * -- prints the description; and a silent task that runs and prints nothing
 * leaves "I don't understand." rather than the library answer.  4.0 dropped
 * this: run400 falls through to the library examine in both cells.
 *
 * The spent half is PORTED (2026-09-13): run_spent_task_390() in
 * run_all_commands() makes the claim where run390's checktask makes it, ahead
 * of everything but the take/drop/inventory/put handlers and the NPC examine.
 * It costs the walkthroughs that used to win past a spent `*` task -- The
 * Long Journey Home stops at 5/90 in the Lair, exactly where run390 does --
 * and those goldens now hold the Runner's brick.
 *
 * The silent half is PORTED too (2026-09-20, 3.9 only): its clock went first
 * (2026-09-19, the line is not a turn) and now the claim, silent_task_390 in
 * run_all_commands() skipping the whole library block so the DontUnderstand
 * text comes out.  RUNNER_TESTS_TODO.md section 4 priced that at 15 goldens
 * and four lost walkthroughs, but that was before the spent-task port had
 * taken most of those cells: re-measured it costs six goldens and no win --
 * alexis, alexis_worn_cube, lifesimulation, life and everything change one
 * line of text, the_hangover two turns -- and all six rows then match their
 * Runner transcripts exactly.  3.7/3.8 are left alone: checktask is the same
 * routine there, but no probe has been run and no corpus row asks.
 */

/*
 * scr_strict_reference_guard
 *
 * Turns strict %object% matching on for the lifetime of the guard, and off
 * again however the scope is left.  match_case separates the two Runners that
 * bind strictly: 4.0 substitutes the name as authored, 3.90 lower-cases it.
 */
class scr_strict_reference_guard
{
public:
  scr_strict_reference_guard (scr_bool strict, scr_bool match_case)
    : strict_ (strict)
  {
    if (strict_)
      uip_set_strict_reference (TRUE, match_case);
  }

  ~scr_strict_reference_guard ()
  {
    if (strict_)
      uip_set_strict_reference (FALSE, FALSE);
  }

  scr_strict_reference_guard (const scr_strict_reference_guard &) = delete;
  scr_strict_reference_guard &
  operator= (const scr_strict_reference_guard &) = delete;

private:
  const scr_bool strict_;
};


/*
 * run_line_names_word()
 *
 * TRUE when run380's c() (429048) finds WORD in LINE, which must already be
 * lowercased: the word starts the line or follows a space, and ends the line
 * or is followed by a space or a comma.  Only the first occurrence that
 * starts a word is looked at, exactly as c() does.
 */
static scr_bool
run_line_names_word (const std::string &line, const scr_char *name)
{
  std::string word (name ? name : "");
  size_t at = 0;

  if (word.empty ())
    return FALSE;
  for (char &c : word)
    c = scr_tolower (c);
  while ((at = line.find (word, at)) != std::string::npos)
    {
      const size_t end = at + word.size ();

      if (at == 0 || line[at - 1] == ' ')
        return end == line.size () || line[end] == ' ' || line[end] == ',';
      at++;
    }
  return FALSE;
}


/* Put WITH in place of every FIND in TEXT, the way VB's Replace() with a
   count of -1 does. */
void
run_replace_all (std::string &text, const scr_char *find,
                 const scr_char *with)
{
  const size_t length = strlen (find);
  size_t at = 0;

  while ((at = text.find (find, at)) != std::string::npos)
    {
      text.replace (at, length, with);
      at += strlen (with);
    }
}


/*
 * run_val()
 *
 * VB's Val() over a token with no spaces in it: an optional sign, digits,
 * an optional fractional part, and everything from the first character that
 * is none of those thrown away.  Val("007") is 7 and Val("3x") is 3, which
 * is the whole reason a line and the command it just spelled can disagree.
 */
static double
run_val (const std::string &token)
{
  double sign = 1.0, value = 0.0, scale = 0.1;
  size_t at = 0;

  if (at < token.size () && (token[at] == '-' || token[at] == '+'))
    sign = token[at++] == '-' ? -1.0 : 1.0;
  for (; at < token.size () && isdigit ((unsigned char) token[at]); at++)
    value = value * 10.0 + (token[at] - '0');
  if (at < token.size () && token[at] == '.')
    for (at++; at < token.size () && isdigit ((unsigned char) token[at]); at++)
      {
        value += (token[at] - '0') * scale;
        scale *= 0.1;
      }
  return sign * value;
}


/*
 * run_line_number()
 *
 * numintext() (run390 4332C8, run400 Proc_19_52_453A48), and it is nobody's
 * idea of a parser.  Take the LOWEST position in the line at which any of
 * the characters "0".."9" occurs -- a 0..9 loop over InStr keeping the
 * minimum, 433112-43317E -- collect the non-space run STARTING AT THAT
 * CHARACTER (433198-433208), prepend "-" when the character before it is one
 * (433227-433265), and take Val() of that (433268), clamped to a signed
 * 32-bit range and rounded by CLng (4332B1).  Return FALSE, leaving the
 * stored number alone, when the line holds no digit at all: the whole body
 * including the write to MemVar_4681AC sits inside `If var_8A < 32000`
 * (433183-4332BB), so `zork five apples` does not clear what the line
 * before it set.
 */
static scr_bool
run_line_number (const std::string &line, scr_int *number)
{
  size_t at = std::string::npos, end;
  std::string token;
  double value;
  scr_int digit;

  for (digit = 0; digit < 10; digit++)
    {
      const size_t found = line.find ((scr_char) ('0' + digit));

      if (found != std::string::npos
          && (at == std::string::npos || found < at))
        at = found;
    }
  if (at == std::string::npos)
    return FALSE;

  for (end = at; end < line.size () && line[end] != ' '; end++)
    ;
  token = line.substr (at, end - at);
  if (at > 0 && line[at - 1] == '-')
    token.insert (token.begin (), '-');

  value = run_val (token);
  if (value > 2147483647.0)
    value = 2147483647.0;
  if (value < -2147483647.0)
    value = -2147483647.0;
  *number = (scr_int) nearbyint (value);
  return TRUE;
}


/*
 * run_line_number_word()
 *
 * numintext2() (run390 42946C, run400 Proc_19_53_444458): `For n = 0 To
 * &H14`, `c(int2text(n), "")` -- c() with an empty second argument searches
 * the input line -- and no early exit, so it is looking for a number SPELLED
 * OUT, whole-word, and the LAST n whose word is in the line wins.
 */
static scr_bool
run_line_number_word (const std::string &line, scr_int *number)
{
  scr_bool is_found = FALSE;
  scr_int n;

  for (n = 0; n <= 20; n++)
    {
      if (run_line_names_word (line, var_number_word (n)))
        {
          *number = n;
          is_found = TRUE;
        }
    }
  return is_found;
}


/*
 * run_substitute_number_references()
 *
 * Put in place of a task command's %number% and %t_number% the DIGITS of the
 * number the line names, and store that number as the game's referenced one.
 *
 * checktask does this straight after the %object% and %character% walks and
 * before it tests the command at all (run390 44ADDF and 44AE8B; run400's
 * substituter Proc_19_36_45F268 is the same routine in mdlSpreadTheLoad,
 * %number% at 45F03C and %t_number% at 45F0AA).  Two things follow:
 *
 *  - %number% is not a positional wildcard: it is a SUBSTITUTION, like
 *    %object%, so a '*' command carrying one is decided pre-4.0 by checkwild,
 *    whose pieces are found by InStr anywhere in the line and in any order.
 *    p39NUMREF's task 2 is "* zog * %number% *", and run390 runs it on `blip
 *    7 zog` and answers `blip 9 zog 3 blip` with "NUM2 [9]." -- numintext
 *    took the 9 because it is the leftmost digit, not because the pattern
 *    reached it.  run400 refuses both, its own matcher cutting the line as it
 *    goes (runner_probes/numref.run390.txt, runner_probes/numref.run400.txt,
 *    2026-09-20).
 *  - the substituted command has to equal the line that produced it, and
 *    Val() makes that fail: `zork 007 apples` spells "zork 7 apples" and
 *    `zork 3x apples` spells "zork 3 apples", so both are refused at 3.90
 *    and at 4.00 while a positional matcher takes them.
 *
 * Both markers are replaced with digits -- Format(MemVar_4681AC) at 44AE15
 * and CStr(MemVar_4681AC) at 44AEBE -- although the OUTPUT filter spells
 * %t_number% out (45B21B, int2text).  So the words-vs-digits split is real
 * everywhere except here, and a %t_number% command can match nothing: the
 * line says "five" and the command it is compared against says "5"
 * (p*NUMREF task 3 "frob %t_number%" is refused for `frob five` and for
 * `frob 5` in all four Runners).
 *
 * Both write the same Long, and they write it whether or not the task goes
 * on to match: `nurb 5 blip` runs nothing and the next `zap` still prints
 * "ZAP [5] [five].".
 */
static void
run_substitute_number_references (scr_gameref_t game,
                                  const std::string &lowered,
                                  std::string &literal)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_char digits[32];
  scr_int number;

  if (literal.find ("%number%") != std::string::npos
      && run_line_number (lowered, &number))
    {
      var_set_ref_number (vars, number);
      snprintf (digits, sizeof (digits), "%ld", number);
      run_replace_all (literal, "%number%", digits);
    }
  if (literal.find ("%t_number%") != std::string::npos
      && run_line_number_word (lowered, &number))
    {
      var_set_ref_number (vars, number);
      snprintf (digits, sizeof (digits), "%ld", number);
      run_replace_all (literal, "%t_number%", digits);
    }
}


/*
 * run_substitute_variable_references()
 *
 * Put in place of a task command's %<user variable>% the variable's value.
 *
 * The last arm of checktask's substitution, after the numbers: a loop over
 * the whole variable array, each InStr'ing `"%" & Name & "%"` in the command
 * and Replacing it with the value (run390 44AF07-44AFDA, run400
 * 45F105-45F1B3).  What a marker is worth, which markers reach a variable at
 * all and why "%t_<name>%" reaches none are all var_get_command_number().
 */
static void
run_substitute_variable_references (scr_gameref_t game, std::string &literal)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  size_t at;

  for (at = 0; (at = literal.find ('%', at)) != std::string::npos; )
    {
      const size_t end = literal.find ('%', at + 1);
      scr_char digits[32];
      scr_int number;

      if (end == std::string::npos)
        break;
      if (!var_get_command_number (vars,
                                   literal.substr (at + 1,
                                                   end - at - 1).c_str (),
                                   &number))
        {
          at = end + 1;
          continue;
        }
      snprintf (digits, sizeof (digits), "%ld", number);
      literal.replace (at, end - at + 1, digits);
      at += strlen (digits);
    }
}


/*
 * run_pattern_references()
 *
 * The set of %reference% markers a task command carries, as a bitmask, or
 * RUN_REF_OTHER for a marker that is none of the four and reaches no
 * variable of the game's own either.
 */
enum
{
  RUN_REF_OBJECT = 1, RUN_REF_CHARACTER = 2,
  RUN_REF_NUMBER = 4, RUN_REF_TEXT = 8, RUN_REF_VARIABLE = 16,
  RUN_REF_OTHER = 32
};

static scr_int
run_pattern_references (scr_gameref_t game, const scr_char *pattern)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const std::string text (pattern);
  scr_int found = 0;
  size_t at;

  for (at = 0; (at = text.find ('%', at)) != std::string::npos; )
    {
      const size_t end = text.find ('%', at + 1);
      const std::string token = end == std::string::npos
                                ? std::string ()
                                : text.substr (at, end - at + 1);
      scr_int number;

      if (token == "%object%")
        found |= RUN_REF_OBJECT;
      else if (token == "%character%")
        found |= RUN_REF_CHARACTER;
      else if (token == "%number%" || token == "%t_number%")
        found |= RUN_REF_NUMBER;
      else if (token == "%text%")
        found |= RUN_REF_TEXT;
      else if (token.length () > 2
               && var_get_command_number (vars,
                                          token.substr (1,
                                                        token.length () - 2)
                                              .c_str (), &number))
        found |= RUN_REF_VARIABLE;
      else
        return found | RUN_REF_OTHER;
      at = end + 1;
    }
  return found;
}


/*
 * run_pre400_substitute_references()
 *
 * Put in place of a pre-4.0 task command's %object% and %character% the name
 * the LINE names, and say which entity the command has thereby bound.
 * Returns FALSE when the command holds a reference this cannot substitute,
 * in which case there is nothing to test and the tree's answer stands.
 *
 * checktask (run390 44AA5A, run380 43B78B, run370 4332CA) does this before
 * it tests the command at all, and the walk is over the whole object array
 * with no break: every hit stores its index in MemVar_4681A8, so the LAST hit
 * is the reference the task's text expands, while Replace() only ever fires
 * once -- the FIRST hit spells the literal that is then compared.  The two
 * come apart whenever a line names two namesakes, and p39WILDREF answers
 * `blip zog blip rock blip gem blip` against "* zog * %object% *" with "WILD1
 * a gem.": the rock (index 0) made the string that matched and the gem (index
 * 1) is what %object% expands to (runner_probes/wildref.run390.txt,
 * 2026-09-20).
 *
 * 3.90 tests the Short (44AAD6) and then the Aliases (44AB65) of the SAME
 * object before it moves on to the next -- one `For var_138 ... Next
 * var_138`, the Next at 44ABE5 -- and the Alias arm substitutes the ALIAS,
 * .global_8, not the Short.  So the order is obj0.Short, obj0.Alias,
 * obj1.Short, obj1.Alias, ..., and a line naming one object's Short and an
 * earlier object's Alias is spelled with the ALIAS and expands to the
 * later object.  p39TEXTSRC (make_textsrcprobe.py,
 * runner_probes/textsrc.run390.txt, 2026-09-20) answers both `zug rock stone`
 * and `zug stone rock` against "* zug * %object% *" with "WILD [a rock].": the
 * gem is index 0 and binds through its alias "stone", spelling "* zug * stone
 * *", and the rock binds after it.  Both gates are the seen byte .global_44
 * and nothing else -- there is no scope test, so an absent object binds.
 *
 * 4.00 is the one that walks twice, every Short and then every Alias:
 * run400 answers the same two lines with "WILD [a gem]." -- the rock's
 * Short spells the string and the gem's alias is the last hit either way.
 * That path is uip's, not this one.
 *
 * 3.7/3.8 know only the Short and gate on nothing: p38WILDREF binds the coin
 * two rooms away and unseen, and p38TEXTSRC answers `blip stone` against
 * "blip %object%" with the object catch-all, the alias reaching nothing.
 * %character% arrives at 3.90 (44AD2A), by Name, with no gate whatever: the
 * King binds from the Cave he is not in.
 *
 * %number% and %t_number% follow, at 3.90 and not below -- see
 * run_substitute_number_references().  A marker this version has never heard
 * of is left standing, which is a substitution of a kind: below 3.90
 * "%number%" is a literal the player has to type, exactly as "%character%"
 * is below 3.80 (run370 and run380 hold no "%number%", "%t_number%" or
 * "%text%" string anywhere), and "%text%" is one below 4.00.
 *
 * The game's own variables come last (44AF07) -- see
 * run_substitute_variable_references().
 *
 * Not emulated, and measured 2026-09-20 (p39TEXTSRC,
 * runner_probes/textsrc.run390.txt):
 * the string 3.90 searches is not `line` at all but MemVar_468224, a
 * SNAPSHOT of the line taken at the end of the synonym pass (45F20F), so
 * none of generaltasks' own rewrites below it -- "everything"->"all"
 * (45F225), "slap"->"hit" (45F246), "except"/"apart from"->"but" (45F267,
 * 45F288), the `with ` history prepend (45F2AF) -- reaches the walk, while
 * the string the command is TESTED against is the rewritten one.  Task `zog
 * %object%` with objects named `hit` and `slap` is refused by `zog slap`:
 * the walk binds the slap and spells "zog slap", the test is against "zog
 * hit", and the library answers instead ("You hit the hit, but nothing
 * happens.").  The second pair of loops at 44ABFE/44AC98 does repeat the
 * walk against checktask's own `text` argument, but its guard is
 * `MemVar_4681A8 = &HFF` -- the per-TURN referenced object, cleared once at
 * 45EC68 -- so the first task command whose walk binds anything consumes it
 * for every later task on the same line.  `nurb except` with an object
 * named `but` is therefore refused by task `nurb %object%`: task 1's
 * `blip %object%` fell back first, bound the but and spelled "blip but".
 * Scarier keeps one string and no fallback, so it takes both lines.
 *
 * A command holding a marker that is neither a known reference nor a
 * variable of the game's own is still handed back.
 */
static scr_bool
run_pre400_substitute_references (scr_gameref_t game, const scr_char *line,
                                  const scr_char *pattern,
                                  std::string &literal,
                                  scr_int *object, scr_int *character)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = run_get_version (bundle);
  std::string lowered (line);

  literal = pattern;
  *object = *character = -1;
  for (char &c : lowered)
    c = scr_tolower (c);

  if (run_pattern_references (game, pattern) & RUN_REF_OTHER)
    return FALSE;

  if (literal.find ("%object%") != std::string::npos)
    {
      scr_int index;

      for (index = 0; index < gs_object_count (game); index++)
        {
          scr_int alias, alias_count;
          scr_vartype_t vt_key[4];
          const scr_char *name;

          if (version >= TAF_VERSION_390 && !gs_object_seen (game, index))
            continue;
          if (version == TAF_VERSION_390 && obj_is_static (game, index))
            continue;

          /* The object's Short first ... */
          name = prop_get_indexed_string (bundle, "Objects", index, "Short");
          if (run_line_names_word (lowered, name))
            {
              *object = index;
              if (literal.find ("%object%") != std::string::npos)
                run_replace_all (literal, "%object%", name);
            }
          if (version < TAF_VERSION_390)
            continue;

          /* ... then, in the same turn of the same loop, its Aliases, which
           * substitute the ALIAS and not the Short.
           */
          vt_key[0].string = "Objects";
          vt_key[1].integer = index;
          vt_key[2].string = "Alias";
          alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
          for (alias = 0; alias < alias_count; alias++)
            {
              vt_key[3].integer = alias;
              name = prop_get_string (bundle, "S<-sisi", vt_key);
              if (!run_line_names_word (lowered, name))
                continue;
              *object = index;
              if (literal.find ("%object%") != std::string::npos)
                run_replace_all (literal, "%object%", name);
              break;
            }
        }
    }

  if (version >= TAF_VERSION_390
      && literal.find ("%character%") != std::string::npos)
    {
      scr_int npc;

      for (npc = 0; npc < gs_npc_count (game); npc++)
        {
          const scr_char *name =
              prop_get_indexed_string (bundle, "NPCs", npc, "Name");

          if (!run_line_names_word (lowered, name))
            continue;
          *character = npc;
          if (literal.find ("%character%") != std::string::npos)
            run_replace_all (literal, "%character%", name);
        }
    }

  if (version >= TAF_VERSION_390)
    {
      run_substitute_number_references (game, lowered, literal);
      run_substitute_variable_references (game, literal);
    }

  return TRUE;
}


/*
 * run_match_task_commands()
 *
 * Helper for run_game_commands_common().
 *
 * Search task command for a match to the string passed in, returning TRUE
 * if a task command matches, FALSE otherwise.  Ordinary or reverse commands
 * are selected by 'forwards'.
 */
static scr_bool
run_match_task_commands (scr_gameref_t game,
                         scr_int task, const scr_char *string,
                         scr_bool forwards, scr_bool is_library)
{
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, forwards);
  const scr_int command_count = (scr_int) patterns.size ();
  scr_int command;
  scr_bool is_matched;

  /* 3.90 and up bind %object% strictly, and only 4.0 binds it case-
   * sensitively -- see the note above uip_compare_reference_strict().  Task
   * commands only; the library's own patterns keep the tolerant matcher. */
  const scr_int version = run_get_version (gs_get_bundle (game));
  const scr_strict_reference_guard strict_reference
      (version >= TAF_VERSION_390, version >= TAF_VERSION_400);
  const scr_task_commands_guard task_commands;

  /*
   * 4.0 compares a task command against the line as it stands, so the two
   * spaces of a "...with?" continuation (`cut rope with ` & " " & `knife`)
   * match no task: run400 answers `cut rope`, `knife` with the library's
   * "You can't cut the rope with the knife." although the game has a task
   * `cut rope with knife` (runner_probes/withq.run400.txt).  Scarier's
   * matchers want single spaces, so run_player_input() runs such a rerun
   * collapsed and raises run_rerun_skips_tasks for it instead.
   */
  if (run_rerun_skips_tasks && !is_library)
    return FALSE;

  /*
   * 3.9 does not collapse the two spaces either, and its task matcher sees
   * them: p39WITHQ's `saw rope` / `knife` fires the task wired
   * `saw rope with  knife` and not the `saw rope with knife` before it
   * (run390x runner_probes/withq.run390.txt, 2026-09-25).  Scarier's matchers
   * let a double space through, so a pattern that has neither a double space
   * nor a wildcard to swallow it is skipped instead.  A double space in a
   * pattern is modelled only as far as that: such a pattern still matches
   * through the usual single-space matcher.
   */
  if (run_rerun_exact_spaces && !is_library)
    {
      scr_bool any = FALSE;

      for (command = 0; command < command_count && !any; command++)
        any = strstr (patterns[command], "  ") != NULL
              || strpbrk (patterns[command], "*%") != NULL;
      if (!any)
        return FALSE;
    }

  /* Iterate over commands, looking for patterns that match string. */
  is_matched = FALSE;
  for (command = 0; command < command_count; command++)
    {
      const scr_char *pattern;
      scr_int first;

      /* Retrieve the pattern for this command, find its first character. */
      pattern = patterns[command];
      first = strspn (pattern, WHITESPACE);

      /*
       * Make a special case of library calls and commands that begin with a
       * wildcard.  Probed live in run400 (2026-08-22, probes pPREC and
       * pPREC2): a wildcard-leading pattern
       * with failing messaged restrictions blocks the system take only when
       * the pattern explicitly names a verb -- either the library's
       * canonical verb ("* get * tent *" blocks both "get tent" and "take
       * tent") or the verb the player actually typed ("* take * tent *"
       * blocks "take tent" but NOT "get tent").  A verb-less "* ball *"
       * pattern never blocks: the system take wins even though the same
       * pattern with passing restrictions would run as a game command.
       * Failing restrictions with an empty message fall through to the
       * system command silently in every case (that drops out of the
       * loudly-restricted machinery here without special handling).
       *
       * The library constructs its match string with the canonical verb
       * ("get <object>"), so a pattern naming only the typed verb cannot
       * match it; for those, retry the match against the player's actual
       * input, stashed by run_all_commands().
       */
      /*
       * Deliberate deviation: on a line no task matches the Runner's way,
       * a command is matched leniently -- the tolerant tree, none of the
       * Runner's substitute-and-compare below; see
       * run_line_matches_task_strictly().  A %character% command stays
       * strict: thenightmoon's `attack giant rat with longsword` would
       * otherwise run task 5's warning for striking a friend, which no
       * Runner ever shows, and lose the game.
       */
      const scr_bool lenient = run_lenient_tasks
                               && strstr (pattern, "%character%") == NULL;
      if (lenient && version >= TAF_VERSION_390)
        uip_set_strict_reference (FALSE, FALSE);

      const scr_char *matched_input = string;
      if (pattern[first] == SPECIAL_PATTERN)
        ;
      else if (is_library && pattern[first] == WILDCARD_PATTERN)
        {
          if (run_pattern_names_verb (pattern, string))
            is_matched = uip_match (pattern, string, game);
          if (!is_matched && run_dispatch_input != NULL
              && run_pattern_names_verb (pattern, run_dispatch_input))
            {
              is_matched = uip_match (pattern, run_dispatch_input, game);
              matched_input = run_dispatch_input;
            }
        }
      else
        is_matched = uip_match (pattern, string, game);

      if (lenient && version >= TAF_VERSION_390)
        uip_set_strict_reference (TRUE, version >= TAF_VERSION_400);

      const scr_bool wild = strchr (pattern, WILDCARD_PATTERN) != NULL;
      const scr_bool group = strpbrk (pattern, "[{") != NULL;

      /*
       * Which %reference% markers the command carries decides who answers
       * it.  %number%, %t_number% and a marker naming one of the game's own
       * variables are a SUBSTITUTION in every Runner that knows them, never
       * a positional wildcard, so the tree's answer is beside the point:
       * see run_substitute_number_references() and
       * run_substitute_variable_references().  Below 3.90 numbers are not
       * markers at all, nor is %text% below 4.00, and a command carrying
       * one has to be typed with the percent signs in it; below 3.90 the
       * file has no Variables section either, so nothing there can name a
       * variable and the question does not arise.
       */
      const scr_int refs = run_pattern_references (game, pattern);
      const scr_bool numeric = (refs & RUN_REF_NUMBER)
                               && version >= TAF_VERSION_390;
      const scr_bool variable = (refs & RUN_REF_VARIABLE) != 0;
      const scr_bool literal_ref =
          ((refs & RUN_REF_NUMBER) && version < TAF_VERSION_390)
          || ((refs & RUN_REF_TEXT) && version < TAF_VERSION_400);

      /*
       * 4.0's command loop (45D9FC-45DBA4) tests a command three ways, in
       * this order, stopping at the first that takes:
       *
       *   1. `LCase(line) = LCase(cmd)` (45DA51) -- plain equality;
       *   2. `If InStr(cmd, "*") > 0` (45DA8C), Proc_19_50_457D68 -- the
       *      non-backtracking wildcard matcher, uip_wildcard_match_400();
       *   3. `If ([ And ]) Or ({ And })` (45DADB-45DB42), Proc_9_4_45D940 --
       *      NewParse, the group expander, which is the tree here.
       *
       * So a '*' command is answered by the wildcard matcher, which refuses
       * some lines the tree takes (The Town of Azra `buy rawhide armor`
       * against "buy *** *rawhide armor*"), and a GROUP is expanded LAST --
       * after the whole command has been tried as a literal, brackets and
       * all, and after a '*' in it has been matched literally too.
       *
       * Measured on p4GROUP (make_groupprobe.py,
       * runner_probes/group.run400.txt and runner_probes/group.run400.b.txt,
       * 2026-09-20).  Task 1 is "zog [rock/gem]": run400 takes
       * `zog rock` and `zog gem` by step 3 AND `zog [rock/gem]` by step 1,
       * while `zog [gem/rock]`, `zog [rock/gem ]` and `zog rock/gem` are
       * refused -- equality on the raw pattern, no normalising beyond the
       * case fold and the input's own space collapse (`zog  [rock/gem]`
       * takes).  Task 3 is "* blip [red/blue] *": `xxx blip [red/blue] yyy`
       * runs it by step 2 and `xxx blip red yyy` matches nothing, so step 3
       * never expands a group in a '*' command.  run390 answers the same
       * feed identically (runner_probes/group.run390.b.txt) -- its checktask
       * has the same equality-then-checkwild shape with step 3 missing.
       *
       * Only steps 1 and 2 are done here; step 3 is the tree, which has
       * already run.  A command with a %reference% belongs to 458E6C /
       * 46918C, which substitute and then repeat these same three tests.
       *
       * One cell is knowingly left: the Runner collapses a KEYBOARD line's
       * spaces before all three tests, so `zog  [rock/gem]` runs task 1
       * there and not here.  It does not collapse a line it builds itself,
       * which is what run_rerun_skips_tasks above is for, and separating
       * the two would buy one cell that needs a typed line carrying both a
       * double space and a bracket.
       */
      if (version >= TAF_VERSION_400 && !lenient
          && (refs & ~(RUN_REF_NUMBER | RUN_REF_VARIABLE)) == 0)
        {
          if (numeric || variable)
            {
              /*
               * A 4.0 command whose only markers are numbers is substituted
               * first (Proc_19_36_45F268) and then put through the same
               * three tests, on the spelled-out command.  Step 3 is
               * uip_match(), here only as the group expander, and it is
               * reached only when step 2's `If InStr(cmd, "*") > 0` was
               * false, so a group inside a '*' command is still never
               * expanded.  humbug's task 123 is why step 3 has to stay:
               * "[push/press] {button} [%number%/%t_number%]" is a group
               * whose alternatives ARE the two markers, and `push button 2`
               * reaches NewParse spelled "[push/press] {button}
               * [2/%t_number%]".
               *
               * A marker the line did not name is left standing and the
               * tree can be trusted with it: "%t_number%" is a plain word to
               * the tokenizer, and a leftover "%number%" means the line
               * holds no digit at all -- numintext takes the leftmost one,
               * so there is no third case -- and uip_match_number() then
               * refuses wherever the pattern puts it.
               */
              std::string lowered (matched_input), literal;

              for (char &c : lowered)
                c = scr_tolower (c);
              literal = pattern;
              if (numeric)
                run_substitute_number_references (game, lowered, literal);
              run_substitute_variable_references (game, literal);

              is_matched =
                  scr_strcasecmp (literal.c_str (), matched_input) == 0
                  || (wild
                      ? uip_wildcard_match_400 (literal.c_str (),
                                                matched_input)
                      : (group && uip_match (literal.c_str (),
                                             matched_input, game)));
            }
          else if (is_matched && wild)
            is_matched = uip_wildcard_match_400 (pattern, matched_input);
          else if (!is_matched && group)
            is_matched = scr_strcasecmp (pattern, matched_input) == 0
                         || (wild && uip_wildcard_match_400 (pattern,
                                                             matched_input));
        }

      /*
       * 3.7-3.9 send a command with a '*' to checkwild instead, which
       * compares the pieces literally; see uip_wildcard_match_pre400().
       * checkwild is the whole test there and not a veto on the tree:
       * run390's checktask compares a command with no '*' for equality
       * (44B0E2) and, when InStr(cmd, "*") > 0 (44B10D), takes checkwild's
       * answer as the match flag, with nothing else consulted.  So a line
       * the tree refuses runs the task all the same, and the two ways that
       * happens are both measured on p*WILDORD (make_wildorderprobe.py,
       * runner_probes/wildord.run370.rtf, runner_probes/wildord.run380.rtf,
       * runner_probes/wildord.run390.txt, 2026-09-20):
       *
       * - Each middle piece is looked for with InStr over the WHOLE line
       *   and the line is never cut, so ORDER IS FREE.  Task "* king *
       *   rose *" runs on `blip rose blip king blip` in all three
       *   pre-4.0 Runners; 4.0's own matcher cuts, so it refuses there
       *   (runner_probes/wildord.run400.txt) and the tree's answer is
       *   right.
       * - Nothing being consumed, one occurrence satisfies a piece twice:
       *   "* zog * zog *" runs on `a zog b`, again pre-4.0 only.
       *
       * Two of checkwild's refusals matter the other way round:
       *
       * - The text after the last '*' must equal the end of the line, so a
       *   command ending in a stray space matches nothing.  Alchemist (3.90)
       *   task 114's only command taking `give rose to king` is "* rose *
       *   king ", and run390 answers the line with the library's "Rudolph
       *   II. doesn't seem interested in the rose." (runner_transcripts/
       *   alchemist.txt T300).
       * - run380/run370 never pad the line, so "throw %object% *" needs
       *   something after the object.  Marooned (3.80) T53 `throw map` at the
       *   lagoon: run380 skips task 15 ("You toss it into the water and the
       *   shark darts for it") for task 45's "throw %object%" ("You throw it
       *   and it lands in the ocean", runner_transcripts/marooned.rtf).
       *
       * Before either test, checktask puts in place of the command's
       * %object% and %character% the name the line names -- see
       * run_pre400_substitute_references(), which is the whole reason a
       * reference-bearing command reaches checkwild at all.  A task command
       * whose reference the line does not name keeps the literal
       * "%object%" and so matches nothing: p39WILDREF answers `blip zog
       * blip` against "* zog * %object% *" with "I don't understand."
       */
      /*
       * The same substitution decides a pre-3.9 command with no '*' at all,
       * and there the comparison is plain equality: checktask rewrites its
       * copy of the command and tests it against the whole line.  So the
       * Short has to be typed bare.  p37CHREF and p38CHREF answer `nurb
       * rock` with task 2's "NURBED a big rock." and `nurb a big rock` and
       * `nurb big rock` -- the rock's Prefix is "a big" -- with the object
       * catch-all, "I don't understand what you want me to do with the big
       * rock." (runner_probes/chref.run370.b.rtf,
       * runner_probes/chref.run380.b.rtf, 2026-09-20).
       * That is 3.9's and 4.0's answer too, where the strict comparator in
       * uip_compare_candidate() already refuses the prefixed forms; the
       * tolerant tree matcher took all three here because nothing below 3.90
       * had ever turned the substitution on outside checkwild.
       */
      /*
       * A GROUP is 4.0 syntax, and below 4.0 it is not syntax at all: it is
       * punctuation the command has to be typed with.  checktask holds no
       * '[', ']', '{' or '}' literal anywhere -- not in run390 (body
       * 44AA5A-44B6E6), not in run380 (43B6A3-43C51D), not in run370
       * (433227-433E4A) -- so after the substitution there is nothing but
       * the equality at 44B0E2 and, for a '*' command, checkwild at 44B139
       * to route a group to.  Measured on p*GROUP (make_groupprobe.py,
       * runner_probes/group.run370.scrollback.txt,
       * runner_probes/group.run380.scrollback.txt,
       * runner_probes/group.run390.txt, 2026-09-20): all three Runners answer
       * `zog rock` and `zog gem` against task 1's "zog [rock/gem]" with the
       * object catch-all and run the task on `zog [rock/gem]`; `nurb rock` and
       * `nurb the rock` miss "nurb {the} rock" and `nurb {the} rock` takes it;
       * `frob` and `frob up` miss "frob {up}" and `frob {up}` takes it.  So a
       * group command joins the pre-4.0 arm rather than skipping it, and it
       * joins even with no '*' in it, where the test is plain equality.
       *
       * Zero corpus exposure, measured 2026-09-20 over every .taf in
       * games/ and downloaded/ (SCR_DUMP_TASKS, 405 games that load; the 14
       * pre-4.0 stragglers whose dump never fires scanned raw with
       * taf_pattern_scan.plaintext()): 6685 task commands carry a group and
       * every one of them is in a 4.00 file.  Below 4.00 the bracketed
       * lines are all ALR keys and display text ("[month=1]", "[talk=3]").
       */
      if (version < TAF_VERSION_400 && !lenient
          && (wild
              || group
              || numeric
              || variable
              || literal_ref
              || (is_matched
                  && strstr (pattern, "%object%") != NULL)
              /*
               * A 3.9 %character% command is decided the same way, not by
               * the tree's position: checktask's NPC walk (44AD48-44ADC2)
               * stores every NPC whose Name the line contains in
               * MemVar_4681AA but Replace()s only on the first, so the
               * LOWEST-indexed Name in the line spells the command.
               * thenightmoon T50 `attack skeleton guard with longsword`
               * contains NPC 0's Name "Guard" as well as NPC 11's "Skeleton
               * guard"; the command becomes "attack Guard with longsword",
               * equals nothing, and task 5 ("As you strike %character%,
               * they realise you are not as friendly...") never runs --
               * run390 hands the line to dobattle, "You hit the skeleton
               * with your longsword." (runner_transcripts/thenightmoon.txt).
               */
              || (is_matched && version >= TAF_VERSION_390
                  && strstr (pattern, "%character%") != NULL)))
        {
          std::string literal;
          scr_int ref_object, ref_character;
          const scr_bool checkable =
              run_pre400_substitute_references (game, matched_input, pattern,
                                                literal, &ref_object,
                                                &ref_character);

          if (checkable && wild)
            is_matched = uip_wildcard_match_pre400
                (literal.c_str (), matched_input,
                 version >= TAF_VERSION_390);
          else if (checkable)
            is_matched = scr_strcasecmp (literal.c_str (), matched_input) == 0;

          /*
           * The reference the task's text expands is the walk's, not the
           * tree's: the walk reads the line, the tree reads the pattern's
           * position, and where they disagree the Runner prints the walk's.
           */
          if (is_matched && checkable)
            {
              const scr_var_setref_t vars = gs_get_vars (game);

              if (ref_object >= 0)
                {
                  gs_clear_object_references (game);
                  game->object_references[ref_object] = TRUE;
                  var_set_ref_object (vars, ref_object);
                }
              if (ref_character >= 0)
                {
                  gs_clear_npc_references (game);
                  game->npc_references[ref_character] = TRUE;
                  var_set_ref_character (vars, ref_character);
                }
            }

          /* Deliberate deviation: 3.7 keeps the substitution it just made,
             for good, when the command matched on it -- see the note above
             run_forget_game() -- and Scarier does not. */
        }

      /*
       * run390's %object% walk stores every hit in the per-turn referenced
       * object MemVar_4681A8 (44AB0D, 44AB90) before checkwild or the
       * equality test is reached, so a %object% command that then fails to
       * match still leaves its object referenced for the rest of the turn.
       * thenightmoon's task 17 is literal `behead dark elf`, but its
       * restriction "the referenced object must be held" answers "You do not
       * have dead dark elf." there (run390x
       * runner_probes/thenightmoon.run390.probe2.txt), the body having been
       * bound by a %object% command checked earlier.
       */
      if (!is_matched && version == TAF_VERSION_390 && !lenient
          && strstr (pattern, "%object%") != NULL)
        {
          std::string literal;
          scr_int ref_object, ref_character;

          if (run_pre400_substitute_references (game, matched_input, pattern,
                                                literal, &ref_object,
                                                &ref_character)
              && ref_object >= 0)
            var_set_ref_object (gs_get_vars (game), ref_object);
        }

      /* Stop searching if we find a match. */
      if (is_matched)
        {
#ifdef SCARIER_DUMP_TOOLS
          {
            static const scr_bool trace_match =
                getenv ("SCR_TRACE_MATCH") != NULL;
            if (trace_match)
              fprintf (stderr, "MATCH task=%ld pattern=[%s] input=[%s]\n",
                       task, pattern, string);
          }
          {
            /* SCR_TRACE_SCOPE: flag a turn where scope decides which
             * object a %object% task command binds.  What the audit was
             * written for is now measured and ported -- 4.0 makes two
             * passes, present-and-seen then absent-but-seen, and takes the
             * FIRST in index order of whichever pass bound (p4OBJREF,
             * runner_probes/objref.run400.txt; uip_match_entity()) -- so this
             * is no longer a divergence report but a way of finding the corpus
             * turns that exercise the rule, and of watching the older Runners,
             * which have no scope test at all and let the LAST seen namesake
             * win wherever it stands.  SCOPE-MISS = nothing that matched is
             * present, so 4.0 bound on its second pass; SCOPE-BIND = the bound
             * object is absent while a present one also matched, which below
             * 4.0 is the Runner's answer and at 4.0 should no longer
             * happen. */
            static const scr_bool trace_scope =
                getenv ("SCR_TRACE_SCOPE") != NULL;
            if (trace_scope && strstr (pattern, "%object%") != NULL)
              {
                const scr_var_setref_t vars = gs_get_vars (game);
                scr_int object, present, matched, room, bound_present;

                room = gs_playerroom (game);
                present = matched = 0;
                bound_present = FALSE;
                for (object = 0; object < gs_object_count (game); object++)
                  {
                    if (!game->object_references[object])
                      continue;
                    matched++;
                    if (obj_indirectly_in_room (game, object, room))
                      {
                        present++;
                        if (object == var_get_ref_object (vars))
                          bound_present = TRUE;
                      }
                  }
                if (matched > 0 && present == 0)
                  fprintf (stderr, "SCOPE-MISS task=%ld pattern=[%s]"
                           " input=[%s]\n", task, pattern, string);
                else if (present > 0 && !bound_present)
                  fprintf (stderr, "SCOPE-BIND task=%ld pattern=[%s]"
                           " input=[%s]\n", task, pattern, string);
              }
          }
#endif
          break;
        }
    }

  /* Return TRUE if we found a pattern match. */
  return is_matched;
}


/*
 * run_task_restriction()
 *
 * Helper for run_game_commands_common().
 *
 * Adapter for uncovering task restriction state.  An unrestricted task can
 * run unimpeded; a loudly restricted one has a fail message that indicates
 * why it fails, and if run produces that message and changes no state; a
 * silently restricted one has no such message.  Restrictions that fail to
 * parse count as loudly restricted: the task must not run, and the loud
 * arm is the one that reports it.
 */
enum run_restriction_t
{
  RUN_UNRESTRICTED, RUN_RESTRICTED_LOUDLY, RUN_RESTRICTED_SILENTLY
};

static run_restriction_t
run_task_restriction (scr_gameref_t game, scr_int task)
{
  scr_bool restrictions_passed;
  const scr_char *fail_message;

  if (!restr_eval_task_restrictions (game, task,
                                     &restrictions_passed, &fail_message))
    {
      scr_error ("run_task_restriction: restrictions error, %ld\n", task);
      return RUN_RESTRICTED_LOUDLY;
    }

  if (restrictions_passed)
    return RUN_UNRESTRICTED;
  return fail_message ? RUN_RESTRICTED_LOUDLY : RUN_RESTRICTED_SILENTLY;
}


/*
 * run_task_is_silent_and_literal()
 *
 * First half of run_game_commands_common()'s exclude_silent_literal peek (see
 * run_all_commands(); the other half is run_task_reachable_by_library_
 * callback()).  Return TRUE for a task that (a) has no CompleteText of its
 * own to print and (b) has no wildcard-leading command pattern in either
 * direction -- i.e. a task that, if matched here, would run its actions with
 * no visible sign that it ran at all, and whose every pattern is specific
 * enough that the priority-command callback (run_game_task_commands(),
 * is_library=TRUE) could in principle get a look at it first.
 *
 * Silence alone is necessary but nowhere near sufficient, and must never be
 * used on its own: "Sommeril" task 35 ("take silver orb", +5, no message) is
 * silent and literal in exactly this sense, yet the Runner runs it and then
 * lets the library's own take answer "You are already carrying the SILVER
 * ORB." -- which is why the caller also requires that no library short form
 * can reach the pattern.  Both halves are load-bearing: dropping this one
 * costs 27 corpus walkthroughs, dropping the other costs 4.
 *
 * The case that needs excluding is "Space Boy's First Adventure" task 72,
 * "drop cape to the floor" (+250, no message) -- run400 lets the library's
 * ordinary drop win outright and the task never runs at all
 * (runner_probes/et2.run400.txt, runner_probes/et4.run400.txt, 2026-08-23).
 * The case that must NOT be excluded is that same game's task 27,
 * "{take/get}{them/boots}" (CompleteText "Taken."), which must still win over
 * the library's own take (run_v4_walkthroughs.sh space_boy golden, "Taken."
 * not "You take the pair of Flight Boots.").  Task 27 is kept safe here rather
 * than by reachability: the library only ever builds its callback string from
 * the object's display Short name ("get pair of Flight Boots"), never from an
 * alias like "boots", so 27 looks unreachable too -- but it has real
 * CompleteText and so is never silent, and priority's generic take never gets
 * to steal it.
 *
 * "No visible sign that it ran" has to cover every way task_run_task_
 * unrestricted() (sctasks.cpp) can print, not just CompleteText: a
 * ShowRoomDesc auto room-description or an AdditionalMessage are both
 * static per-task properties read the same cheap way, and both print
 * unconditionally when set (see the comment on that code, topaz.taf task 22
 * and marooned.taf task 29).  Found live on "Princess in the Tower"
 * (princess1.taf): task 15, the win task for walking into the tower, has
 * empty CompleteText but a non-zero ShowRoomDesc, so the original
 * CompleteText-only version of this check misclassified it as silent,
 * excluded it from the peek, and let task 18's refusal (identical literal
 * pattern "in") win the peek instead -- breaking the golden.  Also exclude
 * any task with an Execute-Task (action type 5) or End-Game (action type 6)
 * action: task_run_task_action() (sctasks.cpp) is the only place besides
 * CompleteText/ShowRoomDesc/AdditionalMessage that can make a task's status
 * come back TRUE, and only those two action types ever set it -- an
 * End-Game action prints a score summary/ending text of its own, and an
 * Execute-Task action cascades into another task whose own output can't be
 * predicted from here.  The other action types (move object, move NPC,
 * change object status, change variable, change score, change battle
 * attribute) never do.  A task tripping either check just falls through to
 * a full, unexcluded try in the next pass, same as any wildcard-leading
 * task -- this is a conservative "don't know, so don't peek" exclusion, not
 * a correctness requirement in itself.
 */
/*
 * run_task_reachable_by_library_callback()
 *
 * Second half of the exclude_silent_literal test (see run_all_commands()).
 * Return TRUE if the library's own priority-command callback could ever put
 * this task's pattern in front of run_game_task_commands() -- that is, if
 * some object's canonical "verb OBJECT" short form (the only shape
 * lib_try_game_command_common() ever constructs: "<verb> <Prefix> <Short>"
 * and, prefix dropped, "<verb> <Short>") matches one of the task's patterns.
 *
 * This is what actually separates the two run400-probed cases the peek has
 * to tell apart, and it is a property of the *pattern*, not of the task's
 * silence.  "Space Boy's First Adventure" task 72 is "drop cape to the
 * floor": the library's short forms are "drop the cape" and "drop cape", and
 * neither matches a pattern that insists on the trailing "to the floor", so
 * the Runner's library claims the command outright and the task never runs
 * (runner_probes/et2.run400.txt, runner_probes/et4.run400.txt, 2026-08-23).
 * "Sommeril" task 35 is the bare
 * "take silver orb", which IS the library's own short form for that object,
 * so the Runner runs it (silently, +5) and the library's take then answers
 * "You are already carrying the SILVER ORB." -- the shape the golden records.
 * Excluding the latter along with the former loses the task and five points;
 * see the corpus note in run_all_commands().
 *
 * uip_match() binds game object references as it goes, and this runs at the
 * moment a real match has already bound them for the task about to run, so
 * save and restore them around the probe -- same contract
 * lib_try_game_command_common() keeps for its own speculative matches.
 */
static scr_bool
run_task_reachable_by_library_callback (scr_gameref_t game, scr_int task,
                                        const scr_char *string,
                                        scr_bool is_forwards)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, is_forwards);
  const scr_ref_entity_guard ref_entity (game);
  std::vector<scr_bool> references (game->object_references);
  const scr_char *verb;
  scr_int verb_length, object;
  scr_bool is_reachable = FALSE;

  /* Isolate the verb the player typed; no verb, nothing to construct. */
  verb = string + strspn (string, WHITESPACE);
  verb_length = strcspn (verb, WHITESPACE);
  if (verb_length == 0)
    return FALSE;

  for (object = 0; object < gs_object_count (game) && !is_reachable; object++)
    {
      scr_vartype_t vt_key[3];
      const scr_char *prefix, *name;
      std::string candidates[2];
      scr_int form;

      vt_key[0].string = "Objects";
      vt_key[1].integer = object;
      vt_key[2].string = "Prefix";
      prefix = prop_get_string (bundle, "S<-sis", vt_key);
      vt_key[2].string = "Short";
      name = prop_get_string (bundle, "S<-sis", vt_key);
      if (scr_strempty (name))
        continue;

      candidates[0] = std::string (verb, verb_length) + " " + name;
      candidates[1] = std::string (verb, verb_length) + " "
                      + (prefix ? prefix : "") + " " + name;

      for (form = 0; form < 2 && !is_reachable; form++)
        {
          scr_int command;

          for (command = 0; command < (scr_int) patterns.size (); command++)
            {
              if (uip_match (patterns[command], candidates[form].c_str (),
                             game))
                {
                  is_reachable = TRUE;
                  break;
                }
            }
        }
    }

  game->object_references = references;
  return is_reachable;
}


static scr_bool
run_task_is_silent_and_literal (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  const scr_char *completetext, *additionalmessage;
  scr_int direction, showroomdesc, action_count, action;

  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "CompleteText";
  completetext = prop_get_string (bundle, "S<-sis", vt_key);
  /* 4.0: raw, as task_run_task_unrestricted() tests it. */
  if (run_get_version (bundle) == TAF_VERSION_400
      ? completetext[0] != '\0' : !scr_strempty (completetext))
    return FALSE;

  vt_key[2].string = "ShowRoomDesc";
  showroomdesc = prop_get_integer (bundle, "I<-sis", vt_key);
  if (showroomdesc != 0)
    return FALSE;

  vt_key[2].string = "AdditionalMessage";
  additionalmessage = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (additionalmessage))
    return FALSE;

  vt_key[2].string = "Actions";
  action_count = prop_get_child_count (bundle, "I<-sis", vt_key);
  for (action = 0; action < action_count; action++)
    {
      scr_int type;

      vt_key[3].integer = action;
      vt_key[4].string = "Type";
      type = prop_get_integer (bundle, "I<-sisis", vt_key);
      if (type == 5 || type == 6)
        return FALSE;
    }

  for (direction = 0; direction < 2; direction++)
    {
      const scr_bool is_forwards = !direction;
      const std::vector<const scr_char *> &patterns =
          run_task_command_patterns (game, task, is_forwards);
      scr_int command;

      for (command = 0; command < (scr_int) patterns.size (); command++)
        {
          const scr_char *pattern = patterns[command];
          const scr_int first = strspn (pattern, WHITESPACE);

          if (pattern[first] == WILDCARD_PATTERN)
            return FALSE;
        }
    }
  return TRUE;
}


/*
 * run_game_commands_common()
 * run_game_commands_in_parser_context()
 *
 * The central handler for running, or at least trying to run, game-defined
 * tasks that have commands that match the input string.  Here's the algorithm
 * as currently understood (and it may not be right, so be warned):
 *
 *  for each task executable in the current room
 *    for direction in forwards, backwards
 *      for each command string defined by the task for this direction
 *        match against player input
 *      if any command string matched player input
 *        if task restrictions pass
 *          run the task actions in the current direction
 *          if the task actions produced output
 *            return
 *          is_matched := true
 *          break out of all loops
 *
 *  if not is_matched and we're allowing restrictions to fail tasks
 *    for each task executable in the current room
 *      for direction in forwards, backwards
 *        for each command string defined by the task for this direction
 *          match against player input
 *        if any command string matched player input
 *          if task restrictions fail with an error message
 *            run the task, to persuade it to print this error message
 *            return
 *
 * Part of the fun and games is that run_game_task_commands() is called by the
 * library to try to run "get " and "drop " game commands for standard get/drop
 * handlers and get_all/drop_all handlers.  No pressure, then.
 *
 * exclude_silent_literal makes the first (unrestricted) loop below a "peek":
 * a task that matches, would win, and is both silent (run_task_is_silent_and_
 * literal()) and unreachable from the library's own callback string
 * (run_task_reachable_by_library_callback()) does not win -- and, crucially,
 * neither does any task after it.  The whole pass is abandoned and returns
 * FALSE, because ADRIFT task precedence is "lowest matching index wins": a
 * task that matched cannot be stepped over in favour of a later one without
 * inventing a match the Runner never makes.  See run_all_commands().  This
 * has no effect on the second (loudly-restricted) loop, which is about
 * failing-restriction messages, not CompleteText.
 */
/*
 * run_task_run_speaks()
 *
 * Run a matched task and say whether the line counts as answered.  3.9 and
 * 4.0 test the turn's message buffer, not the task's own text: run390 tasks()
 * saves msg on entry and reports handled when it has changed (42BDAD), and
 * run400's dispatcher reports handled when the buffer is non-empty (44CCC0).
 * So text printed by anything the task's actions set off counts -- an event
 * an execute-task action starts prints its StartText into the same buffer
 * (evt_check_events_started_by_task()).  baroo (4.00, runner_transcripts/
 * baroo.txt T107): `close machine` runs task 113, which has no CompleteText;
 * its execute-task action starts the convertor event, and the Runner prints
 * that StartText alone, where the library close used to follow it with "The
 * machine is now closed.".
 */
static scr_bool
run_task_run_speaks (scr_gameref_t game, scr_int task, scr_bool is_forwards)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const size_t length = pf_buffer_length (filter);

  if (task_run_task (game, task, is_forwards))
    return TRUE;

  return run_get_version (gs_get_bundle (game)) >= TAF_VERSION_390
         && pf_buffer_length (filter) > length;
}

/*
 * run_restriction_cache_task_pick()
 *
 * The restriction-cache half of run400's task_pick Proc_19_66_454EF0, as the
 * dispatcher 44CCE0 calls it for the typed line (@44CBDB): tasks in index
 * order that pass the class filter, are in scope for the room and whose
 * state allows a run -- including a spent one with a RepeatText -- have
 * their restrictions walked with arg_10 = 1 (@454DDF) BEFORE the pattern
 * match, so a failing walk rewrites the task's cached results, and the
 * look-up stops at the first task that passes and matches.  Tasks after it
 * keep whatever they last cached.  See restr_cache_fallback().
 */
void
run_restriction_cache_task_pick (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int task_count = gs_task_count (game);
  scr_int task;

  for (task = 0; task < task_count; task++)
    {
      const scr_bool forwards = task_can_run_task_directional (game, task,
                                                               TRUE);
      const scr_bool reverse = task_can_run_task_directional (game, task,
                                                              FALSE);
      const scr_bool spent = task_is_done_refused (game, task)
                             && !scr_strempty (prop_get_indexed_string (
                                    bundle, "Tasks", task, "RepeatText"));
      const scr_char *fail_message;
      scr_bool pass;

      if (!run_task_passes_class_filter (game, task)
          || !task_where_allows_run (game, task))
        continue;
      if (!forwards && !reverse && !spent)
        continue;

      if (!restr_eval_task_restrictions_cached (game, task,
                                                &pass, &fail_message)
          || !pass)
        continue;

      if (((forwards || spent)
           && run_match_task_commands (game, task, string, TRUE, FALSE))
          || (reverse
              && run_match_task_commands (game, task, string, FALSE, FALSE)))
        break;
    }
}


/*
 * run_line_matches_task_strictly()
 *
 * Deliberate deviation, the gate for it (2026-09-27).  TRUE if any task the
 * player could run here has a command that matches the line the Runner's
 * way.  When none does, run_all_commands() gives the whole line lenient
 * task matching: the tolerant %object% matcher Scarier had before the
 * Runner ports -- articles, Prefixes, aliases and case forgiven, no seen
 * gate, no pre-4.0 substitute-then-compare -- a forgiven trailing space in a
 * command (uip_set_lenient_tasks()), and, in the library's retries, the
 * canonical verb and the pre-4.0 Prefix form (lib_try_game_command_common()).
 * Every line the Runner matches keeps the Runner's answer; only lines it
 * would turn away from every task are looked at again.
 *
 * The walk is a peek: the references it binds are put back, so the real
 * dispatch starts from the state it always did.
 */
scr_bool
run_line_matches_task_strictly (scr_gameref_t game, const scr_char *string)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const std::vector<scr_bool> object_references = game->object_references;
  const std::vector<scr_bool> npc_references = game->npc_references;
  const scr_int ref_object = var_get_ref_object (vars);
  const scr_int ref_character = var_get_ref_character (vars);
  const scr_int ref_number = var_get_ref_number (vars);
  const std::string ref_text (var_get_ref_text (vars));
  const scr_int task_count = gs_task_count (game);
  scr_bool matched = FALSE;
  scr_int task;

  for (task = 0; task < task_count && !matched; task++)
    {
      if (!task_where_allows_run (game, task))
        continue;
      matched = run_match_task_commands (game, task, string, TRUE, FALSE)
                || run_match_task_commands (game, task, string, FALSE, FALSE);
    }

  game->object_references = object_references;
  game->npc_references = npc_references;
  var_set_ref_object (vars, ref_object);
  var_set_ref_character (vars, ref_character);
  var_set_ref_number (vars, ref_number);
  var_set_ref_text (vars, ref_text.c_str ());
  return matched;
}


scr_bool
run_lenient_task_matching (void)
{
  return run_lenient_tasks;
}


/*
 * Set while run_takes_second_pass_370() runs the matcher a second time for
 * the same typed line, which the one-task-per-line rule below would refuse.
 */
scr_bool run_matcher_second_pass = FALSE;

scr_bool
run_game_commands_common (scr_gameref_t game, const scr_char *string,
                          scr_bool include_restrictions, scr_bool is_library,
                          scr_bool exclude_silent_literal)
{
  scr_bool is_matched = FALSE, is_handled = FALSE, is_abandoned = FALSE;
  scr_int task_count, task, direction;

  /*
   * Matching is expensive, so it helps to use a cache of results from the
   * first loop in the second.  If we're using the second, that is.  The cache
   * stays empty when restrictions are off (the second loop is then skipped).
   */
  /*
   * The Runner dispatches ONE task per typed line.  run400's dispatcher
   * Proc_19_24_44CCE0 asks the pre-matcher (Proc_19_66_454EF0) for a single
   * task, runs it, and returns "handled" only if the message buffer is
   * non-empty (44CCC0); the restriction-failure pass (Proc_19_68_45404C)
   * runs only when no task was found at all (44CCA5).  A silent task
   * therefore lets the LIBRARY run, but never a second task.  run390's
   * tasks() (42BDC4) is the same shape: checktask picks one task, execute_task
   * runs it.  Scarier reaches the same line in several passes (peek, no
   * restrictions, restrictions), and the later passes used to re-scan from
   * task 0 with the first task now spent: House (4.00, 2026-09-06,
   * runner_probes/house.run400.kiss.txt) `kiss cathy` as the first line naming
   * Cathy runs the silent once-only task 200 `*cathy*`, and run400 then prints
   * the library's "I'm not sure she would appreciate that!" -- the game's own
   * kiss task 882 `[hug/kiss/touch/shake] [her/cathy]` only runs on the SECOND
   * kiss.  Scarier ran 200 and then 882 on the first line.  Library callbacks
   * (is_library) are left alone: those model the Runner's own pre-matcher
   * look-ups from inside the library handlers, which happen after the
   * dispatcher regardless.
   */
  if (!is_library && !run_matcher_second_pass
      && run_any_task_ran_this_command ())
    return FALSE;

  task_count = gs_task_count (game);
  std::vector<scr_bool> is_matching;
  if (include_restrictions)
    is_matching.assign (task_count, FALSE);

  /*
   * Iterate over every task, ignoring those not runnable.  For each runnable
   * task, try matching task commands, and on matches, check restrictions and
   * if they pass, try running the task.
   *
   * Spent tasks (done, non-repeatable, but still in their rooms) also get
   * their forwards commands matched -- not to run them, but to seed the
   * cache for the loud restriction-failure pass below.  The Runner checks a
   * matched task's restrictions before its done state, so a spent task whose
   * restrictions fail with a message still prints that message (run400,
   * Provenance's squeeze-through-the-hole task: a second "s" at the hole
   * re-prints "The only way you are going to make it through that hole is if
   * you drop everything you are carrying.", 2026-08-22).  A spent task whose
   * restrictions PASS instead falls through to run_task_refusal(), which
   * answers with RepeatText or "You have already done that.".
   *
   * This holds on the library-callback path too, not just for plain game
   * commands: probe DONE, task `* get * gem *` with a "holding the stone"
   * restriction, run400 2026-08-23 -- once the task is spent, `get gem`
   * without the stone answers "BLOCK-GEM." instead of taking the gem, and
   * with the stone falls through to the library take ("Player take the
   * gem.").  So `is_library` does not gate the spent-task match here.
   *
   * All of that is 4.0 only.  The 3.9 twin of the probe (p39done.taf, built
   * by test/adrift4/harness/make_39_doneprobe.py) says run390 orders the two
   * tests the other way about: a spent task answers "You have already done
   * that." whether its restrictions pass or fail, and never prints a fail
   * message (`alpha` and `x book` after dropping the stone, 2026-08-23).
   * Pre-4.0 therefore leaves spent tasks out of the loud restriction pass
   * entirely, and run_task_refusal() answers them.
   */
  const scr_bool is_restriction_first =
      run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400;

  for (task = 0; task < task_count; task++)
    {
      const scr_bool is_refusing = include_restrictions
                                   && is_restriction_first
                                   && !run_task_ran_this_command (task)
                                   && task_is_done_refused (game, task);

      if (!is_refusing && !task_can_run_task (game, task))
        continue;
      if (!run_task_passes_class_filter (game, task))
        continue;

      /*
       * Try matching forwards and reverse commands.  If there's a match for
       * unrestricted tasks, run the task, and if it runs (defined as printing
       * some game output), we're done; otherwise, note the command match but
       * keep searching for other possible matches.
       */
      for (direction = 0; direction < 2; direction++)
        {
          const scr_bool is_forwards = !direction;
          const scr_bool is_runnable_directional =
              task_can_run_task_directional (game, task, is_forwards);

          if (!is_runnable_directional && !(is_refusing && is_forwards))
            continue;

          if (run_match_task_commands (game, task, string,
                                       is_forwards, is_library))
            {
              if (is_runnable_directional
                  && run_task_restriction (game, task) == RUN_UNRESTRICTED)
                {
                  /*
                   * In the peek pass, a silent, literal task does not win --
                   * but neither may any task after it.  Abandon the whole
                   * pass so that task precedence (lowest matching index wins)
                   * is preserved: priority commands get their look next, and
                   * if they don't claim the command the immediately following
                   * unexcluded pass re-matches from task 0.  Skipping just
                   * this task and reading on would hand the command to a
                   * later, lower-precedence task the Runner never reaches --
                   * "The Forum" task 1 (literal "x ... hand", silent, falls
                   * through to the library's own examine) losing to task 2
                   * ("[examine/...]{at}[%object%]", which has CompleteText of
                   * its own), and seven more corpus walkthroughs like it.
                   */
                  if (exclude_silent_literal
                      && run_task_is_silent_and_literal (game, task)
                      && !run_task_reachable_by_library_callback (
                             game, task, string, is_forwards))
                    {
                      is_abandoned = TRUE;
                      break;
                    }

                  run_note_task_ran (game, task);
                  if (run_task_run_speaks (game, task, is_forwards))
                    is_handled = TRUE;
                  is_matched = TRUE;
                  break;
                }

              if (!is_matching.empty ())
                is_matching[task] = TRUE;
            }
        }
      if (is_matched || is_abandoned)
        break;
    }

  if (is_abandoned)
    return FALSE;

  /*
   * If no match, and we've been asked to consider failing restrictions, look
   * through all of the runnable tasks again, this time searching for
   * restricted ones with a fail message.  Use the cache built above to weed
   * out matches that are certain to fail.
   */
  /*
   * 4.0 enters that fallback only when the picker found no task at all, and
   * a spent task with a RepeatText whose restrictions pass IS one it finds
   * (see run_task_refusal()), however far down the table: British Fox's
   * `attack guard` after the basement guard is down answers task 385's
   * "The guard is already unconscious.", never task 331's "The jailors are
   * not here" (runner_transcripts/britishfox.txt turns 215-256).
   */
  if (!is_handled && !is_matched && include_restrictions
      && !(is_restriction_first && run_repeat_found_400))
    {
      for (task = 0; task < task_count; task++)
        {
          scr_bool is_refusing;

          if (!is_matching[task])
            continue;

          /* Spent tasks are eligible here too (4.0); see the first loop. */
          is_refusing = is_restriction_first
                        && !run_task_ran_this_command (task)
                        && task_is_done_refused (game, task);

          /*
           * Check matches of forwards and reverse commands.  If there's a
           * match for restricted tasks (ones that have and will print a fail
           * message if we try to run them), run the task to get the print of
           * the fail message, and we're done.
           */
          for (direction = 0; direction < 2; direction++)
            {
              const scr_bool is_forwards = !direction;

              if ((task_can_run_task_directional (game, task, is_forwards)
                   || (is_refusing && is_forwards))
                  && run_match_task_commands (game, task, string,
                                              is_forwards, is_library))
                {
                  /*
                   * 4.0: the dispatcher's fallback (45404C @44CCA5) answers
                   * from the cached restriction results, printing the
                   * FailMessage of the first cached failure that still
                   * fails; see restr_cache_fallback().  Either way the task
                   * is done with once a cached failure still fails.
                   */
                  if (is_restriction_first)
                    {
                      const scr_char *fail_message;

                      if (restr_cache_fallback (game, task,
                                                &fail_message) < 0)
                        {
                          if (fail_message)
                            {
                              run_note_task_ran (game, task);
                              pf_buffer_paragraph_line (gs_get_filter (game),
                                                        fail_message);
                              is_handled = TRUE;
                            }
                        }
                      break;
                    }

                  if (run_task_restriction (game, task)
                      == RUN_RESTRICTED_LOUDLY)
                    {
                      run_note_task_ran (game, task);
                      if (task_run_task (game, task, is_forwards))
                        {
                          is_handled = TRUE;
                          break;
                        }
                    }
                }
            }
          if (is_handled)
            break;
        }
    }

  /* Return TRUE if any game task handled the command in some way. */
  return is_handled;
}

scr_bool
run_game_commands_in_parser_context (scr_gameref_t game, const scr_char *string,
                                     scr_bool include_restrictions,
                                     scr_bool exclude_silent_literal)
{
  /*
   * Try game commands, either with or without restrictions, and all full and
   * complete parse matching (no special case for game commands that begin
   * with a '*' wildcard).
   */
  return run_game_commands_common (game, string, include_restrictions, FALSE,
                                   exclude_silent_literal);
}

/*
 * run_task_class_filter, run_set_task_class_filter()
 * run_task_passes_class_filter()
 *
 * The 4.0 Runner's task pre-matcher (Proc_19_35_453C50) takes a mode byte and
 * skips every task that Proc_21_57_4494FC rejects for it: mode 1 keeps only
 * tasks whose record byte 104 is set, mode 2 only those with byte 105 set,
 * mode 3 either.  Both bytes are computed once at LOAD (mdlSpreadTheLoad
 * @4931B5 and @493225): byte 104 when any of the task's command patterns
 * contains "get", "take" or "pick" as a substring, byte 105 when one contains
 * "drop", "leave" or "put", and a pattern that is exactly "*" sets both
 * (@493281-49328D).  The mode each caller passes is fixed: the put handler's
 * implicit-take gate and the get handler's refusal exits pre-match with 1,
 * the put and drop handlers' own look-ups (name_object's "Drop what?" and
 * "It is not clear" gates, the insides handler's canonical rebuild, "drop
 * all") with 2, and the insides handler's typed-line fallback with 0, no
 * filter at all.
 *
 * The consequence is visible in House (House.taf): task 459
 * "[put/place/drop] {some} [wood] {in/into/in to} {the} [fireplace/fire
 * place]" carries the put flag only, so "put wood in fireplace" with the
 * wood on the floor sails past the mode-1 take gate into "(Taking the wood
 * first)", where the old unfiltered pre-match would have hit the task and
 * suppressed the take.  The Runner's substring test is a binary-compare
 * InStr on the stored pattern; ours is case-insensitive, which only differs
 * for an author who capitalized a verb inside a pattern.
 */
static scr_int run_task_class_filter = 0;

/* A held-object put with no in/on split admits put-family tasks only,
   whatever the caller's mode; see lib_put_held_unsplit_400(). */
scr_bool run_put_class_only = FALSE;

void
run_set_task_class_filter (scr_int mode)
{
  assert (mode >= 0 && mode <= 3);
  run_task_class_filter = mode;
}

static scr_bool
run_task_passes_class_filter (scr_gameref_t game, scr_int task)
{
  static const scr_char *const TAKE_WORDS[] = { "get", "take", "pick" };
  static const scr_char *const PUT_WORDS[] = { "drop", "leave", "put" };
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, TRUE);
  scr_bool is_take = FALSE, is_put = FALSE;
  const scr_int mode = run_task_class_filter != 0 ? run_task_class_filter
                       : run_put_class_only ? 2 : 0;

  if (mode == 0)
    return TRUE;

  for (const scr_char *pattern : patterns)
    {
      size_t index_;

      if (strcmp (pattern, "*") == 0)
        is_take = is_put = TRUE;
      for (index_ = 0; index_ < 3; index_++)
        {
          if (run_instr (pattern, TAKE_WORDS[index_]) >= 0)
            is_take = TRUE;
          if (run_instr (pattern, PUT_WORDS[index_]) >= 0)
            is_put = TRUE;
        }
    }

  switch (mode)
    {
    case 1:
      return is_take;
    case 2:
      return is_put;
    default:
      return is_take || is_put;
    }
}


/*
 * run_does_command_match()
 *
 * Non-destructive probe: return TRUE if the input string matches a command of
 * any currently runnable game task (forwards or reverse), without running it.
 *
 * This lets the front end give author-defined commands precedence over its own
 * conveniences.  In particular, the Glk port expands single letters such as
 * "c", "k" and "p" into "close", "attack" and "open"; that silently corrupts
 * games which use single letters as menu choices (battle/conversation menus,
 * e.g. attack choices in hyper_b_s.taf, or "C" in The PK Girl).  The port asks
 * here first, and skips its expansion when the game already recognises the raw
 * input.
 *
 * The string is run through the game's input synonyms before matching, exactly
 * as run_game_task_commands() does with real input.  Without that, a game that
 * routes a letter to its tasks indirectly looks unclaimed: "The Warlord, The
 * Princess & The Bulldog" maps "i" to the synonym "iii" and keys its inventory
 * task on "iii", so probing the raw "i" found nothing and the port expanded it
 * to "inventory" -- announcing "[i -> inventory]" for what the author had
 * already handled.
 *
 * Matching has the same incidental side effects as ordinary command matching
 * (it may set referenced-object/NPC/variable state via uip_match()), but this
 * is harmless: the probe runs before input is submitted, and the real command
 * pass that follows re-matches and overwrites that state.
 *
 * With check_restrictions set, the probe is run400's task pre-matcher
 * Proc_19_35_453C50 (mdlSpreadTheLoad.bas:26153) called with its second
 * argument 1, as the put handler's implicit-take gate calls it @46E2C7.  That
 * routine makes two passes over the tasks, and neither ignores restrictions:
 *
 *   1. @453B00-453C0B: a task in scope for the room whose state allows a run
 *      ((not done Or repeatable) Or (done And reversible)) is a hit only if
 *      restriction_walk Proc_19_64_455C60 PASSES and a pattern matches.
 *   2. Proc_19_68_45404C(1, mode) @453C34: a pattern-matching task in scope
 *      whose lowest failing restriction has a NON-EMPTY FailMessage
 *      (Proc_19_2_481DA0(task, i, 1) records it @481D99; the hit is the
 *      message buffer having changed, @453FA1 and @45403A), or, with no
 *      failing restriction, whose RepeatText (text index 2) is non-empty
 *      @453FE2-454028.  A failing restriction with an empty message drops
 *      the task and the walk moves on to the next one @453FC6->454034.
 *
 * Measured live 2026-09-06 on House.taf (runner_probes/house.run400.wood.txt,
 * runner_probes/house.run400.t92.txt, runner_probes/house.run400.t93.txt): at
 * the fireplace, "put wood in fireplace" with the wood on the floor matches
 * task 60 "* %object%" (a take-flagged task restricted to the house
 * spinning, FailMessage empty), and run400 still prints "(Taking the wood
 * first)" -- the restriction-blind probe used to count that task as a hit
 * and skip the take.
 */
static scr_bool
run_task_match_has_text (scr_gameref_t game, scr_int task,
                         scr_bool matched_reverse)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (matched_reverse)
    return !scr_strempty (prop_get_indexed_string (bundle, "Tasks", task,
                                                   "ReverseMessage"));
  return !scr_strempty (prop_get_indexed_string (bundle, "Tasks", task,
                                                 "CompleteText"))
         || !scr_strempty (prop_get_indexed_string (bundle, "Tasks", task,
                                                    "AdditionalMessage"))
         || (gs_task_done (game, task)
             && !prop_get_indexed_boolean (bundle, "Tasks", task, "Repeatable")
             && !scr_strempty (prop_get_indexed_string (bundle, "Tasks", task,
                                                        "RepeatText")));
}

scr_bool
run_does_command_match (scr_gameref_t game, const scr_char *string,
                        scr_bool check_restrictions, scr_int *match_kind)
{
  scr_int task_count, task, direction;

  /* The pre-matcher takes its caller's mode alone: name_object's unfiltered
     look-up still sees a non-put task on a held-object put line. */
  struct put_class_suspend
  {
    const scr_bool saved;
    put_class_suspend () : saved (run_put_class_only)
    { run_put_class_only = FALSE; }
    ~put_class_suspend () { run_put_class_only = saved; }
  } const suspend;

  /*
   * match_kind, when asked for, is the pre-matcher's own return value:
   * 1 for a hit whose matched direction has text to print, or a fallback
   * hit (a failing restriction's message, a spent task's RepeatText --
   * 453C34 stores 1 for both), and 2 for a first-pass hit on a task that
   * would run silently (453BEC-453BFE).  Callers such as the take piece
   * 46302C exit only on a 1; a 2 dispatches and carries on.  A hit on a
   * failing restriction's message is reported as 3 (the Runner's 1 still):
   * 45404C is called with arg_C=1 there and restores the message buffer, so
   * nothing prints, which the per-piece take refusal needs to know.
   */
  if (match_kind)
    *match_kind = 0;

  /* Only meaningful while a game is actually running. */
  if (!run_is_running (game))
    return FALSE;

  /*
   * Apply input synonyms, so indirection through a synonym still counts --
   * for the interface's probe of a raw typed line only.  The Runner's
   * pre-matcher 453C50 applies none: its typed line has been through the
   * synonym table already (run_player_input()), and a line the library
   * rebuilds reaches it as built.  S.E.R.E. (4.00) maps get -> take and has
   * task 40 "take *  sniper * rifle" (two spaces, so the typed line misses)
   * with the alternative "get * rifle": run400 answers `take sniper rifle`
   * in the barn with the task, which the take piece's "get the sniper rifle"
   * pre-matches (runner_transcripts/sere.txt T22).  Filtered, that line read
   * "take the sniper rifle" and the library took the rifle.
   */
  scr_owned_string filtered (check_restrictions
                             ? NULL
                             : pf_filter_input (string, gs_get_bundle (game)));
  if (filtered)
    string = scr_normalize_string (filtered.get ());

  /*
   * With restrictions checked, the Runner's two passes are separate loops:
   * 453C50 walks every task for a first-pass hit and only then calls the
   * fallback 45404C (@453C34).  A task whose failing restriction has a
   * message therefore loses to ANY later task that passes -- professor.taf's
   * `take mail` rewrites to "get mail from mailbox on-a rope", which fails
   * task 7 `get * rope` loudly but is a silent first-pass hit further down,
   * so run400 takes the mail (runner_probes/professor.run400.txt).
   */
  if (check_restrictions)
    {
      scr_int pass_number;

      task_count = gs_task_count (game);
      for (pass_number = 0; pass_number < 2; pass_number++)
        {
          for (task = 0; task < task_count; task++)
            {
              const scr_char *fail_message;
              scr_bool matched_forwards, matched_reverse, pass;

              if (!run_task_passes_class_filter (game, task)
                  || !task_where_allows_run (game, task))
                continue;

              matched_forwards = run_match_task_commands (game, task, string,
                                                          TRUE, FALSE);
              matched_reverse = !matched_forwards
                                && task_can_run_task_directional (game, task,
                                                                  FALSE)
                                && run_match_task_commands (game, task, string,
                                                            FALSE, FALSE);
              if (!matched_forwards && !matched_reverse)
                continue;

              /*
               * 4.0: the fallback 45404C reads the task's cached restriction
               * results instead of walking; see restr_cache_fallback().
               */
              if (pass_number == 1
                  && run_get_version (gs_get_bundle (game))
                     >= TAF_VERSION_400)
                {
                  if (restr_cache_fallback (game, task, &fail_message) < 0)
                    {
                      if (!fail_message)
                        continue;
                      if (match_kind)
                        *match_kind = 3;
                      return TRUE;
                    }
                  if (scr_strempty (prop_get_indexed_string (
                          gs_get_bundle (game), "Tasks", task, "RepeatText")))
                    continue;
                  if (match_kind)
                    *match_kind = 1;
                  return TRUE;
                }

              if (!restr_eval_task_restrictions (game, task,
                                                 &pass, &fail_message))
                pass = TRUE, fail_message = NULL;

              if (pass_number == 1)
                {
                  /* Fallback pass: the failing restriction has a message. */
                  if (!pass && fail_message)
                    {
                      /* 453C34's 1, told apart for the take refusal. */
                      if (match_kind)
                        *match_kind = 3;
                      return TRUE;
                    }
                  continue;
                }

              /*
               * First pass: a runnable task whose restrictions pass.  Our
               * state test also admits a spent task with a RepeatText,
               * which is the fallback pass's other hit.
               */
              if (pass && (matched_reverse
                           || task_can_run_task_directional (game, task, TRUE)))
                {
                  if (match_kind)
                    *match_kind = run_task_match_has_text (game, task,
                                                           matched_reverse)
                                  ? 1 : 2;
                  return TRUE;
                }
            }
        }
      return FALSE;
    }

  /* Iterate over every task, ignoring those not runnable. */
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      if (!run_task_passes_class_filter (game, task))
        continue;

      if (!task_can_run_task (game, task))
        continue;

      /* A match in either direction means the game claims this command. */
      for (direction = 0; direction < 2; direction++)
        {
          const scr_bool is_forwards = !direction;

          if (task_can_run_task_directional (game, task, is_forwards)
              && run_match_task_commands (game, task, string,
                                          is_forwards, FALSE))
            return TRUE;
        }
    }

  return FALSE;
}


/*
 * run_task_run_by_index()
 *
 * Run a task selected by its index rather than by matching input -- the 4.0
 * Runner's Sub_20_22.  Every one of that routine's callers goes through here:
 * an "execute task" action, an event running its TaskAffected, a walk's
 * CharTask or ObjectTask, and the battle system.
 *
 * The reason it is not simply task_run_task() is the preamble: before running
 * the task, run400 walks the task's *alternate* commands looking for a task
 * command function, and a getdynfromroom() found there sets the Referenced
 * Object for the run.  The task's primary command is not in the array it
 * scans, so a task whose only command is the function never evaluates it.
 * Verified live in run400 (RUNNER_TESTS_TODO.md section 9): wrapping each
 * probe in an "execute task" action is the only way to make its function
 * fire at all, and a probe carrying the function as its sole command stays
 * silent.
 */
scr_bool
run_task_run_by_index (scr_gameref_t game, scr_int task)
{
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, TRUE);
  scr_int command;

  /*
   * Sub_20_22 hands every one of its tasks to execute_task with mode 1
   * (run400 45FA66), so the task's CompleteText joins the turn's string
   * rather than replacing it.  RAII because task_run_task() can throw.
   */
  struct dispatch_guard
  {
    dispatch_guard () { task_push_dispatched_run (); }
    ~dispatch_guard () { task_pop_dispatched_run (); }
  } guard;

  for (command = 1; command < (scr_int) patterns.size (); command++)
    {
      if (run_is_task_function (patterns[command], game))
        break;
    }

  /* The dispatch filter's walk (45FA02) caches a failing result. */
  if (run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
      && game->is_running)
    {
      const scr_char *fail_message;
      scr_bool pass;

      restr_eval_task_restrictions_cached (game, task, &pass, &fail_message);
    }

  return task_run_task (game, task, TRUE);
}


/*
 * run_npc_walk_task()
 *
 * Run the task triggered by an NPC walk meeting a character or an object (a
 * walk's CharTask or ObjectTask).  The 3.9 Runner does not run this one task
 * by its stored index: it copies the task's command text into the input
 * global and calls the task matcher, instruction-for-instruction the same
 * dispatch its events use (Form1.characters at 0005AAD5/0005AB88 vs
 * Form1.checkevent at 00048D83, all three "copy tasks[n-1].command[0], call
 * Form1.tasks(1)"), so everything verified for event dispatch holds here
 * too: the first task in list order that matches and passes where +
 * restrictions fires, an earlier runnable `*` wildcard steals the execution
 * outright, and a restricted match is skipped silently.  Verified live in
 * run390 (test/adrift4/harness/make_39_walkprobe.py variants E/F/G): with a wildcard first
 * the arrival turn prints the wildcard's text twice and the walk task's
 * never, with the walk task first it fires itself, and a restricted walk
 * task prints nothing.  The matcher dispatch is also what fans one walk
 * trigger out across same-command tasks -- e.g. "Lair of the CyberCow" has
 * two "#lured" tasks (steeple and chapel yard) and the fairy snatches the
 * milk bowl in whichever of those rooms the player is standing.
 *
 * The 4.0 Runner instead runs the task directly by index -- its walk handler
 * calls the same direct task runner (Sub_20_22) as its events -- so there is
 * no interception, and a failing restriction prints its FailMessage (which
 * task_run_task does).  Verified live in run400 (test/adrift4/harness/make_400_walkprobe.py
 * variants E/G): the walk task fires with a wildcard listed before it, and
 * a restricted walk task prints its FailMessage on every arrival turn.
 */
static void run_task_command_dispatch (scr_gameref_t game, scr_int eventtask);


void
run_npc_walk_task (scr_gameref_t game, scr_int walktask)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (run_get_version (bundle) < TAF_VERSION_400)
    run_task_command_dispatch (game, walktask);
  else if (task_can_run_task_directional (game, walktask, TRUE))
    run_task_run_by_index (game, walktask);
}


/*
 * run_event_task()
 *
 * Run the task executed by a finishing event (its TaskAffected, with
 * "task finished" unset) in a pre-4.0 game.  The reference Runner does not
 * run this task by its stored index: it submits the task's command
 * text through the task matcher, and the first task in list order that
 * matches the text -- a `*` wildcard matches it like any other input -- and
 * whose "where" and restrictions pass is the one that fires.  So a runnable
 * wildcard task earlier in the list steals the event's execution outright
 * (its text prints instead, the affected task does not run), and the theft
 * happens even when the affected task itself could not run where the player
 * is standing.  Restricted matches are passed over silently.
 *
 * All of this is verified against the live 3.9 Runner (see
 * RUNNER_TESTS_TODO.md section 2): "thetest" depends on the stealing -- its
 * "Nice try fish face!" `*` task fires on the same turn as the library drop
 * because an always-restarting one-turn event executes a task every turn,
 * and the author's ALRs splice the two messages -- while a probe with the
 * wildcard placed after the affected task shows list order deciding the
 * winner, and a probe without any event shows no same-turn firing at all.
 *
 * The literal-text comparison below backstops the pattern matcher for the
 * customary un-typeable "#name" commands, which never survive the player
 * input path; the matcher is still consulted so wildcard patterns match.
 *
 * run_task_command_dispatch() is the dispatch itself, shared with the 3.9
 * walk CharTask/ObjectTask path above (in the 3.9 Runner both are the same
 * P-code sequence); run_event_task() is the event-facing name.
 */
/*
 * run_note_dispatched_task_ran()
 *
 * An event's or a walk's task counts as "a task ran" for the pre-4.0
 * ambiguity prompt too.  run390's execute_task sets MemVar_468198 on entry
 * (43F032), whoever called it, and events() (46067A) and the walks in
 * characters() (460675) run before the prompt's guard `(468190 < 0) Or
 * (468198 = 1)` at 4606BD.  cybercow_win T118 `x berry`, with the held
 * berry and the static one in the hair both seen: the "#Rain" event runs
 * task 18 every turn, so run390 prints no "Which berry." and answers
 * examines()'s own "Nothing special." (referencedob -1, 44BF94; the game's
 * ALR makes it "I can tell you nothing about that").
 *
 * 3.80 has the same route and 3.70 has none (measured 2026-09-20).
 * p3xEVQ2 from make_3738_eventflagprobe.py is two hats both Short "hat"
 * with the adjective as the Prefix's last word, plus an immediate event
 * that restarts every turn and runs a `zzev` task; each version is built
 * twice, once with TaskAffected 0.  run380's control answers "Which hat.
 * The red hat or the blue hat?" to all of `poke hat`, `x hat`, `take hat`
 * and `put hat`, and with the event running the task `x hat` becomes
 * "Nothing special.  EVENT TASK RAN." and `take hat` "Take what?  EVENT
 * TASK RAN." -- generaltasks calls events() 44317E before the guard 4431B0
 * `(44F124 < 0) Or (44F12C = 1)`, and checkevent 43A762 dispatches
 * tasks(CByte(1)), which sets 44F12C at 44D0BA.  run370's guard 43C8D3 is
 * `(446140 < 0)` alone and its two files are byte-identical.
 *
 * The flag alone is not enough: it lifts the prompt off whatever the
 * handlers said beneath it.  lib_disambiguate_object_common()'s every-
 * candidate-refused path supplies takes()' "Take what?" and 3.8
 * examines()' "Nothing special.", and lib_verb_object_catch_all_pre390()
 * lets co()'s crowded arm keep run380's therest silent, so `poke hat` and
 * `put hat` leave the buffer empty, never tick, and keep the prompt.
 */
static void
run_note_dispatched_task_ran (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380)
    run_co_task_claimed = TRUE;
}

static void
run_task_command_dispatch (scr_gameref_t game, scr_int eventtask)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4], vt_command;
  const scr_char *command;
  scr_int task_count, task;

  /*
   * checkevent dispatches by command text with mode 1 too (run390 42D3F5,
   * run380 43A762), so whichever task the text matches joins the turn's
   * string like Sub_20_22's.
   */
  struct dispatch_guard
  {
    dispatch_guard () { task_push_dispatched_run (); }
    ~dispatch_guard () { task_pop_dispatched_run (); }
  } guard;

  /*
   * Get the task's first command pattern; nothing to match if absent.  A
   * task can carry no command at all (Dolg's task 661 in the casino, whose
   * command list is empty), so read with prop_get() rather than the fatal
   * prop_get_string().
   */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = eventtask;
  vt_key[2].string = "Command";
  vt_key[3].integer = 0;
  command = prop_get (bundle, "S<-sisi", &vt_command, vt_key)
            ? vt_command.string : "";
  if (scr_strempty (command))
    {
      /* No command text to dispatch; run the task directly. */
      if (task_can_run_task_directional (game, eventtask, TRUE)
          && run_task_restriction (game, eventtask) == RUN_UNRESTRICTED)
        {
          run_note_dispatched_task_ran (game);
          task_run_task (game, eventtask, TRUE);
        }
      return;
    }

  /* Run the first task in list order the command text matches. */
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      scr_bool is_matched;

      if (!task_can_run_task (game, task)
          || !task_can_run_task_directional (game, task, TRUE))
        continue;

      if (task == eventtask)
        is_matched = TRUE;
      else
        {
          const scr_char *other;

          is_matched = run_match_task_commands (game, task, command,
                                                TRUE, FALSE);
          if (!is_matched)
            {
              vt_key[1].integer = task;
              other = prop_get (bundle, "S<-sisi", &vt_command, vt_key)
                      ? vt_command.string : "";
              vt_key[1].integer = eventtask;
              is_matched = !scr_strempty (other)
                           && scr_strcasecmp (command, other) == 0;
            }
        }
      if (!is_matched)
        continue;

      if (run_task_restriction (game, task) == RUN_UNRESTRICTED)
        {
#ifdef SCARIER_DUMP_TOOLS
          {
            static const scr_bool trace_evtask =
                getenv ("SCR_TRACE_EVENT_TASK") != NULL;
            if (trace_evtask && task != eventtask)
              fprintf (stderr, "EVTASK steal: task=%ld stole [%s] from"
                       " task=%ld\n", task, command, eventtask);
          }
#endif
          run_note_dispatched_task_ran (game);
          task_run_task (game, task, TRUE);
          return;
        }
    }

#ifdef SCARIER_DUMP_TOOLS
  {
    static const scr_bool trace_evtask =
        getenv ("SCR_TRACE_EVENT_TASK") != NULL;
    if (trace_evtask)
      fprintf (stderr, "EVTASK no-run: [%s] task=%ld candone=%d dir=%d\n",
               command, eventtask,
               (int) task_can_run_task (game, eventtask),
               (int) task_can_run_task_directional (game, eventtask, TRUE));
  }
#endif
}

void
run_event_task (scr_gameref_t game, scr_int eventtask)
{
  run_task_command_dispatch (game, eventtask);
}


/*
 * run_defer_loud_tasks_to_movement()
 *
 * In version 3.8 (and so also 3.7) a task whose command matches but whose
 * restrictions fail does NOT get to swallow a direction the player can
 * actually walk in: the movement happens, and the task's fail message is
 * never printed.  Verified live in run380 with "The Twilight" (2026-08-04):
 * its task 6 is "w" at the Cliff Top, restricted to Gale being present, and
 * before she joins you the Runner answers a bare "w" with "You move west."
 * and no message at all.  Under the version 4.0 ordering the message wins
 * instead, which strands the player on the very first move of that game --
 * and again later, since it also blocks the attic with "d"/"u" tasks
 * restricted to the Sentinel and the apparition being present.
 *
 * This is specific to movement.  Other standard commands still lose to the
 * message: in the same session "ask gale about mansion" printed task 4's
 * "You can't do that in your present company." even though the Runner has a
 * perfectly good answer of its own for asking an absent character (it says
 * "You can't talk to that." when no task matches at all).  So all this does
 * is let a *successful* move jump the queue.  A move that would be refused
 * changes nothing: the loud task still gets its say, and the refusal is
 * printed afterwards by run_standard_commands() if no task claims the input.
 */
scr_bool
run_defer_loud_tasks_to_movement (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (run_get_version (bundle) > TAF_VERSION_380)
    return FALSE;

  return run_movement_succeeds (game, string);
}


/*
 * run_task_has_catchall_command()
 *
 * TRUE if any of the task's forward command patterns is a bare "*".
 *
 * Such a task matches every line the player types, and the Runner treats it as
 * a deliberate catch-all rather than as something the player got into the wrong
 * room for: run390's checktask sets the room flag for an out-of-room match and
 * then walks the task's own 25 command slots, clearing it again the moment one
 * of them is exactly "*" (loc_44B684 sets it, loc_44B6BC clears it).  Only the
 * forward Command list is walked, not ReverseCommand.
 */
static scr_bool
run_task_has_catchall_command (scr_gameref_t game, scr_int task)
{
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, TRUE);

  for (const scr_char *pattern : patterns)
    {
      if (strcmp (pattern, "*") == 0)
        return TRUE;
    }
  return FALSE;
}

/*
 * Optional "repeat assist" mode (opt-in, off by default; sibling of the
 * combat and move assists).  The pre-4.0 spent-task claim below is the
 * Runner's, measured, but in a few 3.90 games it sits on the critical path:
 * a finished, non-repeatable task whose command is an exit (Vampire's T61
 * `e` out of the Bozo backyard, Merry Murders' T46 `n` into the archives)
 * answers every later use of that exit with its RepeatText, so the player
 * can never leave and the game walls (70/100, 120/135).  With the assist on
 * the claim is skipped, and the line goes on to the handlers below tasks(0)
 * -- movement, look, examine -- as it did before the claim was ported.
 * Strictly opt-in, as it deliberately diverges from run390.
 */
scr_bool run_repeat_assist = FALSE;

void
run_set_repeat_assist (scr_bool flag)
{
  run_repeat_assist = flag;
}

scr_bool
run_get_repeat_assist (void)
{
  return run_repeat_assist;
}

/*
 * run_spent_task_390()
 *
 * The pre-4.0 task dispatcher's claim, read off run390's checktask
 * (Proc_19_?_44A9EE; generaltasks calls tasks(0) at 45F48B, which calls
 * checktask(text, 1)).  The scan runs over the WHOLE task table IN INDEX
 * ORDER, and for each task walks its command slots.  A task whose command
 * matches -- a bare "*" included -- and that is done and not repeatable
 * (44B4F4, 44B4FE) writes its RepeatText slot straight into the message
 * buffer (44B537: MemVar_468154 = record(200).global_0, no test of what the
 * buffer already held) and then CONTINUES with the next task (GoTo 44B66C ->
 * 44B6CC, which sits just above the loop's Next at 44B6DA).  A live match
 * goes to the restriction loop at 44B5F4 instead: all pass and the task is
 * the scan's result (44B663), one fails and passrest has written its fail
 * message, if it has one, over whatever the buffer held (452BB8), and the
 * scan moves on (44B636).  So the buffer ends up holding the LAST message
 * written in table order, RepeatText or fail message alike, and tasks()
 * (42BDC4) either executes the passing task -- whose text replaces the
 * buffer, which is why inverness's third `knock` prints task 23's text
 * although the spent task 22 stands above it
 * (runner_probes/inverness.run390.txt) -- or, when the buffer changed and
 * nothing passed, returns -1 and generaltasks prints the buffer and skips
 * everything below tasks(0): movement, look, examine, score, the room
 * refusal and therest().
 *
 * At load, openadv substitutes person(0) & " have already done that." into
 * an empty RepeatText slot (465A8B-465AB9), so the default message and an
 * authored RepeatText are one field; an authored " " (Vampire.taf) is not
 * empty and prints as itself.  The slot is therefore never empty at 44B537,
 * and a spent match always changes the buffer.
 *
 * Measured on the Runner transcripts of The Long Journey Home (run390
 * runner_probes/journ2.run390.t5.txt: `fly`, `north`, `w`, `x card`, `x
 * king`, `e` all "You have already done that." after task 5's `*` is spent;
 * `i` still lists the inventory and `x creature` still describes the
 * creature), Lair of the CyberCow (runner_transcripts/cybercow_win.txt: the
 * second `fix robot` prints task 80's RepeatText "The invincible robot is
 * structurally complete...", not the library's "I don't think you can fix
 * the robot."), inverness (runner_probes/inverness.run390.txt: `z`, `look`,
 * `score` all claimed; the five `knock`s run tasks 20, 22, 23, 24, 25 in
 * turn), circus (runner_transcripts/circus_sold_points.txt: `ask barb about
 * tape` claimed by task 77, and the NPC walk ticks, so the claim is a turn)
 * and chicago's `listen` (task 18).  4.0 moved the claim
 * to 48A481 and asks the restrictions there; see run_task_refusal() and
 * run_repeat_survivor_400().
 *
 * Returns the claiming spent task, or -1 when no spent task matches or a
 * live matching task passes its restrictions (that task runs instead).
 * When a task is returned, *message is what the buffer holds at the end of
 * the scan: the last RepeatText or restriction fail message written, never
 * NULL.  Prints nothing.
 */
scr_int
run_spent_task_390 (scr_gameref_t game, const scr_char *string,
                    const scr_char **message)
{
  scr_int task_count, task, spent;
  const scr_char *buffer;

  *message = NULL;
  if (scr_strempty (string))
    return -1;

  spent = -1;
  buffer = NULL;
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      scr_bool pass;
      const scr_char *fail_message;

      /*
       * run390's reverse pass (44B1B9-44B4D7), skipped when a forward
       * command matched a task that is not done (44B18F).  A reverse command
       * of a done or repeatable task is checktask's -2 in the task's rooms
       * (44B4A4 / 44B64D) -- the reversal runs, and the task passes below
       * handle it -- and nothing elsewhere.  A reverse command of any other
       * task writes its RepeatText into an EMPTY buffer (44B4B2), with no
       * room test: matt's `out` is "You have already done that." everywhere
       * until the boss room is entered (probe p39REV, run390x
       * runner_probes/rev.run390.txt, 2026-09-24).
       */
      if (!buffer
          && task_is_reverse_refused_390 (game, task)
          && !run_match_task_commands (game, task, string, TRUE, FALSE)
          && run_match_task_commands (game, task, string, FALSE, FALSE))
        {
          buffer = prop_get_indexed_string (gs_get_bundle (game), "Tasks",
                                            task, "RepeatText");
          spent = task;
          continue;
        }

      if (!task_where_allows_run (game, task))
        continue;

      if (task_can_run_task_directional (game, task, FALSE)
          && run_match_task_commands (game, task, string, FALSE, FALSE))
        return -1;

      if (!run_match_task_commands (game, task, string, TRUE, FALSE))
        continue;

      if (task_is_done_refused (game, task))
        {
          const scr_prop_setref_t bundle = gs_get_bundle (game);

          buffer = prop_get_indexed_string (bundle, "Tasks", task,
                                            "RepeatText");
          spent = task;
          continue;
        }

      if (!task_can_run_task_directional (game, task, TRUE))
        continue;

      /* A live task with passing restrictions executes instead. */
      if (!restr_eval_task_restrictions (game, task, &pass, &fail_message))
        pass = TRUE, fail_message = NULL;
      if (pass)
        return -1;
      if (fail_message && !scr_strempty (fail_message))
        buffer = fail_message;
    }

  if (spent >= 0)
    *message = buffer;
  return spent;
}

/*
 * run_spent_survivor_390()
 *
 * The handlers run390's generaltasks runs ABOVE tasks(0), each of which ends
 * the line when it printed anything (GoTo loc_460589 on a nonzero result):
 * takes() 45F439, drops() 45F44A, inventory() 45F45B and insides() 45F471.
 * takes() answers only for an object co() resolved -- "Take what?" is a
 * later fallback -- so `take shovel` with no shovel about falls to the
 * spent task (journ2).  A put that the target turns away is still
 * insides()'s answer, so the tentative priority pass's deferral is finished
 * here out of the standard rows.
 *
 * Below tasks(0) only the character pass survives, because characters()
 * (45ACD8, called at 460675 on every line) OVERWRITES the message buffer:
 * `x creature` after the journ2 claim prints the creature's description.
 * The other character answers are not measured to survive -- circus's `ask
 * barb about tape` does not -- so only the NPC examine is taken here.
 *
 * TRUE if a survivor answered the line.
 */
scr_bool
run_spent_survivor_390 (scr_gameref_t game, const scr_char *string)
{
  static scr_commands_t NPC_EXAMINE_COMMANDS[] = {
#ifdef SCARIER_NO_ABBREVIATIONS
    {"[ex/exam/examine/look {at}] %character%", lib_cmd_examine_npc},
#else
    {"[x/ex/exam/examine/look {at}] %character%", lib_cmd_examine_npc},
#endif
    {NULL, NULL}
  };

  if (run_priority_commands (game, string))
    return TRUE;
  if (run_priority_deferred && run_standard_commands (game, string))
    return TRUE;

  return run_try_command_table (NPC_EXAMINE_COMMANDS, game, string);
}

/*
 * run_spent_claim_390()
 *
 * Print the spent claim: the buffer run_spent_task_390() ended with, which
 * for a RepeatText the author left empty is the already-done default that
 * openadv installs at load (see run_spent_task_390()).
 */
void
run_spent_claim_390 (scr_gameref_t game, const scr_char *message)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);

  if (message[0] != NUL)
    {
      pf_buffer_paragraph_line (filter, message);
      return;
    }

  pf_buffer_string (filter,
                    prop_get_global_integer (bundle, "Perspective")
                    == LIB_FIRST_PERSON ? "I" : "You");
  pf_buffer_paragraph_line (filter, " have already done that.");
}

/*
 * run_task_refusal()
 *
 * The Runners have two answers of their own for a command that matches a task
 * the main dispatcher would not run, both of which Scarier used to leave to the
 * standard library or to the game's DontUnderstand text:
 *
 *   - the player is in the wrong room for the task.  Pre-4.0 only: 3.7 and 3.8
 *     answer "You can't do that here.", 3.9 "You can't do that here!", and 4.0
 *     dropped the message (the string is still in run400.exe, unused).
 *   - the task has already been done and is not repeatable.  The Runner prints
 *     the task's RepeatText if it has one -- in EVERY version, 4.0 included --
 *     and otherwise "You have already done that.", which like the room refusal
 *     is a pre-4.0 message only (` have already done that.` is a UTF-16 literal
 *     in run370/run380/run390 and absent from run400).
 *
 * Measured live against run370 (Castle Quest), run380 (Marooned), run390 (The
 * Hangover plus the p39where probe built by test/adrift4/harness/
 * make_39_whereprobe.py) and run400 (make_400_whereprobe.py), 2026-08-09/10.
 *
 * The conditions are narrow, and the probes say how narrow.  A task refused by
 * a *restriction* that fails silently gets "I don't understand.", as does a
 * command that matches no task pattern at all.  Runner P-code guards the room
 * message with `OUT = "" And FLAG = 1`, so any output at all -- including a
 * standard library answer -- suppresses it, which is why this runs last, only
 * when nothing else claimed the input.
 *
 * The two refusals are ordered, and the order is measured rather than assumed:
 * probe task "theta" is done AND out of its room, and run390 answers "You can't
 * do that here!", so the room half is tested first.  That is why the
 * already-done test carries the room condition with it (task_is_done_refused()).
 *
 * Within one command the Runner scans every task and does not stop at the first
 * refusable one.  The already-done half writes its message as it goes, so the
 * first such task wins and ends the scan; the room half only raises a flag, and
 * each later out-of-room match overwrites it -- so it is the LAST matching
 * out-of-room task that decides, and a catch-all "*" task clears the flag for
 * good (run_task_has_catchall_command()).  Melbourne Beach (3.90) is the
 * measured case: its task 94 is `*` confined to room 0, so run390 answers "I
 * don't understand what you mean!" and not "You can't do that here!" for `play
 * volleyball` and `use shower` typed outside their rooms
 * (runner_probes/melbourne_beach.run390.txt).
 *
 * Both count as a turn, unlike DontUnderstand: in the probe, an event ticking
 * once a turn fires on either refusal and not on the parser complaint, matching
 * the `handled = 1` the Runner sets alongside the message.  Hence the TRUE
 * return, which lets the caller run the turn.  (Measured for all three pre-4.0
 * answers, and for 4.0's RepeatText on 2026-09-08: p4REPEAT2/p4REPEAT3 carry a
 * once-a-turn event and every RepeatText line in
 * runner_probes/repeat2.run400.txt and runner_probes/repeat3.run400.txt is
 * followed by its "TICK.".)
 *
 * The leading word follows Perspective, which pre-4.0 has only two of: run390
 * answers "I can't do that here!" / "I have already done that." for Perspective
 * 0 and the "You" forms for every other value, third person being a 4.0
 * addition (its inventory says "You are carrying nothing." for Perspective 2 as
 * well).
 */
enum { REFUSAL_NONE = 0, REFUSAL_ROOM, REFUSAL_DONE };

/*
 * Which of the four calls a scan is: the pass ahead of the standard library
 * (which answers the already-done half), the pass INSIDE it that answers the
 * room half, the pass after all of it, and a silent look-ahead that only
 * reports whether the pre-library pass has something to say.
 * run_all_commands() needs that last answer before it runs the priority
 * commands, because 4.0's RepeatText outranks those as well.
 */

/*
 * The two halves do NOT sit at the same point in the dispatch order, and
 * chicago.taf (3.90) is the measurement that separates them.  Its task 18 is
 * `listen`, confined to rooms 2 and 8 and not repeatable, and the walkthrough
 * types `listen` twice inside those rooms.  The second time run390 answers
 * "You have already done that." -- not the library's "You hear nothing out of
 * the ordinary.", which run390 certainly has (` hear nothing out of the
 * ordinary.` is a UTF-16 literal in all four Runners, so this is not a
 * vocabulary difference).  The already-done message therefore beats the
 * standard library, exactly as the P-code shape says: the done half writes its
 * message during the task scan, while the room half only raises a flag that
 * `OUT = "" And FLAG = 1` later discards if anything else printed.
 *
 * So the done half runs BEFORE run_standard_commands() and the room half
 * later, and `done_only` / `room_only` select which.  The scan itself is
 * identical in every pass, because the room half still has to be tested first
 * WITHIN a pass -- probe task "theta" is done AND out of its room and run390
 * answers the room refusal.  The pre-library pass simply declines to emit a
 * room refusal and leaves it to the ones below.
 *
 * How much later the room half runs is measured, and it is NOT after the whole
 * library.  run390's generaltasks() (Public Sub generaltasks '460D6C, body from
 * loc_45EC34) clears the flag at 45EC7C, runs its() then tasks() (which is what
 * sets the flag, in checktask at loc_44B681), then its named per-verb handlers
 * -- wears() removes() dobattle() dohints() sitstand() openclose() viewroom()
 * the take code, adventure_Click(), whereis(), fonts(), gotoplace() -- and only
 * THEN reaches
 *
 *     loc_45FFE8:  If msg = "" And MemVar_468228 = 1 Then
 *                      msg = person(0) & " can't do that here!"
 *     loc_460004:  If msg = "" Then Call therest()
 *
 * therest() is the generic catch-all bucket: "You can't <verb> that.", "Give
 * what?", " is for sale.", the "Uh huh, yes, very interesting." of `say`.  All
 * of those lose to the room refusal, because the flag is tested one line above
 * the call.  Scarier's analogue of therest() is STANDARD_FALLBACK_COMMANDS, so
 * the room pass sits between run_standard_verb_commands() and
 * run_standard_fallback_commands(), not after both.
 *
 * Measured on ALEXIS.TAF under run390
 * (runner_probes/alexis_worn_cube.run390.txt): `turn
 * ring`, `buy metal helmet`, `open cupboard`, `open door`, `unlock door`, `give
 * stones to larnt` and `say the password` are all answered "You can't do that
 * here!" where Scarier reached "You can't turn that.", "I don't think that is
 * for sale.", "You can't open that.", "You can't unlock that." and "Uh huh,
 * yes, very interesting." -- eight turns on that row alone, every one of them a
 * STANDARD_FALLBACK_COMMANDS row.  The handlers that run ABOVE loc_45FFE8 keep
 * their answers, which is why the post-library pass has to stay: `put water in
 * pan` on the same row is run390's "You can't do that!" from a handler, not the
 * room refusal.
 *
 * What is NOT above the flag test, contrary to how generaltasks reads at a
 * glance, is the character catch-all.  Both `Call characters()` sites in
 * generaltasks (45FD08 and 460675) are the turn-advance pair `characters() :
 * events()` -- the NPC walk and the event tick -- and the second of them is
 * BELOW `Call therest()` at 460004; the "I don't understand what you want to
 * do with <Name>." / "<Name> is not here!" / "Who?" tail rides along there.
 * The object catch-all is below it too (45D35C inside therest(), 46024A in
 * generaltasks' own tail).  So both stay at the foot of the fallback table,
 * and the golden that pins it is the_hangover (3.90): `give approval notes to
 * platypus` with the platypus elsewhere answers "You can't do that here!" and
 * not "Platypus is not here!" (runner_transcripts/the_hangover.txt line
 * 215).  Moving the character row above the refusal also cost goldilocks
 * and yak_shaving (both 4.00) their `give X to Y` -> "Give what?", which is
 * the same ordering seen from 4.0's side.
 *
 * 4.0 has no already-done message at all, only RepeatText, and it sits ahead
 * of the library too -- ahead of MORE of it, in fact.  Measured 2026-09-08
 * with the p4REPEAT probes: the dispatcher prints the RepeatText and jumps
 * past every general verb, movement and take included, so the 4.0 pre-library
 * pass carries none of the three pre-4.0 conditions.  Only the handlers that
 * run before the dispatcher, or below the jump, keep their answer; they are
 * listed in run_repeat_survivor_400(), and the caller checks them.
 */
scr_bool
run_task_refusal (scr_gameref_t game, const scr_char *string,
                  run_refusal_pass_t pass)
{
  const scr_bool done_only =
      pass == REFUSAL_PASS_PRE || pass == REFUSAL_PASS_PROBE;
  const scr_bool room_only = pass == REFUSAL_PASS_MID;

  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int version, perspective, task_count, task, direction;
  scr_int refusal, refused_task;
  scr_bool is_room_refused;
  const scr_char *repeattext;

  /*
   * An empty input line element is not a command and gets no complaint of any
   * kind -- the same guard the DontUnderstand fallback uses.  Without it a
   * game with a bare "*" task command outside the player's room turns every
   * press-a-key blank line into a refusal.
   */
  if (scr_strempty (string))
    return FALSE;

  /*
   * A line that has already run a task gets no refusal.  In the Runner the
   * RepeatText is not a late pass at all: it is printed inside the task
   * dispatcher (Proc_19_24_44CCE0, called once at 48A481), which asks the
   * pre-matcher Proc_19_66_454EF0 for exactly ONE task and then either runs
   * it, reverses it, or prints its RepeatText.  So the task that refuses and
   * the task that runs are always the same one, and a line that ran a task
   * can never also be refused -- the restriction-failure pass at 44CCA5 is
   * likewise entered only when no task was found.
   *
   * Measured on easter.taf (run400 runner_probes/easter.run400.txt:304-308):
   * the winning `show basket to shopkeeper` runs task 63, which is silent
   * and only ends the game, and run400 prints nothing before the WinText --
   * not the "Since you already have Max's list..." RepeatText that a scan
   * over the other matching tasks turns up here.
   */
  if (run_any_task_ran_this_command ())
    return FALSE;

  version = run_get_version (bundle);

  /*
   * Walk every task, looking for ones whose command matches the input and that
   * the dispatcher passed over for one of the two refusable reasons.  A task
   * blocked by anything else can never raise a refusal.  The already-done half
   * stops the scan where it fires; the room half keeps going, last one wins.
   */
  refusal = REFUSAL_NONE;
  refused_task = -1;
  is_room_refused = FALSE;
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      /* The room half first -- see the note above on probe task "theta". */
      if (version < TAF_VERSION_400)
        {
          for (direction = 0; direction < 2; direction++)
            {
              const scr_bool is_forwards = !direction;

              /*
               * run390 raises the flag for a forward match only (44B66F);
               * an out-of-room reverse command is not understood (probe
               * p39REV `unpoke`).
               */
              if (!is_forwards && version == TAF_VERSION_390)
                continue;

              if (task_is_room_refused (game, task, is_forwards)
                  && run_match_task_commands (game, task, string,
                                              is_forwards, FALSE))
                {
                  is_room_refused =
                      !run_task_has_catchall_command (game, task);
                  break;
                }
            }
        }

      /*
       * A task the current command has just completed is not "already done"
       * for that command -- p39done.taf's silent `* x * scroll *` task runs,
       * prints nothing, and run390 then answers "I don't understand." rather
       * than "You have already done that." (2026-08-23).
       */
      /*
       * 4.0 also asks the restrictions.  A spent task whose restrictions now
       * fail is not the one the picker hands the dispatcher, and the library
       * gets the line as if the task were not there at all: `A Witch Tale`
       * task 2 is the literal `* north` at the bridge, spent from the first
       * crossing attempt and carrying a RepeatText, but restricted on `say
       * grue` being undone -- once the riddle is answered run400 walks the
       * player north (runner_transcripts/witchtale.txt) instead of printing "I
       * try to cross the bridge...".  Only silent failures are covered here; a
       * fail message of its own is the restriction pass's business, not this
       * one's.  Pre-4.0 keeps the shape chicago.taf measured.  At 4.0 the
       * post-library pass asks the restrictions too: it once let a spent,
       * restriction-failing task through as a fallback, for `The Magic Show`'s
       * "One rabbit trick is enough for any given act" on `show rabbit to
       * audience`, but run400 answers that line with the object catch-all "I
       * don't understand what you want to do with the audience."
       * (runner_probes/magicshow.run400.a.txt:47,
       * runner_probes/magicshow.run400.b.txt:40), and hcw's literal `2` (task
       * 240, spent, both restrictions failing) with the DontUnderstand text
       * (runner_probes/hcw.run400.txt, turn 227), not either RepeatText.
       */
      /*
       * The room-half pass does not look at the done half at all: the done
       * half breaks out of the scan where it fires, and a task refused as
       * done further up the table would then hide a later out-of-room match
       * from the "last one wins" rule.
       */
      /*
       * 4.0 also passes over a spent task with no RepeatText of its own: every
       * 4.0 answer below needs one, and the picker offers the dispatcher the
       * spent task that has it.  The Crooked Estate's wallpaper is tasks 47
       * (spent, silent) and 48 (spent, RepeatText) on the same commands, and
       * the third `peel wallpaper` gets 48's "I rip another, but another layer
       * hides behind that." (runner_transcripts/crookedestate.txt), not the
       * object catch-all.
       */
      if (!room_only
          && !run_task_ran_this_command (task)
          && task_is_done_refused (game, task)
          && (version < TAF_VERSION_400
              || (run_task_restriction (game, task) == RUN_UNRESTRICTED
                  && !scr_strempty (prop_get_indexed_string
                                      (bundle, "Tasks", task, "RepeatText"))))
          && run_match_task_commands (game, task, string, TRUE, FALSE))
        {
          refusal = REFUSAL_DONE;
          refused_task = task;
          break;
        }
    }
  if (refusal == REFUSAL_NONE && is_room_refused)
    refusal = REFUSAL_ROOM;
  if (refusal == REFUSAL_NONE)
    return FALSE;

  /*
   * The mid-library pass answers the room half and nothing else -- it stands
   * where run390 tests the flag, one line above Call therest(), so it outranks
   * the fallback verbs and nothing more.  The already-done half is not its
   * business: the pre-library pass has already had its go at that, and the
   * post-library pass keeps the wider fallback shape chicago.taf measured.
   */
  if (room_only && refusal != REFUSAL_ROOM)
    return FALSE;

  /*
   * The pre-library pass answers the already-done half and nothing else; a
   * room refusal found here waits for the mid-library pass, where the
   * Runner's own "did anything print?" guard applies to it.
   */
  if (done_only)
    {
      const scr_char *repeat;

      /*
       * Only the DONE half is answered early: the room half is guarded by
       * the Runner's own "did anything print?" test and stays late.
       *
       * Pre-4.0 this pass is a fallback now.  The claim itself is made in
       * run_all_commands() before any handler runs, by run_spent_task_390(),
       * which is where run390 makes it; what reaches here is a line that
       * probe declined because a live task earlier in the table outranked
       * the spent one, and that live task then did not run after all.
       */
      if (refusal != REFUSAL_DONE)
        return FALSE;

      repeat = prop_get_indexed_string (bundle, "Tasks", refused_task,
                                        "RepeatText");

      /*
       * 4.0 has no default already-done message and none of those three
       * conditions: measured 2026-09-08 (p4REPEAT/p4REPEAT2/p4REPEAT3 under
       * run400, runner_probes/repeat.run400.txt, repeat2.run400.txt and
       * repeat3.run400.txt), an authored RepeatText is printed by the task
       * dispatcher itself, ahead of nearly the whole library, and takes the
       * line away from a wildcard command and from movement just as readily
       * as from a literal one.  What it does NOT take
       * is listed in run_repeat_survivor_400(), which the caller tests -- the
       * answer is needed before the priority commands run, so a probe pass
       * reports it without printing anything.
       */
      if (version >= TAF_VERSION_400)
        {
          if (scr_strempty (repeat))
            return FALSE;
          if (pass == REFUSAL_PASS_PROBE)
            return TRUE;
        }
    }

  /*
   * An authored RepeatText replaces the already-done message, and is the one
   * part of all this that 4.0 kept.
   */
  repeattext = NULL;
  if (refusal == REFUSAL_DONE)
    {
      repeattext = prop_get_indexed_string (bundle, "Tasks", refused_task,
                                            "RepeatText");
      if (scr_strempty (repeattext))
        {
          repeattext = NULL;
          if (version >= TAF_VERSION_400)
            return FALSE;
        }
    }

  if (repeattext)
    {
      pf_buffer_paragraph_line (filter, repeattext);
      return TRUE;
    }

  perspective = prop_get_global_integer (bundle, "Perspective");

  pf_buffer_string (filter,
                    perspective == LIB_FIRST_PERSON ? "I" : "You");
  if (refusal == REFUSAL_ROOM)
    {
      pf_buffer_paragraph_line (filter,
                                version < TAF_VERSION_390
                                ? " can't do that here."
                                : " can't do that here!");
    }
  else
    pf_buffer_paragraph_line (filter, " have already done that.");
  return TRUE;
}


/*
 * run_put_take_400()
 *
 * A 4.0 line holding a clauseless put and a take.  put_drop_list is the
 * first handler generaltasks calls, and its clauseless branch (46DC34)
 * writes "Where do you want to put <X>?" into the message buffer and falls
 * out without claiming, so get_outer (4582D8) has the line next.  A take
 * that succeeds saves the buffer (var_B0, 473597), composes its own line
 * and appends the saved text after the 44A9F4 separator (4736CD): `take put
 * coin` is "You take the coin. Where do you want to put the coin?".  The
 * already-carrying refusal appends to the buffer as it stands (462D4E):
 * `put take coin` with the coin held is "Where do you want to put the
 * coin?You are already carrying the coin.".  p4ORD (make_orderprobe.py)
 * cells 62 and 69 (run400x runner_probes/ord.run400.rest.txt, 2026-09-21).  A
 * put beside examine or drop already answers as the Runner does; any other
 * take outcome is not measured, and the line goes on as it did.
 */
scr_bool
run_put_take_400 (scr_gameref_t game, const scr_char *string)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const size_t mark = pf_buffer_length (filter);
  std::vector<scr_int> places;
  std::string question, take_line;
  scr_bool moved = FALSE;
  scr_int object;

  if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
      || !lib_input_contains_word (string, "put")
      || !(lib_input_contains_word (string, "take")
           || lib_input_contains_word (string, "get")
           || lib_input_contains_word (string, "pick"))
      || lib_input_contains_word (string, "all")
      || lib_input_contains_word (string, "and")
      || strstr (string, "take off"))
    return FALSE;

  run_dispatch_input = string;
  if (!lib_put_where_question_400 (game, &question))
    return FALSE;

  /* get_outer's own line: the put word plays no part in its object walk. */
  for (const scr_char *scan = string; *scan != NUL; )
    {
      if ((scan == string || scan[-1] == ' ')
          && scr_strncasecmp (scan, "put", 3) == 0
          && (scan[3] == NUL || scan[3] == ' '))
        {
          scan += 3;
          scan += strspn (scan, " ");
          continue;
        }
      take_line.push_back (*scan++);
    }
  while (!take_line.empty () && take_line[take_line.size () - 1] == ' ')
    take_line.erase (take_line.size () - 1);
  std::string hoisted;
  if (run_hoist_verb_line (game, take_line.c_str (), hoisted))
    take_line = hoisted;

  for (object = 0; object < gs_object_count (game); object++)
    {
      places.push_back (gs_object_position (game, object));
      places.push_back (gs_object_parent (game, object));
    }
  scr_bool status;
  {
    const run_dispatch_input_guard input (take_line.c_str ());

    status = run_priority_commands (game, take_line.c_str ());
  }
  if (!status)
    {
      pf_truncate (filter, mark);
      return FALSE;
    }
  for (object = 0; object < gs_object_count (game); object++)
    if (gs_object_position (game, object) != places[2 * object]
        || gs_object_parent (game, object) != places[2 * object + 1])
      moved = TRUE;

  std::string said = pf_cut_tail (filter, mark);
  if (moved)
    {
      while (!said.empty () && said[said.size () - 1] == '\n')
        said.erase (said.size () - 1);
      pf_buffer_string (filter, said.c_str ());
      pf_buffer_string (filter, " ");
      pf_buffer_string (filter, question.c_str ());
      pf_buffer_string (filter, "\n");
    }
  else if (said.find ("already carrying") != std::string::npos)
    {
      pf_buffer_string (filter, question.c_str ());
      pf_buffer_string (filter, said.c_str ());
    }
  else
    pf_buffer_string (filter, said.c_str ());
  return TRUE;
}


/*
 * run_takes_second_pass_370()
 *
 * run370 runs the task matcher TWICE on a take line that names an object.
 * takes() hands the line to tasks(1) itself (see lib_takes_offers_tasks_370())
 * and returns Empty, so generaltasks falls through to tasks(0) (43B972) and
 * matches the same line again against the world the first task left behind.
 * The two modes differ in what they do to the turn's string (00041B90): mode
 * 1 appends the CompleteText, mode 0 REPLACES the string with it.  What the
 * string holds by then is whatever was not printed yet -- a ShowRoomDesc's
 * viewroom prints everything before its exits sentence there and then
 * (@0003315C, pf_print_so_far()), which leaves only the exits in it; the
 * filter's pf_printed_to() note marks where they start.
 *
 * arlo (alices_restaurant) `get out of bus` at the church: the first pass
 * runs task 72 (`get out of *bus*`, Where the bus, Repeatable), which says
 * "You're on foot.", shows room 0 and moves the player there; the second
 * finds task 72's Where failing and task 107 -- the same five patterns,
 * Where room 0 -- matching instead, and its "You are no longer in the bus."
 * overwrites the exits.  So the Runner prints "... There is a mailbox here.
 * You are no longer in the bus." with no exits sentence, on both of the
 * walkthrough's visits (runner_probes/alices_restaurant.run370.rtf,
 * runner_transcripts/alices_restaurant).
 * Where nothing matches the second time -- `get out of bus` at the Dump,
 * `take garbage out of bus` -- the first task's text stands.
 *
 * Only the pass after a task ran is modelled.  A take line the library
 * answers also reaches tasks(1) and tasks(0) after the take, and the all and
 * and arms offer "get <Short>" per object; nothing measured tells those
 * apart from one pass yet.
 */
void
run_takes_second_pass_370 (scr_gameref_t game, const scr_char *string,
                           const scr_char *task_string, size_t task_mark)
{
  const scr_filterref_t filter = gs_get_filter (game);
  size_t clobber, mark;
  scr_int noted, printed;

  if (!lib_takes_offers_tasks_370 (game, string))
    return;

  printed = pf_printed_to (filter);
  clobber = std::max (task_mark, printed < 0 ? (size_t) 0 : (size_t) printed);
  mark = pf_buffer_length (filter);
  noted = run_task_runs_noted;

  run_matcher_second_pass = TRUE;
  run_game_commands_in_parser_context (game, task_string, FALSE, TRUE);
  run_matcher_second_pass = FALSE;

  if (run_task_runs_noted != noted)
    pf_erase (filter, clobber, mark);
}

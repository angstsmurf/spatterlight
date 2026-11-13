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
 * Noun phrases: the articles and names of objects and NPCs.
 */

#include "sclibrar.h"


/*
 * lib_definite_object_name()
 * lib_print_object_np
 * lib_print_object
 *
 * Convenience functions to print out an object's name, with a "normalized"
 * prefix -- any "a"/"an"/"some" is replaced by "the" -- and with the full
 * prefix.  lib_definite_object_name builds the normalized form as a string,
 * for %theobject% too (scvars.cpp): run400 expands that marker through the
 * same name builder in the same normalizing mode, Proc_21_31_448710 mode 0
 * called from Proc_21_15_4619CC.
 *
 * Pre-3.9 normalizes far less, and the branch below spells out exactly what
 * Form1.tense() does instead.  "some" is the case that was measured first:
 * it joined the normalized list in 3.9, and 3.7 and 3.8 leave a "some" prefix
 * exactly as the author wrote it.  Measured live on one game carried across
 * three .taf versions by the ADRIFT generators, so the data is provably
 * identical in all three: microwaveman.taf (3.8) upconverted with gen390 and
 * gen400, `take clothes` on an object whose Prefix is "some" -- run380 "You
 * pick up some aluminum clothes.", run390 "You pick up the aluminum clothes.",
 * run400 "You take the aluminum clothes."  3.7 answers like 3.8: a copy of
 * arlo.taf with the bone's "a" prefix rewritten to "some" gives "You pick up
 * some bone." in run370.  See the standing divergence table in
 * test/adrift4/notes/RUNNER_TESTS.md.
 */
/*
 * lib_compare_article()
 *
 * scr_compare_word() for the article normalizer, but case-SENSITIVE.
 *
 * Form1.tense() is VB6 string equality and Left$() comparison under the
 * default Option Compare Binary, so an author who capitalised the article
 * gets it back untouched -- exactly as an authored "The" already does, there
 * being no "the" test at all.  Measured on probe PFX (run400,
 * runner_probes/pfx.run400.tense.txt), eleven objects one per spelling:
 *
 *     take alpha    (Prefix "a")      Player take the alpha.
 *     take bravo    (Prefix "A")      Player take A bravo.
 *     take charlie  (Prefix "an")     Player take the charlie.
 *     take delta    (Prefix "An")     Player take An delta.
 *     take echo     (Prefix "some")   Player take the echo.
 *     take foxtrot  (Prefix "Some")   Player take Some foxtrot.
 *     take golf     (Prefix "a big")  Player take the big golf.
 *     take hotel    (Prefix "A big")  Player take A big hotel.
 *     take india    (Prefix "SOME")   Player take SOME india.
 *     take juliet   (Prefix "the")    Player take the juliet.
 *
 * The same run's `i` and room listing print every prefix verbatim, so only
 * the definite form was ever folding case.  The live game that turned this
 * up is The X-Files: A New Beginning, whose Stool carries the Prefix "A":
 * run400 answers `u` from it with "(Getting off A Stool first)".
 *
 * The pre-3.9 branch below already compares with strcmp/strncmp, and run370
 * and run380 are the same VB, so this makes all four Runners agree.
 *
 * Scarier deliberately does not follow the Runner here (deviation policy):
 * an author's capital article mid-sentence ("You drop A Bow of Icy Arrows.",
 * "(Getting off A Stool first)") is a display accident, so the comparison
 * folds case and every spelling becomes "the".
 */
static scr_bool
lib_compare_article (const scr_char *string, const scr_char *word,
                     scr_int length)
{
  assert (string && word);

  return scr_compare_word (string, word, length);
}

/*
 * The Runner's own case-sensitive test, for the one caller that builds a
 * command line rather than text: the battle "with" prefix re-parsed on the
 * next turn (lib_battle_definite_name), which keeps the measured spelling.
 */
scr_bool
lib_compare_article_binary (const scr_char *string, const scr_char *word,
                            scr_int length)
{
  assert (string && word);

  return strncmp (string, word, length) == 0
         && (string[length] == NUL || scr_isspace (string[length]));
}


/*
 * lib_definite_prefix()
 *
 * Write the article lib_definite_object_name() opens with to RESULT, and
 * return what is left of PREFIX to follow it.  The 4.0 name builder's own
 * rule, for command lines rather than printed names, is
 * sclibrar_dispatch.cpp's lib_definite_prefix_400().
 */
static const scr_char *
lib_definite_prefix (const scr_char *prefix, scr_bool is_pre_390,
                     std::string &result)
{
  const scr_char *normalized;

  /*
   * Normalize by skipping any leading "a"/"an"/"some", replacing it instead
   * with "the", and skipping any odd "the" already present.  If no prefix at
   * all, add a "the " anyway.
   *
   * This is empirical, based on observed ADRIFT Runner behavior.  The empty
   * prefix case was the last guess left in it, and it is measured now:
   * run400.exe playing La hija del relojero (whose Phoenix has no prefix)
   * answers "coger fenix" with "You take the Fenix de laton de el cajon.",
   * i.e. "the " is right.  lib_print_object below defaults the same empty
   * prefix to "a ", and that is right too -- the same run prints "Encontre un
   * Fenix de laton dentro del cajon." on the container-reveal path.  See the
   * divergence table in test/adrift4/notes/RUNNER_TESTS.md.
   */
  normalized = prefix;

  /*
   * Pre-3.9 is Form1.tense(), and it is a much smaller thing: three
   * literal tests and no default (run370 @420F28, run380 @425FA8, the two
   * byte-identical).  An exact "a" becomes "the"; a prefix opening "a " or
   * "an " has that much replaced by "the "; anything else -- a bare "an",
   * a "some" -- comes back untouched.  So run370 answers `take implement
   * of destruction` with "You pick up an implement of destruction." where
   * 3.9 and 4.0 say "the".  Measured live under Wine on
   * arlo.taf.
   *
   * tense() has no empty-prefix branch either, but pre-3.9 still comes
   * out with "the", because the empty prefix never reaches it: the .taf
   * loader rewrites it on the way in.  run380 @4481B2 reads the Prefix
   * line and, `If (Prefix = vbNullString) Then Prefix = "a"`, substitutes
   * a literal "a" (run370 @43F5DA does the same).  So pre-3.9
   * an empty prefix simply *is* an "a" prefix, and both of scarier's
   * existing defaults -- "the " here, "a " in lib_print_object below --
   * already fall out of it correctly.  4.0 turns out to do it too, at
   * loc_4900EC in its own object loader (mdlSpreadTheLoad.bas:7599), so
   * the substitution is not a pre-3.9 quirk at all; it is only that the
   * defaults here make it invisible.
   *
   * Measured live under Wine on mikes.taf (3.8) in run380,
   * whose dresser, toilet, poop and vans all carry an empty Prefix
   * (deobfuscated straight out of the .taf, so this is not inference):
   * `take vans` answers "You pick up the pair of vans.", `take poop`
   * "You take a poop from the toilet.", `take all from dresser` "You
   * take a socks, a shirt, a underwear and a pair of pants from the
   * dresser."  Note "a underwear" -- the substituted article is never
   * inflected to "an", exactly as lib_print_object's own default isn't.
   *
   * Scarier deliberately normalizes a bare "an" (and the "the"/capital
   * spellings below) in every version all the same (deviation policy):
   * "You pick up an implement of destruction." names an object already
   * in hand as if it were a new one.  Only the grammatical "some" keeps
   * its pre-3.9 spelling.
   */
  if (lib_compare_article (prefix, "a", 1))
    {
      normalized = prefix + 1;
      result += "the";
    }
  else if (scr_compare_word (prefix, "an", 2))
    {
      normalized = prefix + 2;
      result += "the";
    }
  /*
   * No "the" branch.  The Runner's normalizer only ever rewrites the three
   * indefinite articles, so a prefix the author wrote as "The" comes back
   * "The", capital and all.  run400's tense (Proc_21_13_44F474, called from
   * the name builder Proc_21_31_448710 in its normalizing mode 0) tests
   * exactly six things -- the whole string against "a", "an" and "some", and
   * its head against "a ", "an " and "some " -- and returns its argument
   * untouched otherwise; pre-3.9's tense is the same shape with the two "some"
   * tests missing (see the branch above).  Measured live: run400 playing The
   * X-Files: A New Beginning, whose Memo carries the Prefix "The", answers
   * `take all from desk` with "... Your Badge and The Memo from Your Desk.",
   * `x desk` with "Your Coffee Mug and The Memo are on Your Desk" and `burn
   * memo` with "I don't understand what you want me to do with The Memo."
   * (runner_probes/xfiles.run400.txt).  Falling through leaves the
   * prefix in "normalized", which the tail below prints verbatim, so all this
   * branch ever did was lower-case the author's capital.
   *
   * 3.9 is bracketed rather than read: the run390 decompilation does not
   * reach its normalizer.  Both neighbours leave "the" alone, and the only
   * thing 3.9 is known to have added to tense is the "some" pair, so the
   * 3.9 games in the corpus (The Spirit's Flight, whose Spirit Dagger and
   * Orb of Storms both carry a "The" prefix) follow 4.0 here.
   *
   * Scarier deliberately keeps a "the" branch anyway (deviation policy): "You
   * take The Grim Reaper's Scythe." is a capital mid-sentence and nothing
   * more, so an authored "The" is folded.
   */
  else if (lib_compare_article (prefix, "the", 3))
    {
      normalized = prefix + 3;
      result += "the";
    }
  else if (!is_pre_390 && lib_compare_article (prefix, "some", 4))
    {
      normalized = prefix + 4;
      result += "the";
    }
  else if (scr_strempty (prefix))
    {
      normalized = "";
      result += "the";
    }

  return normalized;
}

/*
 * lib_short_without_article()
 *
 * An object's Short name, past any leading article that has more of the
 * name after it.
 */
static const scr_char *
lib_short_without_article (const scr_char *name)
{
  /*
   * Print the object's name, verbatim.  Inherited SCARE looked for a leading
   * article here and stripped it, on the grounds that "some games may avoid
   * prefix and do this instead".  No Runner can do that, and the whole path
   * is readable end to end:
   *
   * - The object loader normalizes the two fields as it reads them.  run400
   *   at loc_4900E3 (mdlSpreadTheLoad.bas:7594) LineInputs the Prefix,
   *   substitutes a literal "a" for an empty one, then loops stripping
   *   trailing spaces; it then LineInputs the Short and loops stripping
   *   *leading* spaces, and moves straight on to the alias count.  Nothing
   *   looks at an article.  run370 @43F5DA and run380 @4481B2 make the same
   *   "a" substitution.
   * - run400 builds every object name in one place, Proc_21_31_448710
   *   (General.bas:7041, 232 call sites), as the single string
   *   `Prefix & " " & Short`.
   * - It hands that to tense, Proc_21_13_44F474 (General.bas:1728), which
   *   tests exactly six things and nothing else: the whole string against
   *   "a", "an" and "some", and its first 2/3/5 characters against "a ",
   *   "an " and "some ".  Callers tense the result a second time (e.g.
   *   Battles.bas:261 then :265), which changes nothing.
   *
   * So the only characters ever inspected are at the head of the
   * concatenation, and after the loader's substitution the head is always the
   * prefix.  An object with Prefix "The" and Short "the Memo" comes out of
   * run400 as "The the Memo"; one with no prefix at all is "a Fenix de
   * laton" going in and "the Fenix de laton" coming out, which is what run400
   * playing La hija del relojero answers to "coger fenix" and what the two
   * defaults in this file and in lib_print_object below already reproduce.
   * Pre-3.9's tense is the same shape with the two "some" tests missing (see
   * the branch above), so this holds in all four Runners.
   *
   * The loader's whitespace trims -- trailing spaces off Prefix, leading ones
   * off Short -- are modelled too, in parse_trim_object_names().
   *
   * Scarier deliberately strips a leading article off the Short all the same
   * (deviation policy), as SCARE did: "the The horn of the angels" and "The
   * the Memo" are doubled articles, nothing more.  Only an article followed
   * by more of the name goes, with the spaces after it, so the old "the  horn"
   * double space does not come back either.
   */
  static const struct { const scr_char *word; scr_int length; } articles[] = {
    {"a", 1}, {"an", 2}, {"the", 3}, {"some", 4}
  };
  size_t index_;

  for (index_ = 0; index_ < sizeof (articles) / sizeof (articles[0]);
       index_++)
    {
      if (scr_compare_word (name, articles[index_].word,
                            articles[index_].length)
          && !scr_strempty (name + articles[index_].length))
        {
          name += articles[index_].length;
          while (scr_isspace (*name))
            name++;
          break;
        }
    }

  return name;
}

std::string
lib_definite_object_name (scr_prop_setref_t bundle, scr_int object)
{
  std::string result;
  scr_vartype_t vt_key[3], vt_version[1];
  const scr_char *prefix, *normalized, *name;
  scr_bool is_pre_390;

  vt_version[0].string = "Version";

  /* Get the object's prefix. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);

  is_pre_390 = prop_get_integer (bundle, "I<-s", vt_version) < TAF_VERSION_390;
  normalized = lib_definite_prefix (prefix, is_pre_390, result);

  /*
   * Print what the normalizer left of the prefix, and a separating space.
   * Both are unconditional: the Runner's name builder is a plain
   * `tense(Prefix) & " " & Short` concatenation, so a prefix it hands back
   * empty still costs a space.  That case is only reachable at all for an
   * authored whitespace-only prefix, which the loader trims to nothing after
   * declining to substitute an "a" into it -- probe ISARE's single-space
   * rings answers `where rings` with " rings are test arena.", leading space
   * and no article (run400, runner_probes/isare.run400.txt).
   * Scarier deliberately gives that empty prefix a "the" instead (deviation
   * policy).
   */
  result += normalized;
  result += ' ';

  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  name = lib_short_without_article (name);
  result += name;
  return result;
}

void
lib_print_object_np (scr_gameref_t game, scr_int object)
{
  pf_buffer_string (gs_get_filter (game),
                    lib_definite_object_name (gs_get_bundle (game),
                                              object).c_str ());
}

void
lib_print_object (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *prefix, *name;

  /*
   * Get the object's prefix and print it, then a space; both unconditionally,
   * the Runner's builder being a plain concatenation.  An object authored
   * with no prefix at all arrives here carrying the literal "a" the loader
   * substituted into it (see parse_trim_object_names in sctafpar.cpp), never
   * inflected to "an" before a vowel: run380 on the pwearv probe, four
   * wearables with empty prefixes and vowel-initial names, answers "You put
   * on a apple." and lists them back as "You are wearing a apple, a orange, a
   * egg and a umbrella".
   */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);
  pf_buffer_string (filter, prefix);
  pf_buffer_character (filter, ' ');

  /* Print object name. */
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  pf_buffer_string (filter, name);
}


/*
 * lib_print_object_raw()
 *
 * Print an object as the pre-3.9 "remove" handler does: the authored prefix, a
 * space, and the name, with no normalizing and no default for an empty prefix.
 * run370 @42980D and run380 @42FF38 both build the message by plain
 * concatenation, `... & " remove " & ob(0) & " " & ob(4) & "."`, where every
 * other lister in those Runners passes the prefix through tense() first
 * (run370 drop @430A50, run380 drop @438C3B).  3.9 unified the two: run390
 * builds both its drop @00045B93 and its remove @00039E15 out of the same
 * General.Sub_3_45 helper, so from 3.9 on "remove" normalizes like the rest.
 * Measured live under Wine, akron.taf (3.8) in run380: "You remove a grubby
 * sweatshirt." and "You remove a pair of ragged jeans." where the same game in
 * Scarier said "the".
 *
 * Scarier deliberately keeps "the" in the success reports all the same
 * (deviation policy) -- an indefinite article on the object just removed,
 * worn or put away reads as if it were some other one -- so this printer is
 * left to the refusals ("You are not wearing a hat!"), where it reads fine.
 */
void
lib_print_object_raw (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *prefix;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;

  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);

  /*
   * "No default for an empty prefix" is right about the message builder and
   * wrong about what reaches it: the pre-3.9 loaders rewrite an empty Prefix
   * to a literal "a" before any of this (run370 @43F5DA, run380 @4481B2, and
   * run400 at loc_4900EC too), so the concatenation never sees one.  That
   * rewrite is modelled in parse_trim_object_names now, so nothing is needed
   * here.
   */
  pf_buffer_string (filter, prefix);
  pf_buffer_character (filter, ' ');

  vt_key[2].string = "Short";
  pf_buffer_string (filter, prop_get_string (bundle, "S<-sis", vt_key));
}


/*
 * lib_print_npc_np
 *
 * Convenience function to print out an NPC's name, without any prefix.
 */
void
lib_print_npc_np (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /* Get the NPC's short description, and print it. */
  pf_buffer_string (filter,
                    prop_get_indexed_string (bundle, "NPCs", npc, "Name"));
}

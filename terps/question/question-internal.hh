/*
 * Copyright (C) 2006  Mark J. Tilford
 * Copyright (C) 2021-2026  Petter Sjölund
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

/* What the question_implementation translation units share among themselves.
 * The class itself is in question-impl.hh; this is only for the handful of
 * free helpers and tables that sit beside its methods.  Nothing outside the
 * question-*.cc files should need it. */

#ifndef QUESTION_INTERNAL_HH
#define QUESTION_INTERNAL_HH

#include <string>
#include <vector>

/* The compass names, in the order the exit tables and the direction bits use.
 * The bound is spelled out so ARRAYSIZE (question-util.hh) works on them from
 * the other units.  Defined in question-runner.cc. */
extern const std::string dir_names[11];
extern const std::string short_dir_names[11];

/* The verbs the engine dispatches itself, beyond the universal ones.
 * `key` is the action/property name a game file stores; `phrases` are the
 * surface forms a player may type for it; `use_default` marks a verb that
 * falls back to the object's anonymous default action when nothing else
 * handles it.
 *
 * Shared by the two places that must agree about them: try_match, which
 * dispatches a typed command, and object_verbs, which lists the verbs an
 * object responds to.  They used to keep separate tables, so a verb added to
 * one was silently missing from the other. */
struct verb_def
{
  const char *key;
  std::vector<const char *> phrases;
  bool use_default;
};

const std::vector<verb_def> &builtin_verbs ();

/* question-parse.cc: a typed line brought into the UTF-8 the game text is
 * in (Latin-1 input is transcoded, well-formed UTF-8 is left alone). */
std::string input_to_utf8 (const std::string &s);

/* question-session.cc: whether a phrase starts with the lead word of a
 * phrasal verb ("take off", "pick up", "look at", ...), which a single-word
 * synonym must not rewrite. */
bool is_phrasal_verb_lead (const std::string &phrase);

#endif

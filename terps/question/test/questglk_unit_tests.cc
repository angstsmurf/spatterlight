/*
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
  questglk_unit_tests -- checks for the presentation helpers both Glk
  frontends share (../questglk-common.cc).

  The status banner is a single Glk grid line, so how a long status line is
  cut down to the cells left beside the room name is pure string arithmetic
  that neither the fixture games nor the frontend smoke harness can reach:
  CheapGlk has no grid window to draw into.  status_tail is that arithmetic,
  lifted out of draw_status_banner so it can be called directly here.

  Build:  make questglk_unit_tests      (in this directory)
  Run:    ./questglk_unit_tests         (exit 0 = all passed)
*/

#include <cstdlib>
#include <iostream>
#include <string>

#include "../questglk-common.hh"

extern "C" {
#include "glkstart.h"
}

/* The helpers are linked in whole, and the Glk calls among them pull in
   CheapGlk's main(), which expects these entry points. */
glkunix_argumentlist_t glkunix_arguments[] = {
    { nullptr, glkunix_arg_End, nullptr }
};

int glkunix_startup_code(glkunix_startup_t *)
{
    return 1;
}

using questglk::match_help_command;
using questglk::match_status_command;
using questglk::status_tail;
using questglk::tail_chars;
using questglk::utf8_cp_len;

namespace {

int failures = 0;

void
check (bool ok, const std::string &what)
{
  std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n";
  if (!ok)
    failures++;
}

void
test_tail_chars ()
{
  check (tail_chars ("abcdef", 3, false) == "def", "byte tail");
  check (tail_chars ("abc", 9, false) == "abc", "byte tail longer than string");
  check (tail_chars ("abc", 0, false) == "", "byte tail of nothing");

  /* "skal" with an a-ring -- five bytes, four codepoints.  A byte-wise cut
     of three would slice the ring in half; a codepoint-wise one must not. */
  check (tail_chars ("sk\xc3\xa5l", 3, true) == "k\xc3\xa5l",
	 "utf-8 tail does not split a codepoint");
  check (tail_chars ("sk\xc3\xa5l", 4, true) == "sk\xc3\xa5l",
	 "utf-8 tail of the whole string");
  check (tail_chars ("\xc3\xa5\xc3\xa4\xc3\xb6", 1, true) == "\xc3\xb6",
	 "utf-8 tail of an all-multibyte string");
}

void
test_status_tail ()
{
  const std::string s = "Score: 30 | Health: 100 | Moves: 412";  /* 36 chars */

  check (status_tail (s, 36, false) == s, "an exactly-fitting status is untouched");
  check (status_tail (s, 99, false) == s, "a comfortably fitting status is untouched");

  /* Cut from the LEFT: the fields at the end of the line survive, and the
     result fills the cells available exactly. */
  check (status_tail (s, 16, false) == "100 | Moves: 412",
	 "a too-long status keeps its tail");
  check (status_tail (s, 17, false) == "100 | Moves: 412",
	 "leading blanks of the tail are dropped");

  /* A cut landing on the space before a separator must not leave a blank
     between the ellipsis the banner draws and the text: 11 cells would
     otherwise give " Moves: 412". */
  check (status_tail (s, 11, false) == "Moves: 412",
	 "a cut in a gap starts at the next word");

  check (status_tail (s, 0, false) == "", "no cells at all: nothing");
  check (status_tail (s, 1, false) == "2",
	 "the narrowest tail that still shows a character");
  check (status_tail ("ab   ", 2, false) == "", "an all-blank tail is nothing");

  /* Measured in codepoints in UTF-8 mode, so an accented status is not cut
     shorter than a plain one of the same visible length.  22 codepoints,
     23 bytes. */
  const std::string a = "H\xc3\xa4lsa: 100 | Steg: 412";
  check (utf8_cp_len (a) == 22 && a.size () == 23, "the accented fixture");
  check (status_tail (a, 22, true) == a, "an exactly-fitting utf-8 status");
  check (status_tail (a, 21, true) == "\xc3\xa4lsa: 100 | Steg: 412",
	 "utf-8 truncation keeps the tail and whole characters");
  check (utf8_cp_len (status_tail (a, 21, true)) == 21,
	 "utf-8 truncation is measured in codepoints, not bytes");
}

void
test_match_status_command ()
{
  check (match_status_command ("status"), "STATUS matches");
  check (match_status_command ("  STATUS  "), "STATUS matches trimmed/uppercase");
  check (match_status_command ("#status"), "#STATUS matches");
  check (!match_status_command ("status bar"), "STATUS BAR is the game's");
  check (!match_status_command ("statuses"), "STATUSES is the game's");
  check (!match_status_command (""), "an empty line is not STATUS");
}

void
test_match_help_command ()
{
  check (match_help_command ("#help"), "#HELP matches");
  check (match_help_command ("  #HELP  "), "#HELP matches trimmed/uppercase");
  check (match_help_command ("#commands"), "#COMMANDS matches");
  check (match_help_command ("metaverbs"), "METAVERBS matches");
  check (!match_help_command ("help"), "plain HELP is the game's own");
  check (!match_help_command ("#help me"), "#HELP ME is not the listing");
  check (!match_help_command (""), "an empty line is not #HELP");
}

}  /* namespace */

void
glk_main (void)
{
  std::cout << "tail_chars:\n";
  test_tail_chars ();
  std::cout << "status_tail:\n";
  test_status_tail ();
  std::cout << "match_status_command:\n";
  test_match_status_command ();
  std::cout << "match_help_command:\n";
  test_match_help_command ();

  std::cout << (failures ? "FAILED" : "all passed") << " (" << failures
	    << " failure" << (failures == 1 ? "" : "s") << ")\n";
  if (failures)
    exit (1);
}

/*
  questglk_unit_tests -- checks for the presentation helpers both Glk
  frontends share (../questglk-common.inc).

  The status banner is a single Glk grid line, so how a long status line is
  cut down to the cells left beside the room name is pure string arithmetic
  that neither the fixture games nor the frontend smoke harness can reach:
  CheapGlk has no grid window to draw into.  fit_status is that arithmetic,
  lifted out of draw_status_banner so it can be called directly here.

  Build:  make questglk_unit_tests      (in this directory)
  Run:    ./questglk_unit_tests         (exit 0 = all passed)
*/

#include <iostream>
#include <string>

#include "../questglk-common.inc"

using questglk::fit_status;
using questglk::match_status_command;
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
test_fit_status ()
{
  const std::string s = "Score: 30 | Health: 100 | Moves: 412";  /* 36 chars */

  check (fit_status (s, 36, false) == s, "an exactly-fitting status is untouched");
  check (fit_status (s, 99, false) == s, "a comfortably fitting status is untouched");

  /* Cut from the LEFT: the fields at the end of the line survive, behind the
     marker, and the result fills the cells available exactly. */
  check (fit_status (s, 20, false) == "... 100 | Moves: 412",
	 "a too-long status keeps its tail behind the marker");
  check (fit_status (s, 20, false).size () == 20,
	 "the truncated status is exactly as wide as the cells available");

  /* A cut landing on the space before a separator must not leave a doubled
     space after the marker: 15 cells would otherwise give "...  Moves: 412".
     Dropping it makes the result one cell narrower than the space available,
     which is fine -- the banner right-aligns whatever it gets back. */
  check (fit_status (s, 15, false) == "... Moves: 412",
	 "leading blanks of the tail are dropped after the marker");

  /* Too narrow for the marker plus any text at all: draw nothing, which is
     what the banner did for every over-long status before truncation. */
  check (fit_status (s, 4, false) == "", "no room for marker plus text: nothing");
  check (fit_status (s, 0, false) == "", "no cells at all: nothing");
  check (fit_status (s, 5, false) == "... 2",
	 "the narrowest status that still shows a character");

  /* Measured in codepoints in UTF-8 mode, so an accented status is not cut
     shorter than a plain one of the same visible length.  22 codepoints,
     23 bytes. */
  const std::string a = "H\xc3\xa4lsa: 100 | Steg: 412";
  check (utf8_cp_len (a) == 22 && a.size () == 23, "the accented fixture");
  check (fit_status (a, 22, true) == a, "an exactly-fitting utf-8 status");
  check (fit_status (a, 15, true) == "... | Steg: 412",
	 "utf-8 truncation keeps the tail");
  check (utf8_cp_len (fit_status (a, 15, true)) == 15,
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

}  /* namespace */

int
main ()
{
  std::cout << "tail_chars:\n";
  test_tail_chars ();
  std::cout << "fit_status:\n";
  test_fit_status ();
  std::cout << "match_status_command:\n";
  test_match_status_command ();

  std::cout << (failures ? "FAILED" : "all passed") << " (" << failures
	    << " failure" << (failures == 1 ? "" : "s") << ")\n";
  return failures ? 1 : 0;
}

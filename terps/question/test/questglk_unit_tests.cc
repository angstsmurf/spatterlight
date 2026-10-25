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

using questglk::COMBO_BOLD;
using questglk::COMBO_CENTER;
using questglk::COMBO_ITALIC;
using questglk::COMBO_MONOSPACED;
using questglk::COMBO_RIGHT;
using questglk::StyleCensus;
using questglk::StyleCombo;
using questglk::StyleTable;
using questglk::match_help_command;
using questglk::match_sidebar_command;
using questglk::match_status_command;
using questglk::style_assign;
using questglk::style_combo;
using questglk::style_combo_describe;
using questglk::style_combo_step;
using questglk::style_lookup;
using questglk::status_leaves_banner;
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

/* Where the status goes: the side pane once less than three quarters of it
   fits in the banner, and back only when all of it does. */
void
test_status_leaves_banner ()
{
  const std::string room = "Hall";                                /* 4 chars */
  const std::string s = "Score: 30 | Health: 100 | Moves: 412";  /* 36 chars */

  /* The banner spends a cell before the room name, one between the two and
     one at the right edge: 4 + 36 + 3 = 43 is the narrowest that holds it. */
  check (!status_leaves_banner (43, room, s, false, false), "all of it fits");
  check (!status_leaves_banner (200, room, s, false, false), "room to spare");

  /* Cut, but 29 of the 36 characters survive behind the ellipsis. */
  check (!status_leaves_banner (36, room, s, false, false),
	 "a cut that keeps three quarters stays in the banner");
  /* 23 of 36. */
  check (status_leaves_banner (30, room, s, false, false),
	 "less than three quarters: to the pane");
  check (status_leaves_banner (5, room, s, false, false),
	 "no room for any of it: to the pane");
  check (status_leaves_banner (0, room, s, false, false),
	 "a banner with no cells: to the pane");

  /* Once there it stays until the whole of it fits again. */
  check (status_leaves_banner (36, room, s, false, true),
	 "three quarters is not enough to come back");
  check (status_leaves_banner (42, room, s, false, true),
	 "one cell short of fitting: still in the pane");
  check (!status_leaves_banner (43, room, s, false, true),
	 "all of it fits: back to the banner");

  check (!status_leaves_banner (10, room, "", false, false),
	 "no status, nothing to move");
  check (!status_leaves_banner (10, room, "", false, true),
	 "a status that went away leaves the pane");

  /* A longer room name takes the cells away just the same. */
  check (status_leaves_banner (43, "The Great Northern Hall", s, false, false),
	 "the room name counts against the status");

  /* Codepoints, not bytes: 22 characters in 23 bytes fit 29 cells. */
  const std::string a = "H\xc3\xa4lsa: 100 | Steg: 412";
  check (!status_leaves_banner (29, room, a, true, true),
	 "a utf-8 status is measured in codepoints");
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

static void
test_match_sidebar_command ()
{
  check (match_sidebar_command ("sidebar wide") == 1, "SIDEBAR WIDE widens");
  check (match_sidebar_command ("  #Sidebar WIDE ") == 1,
         "#SIDEBAR WIDE matches trimmed/mixed case");
  check (match_sidebar_command ("sidebar normal") == -1,
         "SIDEBAR NORMAL narrows");
  check (match_sidebar_command ("sidebar narrow") == -1,
         "SIDEBAR NARROW narrows");
  check (match_sidebar_command ("sidebar") == 0,
         "a bare SIDEBAR is the game's");
  check (match_sidebar_command ("wide") == 0, "WIDE alone is the game's");
  check (match_sidebar_command ("") == 0, "an empty line is no command");
}


/* A state with `points' against a ten-point default, as Quest 4 has it. */
StyleCombo
q4 (bool bold, bool italic, bool center, int points)
{
  return style_combo (bold, italic, false, false, center, false, points, 10);
}

StyleTable
assigned (const StyleCensus &census)
{
  StyleTable table;
  style_assign (census.ranked (), table);
  return table;
}

void
test_style_combo ()
{
  check (style_combo (false, false, true, false, false, false, 12, 12)
	 == COMBO_ITALIC, "underline shares the italic bit");
  check (style_combo_step (q4 (false, false, false, 11)) == 0,
	 "a point over the default is the default size");
  check (style_combo_step (q4 (false, false, false, 20)) == 5,
	 "two points to a step");
  check (style_combo_step (q4 (false, false, false, 13)) == 1,
	 "a size between two steps takes the one nearer the default");
  check (style_combo_step (q4 (false, false, false, 9)) == 0,
	 "...in either direction");
  check (style_combo_step (style_combo (false, false, false, false, false,
					false, 15.6, 12)) == 2,
	 "Quest 5's 130% of 12pt rounds to the nearest step, not down");
  check (style_combo_step (style_combo (false, false, false, false, false,
					false, 13.2, 12)) == 1,
	 "...and 110% is a step up");
  check (style_combo_step (q4 (false, false, false, 99)) == 6,
	 "steps clamp at +6");
  check (style_combo_step (q4 (false, false, false, 2)) == -3,
	 "...and at -3");
  check (style_combo_step (style_combo (false, false, false, false, false,
					false, 12, 14)) == -1,
	 "sizes are relative to the game's default");
  check (style_combo (false, false, false, false, true, true, 12, 12)
	 == COMBO_RIGHT, "right wins over centre");
  check (style_combo_describe (q4 (true, false, true, 20))
	 == "bold+centre size+5", "a state spelled out");
  check (style_combo_describe (0) == "plain", "the plain state spelled out");
}

void
test_style_assign ()
{
  /* Only what the old fixed mapping printed: every state keeps its home. */
  StyleCensus plain;
  plain.count (0, "Some body text.", false);
  plain.count (COMBO_BOLD, "bold", false);
  plain.count (COMBO_ITALIC, "italic", false);
  plain.count (COMBO_BOLD | COMBO_ITALIC, "both", false);
  StyleTable t = assigned (plain);
  check (style_lookup (t, 0) == style_Normal, "plain is Normal");
  check (style_lookup (t, COMBO_BOLD) == style_Subheader, "bold is Subheader");
  check (style_lookup (t, COMBO_ITALIC) == style_Emphasized,
	 "italic (and underline) is Emphasized");
  check (style_lookup (t, COMBO_BOLD | COMBO_ITALIC) == style_Alert,
	 "bold italic is Alert");
  check (style_lookup (t, COMBO_MONOSPACED) == style_Preformatted,
	 "monospaced is Preformatted");
  bool all_natural = true;
  for (int i = 0; i < questglk::STYLE_COUNT; i++)
    all_natural = all_natural && t.natural[i];
  check (all_natural, "nothing is re-hinted");

  /* A |jc|s20|b title: Header takes it. */
  StyleCensus title;
  title.count (0, "Lots and lots of plain body text here.", false);
  title.count (q4 (true, false, true, 20), "My Game", true);
  t = assigned (title);
  check (style_lookup (t, q4 (true, false, true, 20)) == style_Header,
	 "the sized centred title gets the first spare");
  check (!t.natural[style_Header], "...hinted from its state");
  check (t.natural[style_User2] && style_lookup (t, COMBO_BOLD | COMBO_CENTER)
				      == style_User2,
	 "centred bold keeps User2");

  /* A plain sized centred title: the spare that is plain by default, so a
     frontend that ignores hints still draws it plain. */
  StyleCensus plain_title;
  plain_title.count (0, "Lots and lots of plain body text here.", false);
  plain_title.count (q4 (false, false, true, 20), "My Game", true);
  plain_title.count (q4 (true, false, true, 18), "A Studio", true);
  t = assigned (plain_title);
  check (style_lookup (t, q4 (false, false, true, 20)) == style_BlockQuote,
	 "a plain title skips Header for BlockQuote");
  check (style_lookup (t, q4 (true, false, true, 18)) == style_Header,
	 "...and the bold one takes Header");

  /* Unseen states fall back by dropping attributes, never adding them. */
  check (style_lookup (t, q4 (true, false, true, 16)) == style_User2,
	 "a size with no style drops to the unsized state");
  check (style_lookup (t, COMBO_ITALIC | COMBO_CENTER) == style_User1,
	 "centred italic drops the italic before the alignment");
  check (style_lookup (t, q4 (false, true, false, 20)) == style_Emphasized,
	 "sized italic drops the size");
  check (style_lookup (t, COMBO_MONOSPACED | COMBO_BOLD) == style_Preformatted,
	 "monospaced bold drops the bold first");

  /* The introduction outweighs body text twenty to one, so a short sized
     title beats a longer sized aside for the last spare. */
  StyleCensus overflow;
  overflow.count (0, std::string (500, 'x'), false);
  /* Eight sizes, each its own step (-2 to +6, skipping the default). */
  const int sizes[] = { 6, 8, 12, 14, 16, 18, 20, 22 };
  for (int points: sizes)
    overflow.count (q4 (false, false, false, points),
		    std::string (40 + points, 'y'), false);
  overflow.count (q4 (true, true, false, 30), "Title", true);
  t = assigned (overflow);
  check (t.combo[style_lookup (t, q4 (true, true, false, 30))]
	 == q4 (true, true, false, 30), "the intro state wins a slot of its own");
  check (style_lookup (t, q4 (true, true, false, 30)) == style_Alert,
	 "...the spare whose default look is bold italic");
  int sized = 0;
  for (int points: sizes)
    if (style_combo_step (t.combo[style_lookup (t, q4 (false, false, false,
							   points))]) != 0)
      sized++;
  check (sized == 5, "six spares: the title and the five heaviest sizes");
  check (style_lookup (t, q4 (false, false, false, 12)) == style_Normal,
	 "the lightest sizes fall back to Normal");

  /* Centred text that is all sized: User1 went out as a spare, so the
     post-pass takes the last spare back for plain centre. */
  StyleCensus centred;
  centred.count (0, std::string (100, 'x'), false);
  for (int points: sizes)
    centred.count (q4 (false, false, false, points),
		   std::string (30 + points, 'y'), false);
  centred.count (q4 (false, true, true, 16), std::string (20, 'z'), false);
  t = assigned (centred);
  check (style_lookup (t, q4 (false, true, true, 16)) == style_User1,
	 "centred text keeps its alignment when its home went spare");
  check (t.natural[style_User1], "...on the natural User1");
}

}  /* namespace */

void
glk_main (void)
{
  std::cout << "tail_chars:\n";
  test_tail_chars ();
  std::cout << "status_tail:\n";
  test_status_tail ();
  test_status_leaves_banner ();
  std::cout << "match_status_command:\n";
  test_match_status_command ();
  std::cout << "match_help_command:\n";
  test_match_help_command ();

  std::cout << "match_sidebar_command:\n";
  test_match_sidebar_command ();

  std::cout << "styles:\n";
  test_style_combo ();
  test_style_assign ();

  std::cout << (failures ? "FAILED" : "all passed") << " (" << failures
	    << " failure" << (failures == 1 ? "" : "s") << ")\n";
  if (failures)
    exit (1);
}

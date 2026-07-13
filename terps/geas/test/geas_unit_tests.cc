/*
  geas_unit_tests -- checks for engine code the fixture games cannot reach.

  A .asl fixture can only exercise what a *player* can drive.  It cannot hand
  the engine a corrupt save file, and it cannot inspect a record that holds no
  elements.  Those live here, called directly, alongside the parser edge cases
  whose symptom is undefined behaviour rather than wrong output (build with
  -fsanitize=address to see those fail on a pre-fix engine; at -O2 they can
  silently return garbage instead of crashing).

  Build:  make geas_unit_tests      (in this directory)
  Run:    ./geas_unit_tests         (exit 0 = all passed)
*/

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../geas-util.cc"
#include "../istring.cc"
#include "../readfile.cc"
#include "../geasfile.cc"
#include "../geas-state.cc"
#include "../geas-runner.cc"

namespace {

int failures = 0;

void
check (bool ok, const std::string &what)
{
  std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n";
  if (!ok)
    failures++;
}

/* "QUEST300\0<name>\0" then the body, byte-inverted as the writer leaves it. */
std::string
save_file (const std::string &gamename, const std::string &body)
{
  std::string out = std::string ("QUEST300") + char (0) + gamename + char (0);
  for (size_t i = 0; i < body.size (); i++)
    out += char ((unsigned char) (255 - (unsigned char) body[i]));
  return out;
}

std::string
field (const std::string &s)
{
  return s + std::string (1, char (0));
}

/* ---- deserialize_game: every count in a save is a loop bound, and a save is
   untrusted input.  A count that outruns the file must be refused, not
   allocated for. ---- */
void
test_deserialize ()
{
  std::string name;
  GeasState gs;

  check (!deserialize_game ("", name, gs), "empty file rejected");
  check (!deserialize_game ("not a geas save at all", name, gs),
	 "garbage file rejected");

  /* A well-formed minimal save: location, then a zero count for each of the
     six record vectors.  This is the control -- it must load. */
  std::string body = field ("Lobby");
  for (int i = 0; i < 6; i++)
    body += field ("0");
  check (deserialize_game (save_file ("g.asl", body), name, gs),
	 "minimal well-formed save accepted");
  check (name == "g.asl" && gs.location == "Lobby",
	 "minimal save round-trips name and location");

  /* Now the same save, but each count in turn claims billions of records.  The
     file is a few dozen bytes long, so every one of these is a lie: it must be
     caught rather than driving a resize/push loop until we run out of memory. */
  static const char *const which[] = { "props", "objects", "exits", "timers",
				       "svars", "ivars" };
  for (int bad = 0; bad < 6; bad++)
    {
      std::string b = field ("Lobby");
      for (int i = 0; i < 6; i++)
	b += field (i == bad ? "4000000000" : "0");
      GeasState g2;
      std::string n2;
      check (!deserialize_game (save_file ("g.asl", b), n2, g2),
	     std::string ("absurd ") + which[bad] + " count rejected");
    }

  /* An array's *upper bound* is a loop bound too: "one svar, whose highest
     index is four billion" must not try to size the array. */
  {
    std::string b = field ("Lobby");
    b += field ("0") + field ("0") + field ("0") + field ("0");
    b += field ("1") + field ("arr") + field ("4000000000");   /* svars */
    GeasState g2;
    std::string n2;
    check (!deserialize_game (save_file ("g.asl", b), n2, g2),
	   "absurd svar upper bound rejected");
  }
  {
    std::string b = field ("Lobby");
    b += field ("0") + field ("0") + field ("0") + field ("0") + field ("0");
    b += field ("1") + field ("arr") + field ("4000000000");   /* ivars */
    GeasState g2;
    std::string n2;
    check (!deserialize_game (save_file ("g.asl", b), n2, g2),
	   "absurd ivar upper bound rejected");
  }

  /* Truncation anywhere in a save must be survivable: accepted-as-short or
     rejected, but never a runaway.  Reaching the line after the loop is the
     assertion -- a hang or a fault here never gets to report itself. */
  std::string full = save_file ("g.asl", body);
  for (size_t cut = 0; cut < full.size (); cut++)
    {
      GeasState g2;
      std::string n2;
      deserialize_game (full.substr (0, cut), n2, g2);
    }
  check (true, "every truncation of a valid save returns rather than hanging");
}

/* ---- a real state survives the round trip: the reader and the writer must
   agree on the format, including how many elements an array carries. ---- */
void
test_serialize_roundtrip ()
{
  GeasState gs;
  gs.location = "Cellar";
  gs.add_prop ("lamp", "lit");
  SVarRecord sv ("hero");
  sv.set (0, "Rincewind");
  sv.set (2, "third");          /* a sparse array: max() == 2, size() == 3 */
  gs.svars.push_back (sv);
  IVarRecord iv ("score");
  iv.set (0, 42);
  gs.ivars.push_back (iv);
  gs.items.push_back ("a rusty key");

  GeasState back;
  std::string name;
  check (deserialize_game (serialize_game ("g.asl", gs), name, back),
	 "round trip: save reloads");
  check (back.location == "Cellar", "round trip: location");
  check (back.svars.size () == 1 && back.svars[0].max () == 2
	   && back.svars[0].get (0) == "Rincewind"
	   && back.svars[0].get (2) == "third",
	 "round trip: sparse string array");
  check (back.ivars.size () == 1 && back.ivars[0].get (0) == 42,
	 "round trip: numeric variable");
  check (back.items.size () == 1 && back.items[0] == "a rusty key",
	 "round trip: inventory items");

  /* An empty record should never occur in play, but if one ever did, max ()
     must not wrap to SIZE_MAX and become the array bound the serializer writes
     out. */
  SVarRecord empty_s;
  IVarRecord empty_i;
  check (empty_s.max () == 0 && empty_i.max () == 0,
	 "empty record's max() does not underflow");

  GeasState with_empty;
  with_empty.location = "Cellar";
  with_empty.svars.push_back (SVarRecord ());
  with_empty.ivars.push_back (IVarRecord ());
  GeasState back2;
  check (deserialize_game (serialize_game ("g.asl", with_empty), name, back2)
	   && back2.location == "Cellar",
	 "a state holding an empty record still serializes in sync");
}

/* ---- split_lines: "_" continues a line onto the next one.  A line that is
   nothing but the marker leaves an empty accumulator to index. ---- */
void
test_split_lines ()
{
  std::vector<std::string> v = split_lines ("alpha\n_\nbeta\n");
  check (v.size () >= 2, "bare '_' line does not derail split_lines");

  v = split_lines ("msg <one _\n   two>\n");
  check (v.size () == 1 && v[0].find ("one") != std::string::npos
	   && v[0].find ("two") != std::string::npos,
	 "trailing '_' joins the next line");

  v = split_lines ("_");
  check (true, "a file consisting of a single '_' does not fault");

  v = split_lines ("a\r\nb\r\n");
  check (v.size () == 2 && v[0] == "a" && v[1] == "b", "CRLF line endings");
}

}  /* namespace */

int
main ()
{
  std::cout << "deserialize_game:\n";
  test_deserialize ();
  std::cout << "serialize round trip:\n";
  test_serialize_roundtrip ();
  std::cout << "split_lines:\n";
  test_split_lines ();

  std::cout << (failures ? "FAILED" : "all passed") << " (" << failures
	    << " failure" << (failures == 1 ? "" : "s") << ")\n";
  return failures ? 1 : 0;
}

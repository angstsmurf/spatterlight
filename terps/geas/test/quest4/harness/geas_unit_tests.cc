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

#include "../../../geas-util.cc"
#include "../../../istring.cc"
#include "../../../readfile.cc"
#include "../../../geasfile.cc"
#include "../../../geas-state.cc"
#include "../../../geas-runner.cc"

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

/* ---- timers round-trip through a count-*down* wire field, which is where a
   count-up engine can underflow.  No fixture can reach this: the harness plays
   a game to the end, and nothing in a .cmd file writes a save file. ---- */
void
test_timer_roundtrip ()
{
  /* `set interval' moves the interval under a cycle already in flight without
     touching the count (it is the tick that zeroes that), so a timer can sit at
     elapsed > interval indefinitely -- switch it off with ticks banked, shorten
     its interval, and nothing will bring the count back down, because the tick
     skips a stopped timer.  Save in that window. */
  struct { const char *name; bool running; uint interval, elapsed, want; } cases[] =
    {
      { "midway",   true,  10, 3,  3  },  /* the ordinary case */
      { "banked",   false,  3, 8,  2  },  /* interval cut below the count */
      { "atlimit",  true,   5, 5,  4  },  /* the boundary, likewise unwritable */
      { "fresh",    true,   7, 0,  0  },
    };

  GeasState gs;
  gs.location = "Cellar";
  for (const auto &c: cases)
    {
      TimerRecord tr;
      tr.name = c.name;
      tr.is_running = c.running;
      tr.interval = c.interval;
      tr.elapsed = c.elapsed;
      gs.timers.push_back (tr);
    }

  GeasState back;
  std::string name;
  check (deserialize_game (serialize_game ("g.asl", gs), name, back)
	   && back.timers.size () == sizeof cases / sizeof cases[0],
	 "round trip: timers reload");

  for (size_t i = 0; i < back.timers.size () && i < sizeof cases / sizeof cases[0];
       i++)
    {
      /* The two out-of-cycle timers cannot round-trip their count exactly --
	 the wire has no room for one above the interval -- so what they must
	 preserve is the *debt*: a timer that owed one more tick still owes one
	 more tick, rather than being handed a whole fresh interval. */
      check (back.timers[i].is_running == cases[i].running
	       && back.timers[i].interval == cases[i].interval
	       && back.timers[i].elapsed == cases[i].want,
	     std::string ("round trip: timer ") + cases[i].name);
    }

  /* The reader's two garbled-save markers, which is what the writer must never
     produce by accident: a count-down of 0, and one past the interval. */
  check (ticks_left_to_elapsed (10, 0) == 0 && ticks_left_to_elapsed (10, 11) == 0
	   && ticks_left_to_elapsed (10, 10) == 0 && ticks_left_to_elapsed (10, 1) == 9,
	 "a garbled ticks-left field restarts the cycle rather than seeding it");
}

/* ---- the autosave's undo history rides the same wire format as a save file,
   and since the readers were gathered into SaveReader it rides the same reader
   code too -- so a mistake there would show up here rather than only in a
   Spatterlight autorestore, which no test can drive.  Nothing else in the suite
   loads a history. ---- */
void
test_undo_history_roundtrip ()
{
  std::vector<UndoState> states;
  for (int s = 0; s < 3; s++)
    {
      UndoState u;
      u.running = (s != 1);
      u.location = "Room" + string_int (s);
      u.props_len = 7 * s;          /* a snapshot records props by length only */

      ObjectRecord o;
      o.name = "lamp";
      o.parent = "Cellar";
      o.hidden = (s == 2);
      o.invisible = false;
      o.is_room = false;
      u.objs.push_back (o);

      u.exits.push_back (ExitRecord ("Room0", "Room1"));

      TimerRecord t;
      t.name = "fuse";
      t.is_running = true;
      t.interval = 10;
      t.elapsed = (uint) s;
      u.timers.push_back (t);

      SVarRecord sv ("hero");
      sv.set (0, "Rincewind");
      sv.set (2, "third");          /* sparse again: max() == 2 */
      u.svars.push_back (sv);

      IVarRecord iv ("score");
      iv.set (0, 42 + s);
      u.ivars.push_back (iv);

      u.items.push_back ("a rusty key");
      states.push_back (u);
    }

  const std::string wire = serialize_undo_history (states);
  std::vector<UndoState> back;
  check (deserialize_undo_history (wire, back), "undo history: reloads");
  check (back.size () == states.size (), "undo history: every snapshot");

  for (size_t s = 0; s < back.size () && s < states.size (); s++)
    {
      const UndoState &a = states[s], &b = back[s];
      check (b.running == a.running && b.location == a.location
	       && b.props_len == a.props_len,
	     "undo history: snapshot header " + string_int (s));
      check (b.objs.size () == 1 && b.objs[0].name == "lamp"
	       && b.objs[0].parent == "Cellar"
	       && b.objs[0].hidden == a.objs[0].hidden
	       && b.objs[0].invisible == a.objs[0].invisible
	       && !b.objs[0].is_room,
	     "undo history: objects " + string_int (s));
      check (b.exits.size () == 1 && b.exits[0].src == "Room0"
	       && b.exits[0].dest == "Room1",
	     "undo history: exits " + string_int (s));
      check (b.timers.size () == 1 && b.timers[0].name == "fuse"
	       && b.timers[0].is_running && b.timers[0].interval == 10
	       && b.timers[0].elapsed == a.timers[0].elapsed,
	     "undo history: timers " + string_int (s));
      check (b.svars.size () == 1 && b.svars[0].name == "hero"
	       && b.svars[0].max () == 2
	       && b.svars[0].get (0) == "Rincewind"
	       && b.svars[0].get (2) == "third",
	     "undo history: string variables " + string_int (s));
      check (b.ivars.size () == 1 && b.ivars[0].name == "score"
	       && b.ivars[0].get (0) == 42 + (int) s,
	     "undo history: numeric variables " + string_int (s));
      check (b.items.size () == 1 && b.items[0] == "a rusty key",
	     "undo history: items " + string_int (s));
    }

  /* An empty history is legal -- the first turn has nothing to undo to. */
  std::vector<UndoState> none, none_back;
  check (deserialize_undo_history (serialize_undo_history (none), none_back)
	   && none_back.empty (),
	 "undo history: an empty history round-trips");

  /* The magic is what keeps a history written by an older engine, whose
     ObjectRecord::parent meant something else, from being replayed. */
  check (!deserialize_undo_history ("", back), "undo history: empty rejected");
  check (!deserialize_undo_history ("GEASUNDO1" + std::string (1, char (0)), back),
	 "undo history: superseded magic rejected");
  check (!deserialize_undo_history ("not a history at all", back),
	 "undo history: garbage rejected");

  /* Truncated at every byte: an autosave can be cut short, and each of these
     leaves a count promising more records than the stream can hold.  The point
     is that none of them reads past the end (run this under -fsanitize=address)
     or tries to allocate for a count it cannot honour. */
  for (size_t cut = 0; cut < wire.size (); cut++)
    {
      std::vector<UndoState> partial;
      deserialize_undo_history (wire.substr (0, cut), partial);
    }
  check (true, "undo history: no truncation reads past the end");
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
  std::cout << "timer round trip:\n";
  test_timer_roundtrip ();
  std::cout << "undo history round trip:\n";
  test_undo_history_roundtrip ();
  std::cout << "split_lines:\n";
  test_split_lines ();

  std::cout << (failures ? "FAILED" : "all passed") << " (" << failures
	    << " failure" << (failures == 1 ? "" : "s") << ")\n";
  return failures ? 1 : 0;
}

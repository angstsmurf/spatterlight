// geas-functions.inc -- Procedures and functions: run_procedure, user-defined
// and the builtin $functions$ (run_function).
//
// Split out of geas-runner.cc, which #includes it (same single-TU layout
// as the quest5/aslx-*.inc units); the code is unchanged from the split.

void geas_implementation::run_procedure (const string &pname, vector<string> args)
{
  GEAS_DBG << "run_procedure " << pname << " (" << args << ")\n";
  vector<string> backup = function_args;
  function_args = args;
  run_procedure (pname);
  function_args = backup;
}

void geas_implementation::run_procedure (const string &pname)
{
  for (uint i = 0; i < gf.size ("procedure"); i ++)
    if (ci_equal (gf.block ("procedure", i).name, pname))
      {
	const GeasBlock &proc = gf.block ("procedure", i);
	for (uint j = 0; j < proc.data.size(); j ++)
	  {
	    run_script(proc.data[j]);
	  }
	return;
      }
  gi->debug_print ("No procedure " + pname + " found.");
}

string geas_implementation::run_function (const string &pname, vector<string> args)
{
  GEAS_DBG << "run_function (w/ args) " << pname << " (" << args << ")\n";
  /* Parameter is handled specially because it can't change the stack */
  if (pname == "parameter")
    {
      if (args.size() != 1)
	{
	  gi->debug_print ("parameter called with " + string_int(args.size())
			   + " args");
	  return "";
	}
      uint num = parse_int (args[0]);
      if (0 < num && num <= function_args.size())
	{
	  GEAS_DBG << "   --> " << function_args[num-1] << "\n";
	  return function_args[num-1];
	}
      GEAS_DBG << "   --> too many arguments\n";
      return "";
    }
  vector<string> backup = function_args;
  function_args = args;
  for (size_t i = 0; i < args.size(); i ++)
    {
      set_svar ("quest.function.parameter." + string_int (i+1), args[i]);
    }
  string rv = run_function (pname);
  function_args = backup;
  return rv;
}

/* $removeformatting(...)$: strip Quest's own "|" markup, code by code
 * (RemoveFormatting, V4Game.cs:121-220).  The table is the original's, in the
 * original's order -- the one-letter codes are tested before the "x" ones they
 * share a letter with, "|s" eats three characters (a point size) and every
 * colour is two.
 *
 * The stale-length quirk is the original's too: `len` is declared outside the
 * loop and only assigned by a branch that matches, so an unrecognised code
 * removes as many characters as the last recognised one did, not one.  It is
 * reproduced because a game written against Quest was written against that. */
static string remove_formatting (const string &s)
{
  static const struct { const char *code; int len; } codes[] =
    {{"b", 1}, {"xb", 2}, {"u", 1}, {"xu", 2}, {"i", 1}, {"xi", 2},
     {"cr", 2}, {"cb", 2}, {"cl", 2}, {"cy", 2}, {"cg", 2},
     {"n", 1}, {"xn", 2}, {"s", 3}, {"jc", 2}, {"jl", 2}, {"jr", 2},
     {"w", 1}, {"c", 1}};

  string rv = s;
  int len = 0;
  for (;;)
    {
      std::string::size_type pos = rv.find ('|');
      if (pos == string::npos)
	return rv;
      /* Mid(s, pos + 1, 3): the three characters after the bar, or fewer at
       * the end of the string. */
      string code = rv.substr (pos + 1, 3);
      for (const auto &c: codes)
	if (code.rfind (c.code, 0) == 0)
	  {
	    len = c.len;
	    break;
	  }
      if (len == 0)
	len = 1;
      rv = rv.substr (0, pos) + (pos + len + 1 <= rv.length ()
				 ? rv.substr (pos + len + 1) : "");
    }
}

string geas_implementation::bad_arg_count (const string &fname)
{
  gi->debug_print ("Called " + fname + " with " +
		   string_int(function_args.size()) + " arguments.");
  return "";
}

string geas_implementation::run_function (const string &pname)
{
  GEAS_DBG << "geas_implementation::run_function (" << pname << ", " << function_args << ")\n";
  /* Built-in function names are case-sensitive, as in Quest: DoInternalFunction
   * compares each name with == on the un-lowercased string (V4Game.cs:6862 ff.),
   * so $UCase(...)$ is "No such function" there and here.  (geas is laxer than
   * Quest for *user-defined* names below -- Quest's block dictionary is
   * case-sensitive (DefineBlockParam, V4Game.cs:1195-1233), geas matches them
   * with ci_equal.) */
  if (pname == "getobjectname")
    {
      if (function_args.size() == 0)
	return bad_arg_count (pname);
      vector<string> where;
      for (size_t i = 1; i < function_args.size(); i ++)
	{
	  where.push_back (function_args[i]);
	}
      if (where.size() == 0)
	{
	  where.push_back (state.location);
	  where.push_back ("inventory");
	}
      bool is_internal = false;
      return get_obj_name (function_args[0], where, is_internal);
    }
  else if (pname == "loadmethod")
    {
      /* "normal" for a fresh game, "loaded" once a save has been restored. */
      return load_method_;
    }
  /* Quest's deprecated "gettag": the parameter of the first line in the named
   * ROOM block that begins with the given statement, with nested define blocks
   * skipped (FindStatement, V4Game.Part2.cs:6164-6189).  It is room-scoped even
   * for names that are also objects -- "gettag" predates the object namespace
   * (V4Game.cs:6896-6900) -- and a name with no room block yields "", because
   * DefineBlockParam hands back an empty block whose line range is empty.
   *
   * "The Devil's Bargain" ships a hand-rolled standard library that leans on it
   * throughout: a container is a room whose contents the player is walked into
   * with a silent goto, so its listing prefix is read back with
   * $gettag(<container>;prefix)$, and $gettag(<name>;look)$ returning the
   * literal "object" or "character" is how the library tells the two apart. */
  else if (pname == "gettag")
    {
      if (function_args.size() != 2)
	return bad_arg_count(pname);
      string room = trim (function_args[0]), tag = trim (function_args[1]);
      const GeasBlock *gb = gf.find_by_name ("room", room);
      if (gb == NULL)
	return "";
      /* readfile.cc rewrites a room's recognised tags as it reads them, so the
       * raw "prefix <...>" line is gone by now and the value has to come back
       * out of the parsed property (and action) lists instead.  Quest returns
       * the *first* matching line where these return the last assignment, which
       * only differs for a block that states the same tag twice. */
      string rv;
      if (gf.get_room_property (room, tag, rv) || rv != "!")
	return eval_string (rv);
      if (gf.get_room_action (room, tag, rv))
	return eval_string (rv);
      /* Anything readfile.cc did not recognise is still in the block verbatim
       * (a room's "suffix", say), so fall back to Quest's own line scan. */
      for (const string &line: gb->data)
	{
	  string l = lcase (trim (line));
	  if (l.compare (0, tag.length (), lcase (tag)) != 0)
	    continue;
	  /* GetParameter: from the first '<' to the first '>' (V4Game.cs:1865),
	   * then the usual #/%/$ substitutions. */
	  string::size_type lt = line.find ('<'), gt = line.find ('>');
	  if (lt == string::npos || gt == string::npos || gt < lt)
	    return "";
	  return eval_string (line.substr (lt + 1, gt - lt - 1));
	}
      return "";
    }
  else if (pname == "locationof")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return get_obj_parent (function_args[0]);
    }
  else if (pname == "objectproperty")
    {
      if (function_args.size() != 2)
	return bad_arg_count(pname);

      string rv;
      get_obj_property (function_args[0], function_args[1], rv);
      return rv;
    }
  else if (pname == "timerstate")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      string timername = function_args[0];
      /* Timer names CI, like timeron/timeroff (V4Game.cs:7087). */
      for (const auto &timer: state.timers)
	if (ci_equal (timer.name, timername))
	  return timer.is_running ? "1" : "0";
      return "!";
    }
  else if (pname == "displayname")
    {
      if (function_args.size() != 1)
	return bad_arg_count (pname);

      return displayed_name (function_args[0]);
    }
  else if (pname == "capfirst")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return pcase (function_args[0]);
    }
  else if (pname == "instr")
    {
      if (function_args.size() != 2 && function_args.size() != 3)
	return bad_arg_count(pname);

      std::string::size_type rv;
      if (function_args.size() == 2)
	rv = function_args[0].find (function_args[1]);
      else
	{
	  /* 3-arg form is instr(start, text, search); Quest's start is
	   * 1-based. */
	  int start = parse_int (function_args[0]);
	  if (start < 1)
	    start = 1;
	  rv = function_args[1].find (function_args[2], start - 1);
	}

      /* Quest's Instr is 1-based and returns 0 when the search string is not
       * found (it was returning npos, i.e. a huge bogus position). */
      if (rv == string::npos)
	return "0";
      else
	return string_int (rv + 1);
    }
  else if (pname == "lcase")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return lcase (function_args[0]);
    }
  else if (pname == "left")
    {
      if (function_args.size() != 2)
	return bad_arg_count(pname);

      uint i = parse_int (function_args[1]);
      if (i > function_args[0].length())
	return function_args[0];
      else
	return function_args[0].substr (0, i);
    }
  else if (pname == "lengthof")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return string_int (function_args[0].length());
    }
  else if (pname == "mid")
    {
      /* Quest's Mid is 1-based (it is VB's), and its length argument is
       * optional: $mid(<s>; <start>)$ returns the rest of the string from
       * start.  The bundled type library uses both forms (TLFcontentFormat
       * scans a string with $mid(#s#;%n%;1)$ for n = 1..lengthof, then calls
       * the 2-argument form), so a 0-based start or a hard 3-argument
       * requirement silently corrupts its output. */
      if (function_args.size() != 2 && function_args.size() != 3)
	return bad_arg_count(pname);

      const string &str = function_args[0];
      int start = parse_int (function_args[1]);
      if (start < 1)
	start = 1;
      size_t pos = (size_t) (start - 1);
      if (pos >= str.length())
	return "";
      if (function_args.size() == 2)
	return str.substr (pos);
      int len = parse_int (function_args[2]);
      if (len <= 0)
	return "";
      if (pos + (size_t) len > str.length())
	return str.substr (pos);
      return str.substr (pos, (size_t) len);
    }
  else if (pname == "right")
    {
      if (function_args.size() != 2)
	return bad_arg_count(pname);

      uint size = parse_int (function_args[1]);
      if (size > function_args[0].length())
	return function_args[0];
      return function_args[0].substr (function_args[0].length() - size);
    }
  else if (pname == "ubound")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      /* Highest defined index of an array variable (string or numeric).
       * Records carry index 0 plus every set index, so max() == size()-1. */
      for (const auto &i: state.svars)
	if (ci_equal (i.name, function_args[0]))
	  return string_int (i.max());
      for (const auto &i: state.ivars)
	if (ci_equal (i.name, function_args[0]))
	  return string_int (i.max());
      return "0";
    }
  else if (pname == "round")
    {
      /* Quest "$round(<expr>; <decimals>)$": evaluate the (double) expression
       * and round to the given number of decimal places (default 0).  Format
       * without trailing zeros, matching .NET's double-to-string. */
      if (function_args.size () < 1)
	return bad_arg_count (pname);
      double v = eval_double (function_args[0]);
      int dec = (function_args.size () >= 2) ? parse_int (function_args[1]) : 0;
      if (dec < 0)
	dec = 0;
      double p = pow (10.0, dec);
      double r = (v < 0 ? -1.0 : 1.0) * floor (fabs (v) * p + 0.5) / p;
      char buf[64];
      snprintf (buf, sizeof buf, "%.*f", dec, r);
      string out = buf;
      if (dec > 0)
	{
	  std::string::size_type last = out.find_last_not_of ('0');
	  if (last != string::npos && out[last] == '.')
	    last--;
	  out = out.substr (0, last + 1);
	}
      return out;
    }
  else if (pname == "ucase")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return ucase (function_args[0]);
    }
  else if (pname == "rand")
    {
      if (function_args.size() != 2)
	return bad_arg_count(pname);
      /* Signed, and widened, on purpose.  The bounds were read into uints, so a
       * negative range ($rand(-3;3)$) wrapped into a huge positive result, and
       * a reversed one ($rand(1;%n%)$ with n == 0) made the modulus zero -- a
       * division by zero, i.e. undefined: observed as a garbage value at -O2,
       * but a SIGFPE is equally allowed.  A reversed pair is now read as the
       * range the game plainly meant. */
      long long lower = parse_int (function_args[0]),
	upper = parse_int (function_args[1]);
      if (upper < lower)
	{
	  long long tmp = lower;
	  lower = upper;
	  upper = tmp;
	}
      unsigned long long range = (unsigned long long) (upper - lower) + 1ULL;

      /* Quest maps a float draw onto the range -- Int(NextDouble() * (b-a+1))
       * + a over .NET's Random (DoInternalFunction "rand",
       * V4Game.cs:6982-6988) -- so no mapping here can reproduce its values
       * and games cannot depend on specific draws.  The modulo mapping is kept
       * deliberately: the seeded walkthrough corpus was derived against this
       * exact draw sequence, and xoshiro128** has full-quality low bits, so
       * the classic low-bits worry does not apply. */
      return string_int (lower + (long long) (erkyrath_random() % range));
    }
  else if (pname == "speechenabled")
    {
      if (function_args.size() != 0)
	return bad_arg_count(pname);

      /* Deliberately 0 where QuestViva hardcodes "1" (V4Game.cs:7189):
       * GeasInterface::speak is r_not_supported in every host, so a game
       * branching on this would route its text into a no-op. */
      return "0";
    }
  else if (pname == "symbol")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      if (function_args[0] == "gt")
	return ">";
      else if (function_args[0] == "lt")
	return "<";
      gi->debug_print ("Bad symbol argument: " + function_args[0]);
      return "";
    }
  else if (pname == "numberparameters")
    return string_int (function_args.size());
  /* "thisobject" and "objectname" are the same thing -- the calling object's
   * real name -- and "thisobjectname" is its alias (V4Game.cs:7175-7183),
   * which Quest fills in with the name itself for an object that has none,
   * so it is displayed_name here. */
  else if (pname == "thisobject" || pname == "objectname")
    return this_object;
  else if (pname == "thisobjectname")
    return displayed_name (this_object);
  else if (pname == "timerinterval")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      /* Timer names CI and the miss value "!", as in timerstate above; the
       * leading space is VB's Str() and is Quest's too -- "ubound" trims its
       * result and this one does not (V4Game.cs:7118-7130). */
      for (const auto &timer: state.timers)
	if (ci_equal (timer.name, function_args[0]))
	  return " " + string_int (timer.interval);
      gi->debug_print ("No such timer '" + function_args[0] + "'");
      return "!";
    }
  else if (pname == "removeformatting")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return remove_formatting (function_args[0]);
    }
  /* An exit *object*, which is a 4.10 notion geas has no model for: its exits
   * are still the direction properties of a room.  Quest hands back the exit's
   * object name, or "" when there is none (V4Game.cs:7196-7205); "" is the
   * closer of the two answers to give, and below 4.10 the name is not a
   * function at all and falls through to the [ERROR] below. */
  else if (pname == "findexit" && asl_version_ >= 410)
    {
      gi->debug_print ("findexit is not supported");
      return "";
    }

  /* disconnectedby, id, name */

  string rv = "";

  for (uint i = 0; i < gf.size ("function"); i ++)
    if (ci_equal (gf.block ("function", i).name, pname))
      {
	const GeasBlock &proc = gf.block ("function", i);
	GEAS_DBG << "Running function " << proc << endl;
	for (uint j = 0; j < proc.data.size(); j ++)
	  {
	    GEAS_DBG << "  Running line #" << j << ": " << proc.data[j] << endl;
	    run_script(proc.data[j], rv);
	  }
	return rv;
      }
  /* A name that is neither a built-in nor a `define function` block prints a
   * marker rather than nothing: DoFunction logs "No such function" and returns
   * the literal "[ERROR]" (V4Game.cs:6763-6771).  Returning "" instead closed
   * the text up over the hole, which hid the author's typo from anyone reading
   * a transcript -- and hid it from this port's own corpus diffs. */
  gi->debug_print ("No function " + pname + " found.");
  return "[ERROR]";
}

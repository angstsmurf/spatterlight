// geas-panes.inc -- Side panes and text substitution: the inventory/room-
// contents/exits/status panes, the pre-2.80 item and collectable tables, and
// $...$ evaluation (eval_string and friends).
//
// Split out of geas-runner.cc, which #includes it (same single-TU layout
// as the quest5/aslx-*.inc units); the code is unchanged from the split.

v2string geas_implementation::get_inventory ()
{
  /* Below ASL 2.80 the inventory *is* the item table, and objects have nothing
   * to do with it: both the "inventory" command and the inventory pane walk
   * _items from 1 to _numberItems and take every entry whose Got flag is set
   * (V4Game.Part2.cs:4526-4535 and 7409-7417).  So the order is the order the
   * game block's `items <...>' line declares, not the order the player picked
   * things up in, and an object that happens to be filed under "inventory" is
   * not listed at all. */
  if (asl_version_ < 280)
    {
      v2string rv;
      for (const string &decl: item_table_)
	{
	  bool held = false;
	  for (const string &it: state.items)
	    if (it == decl) { held = true; break; }
	  if (held)
	    {
	      vstring tmp;
	      tmp.push_back (decl);
	      tmp.push_back ("object");
	      rv.push_back (tmp);
	    }
	}
      return rv;
    }
  return get_room_contents ("inventory");
}

v2string geas_implementation::get_room_contents ()
{
  return get_room_contents (state.location);
}

v2string geas_implementation::get_room_contents (const string &room)
{
  v2string rv;
  string objname;
  for (const auto &i: state.objs)
    /* Things filed under this room -- which includes whatever is inside a
     * container here, since a container's contents share its room.  This
     * record's own field, so that two objects sharing a name are not both
     * credited to whichever room the first was defined in (see
     * regen_var_objects); the hidden check, kept current by the visibility
     * sweep, drops the contents of closed containers. */
    if (ci_equal (i.parent, room))
      {
	objname = i.name;
	if (!has_obj_property (objname, "invisible") &&
	    !has_obj_property (objname, "hidden"))
	  {
	    vstring tmp;

	    string print_name, temp_str;
	    if (!get_obj_property (objname, "alias", print_name))
	      print_name = objname;
	    tmp.push_back (print_name);

	    string otype;
	    if (!get_obj_property (objname, "displaytype", otype))
	      otype = "object";
	    tmp.push_back (otype);
	    /* The internal name, so a host that lists this entry can ask for its
	       verb menu without re-resolving the printed alias (which may be
	       ambiguous, and would prompt). */
	    tmp.push_back (objname);
	    rv.push_back (tmp);
	  }
      }
  return rv;
}

v2string geas_implementation::get_object_verbs (const string &obj)
{
  /* The same list "verbs <object>" prints, for a host that shows the verb
     menu in its pane; the caller passes the internal name from a
     get_room_contents entry, so no object resolution happens here. */
  return object_verbs (obj);
}

v2string geas_implementation::get_room_exits ()
{
  v2string rv;

  /* Each exit is a {display label, click command} pair.  The label is what the
   * pane shows; the command is what a host runs when the label is clicked as a
   * hyperlink -- the same thing the player would type. */

  /* Directional exits, in the canonical order, skipping the trailing "out"
   * (handled separately below so it can show its destination place). */
  for (size_t i = 0; i + 1 < ARRAYSIZE (dir_names); i ++)
    if (exit_dest (state.location, dir_names[i]) != "")
      {
	string label = dir_names[i];
	if (!label.empty())
	  label[0] = toupper ((unsigned char) label[0]);
	rv.push_back ({label, dir_names[i]});
      }

  /* The "out" exit, with the place it leads to when one is named.  Split here
   * rather than read back from quest.doorways.out: that variable is a pre-4.10
   * one and a 4.10 game never sets it (see regen_var_dirs), while this pane is
   * ours and is drawn at every version. */
  if (exit_dest (state.location, "out") != "")
    {
      string out, prefix, raw = exit_dest (state.location, "out");
      if (raw.find ('<') != string::npos)
	raw = "";   /* an "out { ... }" script is not a destination */
      split_exit_dest (raw, out, prefix);
      rv.push_back ({out != "" ? "Out to " + out : string ("Out"), "out"});
    }

  /* Named "place" exits, listed by their displayed destination name.  Clicking
   * one navigates with "go to <target>", where the target matches what the
   * "go to" handler compares against: the destination room name for a scripted
   * place, or the displayed name for a plain one. */
  for (const auto &place: get_places (state.location))
    if (place.size() > 2 && place[2] != "")
      {
	bool scripted = (place.size () == 5);
	const string &target = scripted ? place[3] : place[2];
	rv.push_back ({place[2], "go to " + target});
      }

  return rv;
}

namespace {
  /* VB's Val(): the value of the leading numeric part of the string, 0 if it
   * does not start with a number. */
  double vb_val (const string &s)
  { return atof (trim (s).c_str ()); }

  /* VB's IsNumeric(): true when the whole string is a number.  Quest uses it to
   * decide whether a collectable operand is a literal or the name of another
   * collectable. */
  bool vb_is_numeric (const string &s)
  {
    string t = trim (s);
    if (t == "")
      return false;
    const char *p = t.c_str ();
    char *end = NULL;
    strtod (p, &end);
    return end != NULL && *end == '\0';
  }
}

void geas_implementation::set_up_items ()
{
  item_table_.clear ();
  std::string::size_type c1, c2;
  /* Quest declares its "have we reached the end of the list?" flag once, ahead
     of the loop over the game block, and never resets it (SetUpItemArrays,
     V4Game.Part2.cs:6963).  So a second items/possitems line contributes only
     its first entry before the do/while falls straight out again -- a VB6 bug,
     but one a game could be relying on, so keep it. */
  bool last_item = false;

  for (const string &line: gf.block ("game", 0).data)
    {
      string keyword = lcase (first_token (line, c1, c2));
      /* Two spellings of the same declaration, both read into the same table
	 and both appended to it: unlike the collectables, a second line adds
	 to the list rather than replacing it. */
      if (keyword != "items" && keyword != "possitems")
	continue;
      string tok = next_token (line, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print (nonparam (keyword, line));
	  continue;
	}
      /* Read with no variable substitution, as the collectables are: this runs
	 before the game does. */
      string s = trim (param_contents (tok));
      if (s == "")
	continue;

      for (std::string::size_type pos = 0; ; )
	{
	  /* The same separator rule as the collectables: the first comma after
	     the entry's start, or a semicolon when the rest of the line holds
	     no comma at all.  King of Wallwood Wood writes its possitems with
	     commas, everything else in the corpus with semicolons. */
	  std::string::size_type sep = s.find (',', pos + 1);
	  if (sep == string::npos)
	    sep = s.find (';', pos + 1);
	  if (sep == string::npos)
	    {
	      sep = s.length ();
	      last_item = true;
	    }
	  item_table_.push_back (trim (s.substr (pos, sep - pos)));
	  pos = sep + 1;
	  if (last_item)
	    break;
	}
    }
}

const string *geas_implementation::find_item (const string &name) const
{
  for (const string &it: item_table_)
    if (it == name)
      return &it;
  return NULL;
}

void geas_implementation::set_up_collectables ()
{
  collectables.clear ();
  std::string::size_type c1, c2;

  for (const string &line: gf.block ("game", 0).data)
    {
      if (lcase (first_token (line, c1, c2)) != "collectables")
	continue;
      string tok = next_token (line, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print (nonparam ("collectables", line));
	  continue;
	}
      /* A second "collectables" line replaces the first rather than adding to
	 it: SetUpCollectables restarts its counter inside the loop over the game
	 block (V4Game.Part2.cs:6866). */
      collectables.clear ();
      /* Read with no variable substitution -- SetUpCollectables passes
	 convertStringVariables = false, since this runs before the game does. */
      string s = trim (param_contents (tok));
      if (s == "")
	continue;

      for (std::string::size_type pos = 0; ; )
	{
	  /* Items are separated by the first comma after the item's start, or by
	     a semicolon if the declaration has no commas at all -- Quest looks
	     for the comma first and only then for the semicolon, so a comma
	     inside a display string does split the list. */
	  std::string::size_type sep = s.find (',', pos + 1);
	  if (sep == string::npos)
	    sep = s.find (';', pos + 1);
	  bool last = (sep == string::npos);
	  if (last)
	    sep = s.length ();
	  string info = trim (s.substr (pos, sep - pos));

	  std::string::size_type sp1 = info.find (' ');
	  std::string::size_type ep = info.find ('=');
	  if (sp1 == string::npos || ep == string::npos || ep < sp1)
	    gi->debug_print ("Malformed collectable '" + info + "' in " + line);
	  else
	    {
	      CollectableDef cd;
	      cd.name = trim (info.substr (0, sp1));
	      std::string::size_type sp2 = info.find (' ', ep);
	      if (sp2 == string::npos)
		sp2 = info.length ();
	      cd.type = trim (info.substr (sp1 + 1, ep - sp1 - 1));
	      cd.init = vb_val (info.substr (ep + 1, sp2 - ep - 1));
	      /* A "d" in front of the type means "leave the line out of the
		 status pane while the value is zero". */
	      if (cd.type.length () > 0 && cd.type[0] == 'd')
		{
		  cd.type = cd.type.substr (1);
		  cd.display_when_zero = false;
		}
	      std::string::size_type obp = info.find ('[');
	      std::string::size_type cbp = info.find (']');
	      if (obp == string::npos || cbp == string::npos || cbp < obp)
		cd.display = "<def>";
	      else
		cd.display = trim (info.substr (obp + 1, cbp - obp - 1));
	      collectables.push_back (cd);
	    }

	  if (last)
	    break;
	  pos = sep + 1;
	}
    }

  for (const CollectableDef &cd: collectables)
    set_ivar (collectable_var (cd.name), (size_t) 0, cd.init);
}

bool geas_implementation::find_collectable (const string &name,
					    size_t &index) const
{
  for (size_t i = 0; i < collectables.size (); i ++)
    if (collectables[i].name == name)
      {
	index = i;
	return true;
      }
  return false;
}

bool geas_implementation::find_collectable_ci (const string &name,
					       size_t &index) const
{
  for (size_t i = 0; i < collectables.size (); i ++)
    if (ci_equal (collectables[i].name, name))
      {
	index = i;
	return true;
      }
  return false;
}

double geas_implementation::get_collectable (const string &name) const
{
  size_t index, n;
  if (find_collectable (name, index)
      && find_ivar (collectable_var (collectables[index].name), n))
    return state.ivars[n].getd (0);
  return 0.0;
}

void geas_implementation::set_collectable (size_t index, double value)
{
  const string &type = collectables[index].type;

  /* CheckCollectable: keep the value inside the range its type declares, so a
     game does not have to guard every "set <Health; +10>" itself. */
  if (type == "%" && value > 100.0)
    value = 100.0;
  if ((type == "%" || type == "p") && value < 0.0)
    value = 0.0;

  std::string::size_type rpos = type.find ('r');
  if (rpos != string::npos)
    {
      if (rpos == 0)
	{
	  /* "r50" -- maximum only.  Quest reads the number with
	     Mid(type, Len(type) - 1), a slip that happens to be right for a
	     two-digit bound and wrong for any other; it is reproduced here
	     because games were written against what it does. */
	  if (type.length () >= 2)
	    {
	      double max = vb_val (type.substr (type.length () - 2));
	      if (value > max)
		value = max;
	    }
	}
      else if (rpos == type.length () - 1)
	{
	  double min = vb_val (type.substr (0, type.length () - 1));
	  if (value < min)
	    value = min;
	}
      else
	{
	  double min = vb_val (type.substr (0, rpos));
	  double max = vb_val (type.substr (rpos + 1));
	  if (value > max)
	    value = max;
	  if (value < min)
	    value = min;
	}
    }

  set_ivar (collectable_var (collectables[index].name), (size_t) 0, value);
}

void geas_implementation::run_set_collectable (const string &param)
{
  std::string::size_type scp = param.find (';');
  if (scp == string::npos)
    {
      gi->debug_print ("No such collectable '" + param + "'");
      return;
    }
  string name = trim (param.substr (0, scp));
  string newval = trim (param.substr (scp + 1));

  size_t index;
  if (!find_collectable (name, index))
    {
      gi->debug_print ("No such collectable '" + param + "'");
      return;
    }

  string op = newval.substr (0, 1);
  string operand = trim (newval.substr (op.length ()));
  double val = vb_is_numeric (operand) ? vb_val (operand)
					: get_collectable (operand);
  double now = get_collectable (collectables[index].name);

  if (op == "+")
    set_collectable (index, now + val);
  else if (op == "-")
    set_collectable (index, now - val);
  else if (op == "=")
    set_collectable (index, val);
}

bool geas_implementation::eval_has (const string &param)
{
  std::string::size_type scp = param.find (';');
  if (scp == string::npos)
    {
      gi->debug_print ("No such collectable in " + param);
      return false;
    }
  string name = trim (param.substr (0, scp));
  string newval = trim (param.substr (scp + 1));

  size_t index;
  if (!find_collectable (name, index))
    {
      gi->debug_print ("No such collectable in " + param);
      return false;
    }

  string op = newval.substr (0, 1);
  string operand = trim (newval.substr (op.length ()));
  double check = vb_is_numeric (operand) ? vb_val (operand)
					 : get_collectable (operand);
  double now = get_collectable (collectables[index].name);

  /* "+" is strictly greater and "-" strictly less -- "if has <Money; +4999>"
     wants 5000, not 4999.  Anything else (including the "has <x;1>" that games
     write when they mean "= 1") is simply false. */
  if (op == "+")
    return now > check;
  if (op == "-")
    return now < check;
  if (op == "=")
    return now == check;
  return false;
}

string geas_implementation::collectable_display (size_t index) const
{
  const CollectableDef &cd = collectables[index];
  double value = get_collectable (cd.name);
  string display;

  if (cd.display == "<def>")
    display = "You have " + fmt_double (value) + " " + cd.name;
  else if (cd.display == "")
    display = "<null>";
  else
    {
      std::string::size_type ep = cd.display.find ('!');
      if (ep == string::npos)
	display = cd.display;
      else
	display = cd.display.substr (0, ep) + fmt_double (value)
	  + cd.display.substr (ep + 1);

      /* "*s*" is the plural: dropped when the value is exactly one. */
      std::string::size_type star = display.find ('*');
      if (star != string::npos)
	{
	  std::string::size_type star2 = display.find ('*', star + 1);
	  if (star2 == string::npos)
	    star2 = display.length ();
	  string between = display.substr (star + 1, star2 - star - 1);
	  display = display.substr (0, star)
	    + (value != 1.0 ? between : string (""))
	    + (star2 < display.length () ? display.substr (star2 + 1) : string (""));
	}
    }

  if (value == 0.0 && !cd.display_when_zero)
    display = "<null>";
  return display;
}

vstring geas_implementation::get_status_vars ()
{
  vstring rv;

  /* Below ASL 2.84 the status pane is the collectables, one line each, and
     "status <...>" variables do not exist yet (UpdateItems,
     V4Game.Part2.cs:7398-7420). */
  if (asl_version_ < 284 && !collectables.empty ())
    {
      for (size_t i = 0; i < collectables.size (); i ++)
	{
	  string line = collectable_display (i);
	  if (line != "<null>")
	    rv.push_back (line);
	}
      return rv;
    }

  string tok, line;
  std::string::size_type c1, c2;

  for (size_t i = 0; i < gf.size("variable"); i ++)
    {
      const GeasBlock &gb = gf.block ("variable", i);

      bool nozero = false;
      string disp;
      bool is_numeric = true;

      GEAS_DBG << "g_s_v: " << gb << endl;

      for (const string &line: gb.data)
	{
	  GEAS_DBG << "  g_s_v:  " << line << endl;
	  tok = first_token (line, c1, c2);
	  /* The block's keywords are CI (BeginsWith, V4Game.Part2.cs:704-728);
	   * the "type string" *value* is not -- Quest compares the raw text
	   * (ibid. 707), so "type String" is an unrecognised type there and
	   * here. */
	  if (ci_equal (tok, "display"))
	    {
	      tok = next_token (line, c1, c2);

	      if (ci_equal (tok, "nozero"))
		{
		  nozero = true;
		  tok = next_token (line, c1, c2);
		}
	      if (!is_param (tok))
		gi->debug_print ("Expected param after display: " + line);
	      else
		disp = tok;
	    }
	  else if (ci_equal (tok, "type"))
	    {
	      tok = next_token (line, c1, c2);
	      if (tok == "string")
		is_numeric = false;
	    }
	}

      GEAS_DBG << "  g_s_v, block 2, tok == '" << tok << "'" << endl;
      if (! (is_numeric && nozero && get_ivar (gb.name) == 0) && disp != "")
	{
	  disp = param_contents (disp);
	  string outval = "";
	  for (size_t j = 0; j < disp.length(); j ++)
	    if (disp[j] == '!')
	      {
		if (is_numeric)
		  outval = outval + string_int (get_ivar (gb.name));
		else
		  outval = outval + get_svar (gb.name);
	      }
	    else if (disp[j] == '*')
	      {
		size_t k;
		for (k = j + 1; k < disp.length() && disp[k] != '*'; k ++)
		  ;
		if (!is_numeric || get_ivar (gb.name) != 1)
		  outval = outval + disp.substr (j+1, k - j - 1);
		j = k;
	      }
	    else
	      outval = outval + disp[j];
	  rv.push_back (eval_string (outval));
	}
    }
  return rv;
}

vector<bool> geas_implementation::get_valid_exits()
{
  vector<bool> rv;
  GEAS_DBG << "Getting valid exits\n";
  rv.push_back (exit_dest (state.location, "northwest") != "");
  rv.push_back (exit_dest (state.location, "north") != "");
  rv.push_back (exit_dest (state.location, "northeast") != "");
  rv.push_back (exit_dest (state.location, "west") != "");
  rv.push_back (exit_dest (state.location, "out") != "");
  rv.push_back (exit_dest (state.location, "east") != "");
  rv.push_back (exit_dest (state.location, "southwest") != "");
  rv.push_back (exit_dest (state.location, "south") != "");
  rv.push_back (exit_dest (state.location, "southeast") != "");
  rv.push_back (exit_dest (state.location, "up") != "");
  rv.push_back (exit_dest (state.location, "down") != "");
  GEAS_DBG << "Done getting valid exits\n";

  return rv;
}

void geas_implementation::print_eval_p (const string &s)
{ print_formatted (pcase (eval_string (s))); }

void geas_implementation::print_eval (const string &s)
{ print_formatted (eval_string (s)); }

/* Quest's GetParameter finishes by evaluating any inline {expression} in the
   parameter it just built -- "msg <2 + 2 = {2+2}>" prints 4 -- with "{{" as the
   escape for a literal brace (EvaluateInlineExpressions, V4Game.cs:2258-2320,
   gated on ASL 3.91).  It applies to every parameter, so games use it to do
   arithmetic on object properties: YOU ARE A TIGER's whiskey shrinks with
   "property <jack_daniels; volume = { #jack_daniels:volume# - 10 }>", and geas,
   which stored the braces verbatim, could never empty the bottle.

   One deliberate difference: Quest replaces the *whole* parameter with the
   string "<ERROR>" when the expression does not evaluate, and its arithmetic is
   not ours -- so a brace geas cannot evaluate is left exactly as it was found
   rather than destroying the surrounding text. */
string geas_implementation::eval_inline_exprs (const string &s)
{
  if (asl_version_ < 391 || s.find ('{') == string::npos)
    return s;

  string rv;
  for (std::string::size_type i = 0; i < s.length(); i ++)
    {
      if (s[i] != '{')
	{
	  rv += s[i];
	  continue;
	}
      if (i + 1 < s.length() && s[i+1] == '{')     /* "{{" is a literal "{" */
	{
	  rv += '{';
	  i ++;
	  continue;
	}
      std::string::size_type close = s.find ('}', i + 1);
      if (close == string::npos)
	{
	  gi->debug_print ("eval_inline_exprs: expected } in '" + s + "'");
	  rv += s.substr (i);
	  break;
	}
      string expr = trim (s.substr (i + 1, close - i - 1)), val;
      char *num_end = NULL;
      if (expr != "")
	strtod (expr.c_str (), &num_end);
      if (eval_numeric_expr (expr, val))
	rv += val;
      else if (num_end != NULL && *num_end == '\0')
	rv += expr;      /* no operators: Quest hands the element back as it is */
      else
	{
	  gi->debug_print ("eval_inline_exprs: not an expression: '" + expr + "'");
	  rv += s.substr (i, close - i + 1);
	}
      i = close;
    }
  /* As Quest does after its own pass, "}}" collapses to "}". */
  for (std::string::size_type i = 0; (i = rv.find ("}}", i)) != string::npos; i ++)
    rv.erase (i, 1);
  return rv;
}

string geas_implementation::eval_string (const string &s)
{
  return eval_inline_exprs (eval_string_body (s));
}

string geas_implementation::eval_string_body (const string &s)
{
  string rv;
  std::string::size_type i, j;
  bool do_print = (s.find('$') != string::npos);
  if (do_print) GEAS_DBG << "eval_string (" << s << ")\n";
  for (i = 0; i < s.length(); i ++)
    {
      if (i + 1 < s.length() && s[i] == '#' && s[i+1] == '@')
	{
	  for (j = i + 1; j < s.length() && s[j] != '#'; j ++)
	    ;
	  if (j == s.length())
	    {
	      gi->debug_print ("eval_string: Unmatched hash in " + s);
	      break;
	    }
	  rv = rv + displayed_name (get_svar (s.substr (i+2, j-i-2)));
	  i = j;
	}
      else if (s[i] == '#')
	{
	  for (j = i + 1; j < s.length() && s[j] != '#'; j ++)
	    ;
	  /* Quest replaces the *entire* parameter with "<ERROR>" when a conversion
	   * character has no partner (ConvertParameter, V4Game.cs:6697-6701), and
	   * logs it as a WarningError the player never sees.  Truncating at the
	   * stray character instead silently dropped the rest of the text. */
	  if (j == s.length())
	    {
	      gi->debug_print ("Line parameter <" + s + "> has missing #");
	      return "<ERROR>";
	    }
	  std::string::size_type k;
	  for (k = i + 1; k < j && s[k] != ':'; k ++)
	    ;
	  if (k == j && j == i + 1)
	    rv += "#";
	  else if (k == j)
	    rv += get_svar (s.substr (i+1, j-i-1));
	  else
	    {
	      /* A property the object hasn't got expands to "!", not to nothing:
	       * GetStringContents reaches GetObjectProperty with its default
	       * logError = true, which logs the miss and returns "!"
	       * (V4Game.Part2.cs:2581, V4Game.cs:6249-6252).  stdverbs.lib makes
	       * this visible everywhere -- an object with no `article` answers
	       * "You can't buy !.". */
	      string propname = s.substr (k+1, j-k-1);
	      if (s[i+1] == '(')
		{
		  if (s[k-1] != ')')
		    {
		      gi->debug_print ("e_s: Missing paren in '" +
				       s.substr (i, j-i) + "' of '" + s + "'");
		      break;
		    }
		  string objvar = s.substr (i+2, k-i-3);
		  string objname = get_svar (objvar);
		  string tmp;
		  if (get_obj_property (objname, propname, tmp))
		    rv += tmp;
		  else
		    {
		      gi->debug_print ("e_s: Evaluating nonexistent object prop "
				       "{" + objname + "}:{" + propname + "}");
		      rv += "!";
		    }
		}
	      else
		{
		  string objname = s.substr (i+1, k-i-1);
		  string tmp;
		  if (get_obj_property (objname, propname, tmp))
		    rv += tmp;
		  else
		    {
		      gi->debug_print ("e_s: Evaluating nonexistent var " + objname);
		      rv += "!";
		    }
		}
	    }
	  i = j;
	}
      else if (s[i] == '%')
	{
	  for (j = i + 1; j < s.length() && s[j] != '%'; j ++)
	    ;
	  if (j == s.length())
	    {
	      gi->debug_print ("Line parameter <" + s + "> has missing %");
	      return "<ERROR>";
	    }
	  if (j == i + 1)
	    rv += "%";
	  else
	    rv += fmt_double (get_dvar (s.substr (i+1, j-i-1)));
	  i = j;
	}
      /* "~name~" is a collectable's value.  The syntax belongs to Quest 2.x and
	 was dropped in ASL 3.20, from which version a ~ is just a ~
	 (ConvertParameter, V4Game.cs:1884-1893 and 6723-6726). */
      else if (s[i] == '~' && asl_version_ < 320)
	{
	  for (j = i + 1; j < s.length() && s[j] != '~'; j ++)
	    ;
	  if (j == s.length())
	    {
	      gi->debug_print ("Line parameter <" + s + "> has missing ~");
	      return "<ERROR>";
	    }
	  if (j == i + 1)
	    rv += "~";
	  else
	    rv += fmt_double (get_collectable (s.substr (i+1, j-i-1)));
	  i = j;
	}
      else if (s[i] == '$')
	{
	  std::string::size_type j = s.find ('$', i + 1);
	  /* Quest applies ConvertParameter once per conversion character, with
	   * one body for all four, so an unpartnered '$' replaces the whole
	   * parameter with "<ERROR>" exactly as an unpartnered '#', '%' or '~'
	   * does (V4Game.cs:6697-6701).  Passing the rest of the string through
	   * instead was the one arm left over. */
	  if (j == string::npos)
	    {
	      gi->debug_print ("Line parameter <" + s + "> has missing $");
	      return "<ERROR>";
	    }
	  /* "$$" is the escape for a literal dollar, the same rule the other three
	   * conversion characters follow: ConvertParameter takes the text between
	   * the pair, finds it empty and appends the character itself
	   * (V4Game.cs:6705-6708).  Evaluating the empty name as a function
	   * instead swallowed the pair whole -- Pure Chaos writes its prices as
	   * "$$1000000" and the player was shown "1000000". */
	  if (j == i + 1)
	    {
	      rv += "$";
	      i = j;
	      continue;
	    }
	  string tmp1 = s.substr (i + 1, j - i - 1);
	  GEAS_DBG << "e_s: first substr was '" << tmp1 << "'\n";
	  string tmp = eval_string_body (tmp1);
	  GEAS_DBG << "e_s: eval substr " + s + "': '" + tmp + "'\n";

	  string func_eval;

	  std::string::size_type paren_open, paren_close;
	  if ((paren_open = tmp.find ('(')) == string::npos)
	    {
	      /* A bare $name$ user-function call takes no arguments, so reset
	       * function_args -- otherwise it inherits the leftover args of the
	       * previously evaluated function (e.g. $desertroom$ seeing the args
	       * of a prior $desertroom(x;y)$ and mis-reading $numberparameters$).
	       * Builtins like numberparameters/parameter instead inspect the
	       * enclosing procedure's args, so leave function_args intact. */
	      bool is_user_func = false;
	      for (uint k = 0; k < gf.size ("function"); k ++)
		if (ci_equal (gf.block ("function", k).name, tmp))
		  { is_user_func = true; break; }
	      if (is_user_func)
		func_eval = run_function (tmp, vector<string> ());
	      else
		func_eval = run_function (tmp);
	    }
	  else
	    {
	      /* The *last* right paren, as Quest's DoFunction does with
	       * InStrRev (V4Game.cs:6745): the argument list has already had its
	       * #vars# and %vars% substituted, so a value carrying parens of its
	       * own must not end it early.  Barbarian's unwield command computes
	       * $left(#command1#; %length1%)$ over "Battle axe (wielded)", and
	       * stopping at the first ')' cut the arg list down to one argument,
	       * so the function returned nothing and the unwielded weapon was
	       * never handed back. */
	      paren_close = tmp.rfind (')');
	      if (paren_close == string::npos || paren_close < paren_open)
		gi->debug_print ("No matching right paren in " + tmp);
	      else
		{
		  /* Trim so "$round (...)$" (space before the paren) dispatches. */
		  string f_name = trim (tmp.substr (0, paren_open));
		  string f_args = tmp.substr (paren_open + 1,
					      paren_close - paren_open - 1);
		  func_eval = run_function (f_name, split_f_args (f_args));
		}
	    }
	  rv = rv + func_eval;
	  i = j;
	}
      else
	rv += s[i];
    }

  return rv;
}

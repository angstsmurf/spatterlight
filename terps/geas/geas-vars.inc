// geas-vars.inc -- String and numeric variables: find/get/set (scalar and
// array), and the "onchange" script hook that runs when index 0 of a variable
// is written.
//
// Split out of geas-runner.cc, which #includes it (same single-TU layout
// as the quest5/aslx-*.inc units); the code is unchanged from the split.

bool geas_implementation::find_ivar (const string &name, size_t &rv) const
{
  for (size_t n = 0; n < state.ivars.size(); n ++)
    if (ci_equal (state.ivars[n].name, name))
      {
	rv = n;
	return true;
      }
  return false;
}

bool geas_implementation::find_svar (const string &name, size_t &rv) const
{
  for (size_t n = 0; n < state.svars.size(); n ++)
    if (ci_equal (state.svars[n].name, name))
      {
	rv = n;
	return true;
      }
  return false;
}

bool geas_implementation::split_var_index (const string &varname, const char *who,
					   string &base, size_t &index) const
{
  std::string::size_type i1 = varname.find ('[');
  if (i1 == string::npos)
    {
      base = varname;
      index = 0;
      return true;
    }
  if (varname[varname.length() - 1] != ']')
    {
      gi->debug_print (string (who) + ": Badly formatted name " + varname);
      return false;
    }
  base = varname.substr (0, i1);
  string indextext = trim (varname.substr (i1+1, varname.length() - i1 - 2));
  GEAS_DBG << who << " (" << varname << ") --> (" << base << ", " << indextext << ")\n";

  /* A subscript is either a decimal literal or the name of a numeric variable
   * holding one.  Anything else -- including an undefined variable, which reads
   * back as the -32767 sentinel -- is refused rather than passed on as a
   * subscript; see kMaxVarIndex. */
  long value;
  bool is_literal = !indextext.empty ();
  for (size_t c3 = 0; c3 < indextext.size (); c3 ++)
    if (indextext[c3] < '0' || indextext[c3] > '9')
      {
	is_literal = false;
	break;
      }
  value = is_literal ? parse_int (indextext) : get_ivar (indextext);
  if (value < 0 || (size_t) value > kMaxVarIndex)
    {
      gi->debug_print (string (who) + ": Bad index [" + indextext + "] in " + varname);
      return false;
    }
  index = (size_t) value;
  return true;
}

void geas_implementation::set_svar (const string &varname, const string &varval)
{
  GEAS_DBG << "set_svar (" << varname << ", " << varval << ")\n";
  string base;
  size_t index;
  if (split_var_index (varname, "set_svar", base, index))
    set_svar (base, index, varval);
}

/* Fire the onchange script of the variable block named for the changed
 * variable, if any.  The LAST "onchange" line in the block wins, since Quest's
 * loader overwrites the field on each one. */
void geas_implementation::run_onchange_script (const string &varname)
{
  for (size_t varn = 0; varn < gf.size("variable"); varn ++)
    {
      const GeasBlock &go (gf.block ("variable", varn));
      if (ci_equal (go.name, varname))
	{
	  string script = "";
	  std::string::size_type c1, c2;
	  for (uint j = 0; j < go.data.size(); j ++)
	    /* CI: Quest reads the block's keywords with BeginsWith, which
	     * lowercases (V4Game.Part2.cs:715). */
	    if (ci_equal (first_token (go.data[j], c1, c2), "onchange"))
	      script = trim (go.data[j].substr (c2 + 1));
	  if (script != "")
	    run_script (script);
	}
    }
}

void geas_implementation::set_svar (const string &varname, size_t index, const string &varval)
{
  size_t n;
  if (!find_svar (varname, n))
    {
      /* Quest keeps string and numeric variables in two independent arrays
	 (_stringVariable / _numericVariable), so the same name can name one of
	 each and "#x#" and "%x%" then read different values.  Games rely on it:
	 "enter <players>" fills the string, and "set numeric <players;
	 #players#>" converts it (SetNumericVariableContents,
	 V4Game.Part2.cs:477-536, which never consults the string array).  geas
	 used to refuse the second definition and leave %players% undefined. */
      SVarRecord svr;
      svr.name = varname;
      n = state.svars.size();
      state.svars.push_back (svr);
    }
  state.svars[n].set(index, varval);
  if (index == 0)
    run_onchange_script (varname);
}

string geas_implementation::get_svar (const string &varname) const
{
  string base;
  size_t index;
  if (!split_var_index (varname, "get_svar", base, index))
    return "";
  return get_svar (base, index);
}
string geas_implementation::get_svar (const string &varname, size_t index) const
{
  /* Deferred regen: quest.objects/quest.formatobjects are rebuilt only when
   * one of them is read.  const_cast because the rebuild writes the two svars;
   * every reader observes exactly what eager regen would have shown it. */
  if (objs_vars_dirty_ && (ci_equal (varname, "quest.objects") ||
			   ci_equal (varname, "quest.formatobjects")))
    const_cast<geas_implementation *> (this)->regen_var_objects ();
  for (const auto &i: state.svars)
    {
      if (ci_equal (i.name, varname))
	return i.get(index);
    }

  gi->debug_print ("get_svar (" + varname + ", " + string_int (index) + "): No such variable defined.");
  return "";
}

int geas_implementation::get_ivar (const string &varname) const
{
  string base;
  size_t index;
  if (!split_var_index (varname, "get_ivar", base, index))
    return -32767;
  return get_ivar (base, index);
}
int geas_implementation::get_ivar (const string &varname, size_t index) const
{
  for (const auto &i: state.ivars)
    if (ci_equal (i.name, varname))
      return i.get(index);
  gi->debug_print ("get_ivar: Tried to read undefined int '" + varname +
		   "' [" + string_int(index) + "]");
  return -32767;
}
double geas_implementation::get_dvar (const string &varname) const
{
  string base;
  size_t index;
  if (!split_var_index (varname, "get_dvar", base, index))
    return -32767.0;
  return get_dvar (base, index);
}
double geas_implementation::get_dvar (const string &varname, size_t index) const
{
  for (const auto &i: state.ivars)
    if (ci_equal (i.name, varname))
      return i.getd(index);
  return -32767.0;
}
void geas_implementation::set_ivar (const string &varname, int varval)
{
  set_ivar (varname, (double) varval);
}

void geas_implementation::set_ivar (const string &varname, double varval)
{
  string base;
  size_t index;
  if (split_var_index (varname, "set_ivar", base, index))
    set_ivar (base, index, varval);
}

void geas_implementation::set_ivar (const string &varname, size_t index, double varval)
{
  size_t n;
  if (!find_ivar (varname, n))
    {
      /* A string variable of the same name is no obstacle -- see set_svar. */
      IVarRecord ivr;
      ivr.name = varname;
      n = state.ivars.size();
      state.ivars.push_back (ivr);
    }
  state.ivars[n].set(index, varval);
  if (index == 0)
    run_onchange_script (varname);
}

ostream &operator<< (ostream &o, const match_binding &mb)
{
  o << "MB['" << mb.var_name << "' == '" << mb.var_text << "' @ "
    << mb.start << " to " << mb.end << "]";
  return o;
}

string match_binding::tostring ()
{
  ostringstream oss;
  oss << *this;
  return oss.str();
}

ostream &operator << (ostream &o, const set<string> &s)
{
  o << "{ ";
  for (set<string>::const_iterator i = s.begin(); i != s.end(); i ++)
    {
      if (i != s.begin())
	o << ", ";
      o << (*i);
    }
  o << " }";
  return o;
}

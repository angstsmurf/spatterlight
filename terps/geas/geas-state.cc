/***************************************************************************
 *                                                                         *
 * Copyright (C) 2006 by Mark J. Tilford                                   *
 *                                                                         *
 * This file is part of Geas.                                              *
 *                                                                         *
 * Geas is free software; you can redistribute it and/or modify            *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * Geas is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with Geas; if not, write to the Free Software                     *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *                                                                         *
 ***************************************************************************/

#include "geas-state.hh"
#include "GeasRunner.hh"
#include "geas-util.hh"
#include "readfile.hh"
#include "general.hh"

#include <sstream>
#include <ostream>
#include <fstream>
#include <iostream>
#include <cstdlib>

using namespace std;

ostream &operator<< (ostream &o, const map <string, string> &m)
{
  for (auto i = m.cbegin(); i != m.cend(); i++)
    o << i->first << " -> " << i->second << "\n";
  return o;
}

class GeasOutputStream { 
  ostringstream o; 
  
public: 
  GeasOutputStream &put (string s) { o << s; o.put (char (0)); return *this; }
  GeasOutputStream &put (char ch) { o.put (ch); return *this; }
  GeasOutputStream &put (int i) { o << i; o.put (char (0)); return *this; }
  GeasOutputStream &put (uint i) { o << i; o.put (char (0)); return *this; }
  GeasOutputStream &put (unsigned long i) { o << i; o.put (char (0)); return *this; } // for Mac OS X ...
  GeasOutputStream &put (unsigned long long i) { o << i; o.put (char (0)); return *this; }
  

  /* The raw stream bytes, unobfuscated and headerless (the undo-history
   * serializer, which only ever travels inside a Spatterlight autosave). */
  string raw_contents () { return o.str(); }

  /* The full save as a self-contained string: a literal "QUEST300\0game\0"
   * header followed by the obfuscated body. */
  string contents (const string &gamename)
  {
    string out = string ("QUEST300") + char (0) + gamename + char (0);
    string tmp = o.str();
    for (uint i = 0; i < tmp.size(); i ++)
      out += char ((unsigned char) (255 - (unsigned char) tmp[i]));
    return out;
  }

  void write_out (const string &gamename, const string &savename)
  {
    ofstream ofs;
    ofs.open (savename.c_str(), std::ios::binary);
    if (!ofs.is_open())
      throw string ("Unable to open \"" + savename + "\"");
    string data = contents (gamename);
    ofs.write (data.data(), data.size());
    GEAS_DBG << "Done writing save game\n";
  }
};

/* Reads back what GeasOutputStream::put() wrote: \0-terminated fields for
 * strings/ints and single raw bytes for chars. */
class GeasInputStream {
  string data;
  size_t pos;
public:
  GeasInputStream (const string &d) : data (d), pos (0) {}
  bool eof () const { return pos >= data.size(); }
  size_t remaining () const { return pos < data.size() ? data.size() - pos : 0; }
  char get_char () { return pos < data.size() ? data[pos++] : char (0); }
  string get_str ()
  {
    string s;
    while (pos < data.size() && data[pos] != 0)
      s += data[pos++];
    if (pos < data.size())
      pos ++;   // skip the terminating null
    return s;
  }
  int get_int () { return atoi (get_str().c_str()); }
  uint get_uint () { return (uint) strtoul (get_str().c_str(), nullptr, 10); }
};

template <class T> void write_to (GeasOutputStream &gos, const vector<T> &v) 
{ 
  gos.put(v.size());
  for (const auto &i: v)
    {
      write_to (gos, i);
    }
}
  
void write_to (GeasOutputStream &gos, const string &s)
{
  gos.put (s);
}

void write_to (GeasOutputStream &gos, const PropertyRecord &pr)
{
  gos.put (pr.name).put (pr.data);
}

void write_to (GeasOutputStream &gos, const ObjectRecord &pr)
{
  gos.put (pr.name).put (char(pr.hidden ? 0 : 1))
    .put (char(pr.invisible ? 0 : 1)).put (pr.parent);
}

void write_to (GeasOutputStream &gos, const ExitRecord &er)
{
  gos.put (er.src).put (er.dest);
}

void write_to (GeasOutputStream &gos, const TimerRecord &tr)
{
  gos.put (tr.name).put (tr.is_running ? 0 : 1).put (tr.interval)
    .put (tr.timeleft);
}


/* The record's upper bound is written as its element count, so write exactly
 * max() + 1 elements: an (unexpected) empty record would otherwise announce one
 * element and write none, leaving the reader to consume the following field as
 * this array's data.  get() serves the usual "undefined" placeholder for any
 * index the record does not actually hold. */
void write_to (GeasOutputStream &gos, const SVarRecord &svr)
{
  gos.put (svr.name);
  gos.put (svr.max());
  for (size_t i = 0; i <= svr.max(); i ++)
    {
      gos.put (svr.get(i));
    }
}

void write_to (GeasOutputStream &gos, const IVarRecord &ivr)
{
  gos.put (ivr.name);
  gos.put (ivr.max());
  for (size_t i = 0; i <= ivr.max(); i ++)
    {
      gos.put (ivr.get(i));
    }
}

void write_to (GeasOutputStream &gos, const GeasState &gs)
{
  gos.put (gs.location);
  write_to (gos, gs.props);
  write_to (gos, gs.objs);
  write_to (gos, gs.exits);
  write_to (gos, gs.timers);
  write_to (gos, gs.svars);
  write_to (gos, gs.ivars);
  write_to (gos, gs.items);
}

void save_game_to (const std::string &gamename, const std::string &savename, const GeasState &gs)
{
  GeasOutputStream gos;
  write_to (gos, gs);
  gos.write_out (gamename, savename);
}

std::string serialize_game (const std::string &gamename, const GeasState &gs)
{
  GeasOutputStream gos;
  write_to (gos, gs);
  return gos.contents (gamename);
}

/* ---- undo history (Spatterlight autosave) ------------------------------- */

static void write_to (GeasOutputStream &gos, const UndoState &u)
{
  gos.put (char (u.running ? 1 : 0));
  gos.put (u.location);
  gos.put ((unsigned long long) u.props_len);
  write_to (gos, u.objs);
  write_to (gos, u.exits);
  write_to (gos, u.timers);
  write_to (gos, u.svars);
  write_to (gos, u.ivars);
  write_to (gos, u.items);
}

static const char *const kUndoHistoryMagic = "GEASUNDO1";

std::string serialize_undo_history (const std::vector<UndoState> &states)
{
  GeasOutputStream gos;
  gos.put ((unsigned long long) states.size());
  for (const UndoState &u : states)
    write_to (gos, u);
  /* Plain, un-obfuscated: this only ever travels inside the autosave. */
  return std::string (kUndoHistoryMagic) + char (0) + gos.raw_contents ();
}

bool deserialize_undo_history (const std::string &data,
                               std::vector<UndoState> &states)
{
  states.clear ();
  const string magic = kUndoHistoryMagic;
  if (data.size() < magic.size() + 1) return false;
  if (data.compare (0, magic.size(), magic) != 0 || data[magic.size()] != 0)
    return false;
  GeasInputStream gis (data.substr (magic.size() + 1));

  /* The same hostile-count guards deserialize_game uses: no element can
   * occupy less than a byte on the wire. */
  auto count = [&gis] (size_t &out) -> bool
    {
      size_t n = gis.get_uint();
      if (n > gis.remaining()) return false;
      out = n;
      return true;
    };
  auto elem_count = [&gis] (size_t &out) -> bool
    {
      size_t mx = gis.get_uint();
      if (mx >= gis.remaining()) return false;
      out = mx + 1;
      return true;
    };

  size_t nstates;
  if (!count (nstates)) return false;
  for (size_t si = 0; si < nstates; si++)
    {
      UndoState u;
      u.running = gis.get_char() != 0;
      u.location = gis.get_str();
      u.props_len = gis.get_uint();
      size_t n, cnt;
      if (!count (n)) return false;
      for (size_t i = 0; i < n; i++)
        { ObjectRecord o; o.name = gis.get_str(); o.hidden = (gis.get_char() == 0);
          o.invisible = (gis.get_char() == 0); o.parent = gis.get_str(); u.objs.push_back (o); }
      if (!count (n)) return false;
      for (size_t i = 0; i < n; i++)
        { string s = gis.get_str(), d = gis.get_str(); u.exits.push_back (ExitRecord (s, d)); }
      if (!count (n)) return false;
      for (size_t i = 0; i < n; i++)
        { TimerRecord t; t.name = gis.get_str(); t.is_running = (gis.get_int() == 0);
          t.interval = gis.get_uint(); t.timeleft = gis.get_uint(); u.timers.push_back (t); }
      if (!count (n)) return false;
      for (size_t i = 0; i < n; i++)
        { SVarRecord v; v.name = gis.get_str();
          if (!elem_count (cnt)) return false;
          for (size_t j = 0; j < cnt; j++) v.set (j, gis.get_str()); u.svars.push_back (v); }
      if (!count (n)) return false;
      for (size_t i = 0; i < n; i++)
        { IVarRecord v; v.name = gis.get_str();
          if (!elem_count (cnt)) return false;
          for (size_t j = 0; j < cnt; j++) v.set (j, gis.get_int()); u.ivars.push_back (v); }
      if (!count (n)) return false;
      for (size_t i = 0; i < n; i++)
        u.items.push_back (gis.get_str());
      states.push_back (u);
    }
  return true;
}

/* Parse a save produced by serialize_game()/save_game_to() into gs.  Returns
 * false if the data is not a recognisable Geas save. */
bool deserialize_game (const std::string &filedata, std::string &gamename, GeasState &gs)
{
  const string magic = "QUEST300";
  if (filedata.size() < magic.size() + 2) return false;
  if (filedata.compare (0, magic.size(), magic) != 0) return false;
  size_t p = magic.size();
  if (filedata[p] != 0) return false;
  p ++;
  gamename.clear();
  while (p < filedata.size() && filedata[p] != 0) gamename += filedata[p++];
  if (p >= filedata.size()) return false;
  p ++;   // skip the null after the game name

  string body;
  body.reserve (filedata.size() - p);
  for (size_t i = p; i < filedata.size(); i ++)
    body += char ((unsigned char) (255 - (unsigned char) filedata[i]));
  GeasInputStream gis (body);

  gs = GeasState();
  gs.running = true;
  gs.location = gis.get_str();

  /* A save file is untrusted input, and every count in it is a loop bound.  No
   * record or array element can occupy less than one byte on the wire (even an
   * empty string costs its NUL), so a count exceeding the bytes left in the
   * stream cannot be honest: reject the save rather than trying to allocate for
   * it.  Without this a corrupt (or hostile) count of a few billion sends us
   * resizing/pushing until we run out of memory. */
  auto count = [&gis] (size_t &out) -> bool
    {
      size_t n = gis.get_uint();
      if (n > gis.remaining()) return false;
      out = n;
      return true;
    };
  /* Same, for an array's highest index: it holds max()+1 elements. */
  auto elem_count = [&gis] (size_t &out) -> bool
    {
      size_t mx = gis.get_uint();
      if (mx >= gis.remaining()) return false;
      out = mx + 1;
      return true;
    };

  size_t n, cnt;
  if (!count (n)) return false;
  for (size_t i = 0; i < n; i ++)
    { string nm = gis.get_str(), d = gis.get_str(); gs.add_prop (nm, d); }
  if (!count (n)) return false;
  for (size_t i = 0; i < n; i ++)
    { ObjectRecord o; o.name = gis.get_str(); o.hidden = (gis.get_char() == 0);
      o.invisible = (gis.get_char() == 0); o.parent = gis.get_str(); gs.objs.push_back (o); }
  if (!count (n)) return false;
  for (size_t i = 0; i < n; i ++)
    { string s = gis.get_str(), d = gis.get_str(); gs.exits.push_back (ExitRecord (s, d)); }
  if (!count (n)) return false;
  for (size_t i = 0; i < n; i ++)
    { TimerRecord t; t.name = gis.get_str(); t.is_running = (gis.get_int() == 0);
      t.interval = gis.get_uint(); t.timeleft = gis.get_uint(); gs.timers.push_back (t); }
  if (!count (n)) return false;
  for (size_t i = 0; i < n; i ++)
    { SVarRecord v; v.name = gis.get_str();
      if (!elem_count (cnt)) return false;
      for (size_t j = 0; j < cnt; j ++) v.set (j, gis.get_str()); gs.svars.push_back (v); }
  if (!count (n)) return false;
  for (size_t i = 0; i < n; i ++)
    { IVarRecord v; v.name = gis.get_str();
      if (!elem_count (cnt)) return false;
      for (size_t j = 0; j < cnt; j ++) v.set (j, gis.get_int()); gs.ivars.push_back (v); }
  if (!gis.eof())   // items were appended by our serializer
    { if (!count (n)) return false;
      for (size_t i = 0; i < n; i ++) gs.items.push_back (gis.get_str()); }
  return true;
}

void GeasState::add_prop (const string &name, const string &data)
{
  /* Maintain the index incrementally only while it is valid; otherwise leave
     it to be rebuilt lazily on the next lookup (e.g. right after a load). */
  if (props_index.valid)
    props_index.map[lcase (name)].push_back (props.size ());
  props.push_back (PropertyRecord (name, data));
}

void GeasState::ensure_props_index () const
{
  if (props_index.valid)
    return;
  props_index.map.clear ();
  for (size_t i = 0; i < props.size (); i++)
    props_index.map[lcase (props[i].name)].push_back (i);
  props_index.valid = true;
}

const vector<size_t> *GeasState::prop_records (const string &name) const
{
  ensure_props_index ();
  /* Build the lowercased key into a reused buffer rather than allocating an
   * lcase() temporary on every lookup (this is on the get_obj_property /
   * get_obj_action runtime path). */
  string &key = props_index.key_scratch;
  size_t nn = name.size ();
  key.resize (nn);
  for (size_t i = 0; i < nn; i++)
    {
      unsigned char c = (unsigned char) name[i];
      key[i] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
    }
  auto it = props_index.map.find (key);
  return it == props_index.map.end () ? nullptr : &it->second;
}

UndoState GeasState::save_undo () const
{
  UndoState u;
  u.running = running;
  u.location = location;
  u.props_len = props.size ();   /* props is append-only: store length, not data */
  u.objs = objs;
  u.exits = exits;
  u.timers = timers;
  u.svars = svars;
  u.ivars = ivars;
  u.items = items;
  return u;
}

void GeasState::restore_undo (const UndoState &u)
{
  running = u.running;
  location = u.location;
  /* props only ever grows, so a snapshot's length is <= the live log and the
   * records below it are unchanged: truncate back to recover the old state.
   * The < guard is belt-and-suspenders (a snapshot can't out-length the live
   * log in normal play); we can't fabricate appended records, so leave props
   * untouched in that impossible case. */
  if (u.props_len <= props.size ())
    props.erase (props.begin () + u.props_len, props.end ());
  objs = u.objs;
  exits = u.exits;
  timers = u.timers;
  svars = u.svars;
  ivars = u.ivars;
  items = u.items;
  props_index.valid = false;   /* derived index no longer matches props */
}

GeasState::GeasState (GeasInterface &gi, const GeasFile &gf)
{
  running = false;

  GEAS_DBG << "GeasState::GeasState()" << endl;
  for (size_t i = 0; i < gf.size ("game"); i ++)
    {
      //const GeasBlock &go = gf.game[i];
      ObjectRecord data;
      data.name = "game";
      data.parent = "";
      data.hidden = false;
      data.invisible = true;
      objs.push_back (data);
    }

  GEAS_DBG << "GeasState::GeasState() done setting game" << endl;
  for (size_t i = 0; i < gf.size ("room"); i ++)
    {
      const GeasBlock &go = gf.block ("room", i);
      ObjectRecord data;
      data.name = go.name;
      data.parent = "";
      data.hidden = data.invisible = true;
      objs.push_back (data);
    }

  GEAS_DBG << "GeasState::GeasState() done setting rooms" << endl;
  for (size_t i = 0; i < gf.size ("object"); i++)
    {
      const GeasBlock &go = gf.block ("object", i);
      ObjectRecord data;
      data.name = go.name;
      if (go.parent == "")
	data.parent = "";
      else
	//data.parent = lcase (param_contents (go.parent));
	data.parent = param_contents (go.parent);
      /* An explicit "parent <X>" line puts the object inside container X rather
       * than in the room it is defined in. */
      for (const auto &line: go.data)
	{
	  std::string::size_type c1, c2;
	  if (first_token (line, c1, c2) == "parent")
	    {
	      std::string p = next_token (line, c1, c2);
	      if (is_param (p))
		data.parent = param_contents (p);
	      break;
	    }
	}
      /* "startin <room>": for a top-level object with no room from nesting or
       * an explicit parent, place it in the named room (Quest's startin). */
      if (data.parent == "")
	for (const auto &line: go.data)
	  {
	    std::string::size_type c1, c2;
	    if (first_token (line, c1, c2) == "startin")
	      {
		std::string p = next_token (line, c1, c2);
		if (is_param (p))
		  data.parent = param_contents (p);
		break;
	      }
	  }
      data.hidden = data.invisible = false;
      objs.push_back (data);
    }

  GEAS_DBG << "GeasState::GeasState() done setting objects" << endl;
  /* Characters (Quest "define character") are placed in the world like
   * objects so they can be looked at, spoken to, etc. */
  for (size_t i = 0; i < gf.size ("character"); i++)
    {
      const GeasBlock &go = gf.block ("character", i);
      ObjectRecord data;
      data.name = go.name;
      if (go.parent == "")
	data.parent = "";
      else
	data.parent = param_contents (go.parent);
      data.hidden = data.invisible = false;
      objs.push_back (data);
    }

  GEAS_DBG << "GeasState::GeasState() done setting characters" << endl;
  for (size_t i = 0; i < gf.size("timer"); i ++)
    {
      const GeasBlock &go = gf.block("timer", i);
      TimerRecord tr;
      string interval = "", status = "";
      for (uint j = 0; j < go.data.size(); j ++)
	{
	  string line = go.data[j];
	  std::string::size_type c1, c2;
	  string tok = first_token (line, c1, c2);
	  if (tok == "interval")
	    {
	      tok = next_token (line, c1, c2);
	      if (!is_param (tok))
		gi.debug_print (nonparam ("interval", line));
	      else
		interval = param_contents(tok);
	    }
	  else if (tok == "enabled" || tok == "disabled")
	    {
	      if (status != "")
		gi.debug_print ("Repeated status for timer");
	      else
		    status = tok;
	    }
	  else if (tok == "action")
	    {
	    }
	  else
	    {
	      gi.debug_print ("Bad timer line " + line);
	    }
	}
      tr.name = go.name;
      tr.is_running = (status == "enabled");
      tr.interval = tr.timeleft = parse_int (interval);
      timers.push_back (tr);
    }

  GEAS_DBG << "GeasState::GeasState() done with timers" << endl;
  for (size_t i = 0; i < gf.size("variable"); i ++)
    {
      const GeasBlock &go (gf.block("variable", i));
      GEAS_DBG << "GS::GS: Handling variable #" << i << ": " << go << endl;
      string vartype;
      string value;
      for (size_t j = 0; j < go.data.size(); j ++)
	{
	  string line = go.data[j];
	  GEAS_DBG << "   Line #" << j << " of var: \"" << line << "\"" << endl;
	  std::string::size_type c1, c2;
	  string tok = first_token (line, c1, c2);
	  if (tok == "type")
	    {
	      tok = next_token (line, c1, c2);
	      if (tok == "")
		gi.debug_print (string("Missing variable type in ")
				+ string_geas_block (go));
	      else if (vartype != "")
		gi.debug_print (string ("Redefining var. type in ")
				+ string_geas_block (go));
	      else if (tok == "numeric" || tok == "string")
		vartype = tok;
	      else
		gi.debug_print (string ("Bad var. type ") + line);
	    }
	  else if (tok == "value")
	    {
	      tok = next_token (line, c1, c2);
	      if (!is_param (tok))
		gi.debug_print (string ("Expected parameter in " + line));
	      else
		value = param_contents (tok);
	    }
	  else if (tok == "display" || tok == "onchange")
	    {
	    }
	  else
	    {
	      gi.debug_print (string ("Bad var. line: ") + line);
	    }
	}
      if (vartype == "" || vartype == "numeric")
	{
	  IVarRecord ivr;
	  ivr.name = go.name;
	  ivr.set (0, parse_int (value));
	  ivars.push_back (ivr);
	}
      else
	{
	  SVarRecord svr;
	  svr.name = go.name;
	  svr.set (0, value);
	  svars.push_back (svr);
	}
    }
  GEAS_DBG << "GeasState::GeasState() done with variables" << endl;
}

ostream &operator<< (ostream &o, const PropertyRecord &pr) 
{ 
  o << pr.name << ", data == " << pr.data; 
  return o; 
}

ostream &operator<< (ostream &o, const ObjectRecord &objr) 
{ 
  o << objr.name << ", parent == " << objr.parent; 
  if (objr.hidden) 
    o << ", hidden"; 
  if (objr.invisible)
    o << ", invisible";
  return o; 
}

ostream &operator<< (ostream &o, const ExitRecord &er) 
{ 
  return o << er.src << ": " << er.dest;
}

ostream &operator<< (ostream &o, const TimerRecord &tr) 
{
  return o << tr.name << ": " << (tr.is_running ? "" : "not ") << "running (" 
	   << tr.timeleft << " // " << tr.interval << ")"; 
}

ostream &operator<< (ostream &o, const SVarRecord &sr) 
{ 
  o << sr.name << ": ";
  if (sr.size () == 0)
    o << "(empty)";
  else if (sr.size() <= 1)
    o << "<" << sr.get(0) << ">";
  else
    for (uint i = 0; i < sr.size(); i ++)
      {
	o << i << ": <" << sr.get(i) << ">";
	if (i + 1 < sr.size())
	  o << ", ";
      }
  return o;
}

ostream &operator<< (ostream &o, const IVarRecord &ir) 
{ 
  o << ir.name << ": ";
  if (ir.size () == 0)
    o << "(empty)";
  else if (ir.size() <= 1)
    o << ir.get(0);
  else
    for (uint i = 0; i < ir.size(); i ++)
      {
	o << i << ": " << ir.get(i);
	if (i + 1 < ir.size())
	  o << ", ";
      }
  return o;
}

ostream &operator<< (ostream &o, const GeasState &gs)
{
  o << "location == " << gs.location << "\nprops: \n";
 
  for (size_t i = 0; i < gs.props.size(); i ++)
    o << "    " << i << ": " << gs.props[i] << "\n";

  o << "objs:\n";
  for (size_t i = 0; i < gs.objs.size(); i ++)
    o << "    " << i << ": " << gs.objs[i] << "\n";

  o << "exits:\n";
  for (size_t i = 0; i < gs.exits.size(); i ++)
    o << "    " << i << ": " << gs.exits[i] << "\n";

  o << "timers:\n";
  for (size_t i = 0; i < gs.timers.size(); i ++)
    o << "    " << i << ": " << gs.timers[i] << "\n";

  o << "string variables:\n";
  for (size_t i = 0; i < gs.svars.size(); i ++)
    o << "    " << i << ": " << gs.svars[i] << "\n";

  o << "integer variables:\n";
  for (size_t i = 0; i < gs.ivars.size(); i ++)
    o << "    " << i << ": " << gs.ivars[i] << "\n";

  return o;
}

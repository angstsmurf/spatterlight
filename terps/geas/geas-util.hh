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

#ifndef __geas_util_hh
#define __geas_util_hh

#include "general.hh"

#define ARRAYSIZE(ar)  ((sizeof(ar))/(sizeof(*ar)))

#include <string>
#include "readfile.hh"
#include <map>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cassert>
#include <sstream>

typedef std::vector<std::string> vstring;

static inline int parse_int (const std::string &s) { return atoi(s.c_str()); }

extern vstring split_param (const std::string &s);
extern vstring split_f_args (const std::string &s);

extern bool is_param (const std::string &s);
extern std::string param_contents (const std::string &s);

extern std::string nonparam (const std::string &, const std::string &);

/* One game-scope verb declaration, as read from a line of the `game' block.
 * Quest writes these as either
 *   verb <name[;synonym;...][:key]> <default script>     (asl source) or
 *   <name[;synonym;...][:key]> <default script>          (compiled form),
 * optionally prefixed with "lib" for a library-supplied verb. */
struct GameVerbLine
{
  bool is_lib;             /* the "lib" prefix was present */
  std::string names_tok;   /* the <...> naming the verb, still unresolved */
  std::string deflt;       /* the default script that follows it */

  GameVerbLine () : is_lib (false) {}
};

/* True if LINE is such a declaration, filling OUT.  The name list comes back as
 * the raw parameter token because the two readers of the `game' block have to
 * resolve it differently -- try_game_verb with eval_param, the verb-menu
 * builder (which is const) with param_contents.  Resolve it, then hand the
 * result to split_verb_names. */
extern bool parse_game_verb_line (const std::string &line, GameVerbLine &out);

/* Split a *resolved* verb name list into the names the verb answers to and the
 * key it looks for on the object.  The key is whatever follows a ':', and the
 * colon is taken out of the name list; with no colon it is the first (or only)
 * name (V4Game.cs:2958-2976).  So "verb <hoist;lift:raise>" is typed as HOIST or
 * LIFT, labelled "Hoist", and looks for `action <raise>'.
 *
 * Quest walks the list with a do-while that trims every segment it splits off
 * at a semicolon but leaves the *last* one (the whole parameter, when there is
 * no semicolon at all) exactly as written (V4Game.cs:2978-3003).  A verb
 * declared "verb <use >" is therefore matched as BeginsWith (cmd, "use  ") --
 * two spaces -- and can never fire.  QDK 4.1.5 emits such names whenever the
 * author leaves a trailing space in the verb box, and games rely on the
 * resulting dead verbs without knowing it: "final of social studies.cas"
 * declares "verb <use >" ahead of "verb <use key in>", and only because the
 * former is unmatchable does USE KEY IN CLOSED DOOR reach the door's
 * "action <use key in>" and open the one locked door in the game.  Trimming
 * here made "use " swallow every USE in the file.  Callers must therefore trim
 * each name themselves before using it as text.
 *
 * The split is on ';' alone, so the "verb <pick up; take, take>" that QDK
 * writes when an author types a comma registers "pick up" and "take, take", not
 * "take" -- which is why TAKE in that game falls through to the later
 * "verb <take>". */
extern void split_verb_names (const std::string &names_str,
			      vstring &names, std::string &key);

extern std::string string_geas_block (const GeasBlock &);

extern bool starts_with (const std::string &, const std::string &);
extern bool ends_with (const std::string &, const std::string &);
extern bool starts_with_i (const std::string &, const std::string &);
extern bool ends_with_i (const std::string &, const std::string &);

template <
typename T,
typename = typename std::enable_if<std::is_integral<T>::value, T>::type
>
std::string string_int (T i)
{
  std::ostringstream o;
  o << i;
  return o.str();
}

extern std::string trim_braces (const std::string &s);

/* Evaluate a numeric expression in double precision: signed decimal literals
 * joined by + - * / with the usual precedence (* and / before + and -), left
 * to right.  Used by Quest's $round(...)$ and other floating-point math. */
extern double eval_double (const std::string &s);
/* The arithmetic `set numeric` does below ASL 3.91.  ExpressionHandler is not
 * reached at those versions: ExecSetVar finds one operator -- the first "+",
 * else the first "*", else the first "/", else a "-" anywhere but the front --
 * and performs that single operation, reading each side with VB's Val(), which
 * takes as much of a number as it finds and stops (V4Game.cs:7286-7343).  So
 * "10*3/4" is thirty, and there is no precedence to speak of.  DIV_BY_ZERO is
 * set when a division was refused, which Quest logs and answers with zero. */
extern double eval_double_pre391 (const std::string &expr, bool &div_by_zero);
/* Format a numeric value the way .NET's Double.ToString() does, which is the
 * form Quest's ExpressionHandler hands an arithmetic result back in: the
 * shortest decimal that reads back as the same double, plain digits between
 * exponent -4 and 16 and scientific outside that ("0.3333333333333333",
 * "1E+18"). */
extern std::string fmt_double_net (double d);
/* Format a numeric value the way Quest *displays* one -- VB's Str(), which is
 * fmt_double_net minus the leading zero of a fraction (".3333333333333333",
 * "-.5"); an integral value prints with no decimal point at all ("37", "-5"). */
extern std::string fmt_double (double d);
/* Evaluate a string as a complete arithmetic expression, the way Quest's
 * ExpressionHandler does: true (and the formatted value in RESULT) only if the
 * whole string is arithmetic and holds an operator, false -- leaving RESULT
 * untouched -- for a bare number, a non-numeric operand, or a division by zero.
 * Used for the two sides of an "is" comparison; see eval_is_operand. */
extern bool eval_numeric_expr (const std::string &s, std::string &result);

extern std::string pcase (std::string s);
extern std::string ucase (std::string s);
extern std::string lcase (std::string s);

/* The canonical index key for a name: surrounding whitespace dropped and the
 * ASCII letters folded to lower case -- i.e. lcase (trim (s)), but written into
 * a caller-owned buffer so a hot lookup allocates nothing.  Every name-keyed
 * table in the engine folds this way and they must agree byte for byte:
 * GeasFile::name_index and obj_types (build_name_key, obj_type_of) and
 * GeasState's props_index / objs_index (PropsIndex::find) are all probed with
 * names the game typed in whatever case and spacing it liked, and a block is
 * registered under its trim()'d name (see readfile.cc).  "Something 'Bout A
 * Hex" needs the trim: it defines its journal as `<Journal >' and reads it back
 * as `<Journal>'.  The fold is ASCII-only, matching ci_equal and lcase (), so a
 * hit here is exactly what the linear ci_equal scans these indices replaced
 * would have matched -- and, unlike the locale-aware tolower (), it does not
 * pay for a locale lookup per character on the property-lookup hot path.
 *
 * fold_lower_append extends `out`; fold_lower_into replaces it.  Neither
 * releases the buffer's capacity, which is what keeps a reused scratch string
 * allocation-free.  `out` and `s` must not alias. */
extern void fold_lower_append (std::string &out, const std::string &s);
extern void fold_lower_into (std::string &out, const std::string &s);

/* Re-encode a line of game text from Windows-1252 to UTF-8.  Quest is a VB6
 * program: it reads its files a byte at a time through Chr(), which maps them
 * through the thread's ANSI code page, and on the Windows the games were
 * written for that is always 1252.  So an author's `©`, `£`, `—` or curly
 * quote is one byte in the file, and the eight-bit range 0x80-0x9F -- which
 * Latin-1 leaves as control codes -- carries the typographic characters QDK
 * inserts on its own.  Passing those bytes through to Glk shows mojibake, and
 * a Latin-1 reading turns the QDK ones into nothing at all. */
extern std::string cp1252_to_utf8 (const std::string &s);

/* True when the whole string is already well-formed UTF-8 (pure ASCII counts).
 * Overlong forms and surrogates are not rejected: this is the sniff that keeps
 * cp1252_to_utf8 off a file some later author saved as UTF-8, not a
 * validator. */
extern bool text_is_utf8 (const std::string &s);

//ostream &operator<< (ostream &o, const vector<string> &v);
//template<class T> std::ostream &operator<< (std::ostream &o, const std::vector<T> &v) { return o;}

/*
template<class K, class V, class CMP, class ALLOC> ostream &operator<< (ostream &o, map<K, V, CMP, ALLOC> &m)
{
  //map <K,V, CMP, ALLOC>::iterator i;
  std::string i;
  for (i = m.begin(); i != m.end(); i ++)
    ;
  //o << "    " << i->first << ", " << i->second << "\n";
  return o;
};
*/



template<class T> std::ostream &operator << (std::ostream &o, std::vector<T> v)
{
  o << "{ '";
  for (size_t i = 0; i < v.size(); i ++)
    {
      o << v[i];
      if (i + 1 < v.size())
	o << "', '";
    }
  o << "' }";
  return o;
}

/* Take the map and key by const reference: passing the map by value (as this
 * did originally) deep-copied the entire red-black tree on every call just to
 * do one lookup, which dominated the runtime profile because obj_types is
 * probed on every property/action lookup inside the per-object scope loops. */
template <class KEYTYPE, class VALTYPE> bool has (const std::map<KEYTYPE, VALTYPE> &m, const KEYTYPE &key) { return m.find (key) != m.end(); };

class Logger
{
 public:
  Logger ();
  ~Logger ();

 private:
  class Nullstreambuf : public std::streambuf
  {
   protected:
    int overflow (int c)
    {
      return traits_type::not_eof (c);
    }
  };

  std::ofstream *logfilestr_;
  std::streambuf *cerrbuf_;
  static Nullstreambuf cnull;
};

#endif

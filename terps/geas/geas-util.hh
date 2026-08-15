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

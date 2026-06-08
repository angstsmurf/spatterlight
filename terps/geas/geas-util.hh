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
/* Format a numeric value the way Quest displays one: an integral value prints
 * with no decimal point ("37", "-5"), otherwise with trailing zeros trimmed
 * ("0.498", "14.8"). */
extern std::string fmt_double (double d);

extern std::string pcase (std::string s);
extern std::string ucase (std::string s);
extern std::string lcase (std::string s);

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

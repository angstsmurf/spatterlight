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

#include "geas-util.hh"
#include <sstream>
#include <cctype>
#include <cmath>
#include <cstdio>
#include "general.hh"
#include "istring.hh"

using namespace std;


/* Recursive-descent evaluator over a position into the string:
 *   expr   := term (('+'|'-') term)*
 *   term   := factor (('*'|'/') factor)*
 *   factor := ('+'|'-') factor | '(' expr ')' | number
 * Unparseable tail is ignored, matching Quest's lenient evaluation. */
namespace {
  struct DoubleParser {
    const string &s;
    size_t i = 0, n;
    DoubleParser (const string &str) : s (str), n (str.length ()) {}
    void skipws () { while (i < n && isspace ((unsigned char) s[i])) i++; }
    double expr () {
      double v = term ();
      for (;;) {
	skipws ();
	if (i < n && (s[i] == '+' || s[i] == '-')) {
	  char op = s[i++];
	  double r = term ();
	  v = (op == '+') ? v + r : v - r;
	} else break;
      }
      return v;
    }
    double term () {
      double v = factor ();
      for (;;) {
	skipws ();
	if (i < n && (s[i] == '*' || s[i] == '/')) {
	  char op = s[i++];
	  double r = factor ();
	  if (op == '*') v *= r;
	  else v = (r != 0.0) ? v / r : 0.0;
	} else break;
      }
      return v;
    }
    double factor () {
      skipws ();
      if (i < n && (s[i] == '+' || s[i] == '-')) {
	char op = s[i++];
	double v = factor ();
	return (op == '-') ? -v : v;
      }
      if (i < n && s[i] == '(') {
	i++;
	double v = expr ();
	skipws ();
	if (i < n && s[i] == ')') i++;
	return v;
      }
      size_t start = i;
      while (i < n && isdigit ((unsigned char) s[i])) i++;
      if (i < n && s[i] == '.') { i++; while (i < n && isdigit ((unsigned char) s[i])) i++; }
      if (i == start) return 0;
      return atof (s.substr (start, i - start).c_str ());
    }
  };
}

double eval_double (const string &s)
{
  DoubleParser p (s);
  return p.expr ();
}

string fmt_double (double d)
{
  if (d == floor (d) && fabs (d) < 1e15)
    {
      char buf[32];
      snprintf (buf, sizeof buf, "%.0f", d);
      return buf;
    }
  char buf[64];
  snprintf (buf, sizeof buf, "%.10g", d);
  return buf;
}

string trim_braces (const string &s)
{
  if (s.length() > 1 && s[0] == '[' && s[s.length() - 1] == ']')
    return s.substr (1, s.length() - 2);
  else
    return s;
}

bool is_param (const string &s)
{
  return s.length() > 1 && s[0] == '<' && s[s.length() - 1] == '>';
}

string param_contents (const string &s)
{
  if (!is_param (s))
    {
      // No GeasInterface here to notify the player; log and return the string
      // unchanged so a malformed token degrades instead of crashing.
      GEAS_DBG << "param_contents: not a parameter: " << s << endl;
      return s;
    }
  return s.substr (1, s.length() - 2);
}

string nonparam (const string &type, const string &var)
{
  return "Non-parameter for " + type + " in \"" + var + "\"";
}

//ostream &operator << (ostream &o, const GeasBlock &gb) { return o; }
//string trim (string s, trim_modes) { return s; }

std::string string_geas_block (const GeasBlock &gb)
{
  ostringstream oss;
  oss << gb;
  return oss.str();
}


bool starts_with (const string &a, const string &b)
{
  return (a.length() >= b.length()) && (a.substr (0, b.length()) == b);
}
bool ends_with (const string &a, const string &b)
{
  return (a.length() >= b.length()) && 
    (a.substr (a.length() - b.length(), b.length()) == b);
}

bool starts_with_i (const string &a, const string &b)
{
  return (a.length() >= b.length()) && ci_equal (a.substr (0, b.length()), b);
  //  return starts_with (lcase(a), lcase(b));
}
bool ends_with_i (const string &a, const string &b)
{
  return (a.length() >= b.length()) && 
    ci_equal (a.substr (a.length() - b.length(), b.length()), b);
  //return ends_with (lcase(a), lcase(b));
}

string pcase (string s)
{
  if (s.length() == 0)
    {
      return s;
    }
  if (islower (s[0]))
    {
      s[0] = toupper(s[0]);
    }
  return s;
}

string ucase (string s)
{
  for (uint i = 0; i < s.length(); i ++)
    s[i] = toupper (s[i]);
  return s;
}

// There's a good chance s is already all-lowercase, in which case
// the test will avoid making a copy
string lcase (string s)
{
  /* ASCII fold rather than the locale-aware isupper/tolower (which showed up
   * hot in the profile): geas works in ASCII/Latin-1 and sets no locale, so
   * this matches the old C-locale behaviour while avoiding the per-char locale
   * lookups.  Matches the fold in ci_equal / GeasFile::build_name_key. */
  for (char &c : s)
    if (c >= 'A' && c <= 'Z')
      c += 32;
  return s;
}

vector<string> split_param (const string &s)
{
  vector<string> rv;
  std::string::size_type c1 = 0, c2;

  for (;;)
    {
      c2 = s.find (';', c1);
      if (c2 == string::npos)
	{
	  rv.push_back (trim (s.substr (c1)));
	  return rv;
	}
      rv.push_back (trim (s.substr (c1, c2-c1)));
      c1 = c2 + 1;
    }
}

vector<string> split_f_args (const string &s)
{
  vector<string> rv = split_param (s);
  for (auto &i: rv)
    {
      /* An empty argument -- "$f()$", or the middle of "$f(a;;b)$" -- made
       * i.length() - 1 wrap to SIZE_MAX, reading (and, if that stray byte
       * happened to be '_', writing) outside the string. */
      if (i.empty ())
	continue;
      /* Quest brackets an argument in underscores to protect leading/trailing
       * spaces; turn those back into the spaces they stand for. */
      if (i[0] == '_')
	{
	  i[0] = ' ';
	}
      if (i[i.length() - 1] == '_')
	{
	  i[i.length() - 1] = ' ';
	}
    }
  return rv;
}

void show_split (const string &s)
{
  vector<string> tmp = split_param (s);
  GEAS_DBG << "Splitting <" << s << ">: ";
  for (const auto &i: tmp)
    GEAS_DBG << "<" << i << ">, ";
  GEAS_DBG << "\n";
}

Logger::Nullstreambuf Logger::cnull;

Logger::Logger ()
    : logfilestr_(NULL), cerrbuf_(NULL)
{
  cerr.flush ();

  const char *const logfile = getenv ("GEAS_LOGFILE");
  if (logfile)
    {
      ofstream *filestr = new ofstream (logfile);
      if (filestr->fail ())
        delete filestr;
      else
        {
          logfilestr_ = filestr;
          cerrbuf_ = cerr.rdbuf (filestr->rdbuf ());
        }
    }

  if (!cerrbuf_)
    cerrbuf_ = cerr.rdbuf (&cnull);
}

Logger::~Logger () {
  cerr.flush ();

  cerr.rdbuf (cerrbuf_);
  cerrbuf_ = NULL;

  if (logfilestr_)
    {
      logfilestr_->close ();
      delete logfilestr_;
      logfilestr_ = NULL;
    }
}

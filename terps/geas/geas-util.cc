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
#include <cstdlib>
#include <cstring>
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

/* VB's Val(): read as much of a number off the front as there is, and answer
   zero when there is none.  Quest uses it on both sides of the one operation it
   performs below 3.91, which is why "3 apples" counts as three. */
static double vb_val_num (const string &s)
{
  return strtod (s.c_str (), NULL);
}

/* See the comment on the declaration in geas-util.hh. */
double eval_double_pre391 (const string &expr, bool &div_by_zero)
{
  div_by_zero = false;
  /* ObscureNumericExps (V4Game.cs:3434-3455) blanks the character after every
     "E" so that the sign of an exponent -- 2.345E+20 -- cannot be mistaken for
     an operator.  The operator is then looked for in this copy but read out of
     the original, so the two must stay in step character for character. */
  string obscured = expr;
  for (string::size_type e = obscured.find ('E'); e != string::npos;
       e = obscured.find ('E', e + 2))
    {
      if (e + 1 < obscured.length ())
	obscured[e + 1] = 'X';
      else
	obscured += 'X';
    }
  /* One operation only, and the search order is the operator's own: the first
     "+" anywhere, else the first "*", else the first "/", else a "-" at the
     second character or later -- a leading one being the sign of a negative
     number (ExecSetVar, V4Game.cs:7286-7304). */
  string::size_type op = obscured.find ('+');
  if (op == string::npos)
    op = obscured.find ('*');
  if (op == string::npos)
    op = obscured.find ('/');
  if (op == string::npos && obscured.length () > 1)
    op = obscured.find ('-', 1);
  if (op == string::npos || op >= expr.length ())
    return vb_val_num (expr);
  double num1 = vb_val_num (expr.substr (0, op));
  double num2 = vb_val_num (expr.substr (op + 1));
  switch (expr[op])
    {
    case '+': return num1 + num2;
    case '-': return num1 - num2;
    case '*': return num1 * num2;
    case '/':
      if (num2 == 0.0)
	{
	  div_by_zero = true;
	  return 0.0;
	}
      return num1 / num2;
    }
  return vb_val_num (expr);
}

/* A stricter cousin of DoubleParser: it refuses everything Quest's
 * ExpressionHandler refuses.  Quest splits the expression on + - * / and
 * demands that every element between the operators be numeric
 * (V4Game.cs:3190-3200), so an operand that is not arithmetic through and
 * through fails instead of being read up to the first unusable character. */
namespace {
  struct StrictParser {
    const string &s;
    size_t i = 0, n;
    bool ok = true;
    StrictParser (const string &str) : s (str), n (str.length ()) {}
    void skipws () { while (i < n && isspace ((unsigned char) s[i])) i++; }
    double expr () {
      double v = term ();
      for (;;) {
	skipws ();
	if (ok && i < n && (s[i] == '+' || s[i] == '-')) {
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
	if (ok && i < n && (s[i] == '*' || s[i] == '/')) {
	  char op = s[i++];
	  double r = factor ();
	  if (op == '*')
	    v *= r;
	  else if (r == 0.0)
	    /* Quest reports "Division by zero" as a failed expression, leaving
	       the operand as it was.  */
	    { ok = false; return 0; }
	  else
	    v /= r;
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
	if (i < n && s[i] == ')')
	  i++;
	else
	  ok = false;
	return v;
      }
      size_t start = i;
      while (i < n && isdigit ((unsigned char) s[i])) i++;
      if (i < n && s[i] == '.') { i++; while (i < n && isdigit ((unsigned char) s[i])) i++; }
      if (i == start || (i == start + 1 && s[start] == '.')) { ok = false; return 0; }
      return atof (s.substr (start, i - start).c_str ());
    }
  };
}

bool eval_numeric_expr (const string &s, string &result)
{
  /* A bare number is arithmetic too, but Quest hands it back verbatim -- the
     result of an expression without operators is the element itself, not a
     reformatted number -- so leave those alone and keep "5.0" as "5.0". */
  if (s.find_first_of ("+-*/()") == string::npos)
    return false;
  StrictParser p (s);
  double v = p.expr ();
  p.skipws ();
  if (!p.ok || p.i != p.n)
    return false;
  /* An arithmetic result is stringified with plain Double.ToString()
     (ExpressionHandler, V4Game.cs:3272), which keeps the leading zero of a
     fraction where the Str() used for display drops it. */
  result = fmt_double_net (v);
  return true;
}

/* .NET's Double.ToString(): the shortest decimal string that reads back as the
   same double, laid out the way the "G" format does it -- plain digits while
   the exponent of the leading digit is above -5 and below 17, scientific
   outside that range, with an uppercase E and a signed two-digit exponent.
   Quest hands arithmetic results to the game in this form, and its display of a
   numeric variable is this with one character removed (see fmt_double). */
string fmt_double_net (double d)
{
  if (d == 0.0)
    return "0";
  char buf[64];
  int prec;
  for (prec = 1; prec < 17; prec ++)
    {
      snprintf (buf, sizeof buf, "%.*e", prec - 1, d);
      if (strtod (buf, NULL) == d)
	break;
    }
  snprintf (buf, sizeof buf, "%.*e", prec - 1, d);
  const char *epos = strchr (buf, 'e');
  int exp = (epos != NULL) ? atoi (epos + 1) : 0;
  if (exp > -5 && exp < 17)
    {
      int dec = prec - 1 - exp;
      if (dec < 0)
	dec = 0;
      snprintf (buf, sizeof buf, "%.*f", dec, d);
      return buf;
    }
  /* C already writes the exponent signed and at least two digits wide, so only
     the case of the "e" is left to fix. */
  string rv = buf;
  for (char &c: rv)
    if (c == 'e')
      c = 'E';
  return rv;
}

/* VB's Str(), which is how Quest prints a numeric variable or a collectable
   (Trim(Conversion.Str(...)), V4Game.cs:6721): the .NET rendering with the
   leading zero of a fraction dropped, so a third is ".3333333333333333" and a
   negative half is "-.5". */
string fmt_double (double d)
{
  string rv = fmt_double_net (d);
  if (rv.compare (0, 2, "0.") == 0)
    rv.erase (0, 1);
  else if (rv.compare (0, 3, "-0.") == 0)
    rv.erase (1, 1);
  return rv;
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

/* The 32 places where Windows-1252 and Latin-1 part company.  Everything
 * outside 0x80-0x9F is its own codepoint in both.  The five holes (0x81,
 * 0x8D, 0x8F, 0x90, 0x9D) are unassigned in the code page; .NET's
 * Encoding.GetEncoding(1252) -- which is what the QuestViva oracle reads with
 * -- passes them through as the matching C1 control, so do the same. */
static const unsigned short CP1252_HIGH[32] =
{
  0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
  0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
  0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
  0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
};

string cp1252_to_utf8 (const string &s)
{
  string rv;
  rv.reserve (s.size());
  for (unsigned char c : s)
    {
      unsigned int cp = (c < 0x80) ? c
	                : (c < 0xA0) ? CP1252_HIGH[c - 0x80]
	                : c;
      if (cp < 0x80)
	rv += (char) cp;
      else if (cp < 0x800)
	{
	  rv += (char) (0xC0 | (cp >> 6));
	  rv += (char) (0x80 | (cp & 0x3F));
	}
      else
	{
	  rv += (char) (0xE0 | (cp >> 12));
	  rv += (char) (0x80 | ((cp >> 6) & 0x3F));
	  rv += (char) (0x80 | (cp & 0x3F));
	}
    }
  return rv;
}

bool text_is_utf8 (const string &s)
{
  for (size_t i = 0; i < s.size();)
    {
      unsigned char c = (unsigned char) s[i];
      int len;
      if (c < 0x80) { i++; continue; }
      if ((c & 0xE0) == 0xC0)      len = 2;
      else if ((c & 0xF0) == 0xE0) len = 3;
      else if ((c & 0xF8) == 0xF0) len = 4;
      else return false;
      if (i + len > s.size())
	return false;
      for (int k = 1; k < len; k++)
	if (((unsigned char) s[i + k] & 0xC0) != 0x80)
	  return false;
      i += len;
    }
  return true;
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

#include "istring.hh"
#include "general.hh"
#include <cstring>
#include <iostream>

using namespace std;

CI_EQUAL   ci_equal_obj;
CI_LESS    ci_less_obj;
CI_LESS_EQ ci_less_eq_obj;

// Code for testing case insensitively by John Harrison 

bool c_equal_i(char ch1, char ch2)
{
  /* ASCII fold, not locale tolower(): this is the per-character comparator in
   * match_command's inner matching loop, so the locale lookups were hot. */
  unsigned char a = (unsigned char) ch1, b = (unsigned char) ch2;
  if (a == b)
    return true;
  if (a >= 'A' && a <= 'Z') a += 32;
  if (b >= 'A' && b <= 'Z') b += 32;
  return a == b;
}

/*
bool ci_less_eq (char ch1, char ch2)
{
  return tolower((unsigned char)ch1) <= tolower((unsigned char)ch2);
}

bool ci_less (char ch1, char ch2)
{
  return tolower((unsigned char)ch1) < tolower((unsigned char)ch2);
}
*/

size_t ci_find(const string& str1, const string& str2)
{
  string::const_iterator pos = search (str1.begin (), str1.end (), 
				       str2.begin (), str2.end (), c_equal_i);
  if (pos == str1.end())
    return string::npos;
  else
    return pos - str1.begin ();
}



bool ci_equal(const string &str1, const string &str2) {
  /* Hand-rolled instead of strcasecmp(): this is the hottest comparison in the
   * engine (object/property/type name matching in per-object loops), and
   * strcasecmp() routes through the locale-aware strcasecmp_l() which dominated
   * the profile.  A length check rejects the common mismatch with no scan, and
   * exact-equal characters need no folding; we only ASCII-fold on a mismatch.
   * geas works in ASCII/Latin-1 and sets no locale, so this matches the old
   * behaviour (C-locale strcasecmp folds only A-Z too). */
  size_t n = str1.size();
  if (n != str2.size())
    return false;
  const unsigned char *a = (const unsigned char *) str1.data();
  const unsigned char *b = (const unsigned char *) str2.data();
  for (size_t i = 0; i < n; i++)
    if (a[i] != b[i])
      {
	unsigned char ca = a[i], cb = b[i];
	if (ca >= 'A' && ca <= 'Z') ca += 32;
	if (cb >= 'A' && cb <= 'Z') cb += 32;
	if (ca != cb)
	  return false;
      }
  return true;
}
bool ci_less_eq(const string &str1, const string &str2) {
  return strcasecmp (str1.c_str(), str2.c_str()) <= 0;
}
bool ci_less(const string &str1, const string &str2) {
  return strcasecmp (str1.c_str(), str2.c_str()) < 0;
}
bool ci_notequal(const string &str1, const string &str2) {
  GEAS_DBG << "ci_notequal ('" << str1 << "', '" << str2 << "') -> " 
	    << ( strcasecmp (str1.c_str(), str2.c_str()) != 0) << "\n";
  return strcasecmp (str1.c_str(), str2.c_str()) != 0; }
bool ci_gt_eq(const string &str1, const string &str2) {
  return strcasecmp (str1.c_str(), str2.c_str()) >= 0;
}
bool ci_gt(const string &str1, const string &str2) {
  return strcasecmp (str1.c_str(), str2.c_str()) > 0;
}


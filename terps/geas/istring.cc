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
  return tolower((unsigned char)ch1) == tolower((unsigned char)ch2);
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
  return strcasecmp (str1.c_str(), str2.c_str()) == 0;
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

